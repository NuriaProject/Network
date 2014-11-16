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

#ifndef NURIA_HTTPCLIENT_HPP
#define NURIA_HTTPCLIENT_HPP

#include "httppostbodyreader.hpp"
#include "network_global.hpp"
#include <QNetworkCookie>
#include <QHostAddress>
#include <QIODevice>
#include <QMetaType>
#include <QMap>
#include <QUrl>

namespace Nuria {

class HttpClientPrivate;
class HttpTransport;
class HttpFilter;
class HttpServer;
class HttpNode;
class SlotInfo;

/**
 * \brief The HttpClient class represents a connection to a client.
 * 
 * This class is used whenever interaction with the HTTP client is necessary.
 * It provides a simple way to transfer data from and to the client.
 * 
 * \note As of now, HttpClient doesn't support <tt>Connection: keep-alive</tt>.
 * This means that a client has to open a connection for each request.
 * This problem will be addressed in the future.
 * 
 * \par Sending and receiving data
 * HttpClient is a subclass of QIODevice.
 * Slots that expect a POST body can read it by using readAll or similar
 * QIODevice methods. To send data to the client simply use write or similar.
 * There is really not much to say about this - You can do anything with it
 * that you can do with any QIODevice subclass.
 * 
 * \note HttpClient is a random-access device. Methods such as read(), pos() and
 * seek() always refer to the POST body. Writing always refers to sending data
 * back to the client.
 * 
 * \par Piping
 * Additionally to the generic read and write methods, you can also pipe other
 * QIODevice*'s to the client. This means that the implementation will take care
 * of data transfer and will close the connection automatically on completion.
 * This is useful when you want to send a large chunk of data. Another use-case
 * is to combine this with QProcess when you want or need to interact with other
 * applications.
 * 
 * This is possible for both directions: You can either pipe a buffer to the
 * client, or pipe the clients post body into a QIODevice.
 * 
 * An example would be to use this to process a file while the user is still
 * uploading it.
 * 
 * \note Don't forget to enable streaming mode in the SlotInfo of this slot.
 * 
 * \sa pipe pipeBody
 * 
 * \par Closing and errors
 * Use close() to close the connection to the client. If an error occurs you're
 * free to use killConnection. 
 * 
 * \warning As of now HttpClient is self-contained. This means that it will use
 * deleteLater() on itself when the connection is closed by any side. Do not do
 * this on your own. Never \c delete or deleteLater a HttpClient instance! Use
 * close instead!
 * 
 * \par Range request and responses
 * If you want to support requests with 'Range' headers use rangeStart() and
 * rangeEnd() to get the range the client requested. Use setContentLength() to
 * set the length of the \b complete response.
 * 
 * \par Compression and filtering
 * Transparent compression is provided by filters. Filters are small classes
 * which can hook themselves into the outgoing stream and e.g. compress or
 * otherwise modify streams and HTTP headers.
 * 
 * Filters are invoked in the order they were added, except for those two
 * "standard" filters, which are always invoked last.
 * 
 * Support for "gzip" (RFC 1952) and "deflate" (RFCs 1951, 1950) compressions
 * schemes are provided by the framework. Custom filters can be written by
 * sub-classing HttpFilter.
 * 
 * \warning You should be aware of the BREACH attack, which targets
 * encrypted and compressed transmissions. Please see:
 * http://en.wikipedia.org/wiki/BREACH_%28security_exploit%29
 * 
 * \warning Also as general note, use TLS. Do not use SSL. SSL is completely
 * broken as in: unsecure.
 * 
 * \note It is recommended to use "gzip" over "deflate" as deflate has severe
 * browser compatibility issues.
 * 
 * \warning Filters are \b not owned by the HttpClient instance.
 * 
 * \sa HttpFilter addFilter removeFilter
 * 
 */
class NURIA_NETWORK_EXPORT HttpClient : public QIODevice {
	Q_OBJECT
	Q_ENUMS(HttpVerb HttpHeader)
	Q_FLAGS(HttpVerbs)
public:
	
