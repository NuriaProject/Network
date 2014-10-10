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
#include "nuria/httpbackend.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/sslserver.hpp"
#include "nuria/httpnode.hpp"
#include <nuria/debug.hpp>

#include "private/httptcpbackend.hpp"
#include "private/httpprivate.hpp"

namespace Nuria {
class HttpServerPrivate {
public:
	
	HttpNode *root;
	QString fqdn;
	
	QVector< HttpBackend * > backends;
	
};
}

Nuria::HttpServer::HttpServer (QObject *parent)
	: QObject (parent), d_ptr (new HttpServerPrivate)
{
	
	// Register meta types
	qRegisterMetaType< HttpClient * > ();
	
	// Create root node
	this->d_ptr->root = new HttpNode (this);
	this->d_ptr->root->setResourceName (QStringLiteral("ROOT"));
	
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
	
	// Delete current root and take ownership of node
	delete this->d_ptr->root;
	this->d_ptr->root = node;
	
	node->setParent (this);
	node->d_ptr->parent = nullptr;
	
}

bool Nuria::HttpServer::listen (const QHostAddress &interface, quint16 port) {
	return addQTcpServerBackend (new QTcpServer, interface, port);
}

bool Nuria::HttpServer::listenSecure (const QSslCertificate &certificate, const QSslKey &privateKey,
                                      const QHostAddress &interface, quint16 port) {
	SslServer *server = new SslServer;
	server->setLocalCertificate (certificate);
	server->setPrivateKey (privateKey);
	
	return addQTcpServerBackend (server, interface, port);
}

QString Nuria::HttpServer::fqdn () const {
	return this->d_ptr->fqdn;
}

void Nuria::HttpServer::setFqdn (const QString &fqdn) {
	this->d_ptr->fqdn = fqdn;
}

void Nuria::HttpServer::addBackend (HttpBackend *backend) {
	backend->setParent (this);
	this->d_ptr->backends.append (backend);
}

QVector< Nuria::HttpBackend * > Nuria::HttpServer::backends () const {
	return this->d_ptr->backends;
}

void Nuria::HttpServer::stopListening (int port) {
	for (int i = 0; i < this->d_ptr->backends.length (); i++) {
		if (this->d_ptr->backends.at (i)->port () == port) {
			delete this->d_ptr->backends.takeAt (i);
			return;
		}
		
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

bool Nuria::HttpServer::addQTcpServerBackend (QTcpServer *server, const QHostAddress &interface, quint16 port) {
	Internal::HttpTcpBackend *backend = new Internal::HttpTcpBackend (server, this);
	
	if (!backend->listen (interface, port)) {
		delete backend;
		return false;
	}
	
	// 
	this->d_ptr->backends.append (backend);
	return true;
}

void Nuria::HttpServer::addTransport (HttpTransport *transport) {
	Q_UNUSED(transport);
}
