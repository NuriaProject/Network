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

#ifndef NURIA_FASTCGIBACKEND_HPP
#define NURIA_FASTCGIBACKEND_HPP

#include <nuria/httpbackend.hpp>
#include <QLocalServer>
#include <QHostAddress>

class QTcpSocket;

namespace Nuria {

namespace Internal { class FastCgiThreadObject; }
class FastCgiBackendPrivate;
class FastCgiTransport;

/**
 * \brief FastCGI integration for HttpServer
 * 
 * This class provides a FastCGI backend for HttpServer. Using this you can
 * write FastCGI servers easily.
 * 
 * \note This implementation only supports writing FCGI responders.
 * 
 * \par Usage
 * 
 * This implementation supports both, opening a TCP socket which the front-end
 * HTTP server pushes requests to or inheriting the socket opened by the
 * front-end server from the \c FCGI_LISTENSOCK_FILENO environment variable.
 * 
 * \par FastCGI parameter
 * 
 * FastCGI front-end servers pass "parameters" to FastCGI applications to tell
 * them about incoming connections. These are comparable to environment
 * variables or HTTP headers.Hello World
 * 
 * The following parameters are needed by FastCgiBackend:
 * 
 * - \c REQUEST_METHOD (HTTP verb)
 * - \c REQUEST_URI (Path)
 * - \c REMOTE_ADDR (Peer address)
 * - \c REMOTE_PORT (Peer port)
 * - \c SERVER_ADDR (Server address)
 * - \c SERVER_PORT (Server port)
 * - \c SERVER_PROTOCOL ("HTTP/1.x")
 * 
 * Parameters whose name begin with "HTTP_" are stripped of that prefix.
 * All FastCGI parameters are forwarded to the HttpClient and can be accessed
 * by HttpClient::requestHeader(), with names are converted from "THIS_FORMAT"
 * to "This-Format". Example:
 * 
 * \code
 * client->requestHeader ("Some-Thing"); // Access the SOME_THING FCGI parameter
 * client->requestHeader ("Host"); // Access the HTTP_HOST FCGI parameter
 * \endcode
 * 
 * \par A note on logging
 * 
 * You should think about where to put your logs. Some front-end servers support
 * receiving log messages through the standard output or error streams. Whatever
 * you choose, if you need to change the default logging target, see
 * Debug::setDestination().
 * 
 * \par Behaviour in high-load scenarios
 * 
 * It should be pointed out that all tested front-end HTTP servers, namely
 * lighttpd and nginx, work equally well with this implementation in normal
 * circumstances. Though it should be noted that benchmarking showed that
 * nginx is inferior to lighttpd one. With about 1k concurrent requests done
 * by the benchmarking utility, nginx suffered from having issues serving the
 * last ca. 10% of all requests. Lighttpd does not have this restriction.
 * 
 * These restriction do not apply when configuring both servers to act as a
 * HTTP proxy and relying on the NuriaFrameworks HTTP implementation.
 * 
 */
class NURIA_NETWORK_EXPORT FastCgiBackend : public HttpBackend {
	Q_OBJECT
public:
	
	/** Constructor. */
	explicit FastCgiBackend (HttpServer *server);
	
	/** Destructor. */
	~FastCgiBackend () override;
	
	/**
	 * Start listening on the socket forwarded to this process.
	 * This is mainly there for front-end servers
	 */
	bool listen ();
	
	/** Listen for TCP connections on \a port and \a iface. */
	bool listen (int port, const QHostAddress &iface = QHostAddress::Any);
	
	/** Listens on \a name using a QLocalServer. */
	bool listenLocal (const QString &name, QLocalServer::SocketOptions options = QLocalServer::NoOptions);
	
	/** Listens on \a handle using a QLocalServer. */
	bool listenLocal (qintptr handle);
	
	/** Stops listening immediately. */
	void stop ();
	
	/**
	 * Returns the max amount of concurrent connections.
	 * Defaults to the count of CPU cores of the host machine.
	 */
	int maxConcurrentConnections () const;
	
	/** Sets the max amount of concurrent connections. */
	void setMaxConcurrentConnections (int count);
	
	/**
	 * Returns the max amount of concurrent requests.
	 * Defaults to: (count of CPU cores) * 10
	 */
	int maxConcurrentRequests () const;
	
	/** Sets the max amount of concurrent requests. */
	void setMaxConcurrentRequests (int count) const;
	
	/**
	 * Sets a custom configuration field for FastCGI front-end servers.
	 * This field will be sent to the FCGI server after receiving a
	 * \c FCGI_GET_VALUES request.
	 * 
	 * If there's already a configuration for \a name, it will be overriden.
	 */
	void setCustomConfiguration (const QByteArray &name, const QByteArray &value);
	
	/** Returns teh custom configuration map. */
	QMap< QByteArray, QByteArray > customConfiguration () const;
	
	/**
	 * Returns the count of connections currently opened by the
	 * FastCGI front-end server.
	 */
	int currentConnectionCount () const;
	
	// HttpBackend interface
	bool isListening () const;
	int port () const;
	bool isSecure () const;
	
signals:
	
	/**
	 * The FastCGI front-end server has opened a new connection to the
	 * back-end.
	 */
	void connectionAdded ();
	
	/**
	 * The FastCGI front-end server has closed a connection.
	 * 
	 * \note Don't connect to this signal using \c Qt::DirectConnection.
	 */
	void connectionClosed ();
	
protected:
	
	void serverThreadCreated (QThread *thread) override;
	
private:
	friend class Internal::FastCgiThreadObject;
	FastCgiBackendPrivate *d_ptr;
	
	void addTcpConnection (qintptr handle);
	void addLocalConnection (qintptr handle);
	
};

}

#endif // NURIA_FASTCGIBACKEND_HPP