	/**
	 * The HttpVersion enum lists the possible HTTP versions.
	 */
	enum HttpVersion {
		HttpUnknown,
		Http1_0, ///< Http 1.0
		Http1_1 ///< Http 1.1
	};
	
	/**
	 * The HttpVerb enum contains a list of possible HTTP request types.
	 * \sa SlotInfo::allowedVerbs
	 */
	enum HttpVerb {
		/**
		 * The client sent an invalid verb. This is also the default
		 * value which is present if isHeaderReady returns \a false.
		 */
		InvalidVerb = 0,
		GET = 1, ///< The client issued a GET request
		POST = 2, ///< The client issued a POST request
		HEAD = 4, ///< The client issued a HEAD request
		PUT = 8, ///< The client issued a PUT request
		DELETE = 16, ///< The client issued a DELETE request
		
		/** OR-combination of all HTTP verbs. */
		AllVerbs = GET | POST | HEAD | PUT | DELETE
	};
	
	Q_DECLARE_FLAGS(HttpVerbs, HttpVerb)
	
	/**
	 * The HttpHeader enum lists the most common HTTP headers for
	 * requests and responses. Read some online resource for further
	 * explanations.
	 */
	enum HttpHeader {
		
		/* Request and response */
		HeaderCacheControl = 0,
		HeaderContentLength,
		HeaderContentType,
		HeaderConnection,
		HeaderDate,
		
		/* Request only */
		HeaderHost = 1000, ///< Host, must be present in a HTTP/1.1 session
		HeaderUserAgent, ///< User-Agent, must be present in a HTTP/1.1 session
		HeaderAccept,
		HeaderAcceptCharset,
		HeaderAcceptEncoding,
		HeaderAcceptLanguage,
		HeaderAuthorization,
		HeaderCookie,
		HeaderRange,
		HeaderReferer,
		HeaderDoNotTrack,
		HeaderExpect,
		
		/* Response only */
		HeaderContentEncoding = 2000,
		HeaderContentLanguage,
		HeaderContentDisposition,
		HeaderContentRange,
		HeaderLastModified,
		HeaderRefresh,
		HeaderSetCookie,
		HeaderTransferEncoding,
		HeaderLocation
		
	};
	
	/** Transfer modes for responses. */
	enum TransferMode {
		
		/**
		 * Default for "Connection: close" requests. The response will
		 * be streamed to the remote peer. It is not possible to send
		 * additional response HTTP headers after the first write()
		 * call. This mode doesn't support reusing the same connection
		 * for other requests.
		 */
		Streaming = 0,
		
		/**
		 * Support for both "Connection" 'close' and 'keep-alive'
		 * connections. All calls to write() will be buffered until
		 * close() is called on the HttpClient instance, after which
		 * all buffered data will be sent. It is possible to change
		 * response headers until that at all times. The Content-Type
		 * and Content-Length response headers will be set
		 * automatically.
		 * 
		 * \note Internally a TemporaryBufferDevice is used.
		 */
		Buffered,
		
		/**
		 * Default for "Connection: keep-alive" requests. The response
		 * will be streamed with a transfer-encoding of 'chunked'. This
		 * mode supports reusing the same connection for further
		 * requests.
		 */
		ChunkedStreaming
	};
	
	/** Connection modes. */
	enum ConnectionMode {
		
		/** The connection will be closed after this request. */
		ConnectionClose = 0,
		
		/** The connection will be kept alive after this request. */
		ConnectionKeepAlive
	};
	
	/** Enumeration of standard filters. */
	enum StandardFilter {
		
		/** Deflate filter. */
		DeflateFilter = 0,
		
		/** GZIP filter. */
		GzipFilter = 1
		
	};
	
	/**
	 * Modes which decide to which specific URL the client is redirected to.
	 */
	enum class RedirectMode {
		
		/**
		 * Don't change the protocol encryption.
		 * Keeps the secure or unsecure connection.
		 */
		Keep = 0,
		
