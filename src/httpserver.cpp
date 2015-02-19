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

#include "nuria/httpserver.hpp"

#include <QStringList>
#include <QMutex>

#include "nuria/httptransport.hpp"
#include "nuria/httpbackend.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/httpnode.hpp"
#include <nuria/logger.hpp>

#include "private/httptcpbackend.hpp"
#include "private/httpprivate.hpp"
#include "private/httpthread.hpp"
#include "private/tcpserver.hpp"

namespace Nuria {
class HttpServerPrivate {
public:
	
	HttpNode *error = nullptr;
	HttpNode *root;
	QString fqdn;
	
	QVector< HttpBackend * > backends;
	QVector< Internal::HttpThread * > threads;
	int threadIndex = 0;
	int activeThreads = 0;
	
	// 
	int timeoutConnect = HttpTransport::DefaultConnectTimeout;
	int timeoutData = HttpTransport::DefaultDataTimeout;
	int timeoutKeepAlive = HttpTransport::DefaultKeepAliveTimeout;
	int minBytesReceived = HttpTransport::DefaultMinimumBytesReceived;
	
};
}

Nuria::HttpServer::HttpServer (QObject *parent)
	: QObject (parent), d_ptr (new HttpServerPrivate)
{
	
	// Register meta types
	qRegisterMetaType< HttpTransport::Timeout > ();
	qRegisterMetaType< HttpTransport * > ();
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

Nuria::HttpNode *Nuria::HttpServer::errorNode () const {
	return this->d_ptr->error;
}

void Nuria::HttpServer::setErrorNode (HttpNode *node) {
	if (node == this->d_ptr->error) {
		return;
	}
	
	delete this->d_ptr->error;
	this->d_ptr->error = node;
	
	if (node) {
		node->setParent (this);
		node->d_ptr->parent = nullptr;
	}
	
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

int Nuria::HttpServer::timeout (HttpTransport::Timeout which) {
	switch (which) {
	case HttpTransport::ConnectTimeout: return this->d_ptr->timeoutConnect;
	case HttpTransport::DataTimeout: return this->d_ptr->timeoutData;
	case HttpTransport::KeepAliveTimeout: return this->d_ptr->timeoutKeepAlive;
	}
	
	return 0;
}

void Nuria::HttpServer::setTimeout (HttpTransport::Timeout which, int msec) {
	switch (which) {
	case HttpTransport::ConnectTimeout:
		this->d_ptr->timeoutConnect = msec;
		break;
	case HttpTransport::DataTimeout:
		this->d_ptr->timeoutData = msec;
		break;
	case HttpTransport::KeepAliveTimeout:
		this->d_ptr->timeoutKeepAlive = msec;
		break;
	}
	
}

int Nuria::HttpServer::minimalBytesReceived () const {
	return this->d_ptr->minBytesReceived;
}

void Nuria::HttpServer::setMinimalBytesReceived (int bytes) {
	this->d_ptr->minBytesReceived = bytes;
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

void Nuria::HttpServer::invokeError (HttpClient *client, int statusCode, const QByteArray &cause) {
	
	// Serve error through the error node.
	bool success = false;
	if (this->d_ptr->error) {
		QString path = QString::number (statusCode);
		success = this->d_ptr->error->invokePath (path, { path }, 0, client);
	}
	
	// If the error node didn't serve an error for this, send a generic message
	if (!success && client->isOpen () && !client->responseHeaderSent ()) {
		client->write (cause);
	}
	
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
		connect (transport, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)),
		         this, SLOT(forwardTimeout(Nuria::AbstractTransport::Timeout)));
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
	thread->incrementRunning (transport);
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

void Nuria::HttpServer::forwardTimeout (Nuria::AbstractTransport::Timeout mode) {
	HttpTransport *transport = qobject_cast< HttpTransport * > (sender ());
	if (transport) {
		emit connectionTimedout (transport, mode);
	}
	
}
