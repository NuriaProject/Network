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

#ifndef NURIA_INTERNAL_FASTCGITHREADOBJECT_HPP
#define NURIA_INTERNAL_FASTCGITHREADOBJECT_HPP

#include "fastcgistructures.hpp"
#include <QAtomicInt>
#include <QObject>

class QIODevice;

namespace Nuria {
class FastCgiBackend;

namespace Internal {
class FastCgiTransport;

struct SocketData {
	QMap< uint16_t, FastCgiTransport * > transports;
};

/**
 * \brief Server-thread object managing FastCGI connections
 * 
 * This class implements the FastCGI protocol. There's an instance of this
 * class in each HttpServer-thread.
 * 
 */
class FastCgiThreadObject : public QObject {
	Q_OBJECT
public:
	enum ConnType { Tcp, Pipe, Device };
	
	explicit FastCgiThreadObject (FastCgiBackend *backend);
	~FastCgiThreadObject () override;
	
	void transportDestroyed (FastCgiTransport *transport);
	
	// Thread-safe functions
	void addSocketLater (qintptr socket, ConnType type);
	int connectionCount () const;
	
public slots:
	
	// 'type' must not be 'Device'
	void addSocket (int handle, int type, bool increase = true);
	void addDeviceInternal (QIODevice *device);
	
private:
	QIODevice *handleToDevice (int handle, ConnType type);
	QIODevice *handleToSocket (int handle);
	QIODevice *handleToPipe (int handle);
	
	FastCgiTransport *getTransport (QIODevice *socket, uint16_t id);
	void addTransport (QIODevice *socket, uint16_t id, FastCgiTransport *transport);
	
	void connectionLost ();
	void incomingConnection ();
	void processData ();
	void processFcgiData (QIODevice *socket);
	bool processRecord (QIODevice *socket, FastCgiRecord record);
	bool processCompleteRecord (QIODevice *socket, FastCgiRecord record, const QByteArray &body);
	
	// 
	NameValueMap getValuesMap();
	bool processUnknownType (QIODevice *socket, FastCgiType type);
	bool processGetValues (QIODevice *socket, const QByteArray &body);
	bool processBeginRequest (QIODevice *socket, FastCgiRecord record, const QByteArray &body);
	bool processAbortRequest (QIODevice *socket, FastCgiRecord record);
	bool processParams (QIODevice *socket, FastCgiRecord record, const QByteArray &body);
	bool processStdIn (QIODevice *socket, FastCgiRecord record, const QByteArray &body);
	
	FastCgiBackend *m_backend;
	QMap< QIODevice *, SocketData > m_sockets;
	QAtomicInt m_socketCount;
	
};

}
}

Q_DECLARE_METATYPE(Nuria::Internal::FastCgiThreadObject::ConnType)

#endif // NURIA_INTERNAL_FASTCGITHREADOBJECT_HPP