		/** Redirect the client to the SSL secured path. */
		ForceSecure,
		
		/** Redirect the client to the unsecured path. */
		ForceUnsecure
	};
	
	/** Map used to store HTTP headers in a name -> value fashion. */
	typedef QMultiMap< QByteArray, QByteArray > HeaderMap;
	
	/**
	 * Constructs a new HttpClient instance which uses \a transport as
	 * communication back-end.
	 * 
	 * Ownership of the HttpClient will be transferred to \a transport.
	 */
	explicit HttpClient (HttpTransport *transport, HttpServer *server);
	
	/** Destructor. */
	virtual ~HttpClient ();
	
	/** Returns the internal HttpTransport instance. */
	HttpTransport *transport () const;
	
	/**
	 * Returns the HTTP name for a statuscode.
	 * If none is known for \a code, an empty string is returned.
	 */
	static QByteArray httpStatusCodeName (int code);
	
	/**
	 * Returns the name of a HTTP standard header.
	 */
	static QByteArray httpHeaderName (HttpHeader header);
	
	/** Returns \a true if the client has sent the complete header. */
	bool isHeaderReady () const;
	
	/**
	 * Returns \a true if the response header has been sent. The response
	 * header will be sent on the first write call to the instance or if
	 * the sendResponseHeader method has been called explicitly.
	 */
	bool responseHeaderSent () const;
	
	/** Returns the HTTP verb used by the client. */
	HttpVerb verb () const;
	
	/** Returns the complete requested path. */
	QUrl path () const;
	
	/**
	 * Returns if the client has sent a value for \a key in the
	 * request header.
	 */
	bool hasRequestHeader (const QByteArray &key) const;
	
	/** \overload */
	bool hasRequestHeader (HttpHeader header) const;
	
	/**
	 * Returns a value of a header from the request.
	 * If the client sent multiple values for the header, this method
	 * returns the last one.
	 * Returns an empty QByteArray if there was no header named \a key.
	 * \sa hasRequestHeader
	 */
	QByteArray requestHeader (const QByteArray &key) const;
	
	/** \overload */
	QByteArray requestHeader (HttpHeader header) const;
	
	/**
	 * Returns the values of a header from the request.
	 * This method is useful for headers which may appear multiple times.
	 * Returns an empty QByteArray if there was no header named \a key.
	 * \sa hasRequestHeader
	 */
	QList< QByteArray > requestHeaders (const QByteArray &key) const;
	
	/** \overload */
	QList< QByteArray > requestHeaders (HttpHeader header) const;
	
	/** Returns a map of received request headers. */
	const HeaderMap &requestHeaders () const;
	
	/** Returns \a true when \a key has a value in the response header */
	bool hasResponseHeader (const QByteArray &key) const;
	
	/** \overload */
	bool hasResponseHeader (HttpHeader header) const;
	
	/**
	 * Returns the current response headers.
	 */
	const HeaderMap &responseHeaders () const;
	
	/**
	 * Returns the response headers with key \a key.
	 */
	QList< QByteArray > responseHeaders (const QByteArray &key) const;
	
	/** \overload */
	QList< QByteArray > responseHeaders (HttpHeader header) const;
	
	/**
	 * Sets a response header. Returns \a true if the response header has
	 * not yet been sent. If it was already sent to the client, the function
	 * will fail as HTTP doesn't allow to send additional headers after the
	 * HTTP header. In this case the response header map won't be modified.
	 * If \a append is \c true and there is already a value for \a key, then
	 * the new value will be appended to the list.
	 * 
	 * \note If you want to set the 'Content-Length' header please use
	 * setContentLength().
	 * \sa contentLength setContentLength
	 * \sa rangeStart rangeEnd setRangeStart setRangeEnd
	 */
	bool setResponseHeader (const QByteArray &key, const QByteArray &value, bool append = false);
	
	/** \overload */
	bool setResponseHeader (HttpHeader header, const QByteArray &value, bool append = false);
	
