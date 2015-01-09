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

#include "httptcpbackend.hpp"

#include "httptcptransport.hpp"
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
