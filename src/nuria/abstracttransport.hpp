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

#ifndef NURIA_ABSTRACTTRANSPORT_HPP
#define NURIA_ABSTRACTTRANSPORT_HPP

#include "network_global.hpp"
#include <QHostAddress>
#include <QIODevice>

namespace Nuria {

class AbstractTransportPrivate;

/**
 * \brief Abstract transport class for servers
 * 
 * A transport is responsible for receiving data from and sending data back to
 * a client through a specific connection. The connection can pretty much be
 * anything.
 * 
 * This class is meant to be a helper class offering basic transport management
 * functionality.
 * 
 * \par Transport information
 * 
 * AbstractTransport offers an interface to expose used local and remote address
 * and port of a networked connection. It can also keep track of processed
 * requests in general, and help count traffic.
 * 
 * \par Timeouts
 * 
 * This class can aid with detecting and handling timeouts. Timeouts are
 * categorized (See the Timeout enumeration). Each category can have a
 * different timeout time, configurable through setTimeout(). Implementations
 * can use startTimeout() to control which catergory is currently active.
 * To modify the default timeout behaviour, you can override handleTimeout().
 * 
 * The general flow when using timeouts is:
 * 
 * - After connecting, set the timeout to ConnectTimeout
 * - After the client began sending data, set the timeout to DataTimeout
 * - When the request is completed, disable the timeout
 * - Upon serving the response and waiting for another request, set the timeout to KeepAliveTimeout
 * 
 */
class NURIA_NETWORK_EXPORT AbstractTransport : public QObject {
	Q_OBJECT
	Q_ENUMS(Timeout)
public:
	
	// Default configuration
	enum {
		
		/** Default value for ConnectTimeout. In msec. */
		DefaultConnectTimeout = 2000,
		
		/** Default value for DataTimeout. In msec. */
		DefaultDataTimeout = 5000,
		
		/** Default value for KeepAliveTimeout. In msec. */
		DefaultKeepAliveTimeout = 30000,
		
		/** Default value for minimalBytesReceived() in bytes. */
		DefaultMinimumBytesReceived = 512,
		
	};
	
	/**
	 * Enumeration of different timeout timers. If one of these trigger,
	 * the connection is closed by the implementation.
	 */
	enum Timeout {
		/**
		 * No timer. This is the initial timeout state.
		 */
		Disabled = 0,
		
		/**
		 * The client has connected, but hasn't sent anything yet.
		 */
		ConnectTimeout = 1,
		
		/**
		 * Timeout for when the client has sent something, but didn't
		 * complete the request, and just does nothing afterwards.
		 * 
		 * This mode is affected by minimalBytesReceived().
		 * Implementations need to use addBytesReceived() for this to
		 * work.
		 */
		DataTimeout = 2,
		
		/**
		 * Timeout for keep-alive connections, meaning, the client has
		 * made a request and we're now keeping the connection open
		 * waiting for further requests.
		 */
		KeepAliveTimeout = 3
	};
	
	/** Constructor. */
	explicit AbstractTransport (QObject *parent = nullptr);
	
	/** Destructor. */
	~AbstractTransport ();
	
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
	
	/** Returns the currently active timeout mode. */
	Timeout currentTimeoutMode () const;
	
	/**
	 * Returns the timeout time for \a which in msec.
	 * A value of \c -1 disables the timeout.
	 */
	int timeout (Timeout which);
	
	/**
	 * Sets the timeout \a which to \a msec.
	* Passing \c -1 for \a msec disables the timeout.
	 */
	void setTimeout (Timeout which, int msec);
	
	/**
	 * Returns the amount of bytes to be minimal received from the client in
	 * the last \c timeout(DataTimeout) milliseconds. Depending on the
	 * implementation, the maximum timeout could be twice the set timeout
	 * time.
	 * 
	 * The default is \c 512. A value of \c -1 disables this feature,
	 * meaning that the implementation will trigger a timeout regardless
	 * of how much data has been received.
	 */
	int minimumBytesReceived () const;
	
	/** Sets the minimal bytes received amount. */
	void setMinimumBytesReceived (int bytes);
	
	/** Returns the amount of bytes received from the client in bytes. */
	quint64 bytesReceived () const;
	
	/** Returns the amount of bytes sent to the client in bytes. */
	quint64 bytesSent () const;
	
public slots:
	
	/**
	 * Closes the underlying transport immediately, discarding any data
	 * in the send buffer (If one exists). This is useful when an
	 * implementation encountes a fatal error which should result in
	 * disconnecting from the peer entirely.
	 */
	virtual void forceClose () = 0;
	
	/**
	 * Called by code \b using this class after the instance has been moved
	 * to the target thread. This method is called exactly \b once in the
	 * transports lifetime.
	 * 
	 * The default implementation does nothing.
	 */
	virtual void init ();
	
signals:
	
	/** Emitted when the connection to the client has been lost. */
	void connectionLost ();
	
	/** The value of \a timeout has been changed to \a msec. */
	void timeoutChanged (Timeout timeout, int msec);
	
	/**
	 * Emitted when connection timed out in \a mode.
	 */
	void connectionTimedout (Nuria::AbstractTransport::Timeout mode);
	
protected:
	
	/** For internal sub-classes. Takes ownership of \a d. */
	explicit AbstractTransport (AbstractTransportPrivate *d, QObject *parent = nullptr);
	
	/**
	 * Called by AbstractTransport when a timeout has occured.
	 * 
	 * The default implementation loggs the occurence and calls forceClose()
	 * on itself afterwards.
	 */
	virtual void handleTimeout (Timeout timeout);
	
	/** Used by implementations to update the request count. */
	void setCurrentRequestCount (int count);
	
	/** Convenience function to increment the request count by 1. */
	void incrementRequestCount ();
	
	/**
	 * Adds \a bytes to the amount of traffic received by this transport.
	 */
	void addBytesReceived (uint64_t bytes);
	
	/**
	 * Adds \a bytes to the amount of traffic sent by this transport.
	 */
	void addBytesSent (uint64_t bytes);
	
	/**
	 * Starts the timer with timeout for \a mode.
	 */
	void startTimeout (Timeout mode);
	
	/**
	 * Stops the timeout timer. Equivalent to calling startTimeout() with
	 * Disabled.
	 */
	void disableTimeout ();
	
	// For NuriaFramework sub-classes
	AbstractTransportPrivate *d_ptr;
private:
	void triggerTimeout ();
	
};

}

Q_DECLARE_METATYPE(Nuria::AbstractTransport::Timeout);
Q_DECLARE_METATYPE(Nuria::AbstractTransport*);

#endif // NURIA_ABSTRACTTRANSPORT_HPP