	/**
	 * Sets the response header map.
	 * \sa setResponseHeader
	 */
	bool setResponseHeaders (const HeaderMap &headers);
	
	/**
	 * Takes \a device and pipes its contents into the client socket.
	 * After this call, any other write call will fail. The ownership
	 * of \a device will be transferred to this instance and thus will
	 * be destroyed when the client quits or \a device has no more data
	 * to read.
	 * \note \a device must be readable and open.
	 */
	bool pipeToClient (QIODevice *device, qint64 maxlen = -1);
	
	/**
	 * Uses \a device from now on as buffer device. You can use this for
	 * example when you write an application which takes the POST body data,
	 * processes it somehow and then pipeToClient() the data back.
	 * The data in the current buffer is written to \a device.
	 * 
	 * Upon calling, the openMode of the HttpClient will be changed to
	 * QIODevice::WriteOnly.
	 * 
	 * \note \a device must be open and writable.
	 * \warning Make sure \a device is not \c nullptr.
	 */
	bool pipeFromPostBody (QIODevice *device, bool takeOwnership = false);
	
	/**
	 * Returns the start position of a 'range' request.
	 * If the client sent a valid 'Range' header, this method will return
	 * the start of the requested range. Else \c -1 is returned.
	 */
	qint64 rangeStart () const;
	
	/**
	 * Returns the end position of a 'range' request.
	 * If the client sent a valid 'Range' header, this method will return
	 * the end of the requested range. Else \c -1 is returned.
	 */
	qint64 rangeEnd () const;
	
	/**
	 * Returns the Content-Length. Returns \c -1 if the content length is
	 * unknown.
	 */
	qint64 contentLength () const;
	
	/**
	 * Sets the start position of a range response.
	 * \note If the response header has already been sent the call
	 * will fail.
	 */
	bool setRangeStart (qint64 pos);
	
	/**
	 * Sets the end position of a range response.
	 * \note If the response header has already been sent the call
	 * will fail.
	 */
	bool setRangeEnd (qint64 pos);
	
	/**
	 * Sets the Content-Length. Used in conjunction with range responses.
	 * \note If the response header has already been sent the call
	 * will fail.
	 */
	bool setContentLength (qint64 length);
	
	/**
	 * Returns \c false, meaning this is a random-access device.
	 * \warning If you've set a buffer device through pipeFromPostBody(),
	 * then \c true is returned.
	 */
	virtual bool isSequential () const;
	
	/**
	 * Convenience function, returns the length of the POST body using
	 * the Content-Length header.
	 */
	qint64 postBodyLength () const;
	
	/**
	 * Returns how many bytes have been transferred of the POST body.
	 */
	qint64 postBodyTransferred ();
	
	/**
	 * See QAbstractSocket::localAddress.
	 */
	QHostAddress localAddress () const;
	
	/**
	 * See QAbstractSocket::localPort.
	 */
	quint16 localPort () const;
	
	/**
	 * See QAbstractSocket::peerAddress.
	 */
	QHostAddress peerAddress () const;
		
	/**
	 * See QAbstractSocket::peerPort.
	 */
	quint16 peerPort () const;
	
	/** 
	 * Returns the HTTP response code.
	 * Default is <tt>200 OK</tt>
	 */
	int responseCode () const;
	
	/**
	 * Sets the HTTP response code.
	 */
	void setResponseCode (int code);
	
	/**
	 * Returns \c true if the connection is secured.
	 * \sa HttpTransport::isSecure
	 */
	bool isConnectionSecure () const;
	
	/**
	 * Returns the HTTP server this client belongs to.
	 */
	HttpServer *httpServer () const;
	
	/** List of cookies. */
	typedef QMap< QByteArray, QNetworkCookie > Cookies;
	
	/**
	 * Returns all received cookies from the client.
	 * \note This method refers to cookies received from the client.
	 * \sa cookie hasCookie
	 */
	Cookies cookies ();
	
