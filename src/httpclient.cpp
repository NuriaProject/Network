/* Copyright (c) 2014, The Nuria Project
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *    1. The origin of this software must not be misrepresented; you must not
 *       claim that you wrote the original software. If you use this software
 *       in a product, an acknowledgment in the product documentation would be
 *       appreciated but is not required.
 *    2. Altered source versions must be plainly marked as such, and must not be
 *       misrepresented as being the original software.
 *    3. This notice may not be removed or altered from any source
 *       distribution.
 */

#include "nuria/httpclient.hpp"

#include <QSslSocket>
#include <QTcpSocket>
#include <QDateTime>
#include <QProcess>
#include <QTimer>
#include <QDir>

// We need gmtime_r
#include <ctime>

#include <nuria/temporarybufferdevice.hpp>
#include "nuria/httpmultipartreader.hpp"
#include "nuria/httptransport.hpp"
#include "nuria/httpparser.hpp"
#include "nuria/httpserver.hpp"
#include "nuria/httpwriter.hpp"
#include "nuria/httpnode.hpp"
#include <nuria/debug.hpp>

#include "private/httpprivate.hpp"

Nuria::HttpClient::HttpClient (HttpTransport *transport, HttpServer *server)
	: QIODevice (server), d_ptr (new HttpClientPrivate)
{
	
	// Initialize
	this->d_ptr->bufferDevice = new TemporaryBufferDevice (this);
	this->d_ptr->transport = transport;
	this->d_ptr->server = server;
	
	// Reparent transport
	this->d_ptr->transport->setParent (this);
	setOpenMode (transport->openMode ());
	
	// 
	connect (transport, SIGNAL(aboutToClose()), SLOT(clientDisconnected()));
	connect (transport, SIGNAL(readyRead()), SLOT(receivedData()));
	connect (transport, SIGNAL(bytesWritten(qint64)), SIGNAL(bytesWritten(qint64)));
	connect (this, SIGNAL(disconnected()), SIGNAL(aboutToClose()));
	
	// Insert default response headers
	this->d_ptr->responseHeaders.insert (httpHeaderName (HeaderConnection), QByteArrayLiteral("Close"));
	
	// If there's already data available, invoke receivedData() later.
	if (transport->bytesAvailable () > 0) {
		QMetaObject::invokeMethod (this, "receivedData", Qt::QueuedConnection);
	}
	
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

qint64 Nuria::HttpClient::parseIntegerHeaderValue (const QByteArray &value) {
	bool ok = false;
	
	if (value.isEmpty ()) {
		return -1;
	}
	
	qint64 result = value.toLongLong (&ok);
	return (!ok) ? -1 : result;
}

bool Nuria::HttpClient::readPostBodyContentLength () {
	if (!requestHasPostBody ()) {
		return true;
	}
	
	// 
	QByteArray contentLength = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderContentLength));
	this->d_ptr->postBodyLength = parseIntegerHeaderValue (contentLength);
	
	// If there is no Content-Length header, kill the connection
	if (this->d_ptr->postBodyLength < 0) {
		killConnection (400);
		return false;
	}
	
	// Check if the client wants to send too much data.
	qint64 maxContentLength = 1024 * 1024 * 4;
	if (this->d_ptr->slotInfo.isValid ()) {
		maxContentLength = this->d_ptr->slotInfo.maxBodyLength ();
	}
	
	if (this->d_ptr->postBodyLength > maxContentLength) {
		killConnection (413);
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
	
	// Chunked transfer
	HttpWriter writer;
	this->d_ptr->chunkedBodyTransfer = true;
	
	// Respond with '100 Continue'
	this->d_ptr->transport->write (writer.writeResponseLine (Http1_1, 100, QByteArray ()));
	this->d_ptr->transport->write ("\r\n");
	this->d_ptr->transport->flush ();
	
	return true;
}

