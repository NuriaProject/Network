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

#include "httpclient.hpp"

#include <QTemporaryFile>
#include <QSslSocket>
#include <QTcpSocket>
#include <QDateTime>
#include <QProcess>
#include <QBuffer>
#include <QTimer>
#include <QDir>

// We need gmtime_r
#include <ctime>

#include "httpserver.hpp"
#include "httpnode.hpp"
#include <debug.hpp>

#include "private/httpprivate.hpp"

// Avoid annoying localized month and day names
static const char *g_days[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static const char *g_months[] = { "Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
				  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

Nuria::HttpClient::HttpClient (QTcpSocket *socket, HttpServer *server)
	: QIODevice (server)
{
	
#ifdef QT_DEBUG
	nDebug() << "Creating" << this;
#endif
	
	// Create and initialize d_ptr
	this->d_ptr = new HttpClientPrivate;
	this->d_ptr->socket = socket;
	this->d_ptr->server = server;
	
	// Reparent socket
	this->d_ptr->socket->setParent (this);
	
	// SSL socket?
	QSslSocket *sslSocket = qobject_cast< QSslSocket * > (socket);
	
	// 
	setOpenMode (QIODevice::ReadWrite);
	
	// 
	connect (socket, SIGNAL(disconnected()), SLOT(clientDisconnected()));
	connect (socket, SIGNAL(readyRead()), SLOT(receivedData()));
	
	// Forward signals
	connect (socket, SIGNAL(aboutToClose()), SIGNAL(aboutToClose()));
	
	if (sslSocket) {
		connect (sslSocket, SIGNAL(encryptedBytesWritten(qint64)), SIGNAL(bytesWritten(qint64)));
	} else {
		connect (socket, SIGNAL(bytesWritten(qint64)), SIGNAL(bytesWritten(qint64)));
	}
	
	// Insert default response headers
	this->d_ptr->responseHeaders.insert (httpHeaderName (HeaderConnection), "Close");
	
}

bool Nuria::HttpClient::readRequestHeaders () {
	
	// Read Range header
	if (this->d_ptr->requestHeaders.contains (httpHeaderName (HeaderRange))) {
		// "Range: bytes=500-1234"
		
		QByteArray value = this->d_ptr->requestHeaders.value (httpHeaderName (HeaderRange));
		int begin = value.indexOf ('=') + 1;
		
		if (begin == 0) {
			return false;
		}
		
		int end = value.indexOf ('-', begin);
		if (end == -1) {
			return false;
		}
		
		// 
		bool startOk = false;
		bool endOk = false;
		
		// 
		QByteArray rawStart = value.mid (begin, end - begin);
		QByteArray rawEnd = value.mid (end + 1);
		
		this->d_ptr->rangeStart = rawStart.toLongLong (&startOk);
		this->d_ptr->rangeEnd = rawEnd.toLongLong (&endOk);
		
		// Failed?
		if (!startOk || !endOk || this->d_ptr->rangeStart < 0 ||
		    this->d_ptr->rangeEnd <= this->d_ptr->rangeStart) {
			return false;
		}
		
	}
	
	// Read cookies
	return true;
}

void Nuria::HttpClient::readRequestCookies () {
	
	// Already read?
	if (!this->d_ptr->requestCookies.isEmpty ()) {
		return;
	}
	
	// Get 'Cookie' headers
	// Note: As of RFC6265 a client is only allowed to send a single Cookie
	// header. Thing is, some clients probably don't care and do it anyway.
	QList< QByteArray > headers = this->d_ptr->requestHeaders.values ("Cookie");
	for (int i = 0; i < headers.length (); i++) {
		
		// 
		QList< QByteArray > items = headers.at (i).split (';');
		
		// Parse items
		for (int j = 0; j < items.length (); j++) {
			const QByteArray &cur = items.at (j);
			int delim = cur.indexOf ('=');
			
			// Break if the delimeter can't be found.
			if (delim == -1) {
				break;
			}
			
			// Read name and value. Decode value.
			QByteArray name = cur.left (delim).trimmed ();
			QByteArray value = QByteArray::fromPercentEncoding (cur.mid (delim + 1));
			
			// Store cookie
			this->d_ptr->requestCookies.insert (name, value);
			
		}
		
	}
	
	// Done.
	
}

qint64 Nuria::HttpClient::readData (char *data, qint64 maxlen) {
	
	// 
	if (!this->d_ptr->bufferDevice) {
		return 0;
	}
	
	// Read the data from a buffer.
	return this->d_ptr->bufferDevice->read (data, maxlen);
	
}

qint64 Nuria::HttpClient::writeData (const char *data, qint64 len) {
	
	if (this->d_ptr->pipeDevice)
		return -1;
	
	// Make sure that the response header has been sent
	sendResponseHeader ();
	
	return this->d_ptr->socket->write (data, len);
	
}

bool Nuria::HttpClient::resolveUrl (const QUrl &url) {
	return this->d_ptr->server->invokeByPath (this, url.path ());
	
}

bool Nuria::HttpClient::bufferPostBody () {
	
	// Parse body length if not already done
	if (this->d_ptr->postBodyLength < 0) {
		QByteArray value = this->d_ptr->requestHeaders.value ("Content-Length");
		
		// If there is no Content-Length header, kill the connection
		if (value.isEmpty ()) {
			return false;
		}
		
		// Same if the value is not a parsable integer
		bool ok = false;
		this->d_ptr->postBodyLength = value.toLongLong (&ok);
		
		if (!ok) {
			return false;
		}
		
		// And last but not least, check if the client wants to send too much data.
		if (this->d_ptr->slotInfo.isValid () &&
		    this->d_ptr->postBodyLength > this->d_ptr->slotInfo.maxBodyLength ()) {
			killConnection (413);
			return false;
		}
		
		// Chunked transfer?
		QByteArray expectedHeader = this->d_ptr->requestHeaders.value ("Expect");
		if (!expectedHeader.isEmpty ()) {
			
			if (!qstricmp (expectedHeader, "100-continue")) {
				this->d_ptr->chunkedBodyTransfer = true;
				
//				nDebug() << "Sending 100 Continue response";
				
				// Respond with '100 Continue'
				this->d_ptr->socket->write ("HTTP/1.1 100 Continue\r\n\r\n");
				this->d_ptr->socket->flush ();
				
			} else {
				
				// What's that?
				return false;
				
			}
			
		}
		
	}
	
	// Do we need a new buffer?
	if (!this->d_ptr->bufferDevice) {
		
		// If the clients sends less than m_bufferMemorySize we use a QBuffer.
		// Else we use a temporary file to store the data.
		// We also use a in-memory buffer if the user told us to not buffer the data.
		// This way we can always track how much data has been sent, so the
		// kill-client-if-sent-too-much mechanism works correctly.
		if ((this->d_ptr->slotInfo.isValid () && this->d_ptr->slotInfo.streamPostBody ()) ||
		    this->d_ptr->postBodyLength <= this->d_ptr->bufferMemorySize) {
			
			// Create a QByteArray with enough space to contain the complete body.
			QByteArray data;
			data.reserve (this->d_ptr->postBodyLength + 1);
			
			QBuffer *buffer = new QBuffer (this);
			buffer->setData (data);
			
			buffer->open (QIODevice::ReadWrite);
			this->d_ptr->bufferDevice = buffer;
			
		} else {
			
			// We need a temporary file to store the data.
			QTemporaryFile *file = new QTemporaryFile (this);
			
			if (!file->open ()) {
				nCritical() << "Failed to create temporary file";
				return false;
			}
			
			this->d_ptr->bufferDevice = file;
		}
		
	}
	
	// Write the received data into the buffer
	QByteArray data = this->d_ptr->socket->readAll ();
//	nDebug() << "Received" << data.length () << this->d_ptr->bufferDevice->inherits ("QProcess");
	
	this->d_ptr->postBodyTransferred += data.length ();
	this->d_ptr->bufferDevice->write (data);
	
	// Emit postBodyComplete() when the transfer is complete
	if (this->d_ptr->postBodyTransferred == this->d_ptr->postBodyLength) {
//		nDebug() << "Body complete.";
		
		// If we're piping the body into a process, we need to close
		// the writing channel for some applications to start their work.
		QProcess *process = qobject_cast< QProcess * > (this->d_ptr->bufferDevice);
		if (process) {
			process->closeWriteChannel ();
		}
		
		emit postBodyComplete ();
	} else if (this->d_ptr->postBodyTransferred > this->d_ptr->postBodyLength) {
		
		// The client sent too much. Get rid of the liar.
		return false;
		
	}
	
	return true;
	
}

void Nuria::HttpClient::clientDisconnected () {
	
	// Mark the connection as closed
	setOpenMode (QIODevice::NotOpen);
	
	// Emit disconnected signal
	emit disconnected ();
	
	// and delete this later on.
	deleteLater ();
	
}

/**
 * Helper function for Nuria::HttpClient::receivedData().
 * Takes a key name of a Http header and 'corrects' the case,
 * so 'content-length' becomes 'Content-Length'.
 */
static void correctHeaderKeyCase (QByteArray &key) {
	
	char *data = key.data ();
	for (int i = 0; data[i]; i++) {
		
		// Wrong case for the first character (after a dash)?
		if (islower (data[i]) && (i == 0 || data[i - 1] == '-')) {
			data[i] = toupper (data[i]);
		}
		
	}
	
}

void Nuria::HttpClient::receivedData () {
	
	// Has the HTTP header been received from the client?
	if (this->d_ptr->headerReady) {
		
		if (this->d_ptr->requestType == POST ||
		    this->d_ptr->requestType == PUT) {
			
			// Buffer POST data. Kill the connection if something goes wrong.
			if (!bufferPostBody ()) {
				killConnection (400);
				return;
			}
			
			// If we're in non-streamed mode, we call the
			// associated slot if the POST body is complete.
			if (!(this->d_ptr->slotInfo.isValid () && this->d_ptr->slotInfo.streamPostBody ()) &&
			    this->d_ptr->postBodyLength == this->d_ptr->postBodyTransferred) {
				
				// Seek the buffer to the beginning
				this->d_ptr->bufferDevice->seek (0);
				
//				nDebug() << "Delayed call of" << this->d_ptr->slotInfo->slotName ();
				
				// Call the slot and close the connection afterwards.
				HttpNode::callSlot (this->d_ptr->slotInfo, this);
				
				if (!this->d_ptr->keepConnectionOpen) {
					close ();
				}
				
				return;
				
			}
			
		} else {
			
			// In this case we received data after the HTTP header.
			// And the client used a verb other than POST and PUT,
			// so it shouldn't send a HTTP body. A good reason for
			// killing the connection.
			killConnection (400);
			return;
			
		}
		
		// Emit readRead(). Interesting for streaming applications.
		emit readyRead ();
		return;
	}
	
	// Parse header line
	while (this->d_ptr->socket->canReadLine ()) {
		
		QByteArray raw = this->d_ptr->socket->readLine (1024);
		
		// A line should end in \r\n, but also check for \n.
		if (raw.endsWith ("\r\n")) {
			raw.chop (2);
		} else if (raw.endsWith ('\n')) {
			// Not a HTTP conform request, but if it's the only problem
			// we simply ignore it.
			raw.chop (1);
		} else {
			
			// Line too long, kill connection
			killConnection (400);
			return;
			
		}
		
		// If the line is empty, the header is complete.
		if (raw.isEmpty ()) {
			
			this->d_ptr->headerReady = true;
			
			// If the client uses HTTP 1.1, it must provide a Host header.
			if (this->d_ptr->requestVersion == Http1_1 &&
			    !this->d_ptr->requestHeaders.contains ("Host")) {
				
				killConnection (400);
				return;
				
			}
			
			// Parse headers
			if (!readRequestHeaders ()) {
				killConnection (400);
				return;
			}
			
			// Try to call the associated slot
			if (!resolveUrl (this->d_ptr->path)) {
				killConnection (403);
				return;
			}
			
			// Has the slot closed the connection?
			if (openMode () == QIODevice::NotOpen) {
				return;
			}
			
			// Close the connection if
			// 1) There is no active pipe
			// 2) The client didn't use POST or PUT
			// 3) The connection shouldn't be kept open
			if (!this->d_ptr->pipeDevice && this->d_ptr->requestType != POST &&
			    this->d_ptr->requestType != PUT && !this->d_ptr->keepConnectionOpen) {
				
				close ();
				
			}
			
			// Recall receivedData() so the readyRead signal is emitted.
			if (this->d_ptr->requestType == POST || this->d_ptr->requestType == PUT) {
				receivedData ();
			}
			
			return;
			
		}
		
		// Is this the first line?
		if (this->d_ptr->requestVersion == HttpUnknown) {
			
			// The line should look like:
			// <Verb> <URL> HTTP/<Version>
			
			int startUrl = raw.indexOf (' ');
			int startVersion = raw.lastIndexOf (' ');
			
			// Bad line, kill client
			if (startUrl == -1 || startUrl > startVersion) {
				killConnection (400);
				return;
			}
			
			// Parse verb
			QByteArray verb = raw.left (startUrl);
			if (!qstrcmp (verb, "GET")) {
				this->d_ptr->requestType = GET;
			} else if (!qstrcmp (verb, "POST")) {
				this->d_ptr->requestType = POST;
			} else if (!qstrcmp (verb, "HEAD")) {
				this->d_ptr->requestType = HEAD;
			} else if (!qstrcmp (verb, "PUT")) {
				this->d_ptr->requestType = PUT;
			} else if (!qstrcmp (verb, "DELETE")) {
				this->d_ptr->requestType = DELETE;
			} else {
				
				// Unknown verb, kill connection
				killConnection (501);
				return;
				
			}
			
			// Parse HTTP version
			QByteArray version = raw.right (3);
			if (!qstrcmp (version, "1.0")) {
				this->d_ptr->requestVersion = Http1_0;
			} else if (!qstrcmp (version, "1.1")) {
				this->d_ptr->requestVersion = Http1_1;
			} else {
				
				// Unknown version, kill connection
				killConnection (505);
				return;
				
			}
			
			// Read requested URL
			startUrl++; // Skip space
			QByteArray url = raw.mid (startUrl, startVersion - startUrl);
			
			// Dummy to keep QUrl happy
			url.prepend ("http://localhost");
			this->d_ptr->path = QUrl::fromEncoded (url);
			
//			nDebug() << this->d_ptr->path;
			
			continue;
		}
		
		// Split the line to get the "<name>: <value>" pair
		int beginOfValue = raw.indexOf (": ");
		
		// Not a valid request, abort
		if (beginOfValue == -1) {
			killConnection (400);
		}
		
		// Get the key and value
		QByteArray key = raw.left (beginOfValue);
		QByteArray value = raw.mid (beginOfValue + 2);
		
		// Make sure that keys are case-correct.
		correctHeaderKeyCase (key);
		
		// Store
		this->d_ptr->requestHeaders.insert (key, value);
		
	}
	
}

void Nuria::HttpClient::deviceReadyRead () {
	static const int maxDataLen = 4096;
	
	// If the device is now closed (i.e. not readable), kill the connection
	if (!this->d_ptr->pipeDevice->isOpen () ||
	    !this->d_ptr->pipeDevice->isReadable ()) {
		close ();
		return;
	}
	
	// Respect maxLen
	qint64 toRead = maxDataLen;
	
	if (this->d_ptr->pipeMaxlen >= 0 && this->d_ptr->pipeMaxlen < maxDataLen) {
		toRead = this->d_ptr->pipeMaxlen;
	}
	
	QByteArray data = this->d_ptr->pipeDevice->read (toRead);
	this->d_ptr->pipeMaxlen -= toRead;
	
//	nDebug() << "data len:" << data.length () << "avail:" << this->d_ptr->pipeDevice->bytesAvailable ();
	
	if (!data.isEmpty ()) {
		
		sendResponseHeader ();
		
		// Write data into the client stream
		this->d_ptr->socket->write (data);
//		this->d_ptr->socket->flush ();
		
		// If there is still data available connect to bytesWritten()
		// to send the next chunk of data.
		if (!this->d_ptr->pipeDevice->atEnd ()) {
			
			// Make sure we only have one connection!
			connect (this, SIGNAL(bytesWritten(qint64)),
				 SLOT(deviceReadyRead()), Qt::UniqueConnection);
			
			return;
		}
		
	}
	
	// When there is nothing more to read from the device, delete this.
	if (this->d_ptr->pipeDevice->atEnd () || this->d_ptr->pipeMaxlen == 0) {
		
		// Special case for QProcess: Don't kill the connection while the
		// process is starting up.
		QProcess *process = qobject_cast< QProcess * > (this->d_ptr->pipeDevice);
		if (process && process->state () != QProcess::NotRunning)
			return;
		
		// Disconnect aboutToClose() as it may lead to infinite recursion
		disconnect (this->d_ptr->pipeDevice, SIGNAL(aboutToClose()), this, SLOT(deviceReadyRead()));
		
		// Also get rid of the bytesWritten() connection
		disconnect (this, SIGNAL(bytesWritten(qint64)), this, SLOT(deviceReadyRead()));
		
		// Close the device
		this->d_ptr->pipeDevice->close ();
		
		// Send response header. This is important to do if the user
		// piped an empty QFile. Else, no response at all will be sent
		// which wouldn't be good.
		sendResponseHeader ();
		
		// Close connection
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

const QMultiMap< QByteArray, QByteArray > &Nuria::HttpClient::requestHeaders () const {
	return this->d_ptr->requestHeaders;
}

bool Nuria::HttpClient::hasResponseHeader (const QByteArray &key) const {
	return this->d_ptr->responseHeaders.contains (key);
}

bool Nuria::HttpClient::hasResponseHeader (HttpClient::HttpHeader header) const {
	return this->d_ptr->responseHeaders.contains (httpHeaderName (header));
}

const QMultiMap< QByteArray, QByteArray > &Nuria::HttpClient::responseHeaders () const {
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
	
	if (this->d_ptr->headerSent)
		return false;
	
	QByteArray key = httpHeaderName (header);
	
	if (!append) {
		this->d_ptr->responseHeaders.remove (key);
	}
	
	this->d_ptr->responseHeaders.insert (key, value);
	return true;
}

bool Nuria::HttpClient::setResponseHeaders (const QMultiMap< QByteArray, QByteArray > &headers) {
	
	if (this->d_ptr->headerSent)
		return false;
	
	this->d_ptr->responseHeaders = headers;
	return true;
	
}

bool Nuria::HttpClient::pipe (QIODevice *device, qint64 maxlen) {
	
	// Is device readable?
	if (!device->isReadable ())
		return false;
	
	this->d_ptr->pipeDevice = device;
	device->setParent (this);
	
	this->d_ptr->pipeMaxlen = maxlen;
	
	// Connect to readyRead() and aboutToClose() signals
	connect (device, SIGNAL(readyRead()), SLOT(deviceReadyRead()));
	connect (device, SIGNAL(aboutToClose()), SLOT(deviceReadyRead()));
	
	// If it is a QProcess, connect to its finished() signal
	QProcess *process = qobject_cast< QProcess * > (device);
	if (process) {
		connect (process, SIGNAL(finished(int)), SLOT(deviceReadyRead()));
	}
	
	// If it's a QFile, we can send a proper Content-Length header.
	/// TODO: Same goes for a valid Content-Type header. Qt5 will have something for this iirc.
	QFile *file = qobject_cast< QFile * > (device);
	if (file && !this->d_ptr->headerSent &&
	    !this->d_ptr->responseHeaders.contains (httpHeaderName (HeaderContentLength))) {
		
		setContentLength (file->size ());
		
	}
	
	// Read currently available data
	if (file || device->bytesAvailable () > 0) {
		deviceReadyRead ();
	}
	
	return true;
}

bool Nuria::HttpClient::pipeBody (QIODevice *device, bool takeOwnership) {
	Q_CHECK_PTR(device);
	
	// Copy the current buffer into the new one
	if (this->d_ptr->bufferDevice) {
		this->d_ptr->bufferDevice->seek (0);
		device->write (this->d_ptr->bufferDevice->readAll ());
		delete this->d_ptr->bufferDevice;
	}
		
	this->d_ptr->bufferDevice = device;
	
	if (takeOwnership) {
		device->setParent (this);
	}
	
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
	
	// Change the access flag
	setOpenMode (QIODevice::NotOpen);
	
	// SSL socket?
	QSslSocket *sslSocket = qobject_cast< QSslSocket * > (this->d_ptr->socket);
	
	// If the write buffer is empty just close the socket.
	// TODO: Is there a better way to do this?
	if (sslSocket) {
		
		if (sslSocket->encryptedBytesToWrite () == 0 &&
		    this->d_ptr->socket->bytesToWrite () == 0) {
			this->d_ptr->socket->close ();
			return;
		}
		
	} else if (this->d_ptr->socket->bytesToWrite () == 0) {
		this->d_ptr->socket->close ();
		return;
	}
	
	// If there is still data to be written, connect the correct
	// signal with the forceClose() slot.
	if (sslSocket) {
		connect (sslSocket, SIGNAL(encryptedBytesWritten(qint64)), SLOT(forceClose()));
	} else {
		connect (this->d_ptr->socket, SIGNAL(bytesWritten(qint64)), SLOT(forceClose()));
	}
	
}

void Nuria::HttpClient::forceClose () {
	this->d_ptr->socket->close ();
}

bool Nuria::HttpClient::isSequential () const {
	return true;
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
	return this->d_ptr->socket->localAddress ();
}

quint16 Nuria::HttpClient::localPort () const {
	return this->d_ptr->socket->localPort ();
}

QHostAddress Nuria::HttpClient::peerAddress () const {
	return this->d_ptr->socket->peerAddress ();
}

QString Nuria::HttpClient::peerName () const {
	return this->d_ptr->socket->peerName ();
}

quint16 Nuria::HttpClient::peerPort () const {
	return this->d_ptr->socket->peerPort ();
}

int Nuria::HttpClient::responseCode () const {
	return this->d_ptr->responseCode;
}

void Nuria::HttpClient::setResponseCode (int code) {
	this->d_ptr->responseCode = code;
}

bool Nuria::HttpClient::isConnectionSecure () const {
	return this->d_ptr->socket->inherits ("QSslSocket");
}

Nuria::HttpServer *Nuria::HttpClient::httpServer () const {
	return this->d_ptr->server;
}

Nuria::HttpClient::Cookies Nuria::HttpClient::cookies () {
	readRequestCookies ();
	return this->d_ptr->requestCookies;
}

QByteArray Nuria::HttpClient::cookie (const QByteArray &name) {
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
	
	// Construct
	QByteArray data = cookie.toRawForm(QNetworkCookie::Full);
	
	// Store
	this->d_ptr->responseCookies.insert (cookie.name (), data);
	
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
		QNetworkCookie cookie(name);
		cookie.setExpirationDate(QDateTime::fromMSecsSinceEpoch(0));
		this->d_ptr->responseCookies.insert (name, cookie.toRawForm(QNetworkCookie::Full));
		
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

bool Nuria::HttpClient::sendResponseHeader () {
	
	if (!this->d_ptr->headerReady || this->d_ptr->headerSent)
		return false;
	
	// Create headers for range responses.
	if (this->d_ptr->rangeStart != -1 && this->d_ptr->rangeEnd != -1) {
		QByteArray range = "bytes " + QByteArray::number (this->d_ptr->rangeStart) +
				   "-" + QByteArray::number (this->d_ptr->rangeEnd) + 
				   "/";
		
		// Content-Length known?
		if (this->d_ptr->contentLength > -1) {
			range.append (QByteArray::number (this->d_ptr->contentLength));
		} else {
			range.append ('*');
		}
		
		// Remove old headers
		this->d_ptr->responseHeaders.remove (httpHeaderName (HeaderContentRange));
		this->d_ptr->responseHeaders.remove (httpHeaderName (HeaderContentLength));
		
		// Store header
		this->d_ptr->responseHeaders.insert (httpHeaderName (HeaderContentRange), range);
		
		// Adjust Content-Length header
		QByteArray contentLen = QByteArray::number (this->d_ptr->rangeEnd - this->d_ptr->rangeStart);
		this->d_ptr->responseHeaders.insert (httpHeaderName (HeaderContentLength), contentLen);
		
	} else if (this->d_ptr->contentLength != -1) {
		
		this->d_ptr->responseHeaders.remove (httpHeaderName (HeaderContentLength));
		
		// No range response
		this->d_ptr->responseHeaders.insert (httpHeaderName (HeaderContentLength),
						QByteArray::number (this->d_ptr->contentLength));
		
	}
	
	// Send first line of header (<Http version> <Response code> <Response name>)
	if (this->d_ptr->requestVersion == Http1_0)
		this->d_ptr->socket->write ("HTTP/1.0 ");
	else
		this->d_ptr->socket->write ("HTTP/1.1 ");
	
	this->d_ptr->socket->write (QByteArray::number (this->d_ptr->responseCode));
	this->d_ptr->socket->putChar (' ');
	
	// Choose appropriate status code name if nothing was set
	if (this->d_ptr->responseName.isEmpty ())
		this->d_ptr->socket->write (httpStatusCodeName (this->d_ptr->responseCode));
	else
		this->d_ptr->socket->write (this->d_ptr->responseName.toLatin1 ());
	
	// Send headers
	{
		
		QMap< QByteArray, QByteArray >::ConstIterator it = this->d_ptr->responseHeaders.constBegin ();
		QMap< QByteArray, QByteArray >::ConstIterator end = this->d_ptr->responseHeaders.constEnd ();
		for (; it != end; ++it) {
			
			this->d_ptr->socket->write ("\r\n");
			this->d_ptr->socket->write (it.key ());
			this->d_ptr->socket->write (": ");
			this->d_ptr->socket->write (it.value ());
			
		}
		
	}
	
	// Send cookies
	{
		Cookies::ConstIterator it = this->d_ptr->responseCookies.constBegin ();
		Cookies::ConstIterator end = this->d_ptr->responseCookies.constEnd ();
		
		for (; it != end; ++it) {
			this->d_ptr->socket->write ("\r\nSet-Cookie: ");
			this->d_ptr->socket->write (it.value ());
			
		}
		
	}
	
	// Send a Date header
	if (!this->d_ptr->responseHeaders.contains (httpHeaderName (HeaderDate))) {
		
		char string[40];
		tm gmt;
		time_t timestamp = time (0);
		gmtime_r (&timestamp, &gmt);
		
		// Create a string in the style of "Tue, 15 Nov 1994 08:12:31 GMT"
		qsnprintf (string, 40, "\r\nDate: %s, %i %s %i %02i:%02i:%02i GMT",
			   g_days[gmt.tm_wday], gmt.tm_mday, g_months[gmt.tm_mon],
			   gmt.tm_year + 1900, gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
		
		// Send
		this->d_ptr->socket->write (string);
		
	}
	
	// Send double new-line to end header and we're done.
	this->d_ptr->socket->write ("\r\n\r\n");
//	this->d_ptr->socket->flush ();
	
	this->d_ptr->headerSent = true;
	return true;
	
}

bool Nuria::HttpClient::killConnection (int error, const QString &cause) {
	
	if (!this->d_ptr->socket->isOpen ())
		return false;
	
	// Send (really) minimal error response
	if (!this->d_ptr->headerSent) {
		QByteArray message = cause.toLatin1 ();
		
		// Get status code name if nothing was passed.
		if (cause.isEmpty ()) {
			message = httpStatusCodeName (error);
		}
		
		if (this->d_ptr->requestVersion == Http1_0) {
			this->d_ptr->socket->write ("HTTP/1.0 ");
		} else {
			this->d_ptr->socket->write ("HTTP/1.1 ");
		}
		
		this->d_ptr->socket->write (QByteArray::number (error));
		this->d_ptr->socket->putChar (' ');
		this->d_ptr->socket->write (message);
		this->d_ptr->socket->write ("\r\n\r\n");
		this->d_ptr->socket->write (message);
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
	case HeaderContentEncoding: return QByteArrayLiteral("Content-Encoding");
	case HeaderContentLanguage: return QByteArrayLiteral("Content-Language");
	case HeaderContentDisposition: return QByteArrayLiteral("Disposition");
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

Nuria::HttpClient::~HttpClient () {
#ifdef QT_DEBUG
	nDebug() << "Destroying" << this;
#endif
	
	delete this->d_ptr;
	
}
