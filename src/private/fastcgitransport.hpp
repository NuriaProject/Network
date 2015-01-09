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
