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

#ifndef NURIA_HTTPSERVER_HPP
#define NURIA_HTTPSERVER_HPP

#include "network_global.hpp"
#include <QSslCertificate>
#include <QHostAddress>
#include <QObject>
#include <QSslKey>

class QTcpServer;

namespace Nuria {
namespace Internal { class TcpServer; }
class HttpServerPrivate;
class HttpTransport;
class HttpBackend;
class HttpClient;
class HttpNode;

/**
 * \brief Server for the HyperText Transfer Protocol
 * 
 * This is a implementation of a server for HTTP. The server class itself
 * is responsible for managing the "root node" (The equivalent to the document
 * root in other HTTP servers).
 * 
 * Please see HttpNode and HttpClient
 * 
 * \note For redirections to work properly, you should set the fully qualified
 * domain name of the server using setFqdn().
 * 
 * \par Usage
 * After creating an instance of HttpServer, you can change the node hierarchy
 * using root() or set your own using setRoot(). After that, you'll want to
 * start listening. For TCP, there's listen() and for SSL there's
 * listenSecure().
 * 
 * \note A single HttpServer can listen on multiple back-ends (TCP, SSL, ...)
 * at the same time.
 * 
 * Please see HttpNode for further information.
 * 
 * \par Multithreading
 * 
 * A HttpServer can either do all request execution in the thread it lifes in,
 * which is the default setting, or use threading to offload requests. If
 * threading is active (By using setMaxThreads), all request handling will be
 * done in threads and none will be handled in the HttpServer thread.
 * 
 * \warning When using multi-threading, be aware that your code is also
 * thread-safe.
 * 
 * Tip: You can use DependencyManager to manage resources
 */
class NURIA_NETWORK_EXPORT HttpServer : public QObject {
	Q_OBJECT
public:
	
	/** Additional threading options for setMaxThreads(). */
	enum ThreadingMode {
		
		/**
		 * Uses no threads. All processing is done in the thread of the
		 * HttpServer instance.
		 */
		NoThreading = 0,
		
		/**
		 * When passed to setMaxThreads(), one thread per CPU core will
		 * be created. maxThreads() will return the chosen value instead
		 * of OneThreadPerCore.
		 */
		OneThreadPerCore = -1
	};
	
	/**
	 * Constructor.
	 * \sa listen listenSecure
	 */
	HttpServer (QObject *parent = 0);
	
	/** Destructor. */
	~HttpServer ();
	
	/** Returns the root node. */
	HttpNode *root () const;
	
	/**
	 * Replaces the current root node with \a node. The old root node will
	 * be deleted. HttpServer will take ownership of \a node.
	 */
	void setRoot (HttpNode *node);
	
	/**
	 * Adds a TCP server, listening on \a interface and \a port.
	 * Returns \c true on success.
	 */
	bool listen (const QHostAddress &interface = QHostAddress::Any, quint16 port = 80);
	
	/**
	 * Adds a SSL server, listening on \a interface and \a port.
	 * Returns \c true on success.
	 */
	bool listenSecure (const QSslCertificate &certificate, const QSslKey &privateKey,
	                   const QHostAddress &interface = QHostAddress::Any, quint16 port = 443);
	
	/**
	 * Returns the fully-qualified domain name of this server.
	 * \sa setFqdn
	 */
	QString fqdn () const;
	
	/**
	 * Sets the fully-qualified domain name of this server.
	 */
	void setFqdn (const QString &fqdn);
	
	/**
	 * Adds \a server to the list of back-ends. Use this when you write a
	 * server implementation. Ownership of \a server is transferred to the
	 * HttpServer.
	 */
	void addBackend (HttpBackend *backend);
	
	/** Returns a list of currently installed back-ends. */
	QVector< HttpBackend * > backends () const;
	
	/** Removes the back-end which is listening on \a port. */
	void stopListening (int port);
	
	/**
	 * Returns the amount of threads used to process requests.
	 * The default result is \c NoThreading.
	 */
	int maxThreads () const;
	
	/**
	 * Sets the \a amount of threads to be used for processing requests.
	 * See ThreadingMode for other values that can be passed.
	 * 
	 * If the new value is less than the current one, requests running in
	 * the affected will be processed, after which they're destroyed.
	 */
	void setMaxThreads (int amount);
	
private:
	friend class HttpTransport;
	friend class HttpClient;
	friend class HttpNode;
	
	/**
	 * Internal function used by HttpClient::resolveUrl to invoke a node slot
	 * by the \a path. Returns \a true on success.
	 */
	bool invokeByPath (HttpClient *client, const QString &path);
	bool addTcpServerBackend (Internal::TcpServer *server, const QHostAddress &interface, quint16 port);
	bool addTransport(HttpTransport *transport);
	void startProcessingThreads (int amount);
	void stopProcessingThreads (int lastN);
	int chooseThreadCount ();
	void threadStopped (QObject *obj);
	
	// 
	HttpServerPrivate *d_ptr;
	
};
}

#endif // NURIA_HTTPSERVER_HPP
