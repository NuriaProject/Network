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

#include "fastcgitransport.hpp"

#include "../nuria/fastcgibackend.hpp"
#include "../nuria/httpclient.hpp"
#include "../nuria/httpparser.hpp"
#include "fastcgithreadobject.hpp"
#include "fastcgiwriter.hpp"
#include <QTcpSocket>
#include <cctype>

static const QByteArray fcgiRequestVerb = QByteArrayLiteral("REQUEST_METHOD");
static const QByteArray fcgiRequestPath = QByteArrayLiteral("REQUEST_URI");
static const QByteArray fcgiRemoteAddr = QByteArrayLiteral("REMOTE_ADDR");
static const QByteArray fcgiRemotePort = QByteArrayLiteral("REMOTE_PORT");
static const QByteArray fcgiLocalAddr = QByteArrayLiteral("SERVER_ADDR");
static const QByteArray fcgiLocalPort = QByteArrayLiteral("SERVER_PORT");
static const QByteArray fcgiHttpVersion = QByteArrayLiteral("SERVER_PROTOCOL");

namespace Nuria {
namespace Internal {
class FastCgiTransportPrivate {
public:
	
	Internal::FastCgiThreadObject *object;
	FastCgiBackend *backend;
	QIODevice *device;
	uint32_t requestId;
	HttpClient *client = nullptr;
	
	// 
	quint16 peerPort = -1;
	quint16 localPort = -1;
	QHostAddress peerAddress;
	QHostAddress localAddress;
	HttpClient::HeaderMap headers;
	
	bool firstLineReplaced = false;
	
};
}
}

Nuria::Internal::FastCgiTransport::FastCgiTransport (uint32_t requestId, QIODevice *device, FastCgiThreadObject *object,
                                                     FastCgiBackend *backend, HttpServer *server)
        : HttpTransport (backend, server), d_ptr (new FastCgiTransportPrivate)
{
	this->d_ptr->requestId = requestId;
	this->d_ptr->backend = backend;
	this->d_ptr->object = object;
	this->d_ptr->device = device;
	this->d_ptr->client = new HttpClient (this, server);
	
	connect (this->d_ptr->device, &QIODevice::bytesWritten, this, &FastCgiTransport::bytesWritten);
	connect (this->d_ptr->device, &QIODevice::aboutToClose, this, &FastCgiTransport::abort);
	
	// Initialize
	setCurrentRequestCount (1);
	setMaxRequests (1);
	addToServer ();
}

Nuria::Internal::FastCgiTransport::~FastCgiTransport () {
	
	// If we'd use the destroyed() signal in the object, we'd have to
	// access the transports data which has already been delete'd, as the
	// destroyed() signal is emitted in ~QObject().
	this->d_ptr->object->transportDestroyed (this);
	delete this->d_ptr;
}

void Nuria::Internal::FastCgiTransport::abort () {
	if (this->d_ptr->client) {
		close (this->d_ptr->client);
	}
	
}

QIODevice *Nuria::Internal::FastCgiTransport::device () const {
	return this->d_ptr->device;
}

uint16_t Nuria::Internal::FastCgiTransport::requestId () const {
	return this->d_ptr->requestId;
}

void Nuria::Internal::FastCgiTransport::addFastCgiParams (const QMap< QByteArray, QByteArray > &params) {
	this->d_ptr->headers.unite (params);
	
	// If 'params' is empty, then it was the last FCGI parameter packet and
	// thus we can start processing!
	if (params.isEmpty ()) {
		startClient ();
	}
	
}

Nuria::HttpTransport::Type Nuria::Internal::FastCgiTransport::type () const {
	return Custom;
}

bool Nuria::Internal::FastCgiTransport::isSecure () const {
	return false;
}

bool Nuria::Internal::FastCgiTransport::isOpen () const {
	return this->d_ptr->device->isOpen ();
}

void Nuria::Internal::FastCgiTransport::forceClose () {
	closeFcgiRequest ();
	
	if (QAbstractSocket *sock = qobject_cast< QAbstractSocket * > (this->d_ptr->device)) {
		sock->flush ();
	}
	
	deleteLater ();
}

void Nuria::Internal::FastCgiTransport::init () {
	// 
}

void Nuria::Internal::FastCgiTransport::close (HttpClient *client) {
	if (client == this->d_ptr->client) {
		forceClose ();
	}
	
}

bool Nuria::Internal::FastCgiTransport::sendToRemote (HttpClient *, const QByteArray &data) {
	if (!this->d_ptr->firstLineReplaced) {
		QByteArray d = data;
		replaceFirstLine (d);
		sendData (d);
	} else {
		sendData (data);
	}
	
	return this->d_ptr->device->isOpen ();
}

