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
class HttpServerPrivate;
class HttpClient;
class SslServer;
class HttpNode;

/**
 * \brief TCP/SSL Server for the HyperText Transfer Protocol
 * 
 * This is a implementation of a server for HTTP. The server class itself
 * is responsible for managing the "root node" (The equivalent to the document
 * root in other HTTP servers) and for accepting new connections.
 * 
 * Please see HttpNode and HttpClient
 * 
 * \note For redirections to work properly, you should set the fully qualified
 * domain name of the server using setFqdn().
 * 
 */
class NURIA_NETWORK_EXPORT HttpServer : public QObject {
	Q_OBJECT
public:
	
	/**
	 * Constructor. If you pass \c false for \a supportSsl, the HTTP server
	 * won't support SSL connections.
	 * \sa listen listenSecure
	 */
	HttpServer (bool supportSsl = true, QObject *parent = 0);
	
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
	 * Starts the HTTP server.
	 * Returns \c true on success.
	 */
	bool listen (const QHostAddress &interface = QHostAddress::Any, quint16 port = 80);
	
	/**
	 * Lets the HTTP server listen on a SSL encrypted port.
	 * Returns \c on success.
	 * \note For this to work you have to pass \c true for
	 * the \a supportSsl parameter in the constructor!
	 */
	bool listenSecure (const QHostAddress &interface = QHostAddress::Any, quint16 port = 443);
	
	/** Returns the local SSL certificate. \sa setLocalCertificate */
	QSslCertificate localCertificate () const;
	
	/** Returns the SSL private key. \sa setPrivateKey */
	QSslKey privateKey () const;
	
	/**
	 * Returns the fully-qualified domain name of this server.
	 * \sa setFqdn
	 */
	const QString &fqdn () const;
	
	/**
	 * Sets the local SSL certificate. \b Needed if you want to use SSL encryption.
	 * \sa setPrivateKey
	 */
	void setLocalCertificate (const QSslCertificate &cert);
	
	/**
	 * Sets the SSL private key. \b Needed if you want to use SSL encryption.
	 * \sa setLocalCertificate
	 */
	void setPrivateKey (const QSslKey &key);
	
	/**
	 * Returns the HTTP port the server is listening on.
	 * If the server isn't listening \c -1 is returned.
	 */
	int port () const;
	
	/**
	 * Returns the HTTPS port the server is listening on.
	 * If the server isn't listening \c -1 is returned.
	 */
	int securePort () const;
	
	/**
	 * Sets the fully-qualified domain name of this server.
	 * The server will fall-back using the 'Host' header value from
	 * clients if no value is set (default).
	 */
	void setFqdn (const QString &fqdn);
	
private slots:
	
	void newClient ();
	
private:
	friend class HttpClient;
	friend class HttpNode;
	
	/**
	 * Internal function used by HttpClient::resolveUrl to invoke a node slot
	 * by the \a path. Returns \a true on success.
	 */
	bool invokeByPath (HttpClient *client, const QString &path);
	
	/**
	 * Internal function used to redirect the client to the
	 * secure port.
	 */
	void redirectClientToUseEncryption (HttpClient *client);
	
	// 
	HttpServerPrivate *d_ptr;
	
};
}

#endif // NURIA_HTTPSERVER_HPP
