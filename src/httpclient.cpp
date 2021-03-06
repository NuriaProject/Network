/* Copyright (c) 2014-2015, The Nuria Project
 * The NuriaProject Framework is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 * 
 * The NuriaProject Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with The NuriaProject Framework.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "nuria/httpclient.hpp"

#include <QSslSocket>
#include <QTcpSocket>
#include <QDateTime>
#include <QProcess>
#include <QDir>

#include <nuria/temporarybufferdevice.hpp>
#include "nuria/httpurlencodedreader.hpp"
#include "nuria/httpmultipartreader.hpp"
#include "nuria/httptransport.hpp"
#include "nuria/httpbackend.hpp"
#include "nuria/httpparser.hpp"
#include "nuria/httpserver.hpp"
#include "nuria/httpwriter.hpp"
#include "nuria/websocket.hpp"
#include "nuria/httpnode.hpp"
#include <nuria/logger.hpp>

#include "private/standardfilters.hpp"
#include "private/websocketreader.hpp"
#include "private/httpprivate.hpp"

Nuria::HttpClient::HttpClient (HttpTransport *transport, HttpServer *server)
	: QIODevice (transport), d_ptr (new HttpClientPrivate)
{
	
	// Initialize
	this->d_ptr->bufferDevice = new TemporaryBufferDevice (this);
	this->d_ptr->transport = transport;
	this->d_ptr->server = server;
	
	// 
	setOpenMode (QIODevice::ReadWrite);
	connect (this, &HttpClient::disconnected, this, &QIODevice::aboutToClose);
	
}

Nuria::HttpClient::~HttpClient () {
	delete this->d_ptr;
}

Nuria::HttpTransport *Nuria::HttpClient::transport () const {
	return this->d_ptr->transport;
}

bool Nuria::HttpClient::readRangeRequestHeader () {
	
	auto it = this->d_ptr->requestHeaders.constFind (httpHeaderName (HeaderRange));
	if (it != this->d_ptr->requestHeaders.constEnd ()) {
		HttpParser parser;
		
		QByteArray value = *it;
		return parser.parseRangeHeaderValue (value, this->d_ptr->rangeStart, this->d_ptr->rangeEnd);
	}
	
	return true;
}

void Nuria::HttpClient::readRequestCookies () {
	HttpParser parser;
	
	// Already read?
	if (!this->d_ptr->requestCookies.isEmpty ()) {
		return;
	}
	
	// Allow multiple 'Cookie' headers.
	QList< QByteArray > headers = this->d_ptr->requestHeaders.values (httpHeaderName (HeaderCookie));
	for (const QByteArray &data : headers) {
		if (!parser.parseCookies (data, this->d_ptr->requestCookies)) {
			nError() << "Failed to parse cookies" << data;
			this->d_ptr->requestCookies.clear ();
			return;
		}
		
	}
	
}

static QByteArray redirectMessage (const QByteArray &location, const QByteArray &display) {
	QByteArray msg = QByteArrayLiteral("Redirecting to <a href=\"");
	msg.append (location);
	msg.append ("\">");
	msg.append (display);
	msg.append ("</a>");
	
	return msg;
}

bool Nuria::HttpClient::sendRedirectResponse (const QByteArray &location, const QByteArray &display, int code) {
	if (this->d_ptr->headerSent) {
		return false;
	}
	
	// Rewrite 307 to 301 for HTTP/1.0 clients (307 was introduced in HTTP/1.1)
	if (this->d_ptr->requestVersion == Http1_0 && code == 307) {
		code = 301;
	}
	
	// Set response headers
	this->d_ptr->responseCode = code;
	this->d_ptr->responseHeaders.replace (httpHeaderName (HeaderLocation), location);
	
	// Write short redirect message if this is not a HEAD request
	if (this->d_ptr->requestType != HEAD) {
		QByteArray message = redirectMessage (location, display);
		return (writeData (message.constData (), message.length ()) > 0);
	}
	
	// HEAD request, only make sure that the response header is sent.
	return sendResponseHeader ();	
}

void Nuria::HttpClient::updateRequestedUrl () {
	QByteArray host = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderHost));
	
	// Host header set, add it to the requested URL.
	if (!host.isEmpty ()) {
		QString hostName = QString::fromLatin1 (host);
		int portIdx = hostName.indexOf (QLatin1Char (':'));
		
		// Read port part
		if (portIdx > 0) {
			int port = hostName.midRef (portIdx + 1).toInt ();
			hostName.chop (hostName.length () - portIdx);
			this->d_ptr->path.setPort (port);
		}
		
		// Set hostname
		this->d_ptr->path.setHost (hostName);
	} else {
		// Set hostname and port by server configuration
		this->d_ptr->path.setHost (this->d_ptr->server->fqdn ());
		bool secure = this->d_ptr->transport->isSecure ();
		int usedPort = this->d_ptr->transport->backend ()->port ();
		
		// Set non-standard port
		if ((secure && usedPort != 443) || (!secure && usedPort != 80)) {
			this->d_ptr->path.setPort (usedPort);
		}
		
	}
	
}

void Nuria::HttpClient::bytesSent (qint64 bytes) {
	if (this->d_ptr->pipeDevice && this->d_ptr->pipeDevice->isReadable ()) {
		QMetaObject::invokeMethod (this, "pipeToClientReadyRead", Qt::QueuedConnection);
	}
	
	emit bytesWritten (bytes);
}

qint64 Nuria::HttpClient::parseIntegerHeaderValue (const QByteArray &value) {
	bool ok = false;
	
	if (value.isEmpty ()) {
		return -1;
	}
	
	qint64 result = value.toLongLong (&ok);
	return (!ok) ? -1 : result;
}

bool Nuria::HttpClient::readPostBodyContentLength () {
	QByteArray contentLength = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderContentLength));
	
	// Reject if the request shouldn't have a body but has one.
	if (!requestHasPostBody ()) {
		if (!contentLength.isEmpty ()) {
			killConnection (400);
			return false;
		}
		
		return true;
	}
	
	// 
	this->d_ptr->postBodyLength = parseIntegerHeaderValue (contentLength);
	
	// If there is no Content-Length header, kill the connection
	if (this->d_ptr->postBodyLength < 0) {
		killConnection (400);
		return false;
	}
	
	// Everything is fine
	return true;
}

bool Nuria::HttpClient::send100ContinueIfClientExpectsIt () {
	static const QByteArray continue100 = QByteArrayLiteral("100-continue");
	
	if (!requestHasPostBody () || this->d_ptr->headerSent) {
		return true;
	}
	
	QByteArray expects = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderExpect));
	if (expects.isEmpty ()) {
		return true;
	}
	
	if (expects.toLower () != continue100) {
		// Unknown value
		return false;
	}
	
	// Respond with '100 Continue'
	HttpWriter writer;
	QByteArray data = writer.writeResponseLine (this->d_ptr->requestVersion, 100, QByteArray ());
	data.append ("\r\n");
	
	this->d_ptr->transport->sendToRemote (this, data);
	this->d_ptr->transport->flush (this);
	
	return true;
}

static inline bool byteArrayCanReadLine (const QByteArray &data) {
	return data.contains ("\r\n");
}

static inline QByteArray byteArrayReadLine (QByteArray &data) {
	enum { MaxLength = 4096 };
	
	int idx = data.indexOf ("\r\n");
	if (idx > MaxLength) {
		return QByteArray ();
	}
	
	// 
	QByteArray line = data.left (idx + 2);
	data = data.mid (idx + 2);
	return line;
}

bool Nuria::HttpClient::readAllAvailableHeaderLines (QByteArray &data) {
	HttpParser parser;
	
	while (byteArrayCanReadLine (data)) {
		QByteArray raw = byteArrayReadLine (data);
		
		// A line should end in \r\n, but also check for \n.
		if (!parser.removeTrailingNewline (raw)) {
			// Line too long, kill connection
			killConnection (400);
			return false;
			
		} else if (raw.isEmpty ()) {
			this->d_ptr->headerReady = true;
			
			// If the line is empty, the header is complete.
			return postProcessRequestHeader ();
			
		} else if (this->d_ptr->requestVersion == HttpUnknown) {
			// This is the first line.
			if (!readFirstLine (raw)) {
				killConnection (400);
				return false;
			}
			
		} else if (!readHeader (raw)) {
			killConnection (400);
			return false;
		}
		
	}
	
	// 
	return true;
}

void Nuria::HttpClient::initPath (QByteArray path) {
	if (this->d_ptr->transport->isSecure ()) {
		path.prepend ("https://localhost");
	} else {
		path.prepend ("http://localhost");
	}
	
	// 
	this->d_ptr->path = QUrl::fromEncoded (path);	
}

bool Nuria::HttpClient::readFirstLine (const QByteArray &line) {
	HttpParser parser;
	
	QByteArray path;
	bool success = false;
	
	success = parser.parseFirstLineFull (line, this->d_ptr->requestType,
					     path, this->d_ptr->requestVersion);
	
	// 
	initPath(path);
	return success;
}

bool Nuria::HttpClient::readHeader (const QByteArray &line) {
	HttpParser parser;
	QByteArray key;
	QByteArray value;
	
	if (!parser.parseHeaderLine (line, key, value)) {
		killConnection (400);
		return false;
	}
	
	// Store
	this->d_ptr->requestHeaders.insert (parser.correctHeaderKeyCase (key), value);
	return true;
}

bool Nuria::HttpClient::isReceivedHeaderHttp11Compliant () {
	if (this->d_ptr->requestVersion != Http1_1) {
		return true;
	}
	
	return this->d_ptr->requestHeaders.contains (httpHeaderName (HeaderHost));
}

bool Nuria::HttpClient::verifyPostRequestCompliance () {
	if (!requestHasPostBody ()) {
		return true;
	}
	
	return this->d_ptr->requestHeaders.contains (httpHeaderName (HeaderContentLength));
}

bool Nuria::HttpClient::verifyCompleteHeader () {
	
	// Verify first line
	if (this->d_ptr->requestVersion == HttpUnknown ||
	    this->d_ptr->requestType == InvalidVerb ||
	    this->d_ptr->path.path ().isEmpty ()) {
		killConnection (400);
		return false;
	}
	
	// Verify header
	if (!readRangeRequestHeader () || !isReceivedHeaderHttp11Compliant () ||
	    !verifyPostRequestCompliance ()) {
		killConnection (400);
		return false;
	}
	
	// Has the slot closed the connection?
	if (openMode () == QIODevice::NotOpen) {
		return false;
	}
	
	// 
	updateRequestedUrl ();
	return true;
}

bool Nuria::HttpClient::closeConnectionIfNoLongerNeeded () {
	if (!requestHasPostBody () && !this->d_ptr->keepConnectionOpen &&
	    !this->d_ptr->pipeDevice) {
		close ();
		return true;
	}
	
	return false;
}

bool Nuria::HttpClient::requestHasPostBody () const {
	return (this->d_ptr->requestType == POST || this->d_ptr->requestType == PUT);
}

void Nuria::HttpClient::addFilter (StandardFilter filter) {
	HttpFilter *instance = nullptr;
	
	switch (filter) {
	case DeflateFilter:
		instance = Internal::DeflateFilter::instance ();
		break;
	case GzipFilter:
		instance = Internal::GzipFilter::instance ();
		break;
	}
	
	// Remove the last one if it's a internal one
	if (!this->d_ptr->filters.isEmpty () &&
	    (this->d_ptr->filters.last ()->metaObject () == &Internal::DeflateFilter::staticMetaObject ||
	     (this->d_ptr->filters.last ()->metaObject () == &Internal::GzipFilter::staticMetaObject))) {
		this->d_ptr->filters.removeLast ();
	}
	
	// 
	this->d_ptr->filters.append (instance);
	
}

void Nuria::HttpClient::addFilter (HttpFilter *filter) {
	this->d_ptr->filters.append (filter);
}

void Nuria::HttpClient::removeFilter (StandardFilter filter) {
	switch (filter) {
	case DeflateFilter:
		removeFilter (Internal::DeflateFilter::instance ());
		break;
	case GzipFilter:
		removeFilter (Internal::GzipFilter::instance ());
		break;
	}
	
}

void Nuria::HttpClient::removeFilter (HttpFilter *filter) {
	int idx = this->d_ptr->filters.indexOf (filter);
	
	if (idx != -1) {
		this->d_ptr->filters.remove (idx);
	}
	
}

static QByteArray relativePathToAbsolute (const QString &localPath) {
	QByteArray path = localPath.toLatin1 ();
	
	if (!localPath.startsWith (QLatin1Char ('/'))) {
		path.prepend ('/');
	}
	
	// 
	return path;
}

static int findPort (Nuria::HttpServer *server, bool isSecure) {
	QVector< Nuria::HttpBackend * > backends = server->backends ();
	for (int i = 0; i < backends.length (); i++) {
		Nuria::HttpBackend *cur = backends.at (i);
		if (cur->isSecure () == isSecure) {
			return cur->port ();
		}
		
	}
	
	// 
	nWarn() << "Server" << server << "does not have a back-end with isSecure =" << isSecure;
	return -1;
}

static QUrl redirectionSchemeUrl (const QString &localPath, bool toSecure, Nuria::HttpClientPrivate *d_ptr) {
	static const QString http = QStringLiteral("http");
	static const QString https = QStringLiteral("https");
	
	bool isSecure = d_ptr->transport->isSecure ();
	int port = d_ptr->transport->backend ()->port ();
	QUrl url = d_ptr->path;
	
	
	// Find port if switching security level
	if (isSecure != toSecure) {
		port = findPort (d_ptr->server, toSecure);
	} else if (d_ptr->path.port () != -1) {
		port = d_ptr->path.port ();
	}
	
	// Standard port?
	if ((toSecure && port == 443) || (!toSecure && port == 80)) {
		port = -1;
	}
	
	// Set scheme and path
	url.setScheme (toSecure ? https : http);
	url.setPath (localPath);
	url.setPort (port);
	return url;
}

bool Nuria::HttpClient::redirectClient (const QString &localPath, RedirectMode mode, int statusCode) {
	QByteArray url;
	
	// Transform URL
	switch (mode) {
	case RedirectMode::Keep:
		url = relativePathToAbsolute (localPath);
		break;
	case RedirectMode::ForceSecure:
	case RedirectMode::ForceUnsecure:
		url = redirectionSchemeUrl (localPath, (mode == RedirectMode::ForceSecure), this->d_ptr).toEncoded ();
		break;
	}
	
	// Redirect
	return sendRedirectResponse (url, localPath.toUtf8 (), statusCode);
}

bool Nuria::HttpClient::redirectClient (const QUrl &remoteUrl, int statusCode) {
	return sendRedirectResponse (remoteUrl.toEncoded (), remoteUrl.toDisplayString ().toUtf8 (), statusCode);
}

bool Nuria::HttpClient::postProcessRequestHeader () {
	if (verifyCompleteHeader () &&
	    readPostBodyContentLength () &&
	    readConnectionHeader () &&
	    send100ContinueIfClientExpectsIt () &&
	    invokeRequestedPath ()) {
		return true;
	}
	
	// Failed.
	killConnection (400);
	return false;
}

bool Nuria::HttpClient::contentTypeIsMultipart (const QByteArray &value) const {
	return value.startsWith ("multipart/form-data;");
}

bool Nuria::HttpClient::contentTypeIsUrlEncoded (const QByteArray &value) const {
	return value.startsWith ("application/x-www-form-urlencoded");
}

Nuria::HttpPostBodyReader *Nuria::HttpClient::createHttpMultiPartReader (const QByteArray &header) {
	static const QByteArray boundaryStr = QByteArrayLiteral("boundary=");
	int idx = header.indexOf (boundaryStr);
	
	if (idx == -1) {
		return nullptr;
	}
	
	// 
	QByteArray boundary = header.mid (idx + boundaryStr.length ());
	if (boundary.isEmpty ()) {
		return nullptr;
	}
	
	// 
	return new HttpMultiPartReader (this, boundary, this);
}

Nuria::HttpPostBodyReader *Nuria::HttpClient::createUrlEncodedPartReader (const QByteArray &header) {
	static const QByteArray charsetStr = QByteArrayLiteral("charset=");
	int idx = header.indexOf (charsetStr);
	
	QByteArray charset = QByteArrayLiteral("latin-1");
	if (idx != -1) {
		charset = header.mid (idx + charsetStr.length ());
	}
	
	// 
	return new HttpUrlEncodedReader (this, charset, this);
}

bool Nuria::HttpClient::sendPipeChunkToClient () {
	enum { MaxChunkSize = 16 * 1024 };
	
	// 
	if (!this->d_ptr->pipeDevice || !this->d_ptr->pipeDevice->isReadable ()) {
		return false;
	}
	
	// Respect maxLen
	qint64 toRead = MaxChunkSize;
	if (this->d_ptr->pipeMaxlen >= 0 && this->d_ptr->pipeMaxlen < MaxChunkSize) {
		toRead = this->d_ptr->pipeMaxlen;
	}
	
	// 
	QByteArray data = this->d_ptr->pipeDevice->read (toRead);
	this->d_ptr->pipeMaxlen -= data.length ();
	if (!data.isEmpty ()) {
		
		// Write data into the client stream
		if (writeDataInternal (data) != data.length ()) {
			return false;
		}
		
	}
	
	// 
	if (this->d_ptr->pipeMaxlen == 0) {
		return false;
	}
	
	return !this->d_ptr->pipeDevice->atEnd ();
}

bool Nuria::HttpClient::invokeRequestedPath () {
	if (!resolveUrl (this->d_ptr->path)) {
	        killConnection (403);
	        return false;
	}
	
	closeConnectionIfNoLongerNeeded ();
	return true;
}

bool Nuria::HttpClient::readConnectionHeader () {
	HttpParser parser;
	
	// 
	QByteArray connection = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderConnection));
	this->d_ptr->transferMode = parser.decideTransferMode (this->d_ptr->requestVersion, connection);
	this->d_ptr->connectionMode = ConnectionKeepAlive;
	
	// Close connection?
	if (this->d_ptr->transferMode == Streaming ||
	    (this->d_ptr->transport->maxRequests () != -1 &&
	    this->d_ptr->transport->currentRequestCount () >= this->d_ptr->transport->maxRequests ())) {
		
		this->d_ptr->connectionMode = ConnectionClose;
		if (this->d_ptr->transferMode == ChunkedStreaming) {
			this->d_ptr->transferMode = Streaming;
		}
		
	}
	
	// 
	return true;
}

qint64 Nuria::HttpClient::readData (char *data, qint64 maxlen) {
	if (!this->d_ptr->bufferDevice) {
		return 0;
	}
	
	// Read the data from a buffer.
	return this->d_ptr->bufferDevice->read (data, maxlen);
}

qint64 Nuria::HttpClient::writeData (const char *data, qint64 len) {
	if (this->d_ptr->pipeDevice) {
		return -1;
	}
	
	// Use the out-buffer instead if wanted
	if (this->d_ptr->outBuffer) {
		return this->d_ptr->outBuffer->write (data, len);
	}
	
	return writeDataInternal (QByteArray::fromRawData (data, len));
}

qint64 Nuria::HttpClient::writeDataInternal (QByteArray data) {
	bool initFilters = !this->d_ptr->headerSent;
	sendResponseHeader ();
	
	// 
	if (initFilters) {
		QByteArray head = filterInit ();
		if (!head.isEmpty () && !sendData (head)) {
			return -1;
		}
		
	}
	
	// 
	qint64 origLength = data.length ();
	if (!filterData (data)) {
		return -1;
	}
	
	// Send chunk
	if (sendData (data)) {
		return origLength;
	}
	
	return -1;
}

bool Nuria::HttpClient::sendData (const QByteArray &data) {
	if (this->d_ptr->transferMode == ChunkedStreaming) {
		return sendChunkedData (data);
	}
	
	return this->d_ptr->transport->sendToRemote (this, data);
}

bool Nuria::HttpClient::resolveUrl (const QUrl &url) {
	return this->d_ptr->server->invokeByPath (this, url.path ());
}

bool Nuria::HttpClient::bufferPostBody (QByteArray &data) {
	
	// Determine how much to read
	int toRead = data.length ();
	if (this->d_ptr->postBodyLength >= 0 &&
	    this->d_ptr->postBodyTransferred + toRead > this->d_ptr->postBodyLength) {
		if (this->d_ptr->connectionMode == ConnectionClose) {
			killConnection (413);
			return false;
		}
		
		toRead = this->d_ptr->postBodyLength - this->d_ptr->postBodyTransferred;
		this->d_ptr->postBodyTransferred = this->d_ptr->postBodyLength;
	} else {
		this->d_ptr->postBodyTransferred += toRead;
	}
	
	// Write the received data into the buffer
	this->d_ptr->bufferDevice->write (data.left (toRead));
	data = data.mid (toRead);
	
	// Emit postBodyComplete() when the transfer is complete
	if (this->d_ptr->postBodyTransferred == this->d_ptr->postBodyLength) {
		
		// If we're piping the body into a process, we need to close
		// the writing channel for some applications to start their work.
		QProcess *process = qobject_cast< QProcess * > (this->d_ptr->bufferDevice);
		if (process) {
			process->closeWriteChannel ();
		}
		
		emit postBodyComplete ();
	}
	
	return true;
	
}

void Nuria::HttpClient::clientDisconnected () {
	
	// Mark the connection as closed
	setOpenMode (QIODevice::NotOpen);
	
	// Emit disconnected signal
	emit aboutToClose ();
	
	// and delete this later on.
	deleteLater ();
	
}

void Nuria::HttpClient::processData (QByteArray &data) {
	
	// Has the HTTP header been received from the client?
	if (!this->d_ptr->headerReady) {
		if (!readAllAvailableHeaderLines (data) || data.isEmpty ()) {
			return;
		}
		
	}
	
	// Clients sending data outside of a POST or PUT requests are killed.
	if (!requestHasPostBody ()) {
		killConnection (400);
		return;
	}
		
	// Buffer POST data. Kill the connection if something goes wrong.
	if (!bufferPostBody (data)) {
		killConnection (413);
		return;
	}
	
	// If we're in non-streamed mode, we call the
	// associated slot if the POST body is complete.
	if (!(this->d_ptr->slotInfo.isValid () && this->d_ptr->slotInfo.streamPostBody ()) &&
	    this->d_ptr->postBodyLength == this->d_ptr->postBodyTransferred) {
		QProcess *process = qobject_cast< QProcess * > (this->d_ptr->bufferDevice);
		
		if (!process && this->d_ptr->slotInfo.isValid ()) {
			this->d_ptr->bufferDevice->reset ();
		}
		
		if (this->d_ptr->slotInfo.isValid ()) {
			HttpNode::callSlot (this->d_ptr->slotInfo, this);
		}
		
		if (!this->d_ptr->keepConnectionOpen &&
		    (!process || process->state () == QProcess::NotRunning)) {
			close ();
		}
		
	} else {
		emit readyRead (); // Streamed mode
	}
	
}

bool Nuria::HttpClient::sendChunkedData (const QByteArray &data) {
	QByteArray chunk = QByteArray::number (data.length (), 16);
	chunk.append ("\r\n", 2);
	chunk.append (data);
	chunk.append ("\r\n", 2);
	
	// 
	return this->d_ptr->transport->sendToRemote (this, chunk);
}

void Nuria::HttpClient::pipeToClientReadyRead () {
	
	// 
	if (!sendPipeChunkToClient ()) {
		
		// Special case for QProcess: Don't kill the connection while the
		// process is starting up.
		QProcess *process = qobject_cast< QProcess * > (this->d_ptr->pipeDevice);
		if (process && process->state () != QProcess::NotRunning)
			return;
		
		// Disconnect aboutToClose() as it may lead to infinite recursion
		if (this->d_ptr->pipeDevice) {
			disconnect (this->d_ptr->pipeDevice, 0, this, 0);
			this->d_ptr->pipeDevice->close ();
		}
		
		// Send response header. This is important to do if the user
		// piped an empty buffer.
		sendResponseHeader ();
		closeInternal ();
	}
	
}

bool Nuria::HttpClient::isHeaderReady () const {
	return this->d_ptr->headerReady;
}

bool Nuria::HttpClient::responseHeaderSent () const {
	return this->d_ptr->headerSent;
}

Nuria::HttpClient::HttpVerb Nuria::HttpClient::verb () const {
	return this->d_ptr->requestType;
}

QUrl Nuria::HttpClient::path () const {
	return this->d_ptr->path;
}

bool Nuria::HttpClient::hasRequestHeader (const QByteArray &key) const {
	return this->d_ptr->requestHeaders.contains (key);
}

bool Nuria::HttpClient::hasRequestHeader (Nuria::HttpClient::HttpHeader header) const {
	return this->d_ptr->requestHeaders.contains (httpHeaderName (header));
}

QByteArray Nuria::HttpClient::requestHeader (const QByteArray &key) const {
	return this->d_ptr->requestHeaders.value (key);
}

QByteArray Nuria::HttpClient::requestHeader (Nuria::HttpClient::HttpHeader header) const {
	return this->d_ptr->requestHeaders.value (httpHeaderName (header));
}

QList< QByteArray > Nuria::HttpClient::requestHeaders (const QByteArray &key) const {
	return this->d_ptr->requestHeaders.values (key);
}

QList< QByteArray > Nuria::HttpClient::requestHeaders (Nuria::HttpClient::HttpHeader header) const {
	return this->d_ptr->requestHeaders.values (httpHeaderName (header));
}

Nuria::HttpClient::HeaderMap Nuria::HttpClient::requestHeaders() const {
	return this->d_ptr->requestHeaders;
}

bool Nuria::HttpClient::hasResponseHeader (const QByteArray &key) const {
	return this->d_ptr->responseHeaders.contains (key);
}

bool Nuria::HttpClient::hasResponseHeader (HttpClient::HttpHeader header) const {
	return this->d_ptr->responseHeaders.contains (httpHeaderName (header));
}

Nuria::HttpClient::HeaderMap Nuria::HttpClient::responseHeaders() const {
	return this->d_ptr->responseHeaders;
}

QList< QByteArray > Nuria::HttpClient::responseHeaders (const QByteArray &key) const {
	return this->d_ptr->responseHeaders.values (key);
}

QList< QByteArray > Nuria::HttpClient::responseHeaders (Nuria::HttpClient::HttpHeader header) const {
	return this->d_ptr->responseHeaders.values (httpHeaderName (header));
}

bool Nuria::HttpClient::setResponseHeader (const QByteArray &key, const QByteArray &value, bool append) {
	
	if (this->d_ptr->headerSent)
		return false;
	
	if (!append) {
		this->d_ptr->responseHeaders.remove (key);
	}
	
	this->d_ptr->responseHeaders.insert (key, value);
	
	return true;
}

bool Nuria::HttpClient::setResponseHeader (Nuria::HttpClient::HttpHeader header,
					   const QByteArray &value, bool append) {
	return setResponseHeader (httpHeaderName (header), value, append);
}

bool Nuria::HttpClient::setResponseHeaders (const HeaderMap &headers) {
	
	if (this->d_ptr->headerSent)
		return false;
	
	this->d_ptr->responseHeaders = headers;
	return true;
	
}

bool Nuria::HttpClient::pipeToClient (QIODevice *device, qint64 maxlen) {
	if (!device || !device->isReadable ())
		return false;
	
	this->d_ptr->pipeDevice = device;
	this->d_ptr->pipeMaxlen = maxlen;
	device->setParent (this);
	
	// Connect to readyRead() and aboutToClose() signals
	connect (device, &QIODevice::readyRead, this, &HttpClient::pipeToClientReadyRead);
	connect (device, &QIODevice::aboutToClose, this, &HttpClient::pipeToClientReadyRead);
	
	// If it is a QProcess, connect to its finished() signal
	QProcess *process = qobject_cast< QProcess * > (device);
	if (process) {
		connect (process, SIGNAL(finished(int)), SLOT(pipeToClientReadyRead()));
	}
	
	// If it's a random-access device, we can send a proper Content-Length header.
	if (!device->isSequential () && !this->d_ptr->headerSent &&
	    !this->d_ptr->responseHeaders.contains (httpHeaderName (HeaderContentLength)) &&
	    this->d_ptr->filters.isEmpty ()) {
		
		setContentLength (device->size ());
		
	}
	
	// Decide on Chunked or Streaming mode
	if (!this->d_ptr->headerSent && !this->d_ptr->outBuffer) {
		if (this->d_ptr->connectionMode == ConnectionKeepAlive &&
		    (this->d_ptr->transferMode != Buffered || !this->d_ptr->filters.isEmpty ())) {
			this->d_ptr->transferMode = ChunkedStreaming;
		} else {
			this->d_ptr->transferMode = Streaming;
		}
		
	}
	
	// Read currently available data
	if (!device->isSequential () || device->bytesAvailable () > 0) {
		pipeToClientReadyRead ();
	}
	
	return true;
}

bool Nuria::HttpClient::pipeFromPostBody (QIODevice *device, bool takeOwnership) {
	Q_CHECK_PTR(device);
	
	// Copy the current buffer into the new one
	if (this->d_ptr->bufferDevice) {
		this->d_ptr->bufferDevice->reset ();
		device->write (this->d_ptr->bufferDevice->readAll ());
		delete this->d_ptr->bufferDevice;
	}
	
	this->d_ptr->bufferDevice = device;
	if (takeOwnership) {
		device->setParent (this);
	}
	
	setOpenMode (WriteOnly);
	return true;
}

qint64 Nuria::HttpClient::rangeStart () const {
	return this->d_ptr->rangeStart;
}

qint64 Nuria::HttpClient::rangeEnd () const {
	return this->d_ptr->rangeEnd;
}

qint64 Nuria::HttpClient::contentLength () const {
	return this->d_ptr->contentLength;
}

bool Nuria::HttpClient::setRangeStart (qint64 pos) {
	if (this->d_ptr->headerSent) {
		return false;
	}
	
	this->d_ptr->rangeStart = pos;
	return true;
}

bool Nuria::HttpClient::setRangeEnd (qint64 pos) {
	if (this->d_ptr->headerSent) {
		return false;
	}
	
	this->d_ptr->rangeEnd = pos;
	return true;
}

bool Nuria::HttpClient::setContentLength (qint64 length) {
	if (this->d_ptr->headerSent) {
		return false;
	}
	
	this->d_ptr->contentLength = length;
	return true;
}

void Nuria::HttpClient::close () {
	
	// Do nothing if close() has been called already.
	if (openMode () == QIODevice::NotOpen)
		return;
	
	// 
	if (this->d_ptr->headerReady && !this->d_ptr->headerSent && !this->d_ptr->outBuffer) {
		sendResponseHeader ();
	}
	
	// Pipe the out-buffer if we have one
	if (this->d_ptr->outBuffer) {
		TemporaryBufferDevice *buffer = this->d_ptr->outBuffer;
		this->d_ptr->outBuffer = nullptr;
		buffer->reset ();
		
		pipeToClient (buffer);
		return;
	}
	
	// 
	closeInternal ();
}

void Nuria::HttpClient::closeInternal () {
	if (this->d_ptr->connectionClosed) {
		return;
	}
	
	// 
	this->d_ptr->connectionClosed = true;
	setOpenMode (QIODevice::NotOpen);
	emit aboutToClose ();
	
	// 
	QByteArray data = filterDeinit ();
	if (!data.isEmpty ()) {
		sendData (data);
	}
	
	// Indicate end of stream for chunked streaming mode.
	if (this->d_ptr->transferMode == ChunkedStreaming) {
		static const QByteArray chunkedEnd = QByteArrayLiteral("0\r\n\r\n");
		this->d_ptr->transport->sendToRemote (this, chunkedEnd);
	}
	
	// 
	this->d_ptr->transport->close (this);
}

QByteArray Nuria::HttpClient::filterInit () {
	QByteArray data;
	
	for (int i = 0, total = this->d_ptr->filters.length (); i < total; ++i) {
		HttpFilter *filter = this->d_ptr->filters.at (i);
		data.append (filter->filterBegin (this));
	}
	
	return data;
}

QByteArray Nuria::HttpClient::filterDeinit () {
	QByteArray data;
	
	for (int i = 0, total = this->d_ptr->filters.length (); i < total; ++i) {
		HttpFilter *filter = this->d_ptr->filters.at (i);
		data.append (filter->filterEnd (this));
	}
	
	return data;
}

bool Nuria::HttpClient::filterData (QByteArray &data) {
	for (int i = 0, total = this->d_ptr->filters.length (); i < total; ++i) {
		HttpFilter *filter = this->d_ptr->filters.at (i);
		if (!filter->filterData (this, data)) {
			return false;
		}
		
	}
	
	// Done.
	return true;
}

bool Nuria::HttpClient::filterHeaders (HeaderMap &headers) {
	for (int i = 0, total = this->d_ptr->filters.length (); i < total; ++i) {
		HttpFilter *filter = this->d_ptr->filters.at (i);
		
		// 
		QByteArray name = filter->filterName ();
		if (!name.isEmpty ()) {
			addFilterNameToHeader (headers, name);
		}
		
		// 
		if (!filter->filterHeaders (this, headers)) {
			return false;
		}
		
	}
	
	// Done.
	return true;
}

void Nuria::HttpClient::addFilterNameToHeader (HeaderMap &headers, const QByteArray &name) {
	QByteArray value = headers.value (httpHeaderName (HeaderContentEncoding));
	if (value.isEmpty ()) {
		value = name;
	} else {
		value.append (", ");
		value.append (name);
	}
	
	headers.replace (httpHeaderName (HeaderContentEncoding), value);
}

void Nuria::HttpClient::forceClose () {
	this->d_ptr->transport->forceClose ();
}

bool Nuria::HttpClient::invokePath (const QString &path) {
	this->d_ptr->path.setPath (path);
	return this->d_ptr->server->invokeByPath (this, path);
}

bool Nuria::HttpClient::isSequential () const {
	return !this->d_ptr->bufferDevice->inherits ("TemporaryBufferDevice");
}

qint64 Nuria::HttpClient::postBodyLength () const {
	
	// Fail if the header isn't completely loaded
	if (!this->d_ptr->headerReady)
		return -1;
	
	return this->d_ptr->postBodyLength;
}

qint64 Nuria::HttpClient::postBodyTransferred () {
	return this->d_ptr->postBodyTransferred;
}

QHostAddress Nuria::HttpClient::localAddress () const {
	return this->d_ptr->transport->localAddress ();
}

quint16 Nuria::HttpClient::localPort () const {
	return this->d_ptr->transport->localPort ();
}

QHostAddress Nuria::HttpClient::peerAddress () const {
	return this->d_ptr->transport->peerAddress ();
}

quint16 Nuria::HttpClient::peerPort () const {
	return this->d_ptr->transport->peerPort ();
}

int Nuria::HttpClient::responseCode () const {
	return this->d_ptr->responseCode;
}

void Nuria::HttpClient::setResponseCode (int code) {
	this->d_ptr->responseCode = code;
}

bool Nuria::HttpClient::isConnectionSecure () const {
	return this->d_ptr->transport->isSecure ();
}

Nuria::HttpServer *Nuria::HttpClient::httpServer () const {
	return this->d_ptr->server;
}

Nuria::HttpClient::Cookies Nuria::HttpClient::cookies () {
	readRequestCookies ();
	return this->d_ptr->requestCookies;
}

QNetworkCookie Nuria::HttpClient::cookie (const QByteArray &name) {
	readRequestCookies ();
	return this->d_ptr->requestCookies.value (name);
}

bool Nuria::HttpClient::hasCookie (const QByteArray &name) {
	readRequestCookies ();
	return this->d_ptr->requestCookies.contains (name);
}

void Nuria::HttpClient::setCookie (const QByteArray &name, const QByteArray &value,
				   const QDateTime &expires, bool secure) {
	
	// Construct
	QNetworkCookie cookie (name);
	cookie.setValue (value);
	cookie.setExpirationDate (expires);
	cookie.setSecure (secure);
	
	// Store
	setCookie (cookie);
	
}

void Nuria::HttpClient::setCookie (const QByteArray &name, const QByteArray &value,
				   qint64 maxAge, bool secure) {
	
	// Construct
	QNetworkCookie cookie (name);
	cookie.setSecure (secure);
	cookie.setValue (value);
	
	if (maxAge > 0) {
		cookie.setExpirationDate (QDateTime::currentDateTime ().addMSecs (maxAge));
	}
	
	// Store
	setCookie (cookie);
}

void Nuria::HttpClient::setCookie (const QNetworkCookie &cookie) {
	this->d_ptr->responseCookies.insert (cookie.name (), cookie);
}

void Nuria::HttpClient::removeCookie (const QByteArray &name) {
	readRequestCookies ();
	
	// Remove from server
	this->d_ptr->responseCookies.remove (name);
	
	// Remove from client if stored client-side
	if (this->d_ptr->requestCookies.contains (name)) {
		
		// This should work on most clients. The old value is replaced
		// with garbage and the 'expires' header points to 1970 which
		// is a long time ago. Smart browsers should get that hint.
		QNetworkCookie cookie (name);
		cookie.setExpirationDate(QDateTime::fromMSecsSinceEpoch(0));
		setCookie (cookie);
		
	}
	
}

bool Nuria::HttpClient::keepConnectionOpen () const {
	return this->d_ptr->keepConnectionOpen;
}

void Nuria::HttpClient::setKeepConnectionOpen (bool keepOpen) {
	this->d_ptr->keepConnectionOpen = keepOpen;
}

Nuria::SlotInfo Nuria::HttpClient::slotInfo () const {
	return this->d_ptr->slotInfo;
}

void Nuria::HttpClient::setSlotInfo (const SlotInfo &info) {
	this->d_ptr->slotInfo = info;
}

bool Nuria::HttpClient::hasReadablePostBody () const {
	if (!requestHasPostBody ()) {
		return false;
	}
	
	// 
	QByteArray contentType = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderContentType));
	if (contentTypeIsMultipart (contentType) || contentTypeIsUrlEncoded (contentType)) {
		return true;
	}
	
	// 
	return false;
}

Nuria::HttpPostBodyReader *Nuria::HttpClient::postBodyReader () {
	if (!requestHasPostBody () || this->d_ptr->bodyReader) {
		return this->d_ptr->bodyReader;
	}
	
	// Create new reader
	QByteArray contentType = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderContentType));
	HttpPostBodyReader *reader = nullptr;
	
	// HTTP multi-part ?
	if (contentTypeIsMultipart (contentType)) {
		reader = createHttpMultiPartReader (contentType);
	}
	
	// Url-encoded?
	if (contentTypeIsUrlEncoded (contentType)) {
		reader = createUrlEncodedPartReader (contentType);
	}
	
	// Done.
	this->d_ptr->bodyReader = reader;
	return reader;
}

Nuria::HttpClient::TransferMode Nuria::HttpClient::transferMode () const {
	return this->d_ptr->transferMode;
}

bool Nuria::HttpClient::setTransferMode (TransferMode mode) {
	if (this->d_ptr->headerSent || this->d_ptr->outBuffer) {
		return false;
	}
	
	// 
	if (mode == Buffered) {
		this->d_ptr->outBuffer = new TemporaryBufferDevice (this);
		this->d_ptr->outBuffer->open (QIODevice::ReadWrite);
	} else if (mode == Streaming) {
		this->d_ptr->connectionMode = ConnectionClose;
	}
	
	// 
	this->d_ptr->transferMode = mode;
	return true;
}

Nuria::HttpClient::ConnectionMode Nuria::HttpClient::connectionMode () const {
	return this->d_ptr->connectionMode;
}

bool Nuria::HttpClient::requestCompletelyReceived () const {
	if (!this->d_ptr->headerReady) {
		return false;
	}
	
	// Request body
	if (requestHasPostBody () &&
	    this->d_ptr->postBodyTransferred != this->d_ptr->postBodyLength) {
		return false;
	}
	
	// Complete.
	return true;
}

bool Nuria::HttpClient::atEnd () const {
	if (!requestHasPostBody ()) {
		return true;
	}
	
	// 
	if (this->d_ptr->postBodyTransferred < this->d_ptr->postBodyLength) {
		return false;
	}
	
	return this->d_ptr->bufferDevice->atEnd ();
}

bool Nuria::HttpClient::manualInit (HttpVerb verb, HttpVersion version, const QByteArray &path,
                                    const HeaderMap &headers) {
	if (this->d_ptr->headerReady) {
		return false;
	}
	
	// Store the variables and start processing
	initPath (path);
	this->d_ptr->requestHeaders = headers;
	this->d_ptr->requestType = verb;
	this->d_ptr->requestVersion = version;
	this->d_ptr->headerReady = true;
	return postProcessRequestHeader ();
}

bool Nuria::HttpClient::isWebSocketHandshake () const {
	return (this->d_ptr->headerReady &&
	        Internal::WebSocketReader::isWebSocketRequest (this->d_ptr->requestHeaders));
}

Nuria::WebSocket *Nuria::HttpClient::acceptWebSocketConnection () {
	if (!isWebSocketHandshake () || this->d_ptr->headerSent) {
		return nullptr;
	}
	
	// Pipe additional data into the websocket
	WebSocket *socket = new WebSocket (this);
	pipeFromPostBody (socket->backendDevice ());
	setTransferMode (Streaming);
	setKeepConnectionOpen (true);
	
	// Make the HttpClient accept a POST body
	this->d_ptr->requestType = POST;
	this->d_ptr->postBodyLength = -1;
	
	// Build response headers to complete the handshake.
	QByteArray key = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderSecWebSocketKey));
	QByteArray accept = Internal::WebSocketReader::generateHandshakeKey (key);
	
	setResponseCode (101); // Switching Protocol
	this->d_ptr->responseHeaders.replace (httpHeaderName (HeaderUpgrade), QByteArrayLiteral("websocket"));
	this->d_ptr->responseHeaders.replace (httpHeaderName (HeaderConnection), QByteArrayLiteral("Upgrade"));
	this->d_ptr->responseHeaders.replace (httpHeaderName (HeaderSecWebSocketAccept), accept);
	sendResponseHeader ();
	
	// Later: Add -Extension and -Protocol WebSocket response headers if needed.
	return socket;
}

qint64 Nuria::HttpClient::bytesAvailable () const {
	return QIODevice::bytesAvailable () + this->d_ptr->bufferDevice->bytesAvailable ();
}

qint64 Nuria::HttpClient::pos () const {
	return this->d_ptr->bufferDevice->pos ();
}

qint64 Nuria::HttpClient::size () const {
	return this->d_ptr->bufferDevice->size ();
}

bool Nuria::HttpClient::seek (qint64 pos) {
	if (pos > this->d_ptr->bufferDevice->size ()) {
		return false;
	}
	
	QIODevice::seek (pos);
	return this->d_ptr->bufferDevice->seek (pos);
}

bool Nuria::HttpClient::reset () {
	QIODevice::reset ();
	return this->d_ptr->bufferDevice->reset ();
}

bool Nuria::HttpClient::sendResponseHeader () {
	if (!this->d_ptr->headerReady || this->d_ptr->headerSent) {
		return false;
	}
	
	// Create headers for range responses and HTTP/1.1 compliance
	HttpWriter writer;
	writer.applyRangeHeaders (this->d_ptr->rangeStart, this->d_ptr->rangeEnd,
				  this->d_ptr->contentLength, this->d_ptr->responseHeaders);
	writer.addComplianceHeaders (this->d_ptr->requestVersion, this->d_ptr->responseHeaders);
	
	if (!this->d_ptr->responseHeaders.contains (httpHeaderName (HttpClient::HeaderConnection))) {
		writer.addConnectionHeader (this->d_ptr->connectionMode, this->d_ptr->transport->currentRequestCount (),
		                            this->d_ptr->transport->maxRequests (), this->d_ptr->responseHeaders);
	}
	
	// Apply filters
	if (!filterHeaders (this->d_ptr->responseHeaders)) {
		return false;
	}
	
	// 
	writer.addTransferEncodingHeader (this->d_ptr->transferMode, this->d_ptr->responseHeaders);
	
	// Construct header data
	QByteArray header;
	header.append (writer.writeResponseLine (this->d_ptr->requestVersion,
	                                         this->d_ptr->responseCode,
	                                         this->d_ptr->responseName.toLatin1 ()));
	header.append (writer.writeHttpHeaders (this->d_ptr->responseHeaders));
	header.append (writer.writeSetCookies (this->d_ptr->responseCookies));
	header.append ("\r\n");
	
	// Send header
	this->d_ptr->headerSent = true;
	return this->d_ptr->transport->sendToRemote (this, header);
}

bool Nuria::HttpClient::killConnection (int error, const QString &cause) {
	
	if (!this->d_ptr->transport->isOpen () || this->d_ptr->headerSent) {
		return false;
	}
	
	// Make sure we're allowed to write data
	this->d_ptr->pipeDevice = nullptr;
	setOpenMode (QIODevice::WriteOnly);
	
	// Discard out buffer if used
	if (this->d_ptr->outBuffer) {
		this->d_ptr->outBuffer->reset ();
		this->d_ptr->outBuffer->discard ();
	}
	
	// Serve error response
	if (!this->d_ptr->headerSent) {
		this->d_ptr->responseCode = error;
		this->d_ptr->responseName = cause;
		
		QByteArray causeData = cause.toLatin1 ();
		if (causeData.isEmpty ()) {
			causeData = httpStatusCodeName (error);
		}
		
		this->d_ptr->server->invokeError (this, error, causeData);
	}
	
	// Close connection
	close ();
	
	bool success = !this->d_ptr->headerSent;
	this->d_ptr->headerSent = true;
	
	return success;
}


QByteArray Nuria::HttpClient::httpStatusCodeName (int code) {
	
	switch (code) {
	case 100: return QByteArrayLiteral("Continue");
	case 101: return QByteArrayLiteral("Switching Protocols");
	case 200: return QByteArrayLiteral("OK");
	case 201: return QByteArrayLiteral("Created");
	case 202: return QByteArrayLiteral("Accepted");
	case 203: return QByteArrayLiteral("Non-Authorative Information");
	case 204: return QByteArrayLiteral("No Content");
	case 205: return QByteArrayLiteral("Reset Content");
	case 206: return QByteArrayLiteral("Partial Content");
	case 300: return QByteArrayLiteral("Multiple Choices");
	case 301: return QByteArrayLiteral("Moved Permanently");
	case 302: return QByteArrayLiteral("Found");
	case 303: return QByteArrayLiteral("See Other");
	case 304: return QByteArrayLiteral("Not Modified");
	case 305: return QByteArrayLiteral("Use Proxy");
	case 307: return QByteArrayLiteral("Temporary Redirect");
	case 400: return QByteArrayLiteral("Bad Request");
	case 401: return QByteArrayLiteral("Unauthorized");
	case 402: return QByteArrayLiteral("Payment Required");
	case 403: return QByteArrayLiteral("Forbidden");
	case 404: return QByteArrayLiteral("Not Found");
	case 405: return QByteArrayLiteral("Method Not Allowed");
	case 406: return QByteArrayLiteral("Not Acceptable");
	case 407: return QByteArrayLiteral("Proxy Authentication Required");
	case 408: return QByteArrayLiteral("Request Timeout");
	case 409: return QByteArrayLiteral("Conflict");
	case 410: return QByteArrayLiteral("Gone");
	case 411: return QByteArrayLiteral("Length Required");
	case 412: return QByteArrayLiteral("Precondition Failed");
	case 413: return QByteArrayLiteral("Request Entity Too Large");
	case 414: return QByteArrayLiteral("Request-URI Too Long");
	case 415: return QByteArrayLiteral("Unsupported Media Type");
	case 416: return QByteArrayLiteral("Requested Range Not Satisfiable");
	case 417: return QByteArrayLiteral("Expectation Failed");
	case 418: return QByteArrayLiteral("I'm a teapot"); // See RFC 2324
	case 426: return QByteArrayLiteral("Upgrade Required");
	case 428: return QByteArrayLiteral("Precondition Required");
	case 429: return QByteArrayLiteral("Too Many Requests");
	case 431: return QByteArrayLiteral("Request Header Fields Too Large");
	case 500: return QByteArrayLiteral("Internal Server Error");
	case 501: return QByteArrayLiteral("Not Implemented");
	case 502: return QByteArrayLiteral("Bad Gateway");
	case 503: return QByteArrayLiteral("Service Unavailable");
	case 504: return QByteArrayLiteral("Gateway Timeout");
	case 505: return QByteArrayLiteral("HTTP Version Not Supported");
	}
	
	// Unknown code, return empty string
	return QByteArray ();
	
}

QByteArray Nuria::HttpClient::httpHeaderName (HttpClient::HttpHeader header) {
	
	switch (header) {
	case HeaderCacheControl: return QByteArrayLiteral("Cache-Control");
	case HeaderContentLength: return QByteArrayLiteral("Content-Length");
	case HeaderContentType: return QByteArrayLiteral("Content-Type");
	case HeaderConnection: return QByteArrayLiteral("Connection");
	case HeaderDate: return QByteArrayLiteral("Date");
	case HeaderUpgrade: return QByteArrayLiteral("Upgrade");
	case HeaderSecWebSocketExtensions: return QByteArrayLiteral("Sec-WebSocket-Extensions");
	case HeaderSecWebSocketProtocol: return QByteArrayLiteral("Sec-WebSocket-Protocol");
	case HeaderHost: return QByteArrayLiteral("Host");
	case HeaderUserAgent: return QByteArrayLiteral("User-Agent");
	case HeaderAccept: return QByteArrayLiteral("Accept");
	case HeaderAcceptCharset: return QByteArrayLiteral("Accept-Charset");
	case HeaderAcceptEncoding: return QByteArrayLiteral("Accept-Encoding");
	case HeaderAcceptLanguage: return QByteArrayLiteral("Accept-Language");
	case HeaderAuthorization: return QByteArrayLiteral("Authorization");
	case HeaderCookie: return QByteArrayLiteral("Cookie");
	case HeaderRange: return QByteArrayLiteral("Range");
	case HeaderReferer: return QByteArrayLiteral("Referer");
	case HeaderDoNotTrack: return QByteArrayLiteral("Do-Not-Track");
	case HeaderExpect: return QByteArrayLiteral("Expect");
	case HeaderOrigin: return QByteArrayLiteral("Origin");
	case HeaderSecWebSocketKey: return QByteArrayLiteral("Sec-WebSocket-Key");
	case HeaderSecWebSocketVersion: return QByteArrayLiteral("Sec-WebSocket-Version");
	case HeaderContentEncoding: return QByteArrayLiteral("Content-Encoding");
	case HeaderContentLanguage: return QByteArrayLiteral("Content-Language");
	case HeaderContentDisposition: return QByteArrayLiteral("Content-Disposition");
	case HeaderContentRange: return QByteArrayLiteral("Content-Range");
	case HeaderLastModified: return QByteArrayLiteral("Last-Modified");
	case HeaderRefresh: return QByteArrayLiteral("Refresh");
	case HeaderSetCookie: return QByteArrayLiteral("Set-Cookie");
	case HeaderTransferEncoding: return QByteArrayLiteral("Transfer-Encoding");
	case HeaderLocation: return QByteArrayLiteral("Location");
	case HeaderSecWebSocketAccept: return QByteArrayLiteral("Sec-WebSocket-Accept");
	};
	
	// We should never reach this.
	return QByteArray ();
}
