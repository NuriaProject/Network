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

#include "private/transportprivate.hpp"
#include "nuria/httptransport.hpp"
#include "nuria/httpbackend.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/httpserver.hpp"
#include <nuria/callback.hpp>

namespace Nuria {
class HttpTransportPrivate : public AbstractTransportPrivate {
public:
	
	HttpBackend *backend;
	int maxRequests;
	
};
}

Nuria::HttpTransport::HttpTransport (HttpBackend *backend, HttpServer *server)
	: AbstractTransport (new HttpTransportPrivate, server)
{
	Q_D(HttpTransport);
	
	d->backend = backend;
	
	d->maxRequests = MaxRequestsDefault;
	d->timeoutConnect = server->timeout (ConnectTimeout);
	d->timeoutData = server->timeout (DataTimeout);
	d->timeoutKeepAlive = server->timeout (KeepAliveTimeout);
	d->minBytesReceived = server->minimalBytesReceived ();
	
}

Nuria::HttpTransport::~HttpTransport () {
	
}

Nuria::HttpTransport::Type Nuria::HttpTransport::type () const {
	return Custom;
}

int Nuria::HttpTransport::maxRequests () const {
	return d_func ()->maxRequests;
}

void Nuria::HttpTransport::setMaxRequests (int count) {
	d_func ()->maxRequests = count;
}

Nuria::HttpBackend *Nuria::HttpTransport::backend () {
	return d_func ()->backend;
}

bool Nuria::HttpTransport::flush (HttpClient *client) {
	Q_UNUSED(client)
	return true;
}

void Nuria::HttpTransport::readFromRemote (HttpClient *client, QByteArray &data) {
	this->d_ptr->trafficReceived += data.length ();
	client->processData (data);
}

void Nuria::HttpTransport::bytesSent (HttpClient *client, qint64 bytes) {
	this->d_ptr->trafficSent += bytes;
	client->bytesSent (bytes);
}

bool Nuria::HttpTransport::addToServer () {
	bool wasMoved = d_func ()->backend->httpServer ()->addTransport (this);
	
	if (wasMoved) {
		Callback cb (this, SLOT(init()));
		cb (); // inter-thread invocation.
	} else {
		init ();
	}
	
	return wasMoved;
}