	/**
	 * Returns the value of cookie \a name.
	 * Returns an empty value if there is no cookie with this name.
	 * \note This method refers to cookies received from the client.
	 * \sa cookies hasCookie
	 */
	QNetworkCookie cookie (const QByteArray &name);
	
	/**
	 * Returns if there is a cookie \a name.
	 * \note This method refers to cookies received from the client.
	 * \sa cookies cookie
	 */
	bool hasCookie (const QByteArray &name);
	
	/**
	 * Sets a cookie on the client using a HTTP header.
	 * If there is already a cookie with \a name, it will be overridden.
	 * If \a secure is \c true the client is avised to only send the
	 * cookie to the server if a secure connection is used.
	 * \note This is only possible as long the HTTP response hasn't been
	 * sent yet.
	 */
	void setCookie (const QByteArray &name, const QByteArray &value,
			const QDateTime &expires, bool secure = false);
	
	/**
	 * \overload
	 * Sets a cookie on the client using a HTTP header.
	 * If there is already a cookie with \a name, it will be overridden.
	 * \a maxAge defines the maximum age of the cookie in seconds. If it's
	 * \c 0, the cookie will removed by the client at the end of the
	 * session.
	 * If \a secure is \c true the client is avised to only send the
	 * cookie to the server if a secure connection is used.
	 * \note This is only possible as long the HTTP response hasn't been
	 * sent yet.
	 * \note If maxAge is \c 0 (the default), the cookie will be a 
	 * session cookie.
	 */
	void setCookie (const QByteArray &name, const QByteArray &value,
			qint64 maxAge = 0, bool secure = false);
	
	/**
	 * \overload
	 * Sets a cookie on the client using a HTTP header.
	 * Use this overload if the other overloads aren't specific enough.
	 * \note This is only possible as long the HTTP response hasn't been
	 * sent yet.
	 */
	void setCookie (const QNetworkCookie &cookie);
	
	/**
	 * Removes a cookie. If the cookie is known by the client it will
	 * also removed client-side. If the cookie is only known on
	 * server-side it will only be removed from there. If no cookie
	 * \a name is known nothing happens.
	 * \note This is only possible as long the HTTP response hasn't been
	 * sent yet.
	 */
	void removeCookie (const QByteArray &name);
	
	/**
	 * Returns if the connection should be kept open after the slot has
	 * been called.
	 */
	bool keepConnectionOpen () const;
	
	/**
	 * Sets whether the HTTP connection should be kept open after a slot
	 * has been called. By default, this is not the case. 
	 * \warning Ownership of the instance is transferred to the user.
	 * \note Call close() to delete the instance. It will initiate a
	 * graceful shutdown, making sure everything is written before the
	 * instance is deleted. If the connection is already closed, use
	 * deleteLater().
	 * \note Do not combine this with pipe().
	 */
	void setKeepConnectionOpen (bool keepOpen);
	
	/**
	 * Returns the associated slot info of the client.
	 */
	SlotInfo slotInfo () const;
	
	/**
	 * Sets a SlotInfo for this client. This is needed when you're
	 * sub-classing HttpNode with non-streaming clients.
	 */
	void setSlotInfo (const SlotInfo &info);
	
	/**
	 * Returns \c true only if this request has a POST body and it's
	 * readable by one of the readers known to HttpClient, e.g. for
	 * HTTP multi-part.
	 * 
	 * \sa read()
	 */
	bool hasReadablePostBody () const;
	
	/**
	 * If hasReadablePostBody() returns \c true, returns a suitable
	 * POST body reader instance. Else returns \c nullptr. The reader
	 * instance is owned by the HttpClient. Consecutive calls of this
	 * method will always return the same result. The first call will
	 * create the reader.
	 */
	HttpPostBodyReader *postBodyReader ();
	
	/** Returns the used transfer mode. */
	TransferMode transferMode () const;
	
	/**
	 * Sets the transfer \a mode. This is only possible to do before
	 * anything has been sent. On success, \c true is returned.
	 */
	bool setTransferMode (TransferMode mode);
	
