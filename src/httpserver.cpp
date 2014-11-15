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

#include <QStringList>
#include <QMutex>

#include "nuria/httptransport.hpp"
#include "nuria/httpbackend.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/httpnode.hpp"
#include <nuria/debug.hpp>

#include "private/httptcpbackend.hpp"
#include "private/httpprivate.hpp"
#include "private/httpthread.hpp"
#include "private/tcpserver.hpp"

namespace Nuria {
class HttpServerPrivate {
public:
	
	HttpNode *root;
	QString fqdn;
	
	QVector< HttpBackend * > backends;
	QVector< Internal::HttpThread * > threads;
	int threadIndex = 0;
	int activeThreads = 0;
	
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
	
	// Stop all threads
	for (int i = 0; i < this->d_ptr->threads.length (); i++) {
		this->d_ptr->threads.at (i)->stopGraceful ();
	}
	
	// Wait for all threads
//	for (int i = 0; i < this->d_ptr->threads.length (); i++) {
//		this->d_ptr->threads.at (i)->wait ();
//	}
	
	// Delete all backends while we're still here
	for (int i = 0; i < this->d_ptr->backends.length (); i++) {
		delete this->d_ptr->backends.at (i);
	}
	
	// 
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
	return addTcpServerBackend (new Internal::TcpServer (false), interface, port);
}

#ifndef NURIA_NO_SSL_HTTP
bool Nuria::HttpServer::listenSecure (const QSslCertificate &certificate, const QSslKey &privateKey,
                                      const QHostAddress &interface, quint16 port) {
	Internal::TcpServer *server = new Internal::TcpServer (true);
	server->setLocalCertificate (certificate);
	server->setPrivateKey (privateKey);
	
	return addTcpServerBackend (server, interface, port);
}
#endif

QString Nuria::HttpServer::fqdn () const {
	return this->d_ptr->fqdn;
}

void Nuria::HttpServer::setFqdn (const QString &fqdn) {
	this->d_ptr->fqdn = fqdn;
}

void Nuria::HttpServer::addBackend (HttpBackend *backend) {
	backend->setParent (this);
	this->d_ptr->backends.append (backend);
	notifyBackendOfThreads (backend);
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

int Nuria::HttpServer::maxThreads () const {
	return this->d_ptr->activeThreads;
}

void Nuria::HttpServer::setMaxThreads (int amount) {
	if (amount == OneThreadPerCore) {
		amount = chooseThreadCount ();
	}
	
	// Start/stop threads
	if (amount < this->d_ptr->activeThreads) {
		stopProcessingThreads (this->d_ptr->activeThreads - amount);
	} else if (amount > this->d_ptr->activeThreads) {
		startProcessingThreads (amount - this->d_ptr->activeThreads);
	}
	
	// 
	this->d_ptr->activeThreads = amount;
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

bool Nuria::HttpServer::addTcpServerBackend (Internal::TcpServer *server, const QHostAddress &interface, quint16 port) {
	Internal::HttpTcpBackend *backend = new Internal::HttpTcpBackend (server, this);
	
	if (!backend->listen (interface, port)) {
		delete backend;
		return false;
	}
	
	// 
	this->d_ptr->backends.append (backend);
	notifyBackendOfThreads (backend);
	return true;
}

bool Nuria::HttpServer::addTransport (HttpTransport *transport) {
	if (this->d_ptr->activeThreads < 1 || this->thread () != transport->thread ()) {
		return false;
	}
	
	// Move to the next thread.
	// TODO: This is a naive approach which is bad for servers that have lots of long-running requests.
	int index = this->d_ptr->threadIndex % this->d_ptr->activeThreads;
	this->d_ptr->threadIndex++;
	
	// 
	Internal::HttpThread *thread = this->d_ptr->threads.at (index);
	transport->setParent (nullptr);
	transport->moveToThread (thread);
	thread->incrementRunning ();
	connect (transport, &QObject::destroyed,
	         thread, &Internal::HttpThread::transportDestroyed);
	
	// Transport has been moved to another thread.
	return true;
}

void Nuria::HttpServer::startProcessingThreads (int amount) {
	for (int i = 0; i < amount; i++) {
		Internal::HttpThread *thread = new Internal::HttpThread (this);
		
		connect (thread, &QObject::destroyed, this, &HttpServer::threadStopped);
		this->d_ptr->threads.append (thread);
		notifyBackendsOfNewThread (thread);
		
		thread->start ();
	}
	
}

void Nuria::HttpServer::stopProcessingThreads (int lastN) {
	int count = this->d_ptr->threads.length ();
	int i = count - lastN;
	
	for (; i < count; i++) {
		Internal::HttpThread *thread = this->d_ptr->threads.at (i);
		Internal::HttpThread::staticMetaObject.invokeMethod (thread, "stopGraceful");
	}
	
}

void Nuria::HttpServer::notifyBackendsOfNewThread (QThread *serverThread) {
	for (int i = 0; i < this->d_ptr->backends.length (); i++) {
		this->d_ptr->backends.at (i)->serverThreadCreated (serverThread);
	}
	
}

void Nuria::HttpServer::notifyBackendOfThreads (HttpBackend *backend) {
	if (this->d_ptr->activeThreads < 1) {
		backend->serverThreadCreated (thread ());
		return;
	}
	
	// 
	for (int i = 0; i < this->d_ptr->activeThreads; i++) {
		backend->serverThreadCreated (this->d_ptr->threads.at (i));
	}
	
}

int Nuria::HttpServer::chooseThreadCount () {
	return std::max (0, QThread::idealThreadCount ());
}

void Nuria::HttpServer::threadStopped (QObject *obj) {
	int idx = this->d_ptr->threads.indexOf (static_cast< Internal::HttpThread * > (obj));
	if (idx != -1) {
		this->d_ptr->threads.removeAt (idx);
	}
	
}
