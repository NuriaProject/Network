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

#include "nuria/httpserver.hpp"

#include <QMetaMethod>
#include <QStringList>
#include <QTcpServer>
#include <QDir>

#include "nuria/httptcptransport.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/sslserver.hpp"
#include "nuria/httpnode.hpp"
#include <nuria/debug.hpp>

#include "private/httpprivate.hpp"

namespace Nuria {
class HttpServerPrivate {
public:
	
	QTcpServer *server;
	SslServer *sslServer;
	HttpNode *root;
	int port;
	int securePort;
	QString fqdn;
	
};
}

Nuria::HttpServer::HttpServer (bool supportSsl, QObject *parent)
	: QObject (parent), d_ptr (new HttpServerPrivate)
{
	
	// Register meta types
	qRegisterMetaType< HttpClient * > ();
	
	// 
	this->d_ptr->sslServer = 0;
	this->d_ptr->port = -1;
	this->d_ptr->securePort = -1;
	
	// Create TCP server and node
	this->d_ptr->server = new QTcpServer (this);
	
	this->d_ptr->root = new HttpNode (this);
	this->d_ptr->root->setResourceName (QStringLiteral("ROOT"));
	
	// Connect to m_server's newConnection() signal
	connect (this->d_ptr->server, SIGNAL(newConnection()), SLOT(newClient()));
	
	// Setup SSL server
	if (supportSsl) {
		this->d_ptr->sslServer = new SslServer (this);
		
		connect (this->d_ptr->sslServer, SIGNAL(newConnection()), SLOT(newClient()));
		
	}
	
}

Nuria::HttpServer::~HttpServer () {
	delete this->d_ptr;
}

Nuria::HttpNode *Nuria::HttpServer::root () const {
	return this->d_ptr->root;
}

void Nuria::HttpServer::setRoot (HttpNode *node) {
	
	// Don't do anything if m_root and node are the same
	if (this->d_ptr->root == node)
		return;
	
	// Delete m_root and take ownership of node
	delete this->d_ptr->root;
	this->d_ptr->root = node;
	
	node->setParent (this);
	node->d_ptr->parent = 0;
	
}

bool Nuria::HttpServer::listen (const QHostAddress &interface, quint16 port) {
	if (this->d_ptr->server->listen (interface, port)) {
		this->d_ptr->port = port;
		return true;
	}
	
	this->d_ptr->port = -1;
	return false;
	
}

bool Nuria::HttpServer::listenSecure (const QHostAddress &interface, quint16 port) {
	
	this->d_ptr->securePort = -1;
	if (this->d_ptr->sslServer) {
		if (this->d_ptr->sslServer->listen (interface, port)) {
			this->d_ptr->securePort = port;
			return true;
		}
		
	}
	
	return false;
}

QSslCertificate Nuria::HttpServer::localCertificate () const {
	if (this->d_ptr->sslServer) {
		return this->d_ptr->sslServer->localCertificate ();
	}
	
	return QSslCertificate ();
}

QSslKey Nuria::HttpServer::privateKey () const {
	if (this->d_ptr->sslServer) {
		return this->d_ptr->sslServer->privateKey ();
	}
	
	return QSslKey ();
}

const QString &Nuria::HttpServer::fqdn () const {
	return this->d_ptr->fqdn;
}

void Nuria::HttpServer::setLocalCertificate (const QSslCertificate &cert) {
	if (this->d_ptr->sslServer) {
		this->d_ptr->sslServer->setLocalCertificate (cert);
	}
	
}

void Nuria::HttpServer::setPrivateKey (const QSslKey &key) {
	if (this->d_ptr->sslServer) {
		this->d_ptr->sslServer->setPrivateKey (key);
	}
	
}

int Nuria::HttpServer::port () const {
	return this->d_ptr->port;
}

int Nuria::HttpServer::securePort () const {
	return this->d_ptr->securePort;
}

void Nuria::HttpServer::setFqdn (const QString &fqdn) {
	this->d_ptr->fqdn = fqdn;
}

void Nuria::HttpServer::newClient () {
	QTcpServer *server = qobject_cast< QTcpServer * > (sender ());
	
	if (!server)
		return;
	
	while (server->hasPendingConnections ()) {
		
		QTcpSocket *socket = server->nextPendingConnection ();
		HttpTcpTransport *transport = new HttpTcpTransport (socket, this);
		Q_UNUSED(transport)
		
	}
	
}

bool Nuria::HttpServer::invokeByPath (HttpClient *client, const QString &path) {
	
	// Split the path.
	QStringList parts = path.split (QLatin1Char ('/'), QString::SkipEmptyParts);
	
	// Try to invoke
	if (this->d_ptr->root->invokePath (path, parts, 0, client)) {
		return true;
	}
	
	// 
	if (!client->responseHeaderSent ()) {
		nLog() << "404 - File/Slot not found:" << path;
		client->killConnection (404);
	}
	
	return false;
	
}
