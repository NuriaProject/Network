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

#include "nuria/sslserver.hpp"

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
