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

#include "nuria/httptcptransport.hpp"

#include <QSslSocket>
#include <QTcpSocket>

namespace Nuria {
class HttpTcpTransportPrivate {
public:
	
	QTcpSocket *socket;
	QSslSocket *sslSocket;
	
};
}

Nuria::HttpTcpTransport::HttpTcpTransport (QTcpSocket *socket, QObject *parent)
	: HttpTransport (parent), d_ptr (new HttpTcpTransportPrivate)
{
	
	this->d_ptr->socket = socket;
	this->d_ptr->sslSocket = qobject_cast< QSslSocket * > (socket);
	setOpenMode (ReadWrite);
	
	// Forward signals
	connect (this->d_ptr->socket, SIGNAL(disconnected()), SIGNAL(aboutToClose()));
	connect (this->d_ptr->socket, SIGNAL(readyRead()), SIGNAL(readyRead()));
	
	if (this->d_ptr->sslSocket) {
		connect (this->d_ptr->sslSocket, SIGNAL(encryptedBytesWritten(qint64)),
			 SIGNAL(bytesWritten(qint64)));
	} else {
		connect (this->d_ptr->socket, SIGNAL(bytesWritten(qint64)),
			 SIGNAL(bytesWritten(qint64)));
	}
	
}

Nuria::HttpTcpTransport::~HttpTcpTransport () {
	delete this->d_ptr;
}

qint64 Nuria::HttpTcpTransport::readData (char *data, qint64 maxlen) {
	return this->d_ptr->socket->read (data, maxlen);
}

qint64 Nuria::HttpTcpTransport::readLineData (char *data, qint64 maxlen) {
	return this->d_ptr->socket->readLine (data, maxlen);
}

qint64 Nuria::HttpTcpTransport::writeData (const char *data, qint64 len) {
	return this->d_ptr->socket->write (data, len);
}

Nuria::HttpTransport::Type Nuria::HttpTcpTransport::type () const {
	return (this->d_ptr->sslSocket) ? SSL : TCP;
}

bool Nuria::HttpTcpTransport::isSecure () const {
	return (this->d_ptr->sslSocket != nullptr);
}

QHostAddress Nuria::HttpTcpTransport::localAddress () const {
	return this->d_ptr->socket->localAddress ();
}

quint16 Nuria::HttpTcpTransport::localPort () const {
	return this->d_ptr->socket->localPort ();
}

QHostAddress Nuria::HttpTcpTransport::peerAddress () const {
	return this->d_ptr->socket->peerAddress ();
}

quint16 Nuria::HttpTcpTransport::peerPort () const {
	return this->d_ptr->socket->peerPort ();
}

bool Nuria::HttpTcpTransport::flush () {
	return this->d_ptr->socket->flush ();
}

bool Nuria::HttpTcpTransport::isSequential () const {
	return this->d_ptr->socket->isSequential ();
}

bool Nuria::HttpTcpTransport::open (QIODevice::OpenMode mode) {
	return this->d_ptr->socket->open (mode);
}

void Nuria::HttpTcpTransport::close () {
	setOpenMode (NotOpen);
	
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

qint64 Nuria::HttpTcpTransport::pos () const {
	return this->d_ptr->socket->pos ();
}

qint64 Nuria::HttpTcpTransport::size () const {
	return this->d_ptr->socket->size ();
}

bool Nuria::HttpTcpTransport::seek (qint64 pos) {
	Q_UNUSED(pos);
	return false;
}

bool Nuria::HttpTcpTransport::atEnd () const {
	return this->d_ptr->socket->atEnd ();
}

bool Nuria::HttpTcpTransport::reset () {
	return this->d_ptr->socket->reset ();
}

qint64 Nuria::HttpTcpTransport::bytesAvailable () const {
	return this->d_ptr->socket->bytesAvailable ();
}

qint64 Nuria::HttpTcpTransport::bytesToWrite () const {
	return this->d_ptr->socket->bytesToWrite ();
}

bool Nuria::HttpTcpTransport::canReadLine () const {
	return this->d_ptr->socket->canReadLine () || QIODevice::canReadLine ();
}

bool Nuria::HttpTcpTransport::waitForReadyRead (int msecs) {
	return this->d_ptr->socket->waitForReadyRead (msecs);
}

bool Nuria::HttpTcpTransport::waitForBytesWritten (int msecs) {
	return this->d_ptr->socket->waitForBytesWritten (msecs);
}

void Nuria::HttpTcpTransport::forceClose () {
	QObject::disconnect (this->d_ptr->socket, SIGNAL(disconnected()),
			     this, SIGNAL(aboutToClose()));
	
	this->d_ptr->socket->close ();
	setOpenMode (NotOpen);
	
	emit aboutToClose ();
}

bool Nuria::HttpTcpTransport::closeSocketWhenBytesWereWritten() {
	if (this->d_ptr->socket->bytesToWrite () == 0) {
		forceClose ();
		return true;
	}
	
	return false;
}
