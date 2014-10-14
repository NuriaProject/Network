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

#include "httptcptransport.hpp"

#include "../nuria/httpclient.hpp"
#include "../nuria/httpserver.hpp"
#include "httptcpbackend.hpp"
#include <nuria/debug.hpp>
#include "tcpserver.hpp"

#include <QSslSocket>
#include <QTcpSocket>

namespace Nuria {
namespace Internal {
class HttpTcpTransportPrivate {
public:
	
	qintptr socketHandle = 0;
	QTcpSocket *socket = nullptr;
	QSslSocket *sslSocket = nullptr;
	HttpClient *curClient = nullptr;
	HttpServer *server;
	int timeoutTimer = -1;
	int bytesReceived = 0;
	
};
}
}

Nuria::Internal::HttpTcpTransport::HttpTcpTransport (qintptr handle, HttpTcpBackend *backend, HttpServer *server)
	: HttpTransport (backend, server), d_ptr (new HttpTcpTransportPrivate)
{
	this->d_ptr->socketHandle = handle;
	this->d_ptr->server = server;
	
	addToServer ();
}

Nuria::Internal::HttpTcpTransport::~HttpTcpTransport () {
	delete this->d_ptr;
}

Nuria::HttpTransport::Type Nuria::Internal::HttpTcpTransport::type () const {
	return (this->d_ptr->sslSocket) ? SSL : TCP;
}

bool Nuria::Internal::HttpTcpTransport::isSecure () const {
	return (this->d_ptr->sslSocket != nullptr);
}

QHostAddress Nuria::Internal::HttpTcpTransport::localAddress () const {
	if (!this->d_ptr->socket) {
		return QHostAddress ();
	}
	
	return this->d_ptr->socket->localAddress ();
}


quint16 Nuria::Internal::HttpTcpTransport::localPort () const {
	if (!this->d_ptr->socket) {
		return 0;
	}
	
	return this->d_ptr->socket->localPort ();
}

QHostAddress Nuria::Internal::HttpTcpTransport::peerAddress () const {
	if (!this->d_ptr->socket) {
		return QHostAddress ();
	}
	
	return this->d_ptr->socket->peerAddress ();
}

quint16 Nuria::Internal::HttpTcpTransport::peerPort () const {
	if (!this->d_ptr->socket) {
		return 0;
	}
	
	return this->d_ptr->socket->peerPort ();
}

bool Nuria::Internal::HttpTcpTransport::isOpen () const {
	if (!this->d_ptr->socket) {
		return false;
	}
	
	return this->d_ptr->socket->isOpen ();
}


bool Nuria::Internal::HttpTcpTransport::flush (HttpClient *) {
	if (!this->d_ptr->socket) {
		return false;
	}
	
	return this->d_ptr->socket->flush ();
}

void Nuria::Internal::HttpTcpTransport::close (HttpClient *client) {
	if (client != this->d_ptr->curClient) {
		return;
	}
	
	// Throw the client away
	HttpClient::ConnectionMode mode = HttpClient::ConnectionClose;
	if (this->d_ptr->curClient) {
		mode = this->d_ptr->curClient->connectionMode ();
		this->d_ptr->curClient->deleteLater ();
		this->d_ptr->curClient = nullptr;
	}
	
	// 'Streaming' doesn't support keep-alive connections, so we're done here.
	if (mode == HttpClient::ConnectionClose || wasLastRequest ()) {
		closeInternal ();
	} else {
		startTimeout (KeepAliveTimeout);
	}
	
}

bool Nuria::Internal::HttpTcpTransport::sendToRemote (HttpClient *client, const QByteArray &data) {
	if (client != this->d_ptr->curClient || !this->d_ptr->socket || !this->d_ptr->socket->isOpen ()) {
		return false;
	}
	
	// 
	return (this->d_ptr->socket->write (data) == data.length ());
}

void Nuria::Internal::HttpTcpTransport::timerEvent (QTimerEvent *) {
#ifdef QT_DEBUG
	nDebug() << "Connection to" << this->d_ptr->socket->peerAddress () << "timed out - Closing.";
#endif
	
	// 
	forceClose ();
	
}

void Nuria::Internal::HttpTcpTransport::clientDestroyed (QObject *object) {
	if (object != this->d_ptr->curClient) {
		return;
	}
	
	// 
	close (this->d_ptr->curClient);
	
}

void Nuria::Internal::HttpTcpTransport::bytesWritten (qint64 bytes) {
	if (this->d_ptr->curClient) {
		bytesSent (this->d_ptr->curClient, bytes);
	}
	
}

