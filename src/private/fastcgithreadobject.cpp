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

#include "fastcgithreadobject.hpp"

#include "../nuria/fastcgibackend.hpp"
#include "fastcgitransport.hpp"
#include "fastcgireader.hpp"
#include "fastcgiwriter.hpp"
#include <QLocalSocket>
#include <QTcpSocket>
#include <QMetaType>

Nuria::Internal::FastCgiThreadObject::FastCgiThreadObject (FastCgiBackend *backend)
        : QObject (nullptr), m_backend (backend)
{
	
	qRegisterMetaType< ConnType > ();
	
}

Nuria::Internal::FastCgiThreadObject::~FastCgiThreadObject () {
	// 
}

void Nuria::Internal::FastCgiThreadObject::addSocketLater (qintptr socket, ConnType type) {
	
	// Increase now to avoid race-condition where all connections would be
	// added to the same thread as it hasn't yet added the new connections.
	this->m_socketCount.ref ();
	
	staticMetaObject.invokeMethod (this, "addSocket", Qt::QueuedConnection,
	                               Q_ARG(int, socket), Q_ARG(int, type), Q_ARG(bool, false));
}

int Nuria::Internal::FastCgiThreadObject::connectionCount () const {
	return this->m_socketCount.load ();
}

void Nuria::Internal::FastCgiThreadObject::addSocket (int handle, int type, bool increase) {
	if (increase) {
		this->m_socketCount.ref ();
	}
	
	addDeviceInternal (handleToDevice (handle, ConnType (type)));
}

void Nuria::Internal::FastCgiThreadObject::addDeviceInternal (QIODevice *device) {
	this->m_sockets.insert (device, SocketData { { } });
	processFcgiData (device);
}

QIODevice *Nuria::Internal::FastCgiThreadObject::handleToDevice (int handle, ConnType type) {
	switch (type) {
	case Tcp: return handleToSocket (handle);
	case Pipe: return handleToPipe (handle);
	case Device: return nullptr; // Must not happen.
	}
	
	return nullptr;
}

QIODevice *Nuria::Internal::FastCgiThreadObject::handleToSocket (int handle) {
	QTcpSocket *socket = new QTcpSocket (this);
	socket->setSocketDescriptor (handle);
	
	connect (socket, &QTcpSocket::readyRead, this, &FastCgiThreadObject::processData);
	connect (socket, &QTcpSocket::disconnected, this, &FastCgiThreadObject::connectionLost);
	connect (socket, &QTcpSocket::destroyed, this, &FastCgiThreadObject::connectionLost);
	
	return socket;
}

QIODevice *Nuria::Internal::FastCgiThreadObject::handleToPipe (int handle) {
	QLocalSocket *socket = new QLocalSocket (this);
	socket->setSocketDescriptor (handle);
	
	connect (socket, &QLocalSocket::readyRead, this, &FastCgiThreadObject::processData);
	connect (socket, &QLocalSocket::disconnected, this, &FastCgiThreadObject::connectionLost);
	connect (socket, &QLocalSocket::destroyed, this, &FastCgiThreadObject::connectionLost);
	
	return socket;
}

Nuria::Internal::FastCgiTransport *Nuria::Internal::FastCgiThreadObject::getTransport (QIODevice *socket, uint16_t id) {
	auto it = this->m_sockets.constFind (socket);
	if (it != this->m_sockets.constEnd ()) {
		return it->transports.value (id);
	}
	
	return nullptr;
}

void Nuria::Internal::FastCgiThreadObject::addTransport (QIODevice *socket, uint16_t id,
                                                         FastCgiTransport *transport) {
	auto it = this->m_sockets.find (socket);
	if (it != this->m_sockets.end ()) {
		it->transports.insert (id, transport);
	}
	
}

void Nuria::Internal::FastCgiThreadObject::transportDestroyed (FastCgiTransport *transport) {
	auto it = this->m_sockets.find (static_cast< QTcpSocket * > (transport->device ()));
	if (it != this->m_sockets.end ()) {
		it->transports.take (transport->requestId ());
	}
	
}

void Nuria::Internal::FastCgiThreadObject::connectionLost () {
	QTcpSocket *socket = qobject_cast< QTcpSocket * > (sender ());
	if (!socket) {
		return;
	}
	
	// Destroy all running transports of that socket
	SocketData data = this->m_sockets.take (socket);
	qDeleteAll (data.transports);
	
	emit this->m_backend->connectionClosed ();
}

void Nuria::Internal::FastCgiThreadObject::processData () {
	QTcpSocket *socket = qobject_cast< QTcpSocket * > (sender ());
	if (!socket) {
		return;
	}
	
	// 
	processFcgiData (socket);
}

void Nuria::Internal::FastCgiThreadObject::processFcgiData (QIODevice *socket) {
	FastCgiRecord record;
	
	// Process as many records as possible
	while (FastCgiReader::readRecord (socket, record) &&
	       processRecord (socket, record));
	
}

