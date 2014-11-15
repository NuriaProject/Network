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
