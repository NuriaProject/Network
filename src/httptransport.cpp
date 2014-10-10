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

#include "nuria/httptransport.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/httpserver.hpp"

namespace Nuria {
class HttpTransportPrivate {
public:
	
	int requestCount = 0;
	int maxRequests = HttpTransport::MaxRequestsDefault;
	
	int timeoutConnect = HttpTransport::DefaultConnectTimeout;
	int timeoutData = HttpTransport::DefaultDataTimeout;
	int timeoutKeepAlive = HttpTransport::DefaultKeepAliveTimeout;
	
	HttpBackend *backend;
	
};
}

Nuria::HttpTransport::HttpTransport (HttpBackend *backend, HttpServer *server)
	: QObject (server), d_ptr (new HttpTransportPrivate)
{
	
	this->d_ptr->backend = backend;
	server->addTransport (this);
	
}

Nuria::HttpTransport::~HttpTransport () {
	delete this->d_ptr;
}

Nuria::HttpTransport::Type Nuria::HttpTransport::type () const {
	return Custom;
}

bool Nuria::HttpTransport::isSecure () const {
	return false;
}

QHostAddress Nuria::HttpTransport::localAddress () const {
	return QHostAddress::Null;
}

quint16 Nuria::HttpTransport::localPort () const {
	return 0;
}

QHostAddress Nuria::HttpTransport::peerAddress () const {
	return QHostAddress::Null;
}

quint16 Nuria::HttpTransport::peerPort () const {
	return 0;
}

int Nuria::HttpTransport::currentRequestCount () const {
	return this->d_ptr->requestCount;
}

int Nuria::HttpTransport::maxRequests () const {
	return this->d_ptr->maxRequests;
}

void Nuria::HttpTransport::setMaxRequests (int count) {
	this->d_ptr->maxRequests = count;
}

int Nuria::HttpTransport::timeout (HttpTransport::Timeout which) {
	switch (which) {
	case ConnectTimeout: return this->d_ptr->timeoutConnect;
	case DataTimeout: return this->d_ptr->timeoutData;
	case KeepAliveTimeout: return this->d_ptr->timeoutKeepAlive;
	}
	
	return 0;
}

void Nuria::HttpTransport::setTimeout (HttpTransport::Timeout which, int msec) {
	switch (which) {
	case ConnectTimeout:
		this->d_ptr->timeoutConnect = msec;
		break;
	case DataTimeout:
		this->d_ptr->timeoutData = msec;
		break;
	case KeepAliveTimeout:
		this->d_ptr->timeoutKeepAlive = msec;
		break;
	}
	
	emit timeoutChanged (which, msec);
}

Nuria::HttpBackend *Nuria::HttpTransport::backend () {
	return this->d_ptr->backend;
}

bool Nuria::HttpTransport::flush (HttpClient *client) {
	Q_UNUSED(client)
	return true;
}

void Nuria::HttpTransport::setCurrentRequestCount (int count) {
	this->d_ptr->requestCount = count;
}

void Nuria::HttpTransport::readFromRemote (HttpClient *client, QByteArray &data) {
	client->processData (data);
}

void Nuria::HttpTransport::bytesSent (HttpClient *client, qint64 bytes) {
	client->bytesSent (bytes);
}
