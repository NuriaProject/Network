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

#ifndef NURIA_INTERNAL_HTTPTCPBACKEND_HPP
#define NURIA_INTERNAL_HTTPTCPBACKEND_HPP

#include "../nuria/httptcptransport.hpp"
#include "../nuria/httpbackend.hpp"
#include "../nuria/httpserver.hpp"
#include <QTcpServer>

namespace Nuria {
namespace Internal {

class HttpTcpBackend : public HttpBackend {
	Q_OBJECT
public:
	
	HttpTcpBackend (QTcpServer *server, HttpServer *parent)
	        : HttpBackend (parent), m_server (server)
	{
		server->setParent (this);
		connect (server, &QTcpServer::newConnection, this, &HttpTcpBackend::newClient);
	}
	
	bool listen (const QHostAddress &interface, quint16 port)
	{ return this->m_server->listen (interface, port); }
	
	bool isListening () const override
	{ return this->m_server->isListening (); }
	
	int port () const override
	{ return this->m_server->serverPort (); }
	
	void newClient () {
		while (this->m_server->hasPendingConnections ()) {
			
			QTcpSocket *socket = this->m_server->nextPendingConnection ();
			HttpTcpTransport *transport = new HttpTcpTransport (socket, this, httpServer ());
			Q_UNUSED(transport)
			
		}
		
	}
	
private:
	QTcpServer *m_server;
};

}
}

#endif // NURIA_INTERNAL_HTTPTCPBACKEND_HPP
