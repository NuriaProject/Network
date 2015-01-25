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

#include "tcpserver.hpp"

#include <nuria/logger.hpp>
#include <QTcpSocket>

#ifndef NURIA_NO_SSL_HTTP
#include <QSslSocket>
#endif

Nuria::Internal::TcpServer::TcpServer (bool useSsl, QObject *parent)
	: QTcpServer (parent), m_ssl (useSsl)
{
	
#ifndef NURIA_NO_SSL_HTTP
	if (useSsl) {
		nError() << "Trying to create unsupported SSL server - Was disabled at compile-time.";
		nError() << "Connections will fail.";
	}
#endif
	
}

Nuria::Internal::TcpServer::~TcpServer () {
	// 
}

#ifndef NURIA_NO_SSL_HTTP
const QSslKey &Nuria::Internal::TcpServer::privateKey () const {
	return this->m_key;
}

const QSslCertificate &Nuria::Internal::TcpServer::localCertificate () const {
	return this->m_cert;
}

void Nuria::Internal::TcpServer::setPrivateKey (const QSslKey &key) {
	this->m_key = key;
}

void Nuria::Internal::TcpServer::setLocalCertificate (const QSslCertificate &cert) {
	this->m_cert = cert;
}
#endif

QTcpSocket *Nuria::Internal::TcpServer::handleToSocket (qintptr handle) {
	if (!this->m_ssl) {
		QTcpSocket *socket = new QTcpSocket;
		socket->setSocketDescriptor (handle);
		return socket;
	}
	
	// SSL
#ifndef NURIA_NO_SSL_HTTP
	QSslSocket *socket = new QSslSocket (this);
	
	// Certificate and private key
	socket->setPrivateKey (this->m_key);
	socket->setLocalCertificate (this->m_cert);
	
	// Set handle
	if (!socket->setSocketDescriptor (handle)) {
		delete socket;
		return nullptr;
	}
	
	// 
	socket->startServerEncryption ();
	return socket;
#else	
	return nullptr;
#endif
	
}

bool Nuria::Internal::TcpServer::useEncryption () const {
	return this->m_ssl;
}

void Nuria::Internal::TcpServer::incomingConnection (qintptr handle) {
	emit connectionRequested (handle);
}