	/** Returns the connection mode of this client. */
	ConnectionMode connectionMode () const;
	
	/**
	 * Returns \c true if the request has been completely received.
	 * This includes the POST/PUT body if one exists.
	 */
	bool requestCompletelyReceived () const;
	
	/** Returns \c true if the request has a POST/PUT body. */
	bool requestHasPostBody () const;
	
	/**
	 * Adds \a filter to the filter chain. There can only be the
	 * DeflateFilter or the GzipFilter active at the same time.
	 * If one is already added before calling this method, it will
	 * be replaced.
	 */
	void addFilter (StandardFilter filter);
	
	/** Adds \a filter to the filter chain. */
	void addFilter (HttpFilter *filter);
	
	/** Removes \a filter from the filter chain. */
	void removeFilter (StandardFilter filter);
	
	/** Removes \a filter from the filter chain. */
	void removeFilter (HttpFilter *filter);
	
	/**
	 * Sends a HTTP redirect response, telling the client to go to
	 * \a localPath. Returns \c true if no headers has been sent yet.
	 * 
	 * Use this version of this function when you're redirecting to a URL
	 * which is served by this server.
	 * 
	 * For redirections to work properly, the Nuria::Server instance must
	 * have been told the FQDN ("fully qualified domain name").
	 * 
	 * \note HTTP/1.0 clients will receive a status code of 301 instead of
	 * 307.
	 */
	bool redirectClient (const QString &localPath, RedirectMode mode = RedirectMode::Keep,
	                     int statusCode = 307);
	
	/**
	 * Sends a HTTP redirect response, telling the client to go to
	 * \a remoteUrl. This should only be used for URLs not served by this
	 * server. Returns \c true if no headers has been sent yet.
	 * 
         * \note HTTP/1.0 clients will receive a status code of 301 instead of
         * 307.
	 */
	bool redirectClient (const QUrl &remoteUrl, int statusCode = 307);
	
	/**
	 * Returns \c true if the POST body has been completely read by the
	 * user. Also returns \c true if the request has no POST body at all.
	 * Returns \c false otherwise.
	 */
	bool atEnd () const override;
	
	/**
	 * Initializes the HttpClient instance.
	 * 
	 * This function is only intended for use within HttpTransports which
	 * get the HTTP header in a already parsed fashion to initialize
	 * HttpClient instances without having to assemble new ones.
	 * 
	 * If you're not implementing a HttpTransport, then you'll probably
	 * never need this function.
	 * 
	 * Returns \c true on success. On failure, the client will be closed.
	 * The HttpTransport must be able to receive data through its
	 * sendToRemote() function already when this function is called, as
	 * the HttpClient will start to process the data right away.
	 */
	bool manualInit (HttpVerb verb, HttpVersion version, const QByteArray &path, const HeaderMap &headers);
	
	// 
	qint64 bytesAvailable () const override;
	qint64 pos () const override;
	qint64 size () const override;
	bool seek (qint64 pos) override;
	bool reset () override;
	
signals:
	
	/** Gets emitted when the user just closed the connection. */
	void disconnected ();
	
	/** Gets emitted when the client has sent the header. */
	void headerReady ();
	
	/** Gets emitted when the transfer of the POST body has been completed. */
	void postBodyComplete ();
	
public slots:
	
	/**
	 * Kills the connection to the client immediatly with \a error as HTTP
	 * error code. If \a cause is not empty, it will be sent to the client.
	 * Else a appropriate cause is chosen automatically based on \a error.
	 * 
	 * This is only possible until the response header has been sent. If the
	 * response header has already been sent, the connection to the client
	 * will simply be destroyed.
	 */
	bool killConnection (int error, const QString &cause = QString());
	
	/**
	 * Explicitly sends a response header to the client. Fails if the
	 * client hasn't send a request so far (isHeaderReady() returns
	 * \a false) or if the response header has already been sent (In this
	 * case responseHeaderSent() returns \a true).
	 */
	bool sendResponseHeader ();
	
