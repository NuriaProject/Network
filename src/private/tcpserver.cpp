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

#include "tcpserver.hpp"

#include <QSslSocket>

Nuria::Internal::TcpServer::TcpServer(bool useSsl, QObject *parent)
	: QTcpServer (parent), m_ssl (useSsl)
{
	
}

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

QTcpSocket *Nuria::Internal::TcpServer::handleToSocket (qintptr handle) {
	if (!this->m_ssl) {
		QTcpSocket *socket = new QTcpSocket;
		socket->setSocketDescriptor (handle);
		return socket;
	}
	
	// 
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
}

bool Nuria::Internal::TcpServer::useEncryption () const {
	return this->m_ssl;
}

void Nuria::Internal::TcpServer::incomingConnection (qintptr handle) {
	emit connectionRequested (handle);
}
