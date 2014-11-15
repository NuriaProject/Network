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

#include "nuria/fastcgibackend.hpp"

#include "private/fastcgithreadobject.hpp"
#include "private/fastcgitransport.hpp"
#include "private/localserver.hpp"
#include "private/tcpserver.hpp"
#include "nuria/httpserver.hpp"
#include <QTcpSocket>
#include <QThread>
#include <QVector>
#include <QFile>

namespace Nuria {
class FastCgiBackendPrivate {
public:
	Internal::TcpServer *tcpServer = nullptr;
	Internal::LocalServer *localServer = nullptr;
	bool secure = false;
	int port = 80;
	
	// 
	QMap< QByteArray, QByteArray > config;
	int maxConns = 0;
	int maxRequests = 0;
	
	// 
	QVector< Internal::FastCgiThreadObject * > threads;
	
};
}

Nuria::FastCgiBackend::FastCgiBackend (HttpServer *server)
        : HttpBackend (server), d_ptr (new FastCgiBackendPrivate)
{
	enum { ConnMultiplier = 10 };
	
	// 
	this->d_ptr->maxConns = std::max (0, QThread::idealThreadCount ());
	this->d_ptr->maxRequests = this->d_ptr->maxConns * ConnMultiplier;
	
}

Nuria::FastCgiBackend::~FastCgiBackend () {
	stop ();
	delete this->d_ptr;
}

bool Nuria::FastCgiBackend::listen () {
	bool isSocket = false;
	int socket = qgetenv ("FCGI_LISTENSOCK_FILENO").toInt (&isSocket);
	
	if (!isSocket || socket < 1) {
		return false;
	}
	
	// 
	addTcpConnection (socket);
	return true;
}

bool Nuria::FastCgiBackend::listen (int port, const QHostAddress &iface) {
	this->d_ptr->tcpServer = new Internal::TcpServer (false, this);
	
	connect (this->d_ptr->tcpServer, &Internal::TcpServer::connectionRequested,
	         this, &FastCgiBackend::addTcpConnection);
	
	return this->d_ptr->tcpServer->listen (iface, port);
}

bool Nuria::FastCgiBackend::listenLocal (const QString &name, QLocalServer::SocketOptions options) {
	this->d_ptr->localServer = new Internal::LocalServer (this);
	this->d_ptr->localServer->setSocketOptions (options);
	
	connect (this->d_ptr->localServer, &Internal::LocalServer::connectionRequested,
	         this, &FastCgiBackend::addLocalConnection);
	
	bool r = this->d_ptr->localServer->listen (name);
	return r;
}

bool Nuria::FastCgiBackend::listenLocal (qintptr handle) {
	this->d_ptr->localServer = new Internal::LocalServer (this);
	
	connect (this->d_ptr->localServer, &Internal::LocalServer::connectionRequested,
	         this, &FastCgiBackend::addLocalConnection);
	
	return this->d_ptr->localServer->listen (handle);
}

void Nuria::FastCgiBackend::stop () {
	delete this->d_ptr->localServer;
	delete this->d_ptr->tcpServer;
	
	this->d_ptr->localServer = nullptr;
	this->d_ptr->tcpServer = nullptr;
	
	for (int i = 0; i < this->d_ptr->threads.length (); i++) {
		this->d_ptr->threads.at (i)->deleteLater ();
	}
	
}

int Nuria::FastCgiBackend::maxConcurrentConnections () const {
	return this->d_ptr->maxConns;
}

void Nuria::FastCgiBackend::setMaxConcurrentConnections (int count) {
	this->d_ptr->maxConns = count;
}

int Nuria::FastCgiBackend::maxConcurrentRequests () const {
	return this->d_ptr->maxRequests;
}

void Nuria::FastCgiBackend::setMaxConcurrentRequests (int count) const {
	this->d_ptr->maxRequests = count;
}

void Nuria::FastCgiBackend::setCustomConfiguration (const QByteArray &name, const QByteArray &value) {
	this->d_ptr->config.insert (name, value);
}

QMap< QByteArray, QByteArray > Nuria::FastCgiBackend::customConfiguration () const {
	return this->d_ptr->config;
}

int Nuria::FastCgiBackend::currentConnectionCount () const {
	int count = 0;
	for (int i = 0; i < this->d_ptr->threads.length (); i++) {
		count += this->d_ptr->threads.at (i)->connectionCount ();
	}
	
	return count;
}

bool Nuria::FastCgiBackend::isListening () const {
	return !this->d_ptr->threads.isEmpty ();
}

int Nuria::FastCgiBackend::port () const {
	return this->d_ptr->port;
}

bool Nuria::FastCgiBackend::isSecure () const {
	return this->d_ptr->secure;
}

void Nuria::FastCgiBackend::serverThreadCreated (QThread *thread) {
	Internal::FastCgiThreadObject *object = new Internal::FastCgiThreadObject (this);
	
	if (thread != this->thread ()) {
		object->moveToThread (thread);
	}
	
	this->d_ptr->threads.append (object);
}

static Nuria::Internal::FastCgiThreadObject *leastUsedThread (Nuria::FastCgiBackendPrivate *d_ptr) {
	if (d_ptr->threads.isEmpty ()) {
		return nullptr;
	}
	
	// Find the thread object with the least FCGI connections
	Nuria::Internal::FastCgiThreadObject *leastUsed = d_ptr->threads.first ();
	int least = leastUsed->connectionCount ();
	for (int i = 1; i < d_ptr->threads.length (); i++) {
		int curCount = d_ptr->threads.at (i)->connectionCount ();
		if (curCount < least) {
			leastUsed = d_ptr->threads.at (i);
			least = curCount;
		}
		
	}
	
	// 
	return leastUsed;
}

void Nuria::FastCgiBackend::addTcpConnection (qintptr handle) {
	// Add the connection to the thread with the least connections.
	Internal::FastCgiThreadObject *obj = leastUsedThread (this->d_ptr);
	if (obj) {
		obj->addSocketLater (handle, Internal::FastCgiThreadObject::Tcp);
		emit connectionAdded ();
	}
	
}

void Nuria::FastCgiBackend::addLocalConnection (qintptr handle) {
	// Add the connection to the thread with the least connections.
	Internal::FastCgiThreadObject *obj = leastUsedThread (this->d_ptr);
	if (obj) {
		obj->addSocketLater (handle, Internal::FastCgiThreadObject::Pipe);
		emit connectionAdded ();
	}
	
}
