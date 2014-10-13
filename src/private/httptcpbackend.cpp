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

#include "httptcpbackend.hpp"
#include "tcpserver.hpp"


Nuria::Internal::HttpTcpBackend::HttpTcpBackend (TcpServer *server, HttpServer *parent)
        : HttpBackend (parent), m_server (server)
{
	server->setParent (this);
	
	connect (server, &TcpServer::connectionRequested, this, &HttpTcpBackend::newClient);
	
}

bool Nuria::Internal::HttpTcpBackend::listen (const QHostAddress &interface, quint16 port) {
	return this->m_server->listen (interface, port);
}

bool Nuria::Internal::HttpTcpBackend::isListening () const {
	return this->m_server->isListening ();
}

int Nuria::Internal::HttpTcpBackend::port () const {
	return this->m_server->serverPort ();
}

void Nuria::Internal::HttpTcpBackend::newClient (qintptr handle) {
	HttpTcpTransport *transport = new HttpTcpTransport (handle, this, httpServer ());
	Q_UNUSED(transport)
}

Nuria::Internal::TcpServer *Nuria::Internal::HttpTcpBackend::tcpServer () const {
	return this->m_server;
}