	/**
	 * \note close() will make sure that all data that is in the write 
	 * buffer will be sent to the client before the socket is closed.
	 * 
	 * If no data has been sent yet, but a request header has been received,
	 * a appropriate response header will be sent.
	 * 
	 * \sa forceClose
	 */
	void close () override;
	
	/**
	 * Tells the transport to immediately close the connection, without
	 * trying to send data in the send buffers.
	 * \warning This may cause data-loss!
	 */
	void forceClose ();
	
private slots:
	
	/** The client has disconnected. */
	void clientDisconnected ();
	
	/** The pipe io device has some data for us. */
	void pipeToClientReadyRead ();
	
protected:
	
	/** Implementation of QIODevice::readData. */
	virtual qint64 readData (char *data, qint64 maxlen);
	
	/** Implementation of QIODevice::writeData. */
	virtual qint64 writeData (const char *data, qint64 len);
	
	/**
	 * Resolves the URL from the client. That means that the code tries
	 * to find the associated slot of \a url and calls it if possible.
	 * Returns \c true on success.
	 */
	bool resolveUrl (const QUrl &url);
	
	/**
	 * Takes care of buffering the POST body.
	 */
	bool bufferPostBody (QByteArray &data);
	
private:
	friend class HttpTransport;
	friend class HttpServer;
	friend class HttpNode;
	
	/**
	 * Parses the request headers. Returns \c true on success.
	 */
	bool readRangeRequestHeader ();
	
	/**
	 * Reads the cookies from the request headers if not already done.
	 * This is used to 'lazy-load' the cookies as most responses don't
	 * need to access cookies anyway, so we can save some little memory
	 * here.
	 */
	void readRequestCookies ();
	
	bool sendRedirectResponse (const QByteArray &location, const QByteArray &display, int code);
	void updateRequestedUrl ();
	
	void bytesSent (qint64 bytes);
	void processData (QByteArray &data);
	
	bool sendChunkedData (const QByteArray &data);
	qint64 parseIntegerHeaderValue (const QByteArray &value);
	bool readPostBodyContentLength ();
	bool send100ContinueIfClientExpectsIt ();
	bool readAllAvailableHeaderLines (QByteArray &data);
	bool readFirstLine (const QByteArray &line);
	bool readHeader (const QByteArray &line);
	bool isReceivedHeaderHttp11Compliant ();
	bool verifyPostRequestCompliance ();
	bool verifyCompleteHeader ();
	bool invokeRequestedPath ();
	bool readConnectionHeader ();
	bool closeConnectionIfNoLongerNeeded ();
	bool postProcessRequestHeader ();
	bool contentTypeIsMultipart (const QByteArray &value) const;
	bool contentTypeIsUrlEncoded (const QByteArray &value) const;
	HttpPostBodyReader *createHttpMultiPartReader (const QByteArray &header);
	HttpPostBodyReader *createUrlEncodedPartReader (const QByteArray &header);
	qint64 writeDataInternal (QByteArray data);
	bool sendData (const QByteArray &data);
	void closeInternal ();
	QByteArray filterInit ();
	QByteArray filterDeinit ();
	bool filterData (QByteArray &data);
	bool filterHeaders (HeaderMap &headers);
	void addFilterNameToHeader (HeaderMap &headers, const QByteArray &name);
	void initPath (QByteArray path);
	
	/**
	 * Sends a chunk of the pipeToClient() device to the client.
	 * Returns \c true if there's more data.
	 */
	bool sendPipeChunkToClient ();
	
	// 
	HttpClientPrivate *d_ptr;
	
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Nuria::HttpClient::HttpVerbs)
Q_DECLARE_METATYPE(Nuria::HttpClient::HttpVersion)
Q_DECLARE_METATYPE(Nuria::HttpClient::HttpVerbs)
Q_DECLARE_METATYPE(Nuria::HttpClient::HttpVerb)
Q_DECLARE_METATYPE(Nuria::HttpClient*)

#endif // NURIA_HTTPCLIENT_HPP
