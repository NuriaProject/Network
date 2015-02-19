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
	
private:
	void clientDestroyed (QObject *object);
	void bytesWritten (qint64 bytes);
	void processData (QByteArray &data);
	void appendReceivedDataToBuffer ();
	void dataReceived ();
	void clientDisconnected ();
	void closeInternal ();
	bool wasLastRequest ();
	void connectionReady ();
	
	HttpTcpTransportPrivate *d_ptr;
	
};

}
}

#endif // NURIA_INTERNAL_HTTPTCPTRANSPORT_HPP