bool Nuria::Internal::FastCgiThreadObject::processRecord (QIODevice *socket, FastCgiRecord record) {
	int expected = record.contentLength + record.paddingLength + sizeof(record);
	QByteArray body = socket->peek (expected);
	
	if (body.length () < expected) {
		return false;
	}
	
	// Remove leading record and trailing padding
	body = body.mid (sizeof(record));
	body.chop (record.paddingLength);
	
	// Remove the whole record from the incoming buffer.
	// TODO: Is there a more efficient way to do this?
	socket->read (expected);
	return processCompleteRecord (socket, record, body);
}

bool Nuria::Internal::FastCgiThreadObject::processCompleteRecord (QIODevice *socket, FastCgiRecord record,
                                                                  const QByteArray &body) {
	switch (record.type) {
	case FastCgiType::GetValues: return processGetValues (socket, body);
	case FastCgiType::BeginRequest: return processBeginRequest (socket, record, body);
	case FastCgiType::AbortRequest: return processAbortRequest (socket, record);
	case FastCgiType::Params: return processParams (socket, record, body);
	case FastCgiType::StdIn: return processStdIn (socket, record, body);
	default: // Respond with error
		return processUnknownType (socket, record.type);
	}
	
	return true;
}

NameValueMap Nuria::Internal::FastCgiThreadObject::getValuesMap () {
	NameValueMap map = this->m_backend->customConfiguration ();
	int maxConns = this->m_backend->maxConcurrentConnections ();
	int maxReqs = this->m_backend->maxConcurrentRequests ();
	
	// Insert required fields
	map.insert (QByteArrayLiteral("FCGI_MPXS_CONNS"), QByteArrayLiteral("1"));
	map.insert (QByteArrayLiteral("FCGI_MAX_CONNS"), QByteArray::number (maxConns));
	map.insert (QByteArrayLiteral("FCGI_MAX_REQS"), QByteArray::number (maxReqs));
	
	return map;
}

bool Nuria::Internal::FastCgiThreadObject::processUnknownType (QIODevice *socket, FastCgiType type) {
	FastCgiWriter::writeUnknownTypeResult (socket, type);
	return true;
}

bool Nuria::Internal::FastCgiThreadObject::processGetValues (QIODevice *socket, const QByteArray &body) {
	NameValueMap values;
	if (!FastCgiReader::readAllNameValuePairs (body, values)) {
		return false;
	}
	
	// Prepare result map
	NameValueMap result = getValuesMap ();
	NameValueMap response;
	for (auto it = values.constBegin (), end = values.constEnd (); it != end; ++it) {
		if (result.contains (it.key ())) {
			response.insert (it.key (), it.value ());
		}
		
	}
	
	// Send response
	FastCgiWriter::writeGetValuesResult (socket, response);
	return true;
}

bool Nuria::Internal::FastCgiThreadObject::processBeginRequest (QIODevice *socket, FastCgiRecord record,
                                                                const QByteArray &body) {
	enum { KeepConnectionFlag = 0x01 };
	FastCgiBeginRequestBody data;
	if (!FastCgiReader::readBeginRequestBody (body, data)) {
		return false;
	}
	
	// Only support 'Responder'. Make sure that there's no running request with the same id.
	if (data.role != FastCgiRole::Responder || getTransport (socket, record.requestId)) {
		return false;
	}
	
	// Create the transport instance
	FastCgiTransport *transport = new FastCgiTransport (record.requestId, socket, this, this->m_backend,
	                                                    this->m_backend->httpServer ());
	if ((data.flags & KeepConnectionFlag) == 0) {
		// Destroy the FCGI connection when the transport is finished.
		socket->setParent (transport);
	}
	
	// 
	addTransport (socket, record.requestId, transport);
	return true;
	
}

bool Nuria::Internal::FastCgiThreadObject::processAbortRequest (QIODevice *socket, FastCgiRecord record) {
	FastCgiTransport *transport = getTransport (socket, record.requestId);
	
	// Unknown request id?
	if (!transport) {
		return true;
	}
	
	// 
	transport->abort ();
	return true;
}

bool Nuria::Internal::FastCgiThreadObject::processParams (QIODevice *socket, FastCgiRecord record,
                                                          const QByteArray &body) {
	FastCgiTransport *transport = getTransport (socket, record.requestId);
	
	NameValueMap values;
	if (!transport || !FastCgiReader::readAllNameValuePairs (body, values)) {
		return false;
	}
	
	// 
	transport->addFastCgiParams (values);
	return true;
}

bool Nuria::Internal::FastCgiThreadObject::processStdIn (QIODevice *socket, FastCgiRecord record,
                                                         const QByteArray &body) {
	FastCgiTransport *transport = getTransport (socket, record.requestId);
	if (!transport || body.isEmpty ()) {
		return !transport;
	}
	
	// 
	transport->forwardBodyData (body);
	return true;
}