bool Nuria::HttpClient::readAllAvailableHeaderLines () {
	HttpParser parser;
	
	while (this->d_ptr->transport->canReadLine ()) {
		QByteArray raw = this->d_ptr->transport->readLine (1024);
		
		// A line should end in \r\n, but also check for \n.
		// FIXME: Support 'really long' lines
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

bool Nuria::HttpClient::readFirstLine (const QByteArray &line) {
	HttpParser parser;
	
	QByteArray path;
	bool success = false;
	
	success = parser.parseFirstLineFull (line, this->d_ptr->requestType,
					     path, this->d_ptr->requestVersion);
	
	// 
	path.prepend ("http://localhost");
	this->d_ptr->path = QUrl::fromEncoded (path);
	
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

bool Nuria::HttpClient::verifyRequestBodyOrClose () {
	if (!requestHasPostBody () && this->d_ptr->transport->bytesAvailable () > 0) {
		killConnection (400);
		return false;
	}
	
	return true;
}

bool Nuria::HttpClient::requestHasPostBody () const {
	return (this->d_ptr->requestType == POST || this->d_ptr->requestType == PUT);
}

bool Nuria::HttpClient::postProcessRequestHeader () {
	if (verifyRequestBodyOrClose () &&
	    verifyCompleteHeader () &&
	    readPostBodyContentLength () &&
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

bool Nuria::HttpClient::sendPipeChunkToClient () {
	enum { MaxChunkSize = 8 * 1024 };
	
	// Respect maxLen
	qint64 toRead = MaxChunkSize;
	if (this->d_ptr->pipeMaxlen >= 0 && this->d_ptr->pipeMaxlen < MaxChunkSize) {
		toRead = this->d_ptr->pipeMaxlen;
	}
	
	// 
	QByteArray data = this->d_ptr->pipeDevice->read (toRead);
	this->d_ptr->pipeMaxlen -= data.length ();
	
	if (data.isEmpty ()) {
		return false;
	}
	
	// Write data into the client stream
	sendResponseHeader ();
	this->d_ptr->transport->write (data);
	if (!this->d_ptr->pipeDevice->atEnd ()) {
		
		// Make sure we only have one connection!
		connect (this, SIGNAL(bytesWritten(qint64)),
			 SLOT(pipeToClientReadyRead()), Qt::UniqueConnection);
		
		return true;
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
	
	// Make sure that the response header has been sent
	sendResponseHeader ();
	return this->d_ptr->transport->write (data, len);
	
}

bool Nuria::HttpClient::resolveUrl (const QUrl &url) {
	return this->d_ptr->server->invokeByPath (this, url.path ());
}

bool Nuria::HttpClient::bufferPostBody () {
	
	// Has the client sent too much?
	this->d_ptr->postBodyTransferred += this->d_ptr->transport->bytesAvailable ();
	if (this->d_ptr->postBodyTransferred > this->d_ptr->postBodyLength) {
		return false;
	}
	
	// Write the received data into the buffer
	QByteArray data = this->d_ptr->transport->readAll ();
	this->d_ptr->bufferDevice->write (data);
	
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

void Nuria::HttpClient::receivedData () {
	
	// Has the HTTP header been received from the client?
	if (!this->d_ptr->headerReady) {
		if (!readAllAvailableHeaderLines () ||
		    this->d_ptr->transport->bytesAvailable () == 0) {
			return;
		}
		
	}
	
	// Clients sending data outside of a POST or PUT requests are killed.
	if (!requestHasPostBody ()) {
		killConnection (400);
		return;
	}
		
	// Buffer POST data. Kill the connection if something goes wrong.
	if (!bufferPostBody ()) {
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

void Nuria::HttpClient::pipeToClientReadyRead () {
	
	// If the device is now closed (i.e. not readable), kill the connection
	if (!this->d_ptr->pipeDevice->isOpen () ||
	    !this->d_ptr->pipeDevice->isReadable ()) {
		close ();
		return;
	}
	
	// 
	if (!sendPipeChunkToClient ()) {
		
		// Special case for QProcess: Don't kill the connection while the
		// process is starting up.
		QProcess *process = qobject_cast< QProcess * > (this->d_ptr->pipeDevice);
		if (process && process->state () != QProcess::NotRunning)
			return;
		
		// Disconnect aboutToClose() as it may lead to infinite recursion
		disconnect (this->d_ptr->pipeDevice, SIGNAL(aboutToClose()), this, SLOT(pipeToClientReadyRead()));
		disconnect (this, SIGNAL(bytesWritten(qint64)), this, SLOT(pipeToClientReadyRead()));
		this->d_ptr->pipeDevice->close ();
		
		// Send response header. This is important to do if the user
		// piped an empty QFile. Else, no response at all will be sent.
		sendResponseHeader ();
		close ();
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

const Nuria::HttpClient::HeaderMap &Nuria::HttpClient::requestHeaders () const {
	return this->d_ptr->requestHeaders;
}

bool Nuria::HttpClient::hasResponseHeader (const QByteArray &key) const {
	return this->d_ptr->responseHeaders.contains (key);
}

bool Nuria::HttpClient::hasResponseHeader (HttpClient::HttpHeader header) const {
	return this->d_ptr->responseHeaders.contains (httpHeaderName (header));
}

const Nuria::HttpClient::HeaderMap &Nuria::HttpClient::responseHeaders () const {
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
	if (!device->isReadable ())
		return false;
	
	this->d_ptr->pipeDevice = device;
	this->d_ptr->pipeMaxlen = maxlen;
	device->setParent (this);
	
	// Connect to readyRead() and aboutToClose() signals
	connect (device, SIGNAL(readyRead()), SLOT(pipeToClientReadyRead()));
	connect (device, SIGNAL(aboutToClose()), SLOT(pipeToClientReadyRead()));
	
	// If it is a QProcess, connect to its finished() signal
	QProcess *process = qobject_cast< QProcess * > (device);
	if (process) {
		connect (process, SIGNAL(finished(int)), SLOT(pipeToClientReadyRead()));
	}
	
	// If it's a QFile, we can send a proper Content-Length header.
	QFileDevice *file = qobject_cast< QFileDevice * > (device);
	if (file && !this->d_ptr->headerSent &&
	    !this->d_ptr->responseHeaders.contains (httpHeaderName (HeaderContentLength))) {
		setContentLength (file->size ());
	}
	
	// Read currently available data
	if (file || device->bytesAvailable () > 0) {
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
	
	if (this->d_ptr->headerReady && !this->d_ptr->headerSent) {
		sendResponseHeader ();
	}
	
	setOpenMode (QIODevice::NotOpen);
	this->d_ptr->transport->close ();
}

void Nuria::HttpClient::forceClose () {
	this->d_ptr->transport->close ();
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
	if (contentTypeIsMultipart (contentType)) {
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
	
	// Done.
	this->d_ptr->bodyReader = reader;
	return reader;
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
	
	// Construct header data
	QByteArray firstLine = writer.writeResponseLine (this->d_ptr->requestVersion,
							 this->d_ptr->responseCode,
							 this->d_ptr->responseName.toLatin1 ());
	QByteArray headers = writer.writeHttpHeaders (this->d_ptr->responseHeaders);
	QByteArray cookies = writer.writeSetCookies (this->d_ptr->responseCookies);
	
	// Send header
	this->d_ptr->transport->write (firstLine);
	this->d_ptr->transport->write (headers);
	this->d_ptr->transport->write (cookies);
	this->d_ptr->transport->write ("\r\n");
	
	this->d_ptr->headerSent = true;
	return true;
}

bool Nuria::HttpClient::killConnection (int error, const QString &cause) {
	
	if (!this->d_ptr->transport->isOpen ()) {
		return false;
	}
	
	// Send (really) minimal error response
	if (!this->d_ptr->headerSent) {
		this->d_ptr->responseCode = error;
		this->d_ptr->responseName = cause;
		
		sendResponseHeader ();
		
		QByteArray causeData = cause.toLatin1 ();
		this->d_ptr->transport->write (cause.isEmpty ()
					       ? httpStatusCodeName (error)
					       : causeData);
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
	case HeaderContentEncoding: return QByteArrayLiteral("Content-Encoding");
	case HeaderContentLanguage: return QByteArrayLiteral("Content-Language");
	case HeaderContentDisposition: return QByteArrayLiteral("Content-Disposition");
	case HeaderContentRange: return QByteArrayLiteral("Content-Range");
	case HeaderLastModified: return QByteArrayLiteral("Last-Modified");
	case HeaderRefresh: return QByteArrayLiteral("Refresh");
	case HeaderSetCookie: return QByteArrayLiteral("Set-Cookie");
	case HeaderTransferEncoding: return QByteArrayLiteral("Transfer-Encoding");
	case HeaderLocation: return QByteArrayLiteral("Location");
	};
	
	// We should never reach this.
	return QByteArray ();
}
