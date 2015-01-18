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

#ifndef NURIA_WEBSOCKET_HPP
#define NURIA_WEBSOCKET_HPP

#include "network_global.hpp"
#include <QIODevice>

namespace Nuria {

class WebSocketPrivate;
class HttpClient;

/**
 * \brief Sequential QIODevice for working with WebSockets
 * 
 * WebSocket implements the server-side WebSocket protocol. Instances of it can
 * be created only through HttpClient::acceptWebSocketConnection().
 * 
 * \par Usage
 * WebSocket is a QIODevice, thus, you read and write to it using the known
 * QIODevice methods (read(), write(), readAll(), etc.), but also comes with
 * a WebSocket specific API. Please make sure to read at least the documentation
 * on WebSocket::Mode for different behaviours.
 * 
 * To obtain a WebSocket instance, you first create a slot in a HttpNode like
 * any other slot. To check if the incoming request is a WebSocket handshake
 * in general, you can use HttpClient::isWebSocketHandshake(). You can use this
 * to e.g. allow multiple calling paths, or using a API through WebSockets and
 * usual HTTP requests for browser compliance. The only mandatory step is to
 * call HttpClient::acceptWebSocketConnection(), which returns upon success the
 * WebSocket instance.
 * 
 * \par Framing and streaming data
 * WebSockets supports what it calls "framing", meaning that packets are
 * distinguishable from each other. Basically, think of it as UDP datagrams in a
 * TCP stream. WebSocket supports three mode of operation, allowing easy
 * development of more API-focused applications (the Mode::Frame mode, which is
 * the default mode) to streaming applications for e.g. Audio processing.
 * 
 * \par Protocol extensions
 * The current implementation does not support any WebSocket protocol
 * extensions.
 * 
 * \par Compliance
 * Implementation compliance is checked using the Autobahn Testsuite.
 * See http://autobahn.ws/. The NuriaProject is not affiliated with Autobahn
 * in any way.
 * 
 * Please note that UTF-8 validity checks are currently only run in the Frame
 * mode when the frame has been completely received.
 * 
 */
class NURIA_NETWORK_EXPORT WebSocket : public QIODevice {
	Q_OBJECT
public:
	
	/** Modes for reading incoming frames. */
	enum Mode {
		
		/**
		 * Frames only. readyRead() will only be emitted when a whole
		 * frame has been received. read() calls will read up to a
		 * frames boundary and not further, even if more frames have
		 * already been received. readAll() should \b not be used as
		 * it will read across frame boundaries. Use read() instead:
		 * \code
		 * QByteArray frame = webSocket->read (webSocket->bytesAvailable ());
		 * \endcode
		 * 
		 * It may be easier to use the signal approach:
		 * \code
		 * connect (webSocket, &Nuria::WebSocket::frameReceived, this, &This::handler);
		 * \endcode
		 * 
		 * Setting this mode disables the read buffer. To be able to use
		 * read() calls when in this mode, call setUseReadBuffer() with
		 * \c true as argument explicitly after creating the WebSocket
		 * instance or after setting the mode to Frame.
		 * 
		 * \note This is the default operation mode.
		 */
		Frame,
		
		/**
		 * Frame streaming puts the incoming frames into a single
		 * buffer. readyRead() will be emitted when a whole frame
		 * has been received. readAll() and read() read operate on the
		 * whole receiving buffer, spanning multiple frames if any.
		 * 
		 * This mode does not retain frame boundaries. Setting this mode
		 * enables the read buffer.
		 */
		FrameStreaming,
		
		/**
		 * Treats all incoming data as stream (E.g. like TCP does).
		 * Read behaviour is same as \c FrameStreaming, but readyRead()
		 * is emitted for all incoming frames, even if they were not
		 * completely received yet as indicated by the WebSocket
		 * protocol.
		 * 
		 * \note In this mode, automatic UTF-8 validity checks for text
		 * frames are disabled.
		 * 
		 * Setting this mode enables the read buffer.
		 */
		Streaming,
	};
	
