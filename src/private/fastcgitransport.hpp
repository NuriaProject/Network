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

#ifndef NURIA_INTERNAL_FASTCGITRANSPORT_HPP
#define NURIA_INTERNAL_FASTCGITRANSPORT_HPP

#include "../nuria/httptransport.hpp"
#include "../nuria/httpclient.hpp"
class QIODevice;

namespace Nuria {

class FastCgiBackend;

namespace Internal {

class FastCgiTransportPrivate;
class FastCgiThreadObject;

/**
 * \brief HttpTransport for FastCGI connections
 */
class FastCgiTransport : public HttpTransport {
	Q_OBJECT
public:
	
	/** Constructor */
	explicit FastCgiTransport (uint32_t requestId, QIODevice *device, FastCgiThreadObject *object,
	                           FastCgiBackend *backend, HttpServer *server);
	
	/** Destructor. */
	~FastCgiTransport () override;
	
	/** Aborts the running HttpClient. */
	void abort ();
	
	/** Returns the internal device. */
	QIODevice *device () const;
	
	/** Returns the request id of this transport. */
	uint16_t requestId () const;
	
	/** Adds FastCGI PARAMS fields. */
	void addFastCgiParams (const QMap< QByteArray, QByteArray > &params);
	
	// 
	Type type () const;
	bool isSecure () const;
	bool isOpen () const;
	QHostAddress localAddress () const;
	quint16 localPort () const;
	QHostAddress peerAddress () const;
	quint16 peerPort () const;
	
public slots:
	bool flush (HttpClient *);
	void forceClose ();
	void init ();
	
protected:
	friend class FastCgiThreadObject;
	
	void close (HttpClient *client);
	bool sendToRemote (HttpClient *, const QByteArray &data);
	void forwardBodyData (const QByteArray &data);
	
private:
	HttpClient::HeaderMap processedHeaders ();
	void replaceFirstLine (QByteArray &data);
	void preparePeerData ();
	void startClient ();
	void bytesWritten (qint64 bytes);
	void sendData (const QByteArray &data);
	void closeFcgiRequest ();
	void controlTimer (bool running);
	
	FastCgiTransportPrivate *d_ptr;
};

}
}

#endif // NURIA_INTERNAL_FASTCGITRANSPORT_HPP