void Nuria::Internal::FastCgiTransport::bytesWritten (qint64 bytes) {
	bytesSent (this->d_ptr->client, bytes);
}

void Nuria::Internal::FastCgiTransport::sendData (const QByteArray &data) {
	Internal::FastCgiWriter::writeMultiPartStream (this->d_ptr->device, FastCgiType::StdOut,
	                                               this->d_ptr->requestId, data);
}

void Nuria::Internal::FastCgiTransport::closeFcgiRequest () {
	
	// 1. Send empty stdout message
	Internal::FastCgiWriter::writeStreamMessage (this->d_ptr->device, FastCgiType::StdOut,
	                                             this->d_ptr->requestId, QByteArray ());
	
	// 2. Send end request
	Internal::FastCgiWriter::writeEndRequest (this->d_ptr->device, this->d_ptr->requestId,
	                                          0, FastCgiProtocolStatus::RequestComplete);
	
}

void Nuria::Internal::FastCgiTransport::forwardBodyData (const QByteArray &data) {
	if (this->d_ptr->client && !data.isEmpty ()) {
		QByteArray d = data;
		readFromRemote (this->d_ptr->client, d);
	}
	
}

static QByteArray paramNameToHttpHeader (QByteArray name) {
	bool firstChar = true;
	int len = name.length ();
	char *raw = name.data ();
	
	for (int i = 0; i < len; i++) {
		if (raw[i] == '_') {
			raw[i] = '-';
			firstChar = true;
		} else if (firstChar) {
			raw[i] = toupper (raw[i]);
			firstChar = false;
		} else {
			raw[i] = tolower (raw[i]);
		}
		
	}
	
	return name;
}

Nuria::HttpClient::HeaderMap Nuria::Internal::FastCgiTransport::processedHeaders () {
	HttpClient::HeaderMap result;
	
	// Convert FCGI parameter names (FOO_BAR -> Foo-Bar)
	for (auto it = this->d_ptr->headers.constBegin (), end = this->d_ptr->headers.constEnd (); it != end; ++it) {
		QByteArray name = it.key ();
		if (name.startsWith ("HTTP_")) { // Remove 'HTTP_' prefix
			name = name.mid (5); // "HTTP_"
		}
		
		result.insert (paramNameToHttpHeader (name), it.value ());
	}
	
	return result;
}

void Nuria::Internal::FastCgiTransport::replaceFirstLine (QByteArray &data) {
	this->d_ptr->firstLineReplaced = true;
	
	// Replace the leading "HTTP/1.x" (8 Bytes) with "Status:"
	data.replace (0, 8, "Status:");
	
}

void Nuria::Internal::FastCgiTransport::preparePeerData () {
	QString local = this->d_ptr->headers.value (fcgiLocalAddr);
	QString peer = this->d_ptr->headers.value (fcgiRemoteAddr);
	
	this->d_ptr->localPort = this->d_ptr->headers.value (fcgiLocalPort).toUShort ();
	this->d_ptr->peerPort = this->d_ptr->headers.value (fcgiRemotePort).toUShort ();
	
	this->d_ptr->localAddress.setAddress (local);
	this->d_ptr->peerAddress.setAddress (peer);
	
}

void Nuria::Internal::FastCgiTransport::startClient () {
	HttpParser parser;
	
	// Read HTTP information
	QByteArray httpVersion = this->d_ptr->headers.value (fcgiHttpVersion).mid (5); // Strip "HTTP/"
	
	HttpClient::HttpVerb verb = parser.parseVerb (this->d_ptr->headers.value (fcgiRequestVerb));
	QByteArray path = this->d_ptr->headers.value (fcgiRequestPath);
	HttpClient::HttpVersion version = parser.parseVersion (httpVersion);
	preparePeerData ();
	
	// Headers
	HttpClient::HeaderMap headers = processedHeaders ();
	this->d_ptr->headers.clear ();
	
	// Process
	this->d_ptr->client->setTransferMode (HttpClient::Streaming);
	this->d_ptr->client->manualInit (verb, version, path, headers);
}

QHostAddress Nuria::Internal::FastCgiTransport::localAddress () const {
	return this->d_ptr->localAddress;
}

quint16 Nuria::Internal::FastCgiTransport::localPort () const {
	return this->d_ptr->localPort;
}

QHostAddress Nuria::Internal::FastCgiTransport::peerAddress () const {
	return this->d_ptr->peerAddress;
}

quint16 Nuria::Internal::FastCgiTransport::peerPort () const {
	return this->d_ptr->peerPort;
}

bool Nuria::Internal::FastCgiTransport::flush (HttpClient *) {
	return true;
}
