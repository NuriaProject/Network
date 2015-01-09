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

#ifndef NURIA_INTERNAL_HTTPTCPBACKEND_HPP
#define NURIA_INTERNAL_HTTPTCPBACKEND_HPP

#include "../nuria/httpbackend.hpp"
#include "../nuria/httpserver.hpp"

namespace Nuria {
namespace Internal {

class TcpServer;

class HttpTcpBackend : public HttpBackend {
	Q_OBJECT
public:
	
	HttpTcpBackend (TcpServer *server, HttpServer *parent);
	
	bool listen (const QHostAddress &interface, quint16 port);

	bool isListening () const override;
	int port () const override;
	
	void newClient (qintptr handle);
	
	TcpServer *tcpServer () const;
	
private:
	TcpServer *m_server;
};

}
}

#endif // NURIA_INTERNAL_HTTPTCPBACKEND_HPP