	/** Close reasons for connectionLost(). */
	enum CloseReason {
		
		/** Usual close as was request by one end-point. */
		CloseRequest = 0,
		
		/**
		 * Connection was dropped after a illegal frame has been
		 * received.
		 */
		IllegalFrameReceived,
		
		/** Some kind of error occured while processing the payload. */
		ProcessingErrorOccured
	};
	
	/** Frame types as supported by WebSockets. */
	enum FrameType {
		
		/**
		 * The transmitted data is strictly-conformant UTF-8 data.
		 * This is the default frame type.
		 */
		TextFrame = 0,
		
		/**
		 * The transmitted data may be arbitary binary data.
		 * \note Some browser may not support this.
		 */
		BinaryFrame = 1
	};
	
	/**
	 * Status codes as defined by RFC 6455 Section 7.4.1. These are the
	 * standard error codes used by close() and connectionClosed().
	 */
	enum StatusCode {
		StatusNormal = 1000,
		StatusGoingAway = 1001,
		StatusProtocolError = 1002,
		StatusUnacceptable = 1003,
		// 1004 - 1006 are reserved
		StatusBrokenData = 1007,
		StatusPolicyViolation = 1008,
		StatusTooBig = 1009,
		StatusExtensionRequirementUnsatisfied = 1010,
		StatusInternalServerError = 1011
	};
	
	/** Destructor. */
	~WebSocket () override;
	
	/** Returns the underlying HttpClient. */
	HttpClient *httpClient () const;
	
	/** Returns the reading mode. */
	Mode mode () const;
	
	/**
	 * Sets the reading mode. Changing the mode will discard previously
	 * received but not read data. Thus you should ensure that you don't
	 * loose any data by reading all of it. This function is safe to
	 * call right after the WebSocket has been created by
	 * HttpClient::acceptWebSocketConnection().
	 */
	void setMode (Mode mode);
	
	/**
	 * Returns the frame type being used when sending data through write().
	 */
	FrameType frameType () const;
	
	/** Sets the default frame type. */
	void setFrameType (FrameType type);
	
	/**
	 * Returns \c true if this instance is using the read buffer. The
	 * default is \c false. If no read buffer is used, read() calls on this
	 * instance will fail.
	 */
	bool isUsingReadBuffer ();
	
	/** Configures if the read buffer is used. */
	void setUseReadBuffer (bool useBuffer);
	
	/**
	 * Sends a \a type frame with payload \a data. If \a isLast is not
	 * \c true, the remote will be signalled that this is a fragmented frame
	 * which ends when a sendFrame() call is done with it being \c true.
	 * 
	 * If the previous frame was sent with \a isLast being \c false, \a type
	 * is \b ignored to signal a continuation to the current frame
	 * transaction to the client.
	 */
	void sendFrame (FrameType type, const QByteArray &data, bool isLast = true);
	
	/**
	 * \overload
	 * Please note that \a length must be greater or equal to \c 0. There's
	 * \b no \c -1 shortcut in this function.
	 */
	void sendFrame (FrameType type, const char *data, int length, bool isLast = true);
	
	/**
	 * Convenience method which sends \a data as binary frame. Same as:
	 * \code
	 * sendFrame (WebSocket::BinaryFrame, data, isLast);
	 * \endcode
	 */
	void sendBinaryFrame (const QByteArray &data, bool isLast = true);
	
	/**
	 * Convenience method which sends \a data as binary frame. Same as:
	 * \code
	 * sendFrame (WebSocket::BinaryFrame, data, length, isLast);
	 * \endcode
	 */
	void sendBinaryFrame (const char *data, int length, bool isLast = true);
	
	/**
	 * Convenience method which sends \a data as binary frame. Same as:
	 * \code
	 * sendFrame (WebSocket::TextFrame, data.toUtf8 (), isLast);
	 * \endcode
	 */
	void sendTextFrame (const QString &data, bool isLast = true);
	
