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

#ifndef NURIA_HTTPTRANSPORT_HPP
#define NURIA_HTTPTRANSPORT_HPP

#include "network_global.hpp"
#include <QHostAddress>
#include <QIODevice>

namespace Nuria {

class HttpTransportPrivate;
class HttpBackend;
class HttpClient;
class HttpServer;

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
 * 
 * \note The HttpServer may choose to move a HttpTransport to another thread
 * for parallel execution. This means that the HttpTransport must be able to
 * be run in another thread than its HttpBackend.
 * 
 */
class NURIA_NETWORK_EXPORT HttpTransport : public QObject {
	Q_OBJECT
	Q_ENUMS(Type Timeout)
public:
	
	// Default configuration
	enum {
		
		/** Maximum requests per transport in a keep-alive session. */
		MaxRequestsDefault = 10,
		
		/** Default value for ConnectTimeout. In msec. */
		DefaultConnectTimeout = 2000,
		
		/** Default value for DataTimeout. In msec. */
		DefaultDataTimeout = 5000,
		
		/** Default value for KeepAliveTimeout. In msec. */
		DefaultKeepAliveTimeout = 30000,
		
		/** Default value for minimalBytesReceived() in bytes. */
		DefaultMinimalBytesReceived = 512,
		
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
	 * Constructor. \a server will be automatically the owner.
	 */
	explicit HttpTransport (HttpBackend *backend, HttpServer *server);
	
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
	
	/**
	 * Returns the amount of bytes to be minimal received from the client in
	 * the last \c timeout(DataTimeout) milliseconds. Depending on the
	 * implementation, the maximum timeout could be twice the set timeout
	 * time.
	 * 
	 * The default is \c 512.
	 */
	int minimalBytesReceived () const;
	
	/** Sets the minimal bytes received amount. */
	void setMinimalBytesReceived (int bytes);
	
	/** Returns the HttpBackend associated with this transport. */
	HttpBackend *backend ();
	
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
	
	/**
	 * Called by the HttpServer when the HttpTransport was moved by
	 * addToServer(). If the server didn't move the transport,
	 * addToServer() will call this function.
	 * The default implementation does nothing.
	 */
	virtual void init ();
	
signals:
	
	/** Emitted when the connection to the client has been lost. */
	void connectionLost ();
	
	/** The value of \a timeout has been changed to \a msec. */
	void timeoutChanged (Timeout timeout, int msec);
	
	/**
	 * Emitted when connection timed out in \a mode. After this signal was
	 * emitted, the connection is closed and thus destroyed.
	 */
	void connectionTimedout (Nuria::HttpTransport::Timeout mode);
	
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
	
	/**
	 * Called by \b implementations after their initialization routine to
	 * tell the HttpServer that the transport can now be moved to a
	 * processing thread. Returns \c true if the instance has been moved.
	 * 
	 * \sa init
	 */
	bool addToServer ();
	
private:
	HttpTransportPrivate *d_ptr;
};

}

Q_DECLARE_METATYPE(Nuria::HttpTransport::Timeout);
Q_DECLARE_METATYPE(Nuria::HttpTransport*);

#endif // NURIA_HTTPTRANSPORT_HPP
