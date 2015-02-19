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

#include "abstracttransport.hpp"

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
class NURIA_NETWORK_EXPORT HttpTransport : public AbstractTransport {
	Q_OBJECT
	Q_ENUMS(Type)
public:
	
	// Default configuration
	enum {
		
		/** Maximum requests per transport in a keep-alive session. */
		MaxRequestsDefault = 10,
		
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
	 * Returns the maximum count of requests per transport.
	 * A value of \c -1 indicates that there's no limit.
	 */
	int maxRequests () const;
	
	/** Sets the maximum count of requests per transport. */
	void setMaxRequests (int count);
	
	/** Returns the HttpBackend associated with this transport. */
	HttpBackend *backend ();
	
public slots:
	
	/**
	 * Instructs the underlying transport to send any pending data to the
	 * client now. The default implementation always returns \c true.
	 */
	virtual bool flush (HttpClient *client);
	
protected:
	
	friend class HttpClient;
	
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
	Q_DECLARE_PRIVATE(HttpTransport)
	
};

}

Q_DECLARE_METATYPE(Nuria::HttpTransport*);

#endif // NURIA_HTTPTRANSPORT_HPP