	/**
	 * Convenience method which sends \a data as binary frame. Same as:
	 * \code
	 * sendFrame (WebSocket::TextFrame, data, isLast);
	 * \endcode
	 */
	void sendTextFrame (const QByteArray &data, bool isLast = true);
	
	/**
	 * Sends a ping packet to the remote client with a optional
	 * \a challenge. \a challenge must be shorter than 126 Bytes in length,
	 * else the call will fail.
	 */
	bool sendPing (const QByteArray &challenge = QByteArray ());
	
	/** Forces the HTTP transport to send the data right away. */
	bool flush () const;
	
	/** Returns \c true. */
	bool isSequential () const override;
	
	/**
	 * Calls close() if \a mode is \c OpenModeFlag::NotOpen and returns
	 * \c true. For any other value passed for \a mode, \c false is
	 * returned without changing the mode.
	 */
	bool open (OpenMode mode) override;
	
	/**
	 * Closes the WebSocket connection by sending the closing handshake to
	 * the remote client and closing the TCP connection.
	 */
	void close () override;
	
	/**
	 * Closes the connection sending \a code and a optional \a message,
	 * which must be UTF-8 encoded to be RFC compliant. \a code should be
	 * one of those defined in StatusCode.
	 */
	void close (int code, const QByteArray &message = QByteArray ());
	
	/**
	 * Returns the available bytes. If the current mode is Frame then only
	 * the size of the current frame is returned, or \c 0 if there's nothing
	 * available.
	 */
	qint64 bytesAvailable () const override;
	
signals:
	
	/**
	 * Emitted when a whole frame has been received. The payload is stored
	 * in \a data and is of \a type.
	 * 
	 * This signal is only emitted when the mode is set to \c Frame.
	 */
	void frameReceived (Nuria::WebSocket::FrameType type, const QByteArray &data);
	
	/**
	 * Same as frameReceived(), but is emitted even for partial frames.
	 * If \a last is \c true, this was the last partial frame. Note that
	 * partial TextFrame frames may have incomplete UTF-8 characters at the
	 * \b end of \a data.
	 */
	void partialFrameReceived (Nuria::WebSocket::FrameType type, const QByteArray &data, bool last);
	
	/**
	 * Emitted when a ping request has been received, with \a challenge being
	 * what the client sent in the PING packet. The pong response will be
	 * sent automatically.
	 */
	void pingReceived (const QByteArray &challenge);
	
	/**
	 * Emitted when a pong response has been received. \a challenge contains
	 * the received challenge data as sent by the client, which if this PONG
	 * was a response to a PING request, should match that PING requests
	 * challenge. Also note that clients are allowed to send PONG packets
	 * at will.
	 */
	void pongReceived (const QByteArray &challenge);
	
	/** Emitted when the connection was lost because of \a reason. */
	void connectionLost (Nuria::WebSocket::CloseReason reason);
	
	/**
	 * Emitted when the remote client closed the connection properly.
	 * If one was received, \a code will be the transmitted error code
	 * and \a message will contain the UTF-8 formatted data. If no error
	 * code was transmitted, \a code will be \c -1.
	 * 
	 * After this signal, connectionLost() will be emitted.
	 * 
	 * \sa StatusCode
	 */
	void connectionClosed (int code, const QByteArray &message);
	
protected:
	qint64 readData (char *data, qint64 maxlen) override;
	qint64 writeData (const char *data, qint64 len) override;
	
private:
	friend class WebSocketPrivate;
	friend class HttpClient;
	
	// 
	explicit WebSocket (HttpClient *client);
	QIODevice *backendDevice () const;
	
	void connLostHandler ();
	
	// 
	WebSocketPrivate *d_ptr;
	
};

}

Q_DECLARE_METATYPE(Nuria::WebSocket::CloseReason);
Q_DECLARE_METATYPE(Nuria::WebSocket::FrameType);

#endif // NURIA_WEBSOCKET_HPP
