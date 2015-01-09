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

#ifndef NURIA_HTTPBACKEND_HPP
#define NURIA_HTTPBACKEND_HPP

#include "network_global.hpp"
#include <QObject>

namespace Nuria {
class HttpServer;

/**
 * \brief Base class for HttpServer back-ends
 * 
 * A HttpBackend is the bridge between a (probably low-level) connection medium, like
 * a TCP listen socket, and the HttpServer this back-end is registered to.
 * 
 * For usage see HttpServer::addBackend() and HttpTransport::addToServer().
 */
class NURIA_NETWORK_EXPORT HttpBackend : public QObject {
	Q_OBJECT
public:
	
	/** Constructor. */
	HttpBackend (HttpServer *server);
	
	/** Destructor. */
	~HttpBackend ();
	
	/**
	 * Returns \c true if the server is currently listening for connections.
	 */
	virtual bool isListening () const = 0;
	
	/**
	 * Returns the port this server is listening on, or \c -1 if it's not
	 * listening.
	 */
	virtual int port () const = 0;
	
	/**
	 * Returns \c true if this back-end uses secure connections.
	 * The default implementation returns \c false.
	 */
	virtual bool isSecure () const;
	
	/** Returns the HTTP server this back-end is connected to. */
	HttpServer *httpServer() const;
	
protected:
	friend class HttpServer;
	
	/**
	 * Called by the HttpServer when a new server thread has been created.
	 * This lets back-ends register their own thread-specific bookkeeping
	 * routines.
	 * 
	 * This function will also be called by the HttpServer if no
	 * multi-threading is used. In this case, the function will be called
	 * once with \a thread being the one the server is associated with.
	 * 
	 * To run some kind of initialization after the \a thread has been
	 * started, use one of:
	 * \code
	 * QTimer::singleShot (0, object, SLOT(...)); // Pre Qt5.4
	 * QTimer::singleShot (0, object, &Object::...); // Qt5.4 and up
	 * QTimer::singleShot (0, []() { ... }); // Qt5.4 and up
	 * \endcode
	 * 
	 * \note The function is called in the thread the HttpServer lives in.
	 * 
	 * The default implementation does nothing.
	 */
	virtual void serverThreadCreated (QThread *thread);
	
};
}

#endif // NURIA_HTTPBACKEND_HPP