void Nuria::Internal::HttpTcpTransport::processData (QByteArray &data) {
	if (this->d_ptr->curClient->requestCompletelyReceived ()) {
		return;
	}
	
	// Process ...
	readFromRemote (this->d_ptr->curClient, data);
	
	// 
	if (this->d_ptr->curClient && this->d_ptr->curClient->requestCompletelyReceived ()) {
		killTimeout ();
	}
	
}

void Nuria::Internal::HttpTcpTransport::dataReceived () {
	QByteArray data = this->d_ptr->socket->readAll ();
	
	while (!data.isEmpty () && this->d_ptr->socket->isOpen ()) {
		if (this->d_ptr->curClient && !this->d_ptr->curClient->isOpen ()) {
			close (this->d_ptr->curClient);
		}
		
		// 
		if (!this->d_ptr->curClient) {
			startTimeout (DataTimeout);
			setCurrentRequestCount (currentRequestCount () + 1);
			this->d_ptr->curClient = new HttpClient (this, this->d_ptr->server);
		}
		
		// 
		processData (data);
		
	}
	
}

void Nuria::Internal::HttpTcpTransport::clientDisconnected () {
	if (this->d_ptr->curClient) {
		this->d_ptr->curClient->close ();
		this->d_ptr->curClient->deleteLater ();
		this->d_ptr->curClient = nullptr;
	}
	
	// Destroy this transport
	deleteLater ();
	
}

void Nuria::Internal::HttpTcpTransport::forceClose () {
	disconnect (this->d_ptr->socket, &QTcpSocket::disconnected,
	            this, &HttpTransport::connectionLost);
	
	this->d_ptr->socket->close ();
	deleteLater ();
}

void Nuria::Internal::HttpTcpTransport::init () {
	Internal::HttpTcpBackend *tcpBackend = static_cast< Internal::HttpTcpBackend * > (backend ());
	
	this->d_ptr->socket = tcpBackend->tcpServer ()->handleToSocket (this->d_ptr->socketHandle);
	this->d_ptr->sslSocket = qobject_cast< QSslSocket * > (this->d_ptr->socket);
	
	if (!this->d_ptr->socket) {
		nError() << "Failed to open TCP socket on handle" << this->d_ptr->socketHandle;
		return;
	}
	
	// 
	this->d_ptr->socket->setParent (this);
	
	// Connect to signals
	connect (this->d_ptr->socket, &QTcpSocket::disconnected, this, &HttpTcpTransport::clientDisconnected);
	connect (this->d_ptr->socket, &QIODevice::readyRead, this, &HttpTcpTransport::dataReceived);
	
	if (this->d_ptr->sslSocket) {
		connect (this->d_ptr->sslSocket, &QSslSocket::encryptedBytesWritten,
			 this, &HttpTcpTransport::bytesWritten);
		connect (this->d_ptr->sslSocket, &QSslSocket::encrypted,
		         this, &HttpTcpTransport::connectionReady);
	} else {
		connect (this->d_ptr->socket, &QTcpSocket::bytesWritten,
			 this, &HttpTcpTransport::bytesWritten);
		connectionReady ();
	}
	
}

bool Nuria::Internal::HttpTcpTransport::closeSocketWhenBytesWereWritten() {
	if (this->d_ptr->socket->bytesToWrite () == 0) {
		forceClose ();
		return true;
	}
	
	return false;
}

void Nuria::Internal::HttpTcpTransport::closeInternal () {
	if (closeSocketWhenBytesWereWritten ()) {
	        return;
	}
	
	// Wait for socket to completely send write buffer.
	if (this->d_ptr->sslSocket) {
	        connect (this->d_ptr->sslSocket, SIGNAL(encryptedBytesWritten(qint64)),
		         SLOT(closeSocketWhenBytesWereWritten()));
	} else {
	        connect (this->d_ptr->socket, SIGNAL(bytesWritten(qint64)),
		         SLOT(closeSocketWhenBytesWereWritten()));
	}
	
}

bool Nuria::Internal::HttpTcpTransport::wasLastRequest () {
	return (maxRequests () >= 0 && currentRequestCount () >= maxRequests ());
}

void Nuria::Internal::HttpTcpTransport::startTimeout (HttpTransport::Timeout mode) {
	return;
	killTimeout ();
	
	// Start new timer
	int msec = timeout (mode);
	if (msec >= 0) {
		this->d_ptr->timeoutTimer = startTimer (msec);
	} else {
		this->d_ptr->timeoutTimer = -1;
	}
	
}

void Nuria::Internal::HttpTcpTransport::killTimeout () {
	if (this->d_ptr->timeoutTimer >= 0) {
		killTimer (this->d_ptr->timeoutTimer);
		this->d_ptr->timeoutTimer = -1;
	}
	
}

void Nuria::Internal::HttpTcpTransport::connectionReady () {
	startTimeout (ConnectTimeout);
}
