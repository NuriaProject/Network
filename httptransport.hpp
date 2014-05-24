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

#ifndef NURIA_HTTPTRANSPORT_HPP
#define NURIA_HTTPTRANSPORT_HPP

#include <QHostAddress>
#include <QIODevice>

namespace Nuria {

/**
 * \brief Abstract data transport for HttpClient
 * 
 * A transport is responsible to receive and send to and from a HTTP client
 * through a specific connection. The connection can be anything which can
 * be represented as HttpTransport, e.g. TCP, SSL or a QBuffer (memory buffer).
 */
class HttpTransport : public QIODevice {
	Q_OBJECT
	Q_ENUMS(Type)
public:
	
	/** Transport types. */
	enum Type {
		
		/** Unkown transport, possibly a dummy. */
		Unknown = 0,
		
		/** Connection using TCP. */
		TCP = 1,
		
		/** Connection using SSL over TCP. */
		SSL = 2,
		
		/** Using a in-memory buffer. */
		Memory = 3,
		
		/** Some other custom transport. */
		Custom = 4
	};
	
	/** Constructor. */
	explicit HttpTransport (QObject *parent = 0);
	
	/** Destructor. */
	~HttpTransport ();
	
	/**
	 * Returns the type of the transport. The default implementation returns
	 * \c Custom.
	 */
	virtual Type type () const;
	
	/**
	 * Returns \c true if the connection is somehow protected against
	 * external eavesdropping, e.g. through encryption. The default
	 * implementation returns \c false.
	 */
	virtual bool isSecure () const;
	
	/**
	 * Returns the local address to which the peer is connected.
	 * The default implementation returns QHostAddress::Null.
	 */
	virtual QHostAddress localAddress () const;
	
	/**
	 * Returns the local port to which the peer is connected.
	 * The default implementation returns 0.
	 */
	virtual quint16 localPort () const;
	
	/**
	 * Returns the address of the connected peer.
	* The default implementation returns QHostAddress::Null.
	 */
	virtual QHostAddress peerAddress () const;
	
	/**
	 * Returns the port of the connected peer.
	 * The default implementation returns 0.
	 */
	virtual quint16 peerPort () const;
	
public slots:
	
	/**
	 * Instructs the underlying transport to send any pending data to the
	 * client now. The default implementation always returns \c true.
	 */
	virtual bool flush ();
	
	/**
	 * Closes the underlying transport immediately, discarding any data
	 * in the send buffer (If one exists).
	 */
	virtual void forceClose ();
	
};

}

#endif // NURIA_HTTPTRANSPORT_HPP
