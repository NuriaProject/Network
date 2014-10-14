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

#ifndef NURIA_INTERNAL_HTTPTCPTRANSPORT_HPP
#define NURIA_INTERNAL_HTTPTCPTRANSPORT_HPP

#include "../nuria/httptransport.hpp"

class QTcpSocket;

namespace Nuria {
class HttpServer;

namespace Internal {
class HttpTcpTransportPrivate;
class HttpTcpBackend;

class HttpTcpTransport : public HttpTransport {
	Q_OBJECT
public:
	
	/** Constructor. */
	explicit HttpTcpTransport (qintptr handle, HttpTcpBackend *backend, HttpServer *server);
	
	/** Destructor. */
	~HttpTcpTransport () override;
	
	// 
	Type type () const override;
	bool isSecure () const override;
	QHostAddress localAddress () const override;
	quint16 localPort () const override;
	QHostAddress peerAddress () const override;
	quint16 peerPort () const override;
	bool isOpen () const override;
	
public slots:
	bool flush (HttpClient *) override;
	void forceClose () override;
	void init () override;
	
private slots:
	bool closeSocketWhenBytesWereWritten ();
	
protected:
	void close (HttpClient *client) override;
	bool sendToRemote (HttpClient *client, const QByteArray &data) override;
	void timerEvent (QTimerEvent *) override;
	
private:
	void clientDestroyed (QObject *object);
	void bytesWritten (qint64 bytes);
	void processData (QByteArray &data);
	void dataReceived ();
	void clientDisconnected ();
	void closeInternal ();
	bool wasLastRequest ();
	void startTimeout (Timeout mode);
	void killTimeout ();
	void connectionReady ();
	
	HttpTcpTransportPrivate *d_ptr;
	
};

}
}

#endif // NURIA_INTERNAL_HTTPTCPTRANSPORT_HPP
