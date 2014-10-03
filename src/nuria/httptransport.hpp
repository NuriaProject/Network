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

#include "network_global.hpp"
#include <QHostAddress>
#include <QIODevice>

namespace Nuria {

class HttpTransportPrivate;
class HttpClient;

/**
 * \brief Abstract data transport for HttpClient
 * 
 * A transport is responsible to receive data from and send data back to a HTTP
 * client through a specific connection. The connection can be anything which
 * can be represented as HttpTransport, e.g. TCP, SSL or a memory buffer.
 * 
 * \par Implementing a transport
 * 
 * To implement a transport, you first sub-class HttpTransport. When you
 * receive a new request, you instantiate HttpClient and use readFromRemote()
 * to push HTTP data into the client instance. The client will eventually
 * write a response by calling sendToRemote() and later call close() when the
 * response has been sent.
 * 
 * The transport is responsible for keeping track of its HttpClient instances.
 */
class NURIA_NETWORK_EXPORT HttpTransport : public QObject {
	Q_OBJECT
	Q_ENUMS(Type Timeout)
public:
	
	// Default configuration
	enum {
		
		/** Maximum requests per transport in a keep-alive session. */
		MaxRequestsDefault = 5,
		
		/** Default value for ConnectTimeout. In msec. */
		DefaultConnectTimeout = 2000,
		
		/** Default value for DataTimeout. In msec. */
		DefaultDataTimeout = 5000,
		
		/** Default value for KeepAliveTimeout. In msec. */
		DefaultKeepAliveTimeout = 30000,
		
	};
	
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
	
	/**
	 * Enumeration of different timeout timers. If one of these trigger,
	 * the connection is closed by the implementation.
	 */
	enum Timeout {
		
		/** The client has connected, but hasn't sent anything yet. */
		ConnectTimeout = 0,
		
		/**
		 * Timeout for when the client has sent something, but not a
		 * complete request, and just does nothing afterwards.
		 */
		DataTimeout = 1,
		
		/**
		 * Timeout for keep-alive connections, meaning, the client has
		 * made a request and we're now keeping the connection open
		 * waiting for further requests.
		 */
		KeepAliveTimeout = 2
	};
	
	/**
	 * Constructor. \a parent is expected to be the HTTP server instance.
	 */
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
	
	/** Returns \c true if the transport is open. */
	virtual bool isOpen () const = 0;
	
	/** Returns the count of requests this transport has started. */
	int currentRequestCount () const;
	
	/**
	 * Returns the maximum count of requests per transport.
	 * A value of \c -1 indicates that there's no limit.
	 */
	int maxRequests () const;
	
	/** Sets the maximum count of requests per transport. */
	void setMaxRequests (int count);
	
	/**
	 * Returns the timeout time for \a which in msec.
	 * A value of \c -1 disables the timeout.
	 */
	int timeout (Timeout which);
	
	/** Sets the timeout \a which to \a msec. */
	void setTimeout (Timeout which, int msec);
	
public slots:
	
	/**
	 * Instructs the underlying transport to send any pending data to the
	 * client now. The default implementation always returns \c true.
	 */
	virtual bool flush (HttpClient *client);
	
	/**
	 * Closes the underlying transport immediately, discarding any data
	 * in the send buffer (If one exists). This is used by clients if they
	 * encounter a fatal error which should result in disconnecting from
	 * the peer entirely.
	 */
	virtual void forceClose () = 0;
	
signals:
	
	/** Emitted when the connection to the client has been lost. */
	void connectionLost ();
	
	/** The value of \a timeout has been changed to \a msec. */
	void timeoutChanged (Timeout timeout, int msec);
	
protected:
	
	friend class HttpClient;
	
	/** Used by implementations to update the request count. */
	void setCurrentRequestCount (int count);
	
	/**
	 * Used by \a client to tell the implementation that it's done.
	 */
	virtual void close (HttpClient *client) = 0;
	
	/**
	 * Used by \a client to send \a data to the remote party.
	 */
	virtual bool sendToRemote (HttpClient *client, const QByteArray &data) = 0;
	
	/**
	 * Used by the \b implementation to write \a data into \a client to be
	 * processed. The \a client will remove the parts it read from \a data.
	 * If it didn't read everything, it means that the following data is
	 * probably meant to create another HTTP request by the remote peer.
	 */
	void readFromRemote (HttpClient *client, QByteArray &data);
	
	/**
	 * Called by \b implementations after data sent by \a client after a
	 * call to sendToRemote() has been sent to the client.
	 */
	void bytesSent (HttpClient *client, qint64 bytes);
	
private:
	HttpTransportPrivate *d_ptr;
};

}

Q_DECLARE_METATYPE(Nuria::HttpTransport::Timeout)

#endif // NURIA_HTTPTRANSPORT_HPP
