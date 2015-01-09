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

#include "nuria/httptransport.hpp"
#include "nuria/httpbackend.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/httpserver.hpp"
#include <nuria/callback.hpp>

namespace Nuria {
class HttpTransportPrivate {
public:
	
	int requestCount = 0;
	int maxRequests = HttpTransport::MaxRequestsDefault;
	
	int timeoutConnect = HttpTransport::DefaultConnectTimeout;
	int timeoutData = HttpTransport::DefaultDataTimeout;
	int timeoutKeepAlive = HttpTransport::DefaultKeepAliveTimeout;
	
	int minBytesReceived = HttpTransport::DefaultMinimalBytesReceived;
	
	HttpBackend *backend;
	
};
}

Nuria::HttpTransport::HttpTransport (HttpBackend *backend, HttpServer *server)
	: QObject (server), d_ptr (new HttpTransportPrivate)
{
	
	this->d_ptr->backend = backend;
	this->d_ptr->timeoutConnect = server->timeout (ConnectTimeout);
	this->d_ptr->timeoutData = server->timeout (DataTimeout);
	this->d_ptr->timeoutKeepAlive = server->timeout (KeepAliveTimeout);
	this->d_ptr->minBytesReceived = server->minimalBytesReceived ();
	
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

int Nuria::HttpTransport::minimalBytesReceived () const {
	return this->d_ptr->minBytesReceived;
}

void Nuria::HttpTransport::setMinimalBytesReceived (int bytes) {
	this->d_ptr->minBytesReceived = bytes;
}

Nuria::HttpBackend *Nuria::HttpTransport::backend () {
	return this->d_ptr->backend;
}

bool Nuria::HttpTransport::flush (HttpClient *client) {
	Q_UNUSED(client)
	return true;
}

void Nuria::HttpTransport::init () {
	// 
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

bool Nuria::HttpTransport::addToServer () {
	bool wasMoved = this->d_ptr->backend->httpServer ()->addTransport (this);
	if (wasMoved) {
		Callback cb (this, SLOT(init()));
		cb (); // inter-thread invocation.
	} else {
		init ();
	}
	
	return wasMoved;
}
