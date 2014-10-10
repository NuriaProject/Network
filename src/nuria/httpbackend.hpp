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
	
};
}

#endif // NURIA_HTTPBACKEND_HPP
