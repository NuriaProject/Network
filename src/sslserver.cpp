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

#include "sslserver.hpp"

#include <QSslSocket>

namespace Nuria {
class SslServerPrivate {
public:
	
	QSslKey key;
	QSslCertificate cert;
	
};
}

Nuria::SslServer::SslServer (QObject *parent)
	: QTcpServer (parent), d_ptr (new SslServerPrivate)
{
	
	// 
	
}

Nuria::SslServer::~SslServer () {
	delete this->d_ptr;
}

const QSslKey &Nuria::SslServer::privateKey () const {
	return this->d_ptr->key;
}

const QSslCertificate &Nuria::SslServer::localCertificate () const {
	return this->d_ptr->cert;
}

void Nuria::SslServer::setPrivateKey (const QSslKey &key) {
	this->d_ptr->key = key;
}

void Nuria::SslServer::setLocalCertificate (const QSslCertificate &cert) {
	this->d_ptr->cert = cert;
}

void Nuria::SslServer::connectionEncrypted () {
	QSslSocket *socket = qobject_cast< QSslSocket * > (sender ());
	
	if (!socket) {
		return;
	}
	
	// Add to pending connections
	addPendingConnection (socket);
	emit newConnection ();
	
}

void Nuria::SslServer::sslError (QList< QSslError > errors) {
	QSslSocket *socket = qobject_cast< QSslSocket * > (sender ());
	
	if (!socket) {
		return;
	}
	
	// TODO: Probably do something about SSL errors.
	socket->ignoreSslErrors (errors);
	
}

void Nuria::SslServer::incomingConnection (int handle) {
	
	// Create socket
	QSslSocket *socket = new QSslSocket (this);
	
	// Certificate and private key
	socket->setPrivateKey (this->d_ptr->key);
	socket->setLocalCertificate (this->d_ptr->cert);
	
	// Set handle
	if (!socket->setSocketDescriptor (handle)) {
		delete socket;
		return;
	}
	
	socket->startServerEncryption ();
	
	// Wait for the connection to be encrypted.
	// Once this step is done, connectionEncrypted()
	// will insert the socket into the pending connections list.
	connect (socket, SIGNAL(encrypted()), SLOT(connectionEncrypted()));
	connect (socket, SIGNAL(sslErrors(QList<QSslError>)), SLOT(sslError(QList<QSslError>)));
	
}
