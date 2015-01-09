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

#ifndef NURIA_INTERNAL_TCPSERVER_HPP
#define NURIA_INTERNAL_TCPSERVER_HPP

#include <QTcpServer>

#ifndef NURIA_NO_SSL_HTTP
#include <QSslCertificate>
#include <QSslError>
#include <QSslKey>
#endif

namespace Nuria {
namespace Internal {

class TcpServer : public QTcpServer {
	Q_OBJECT
public:
	explicit TcpServer (bool useSsl, QObject *parent = 0);
	~TcpServer () override;
	
	bool useEncryption () const;
	
#ifndef NURIA_NO_SSL_HTTP
	const QSslKey &privateKey () const;
	const QSslCertificate &localCertificate () const;
	
	void setPrivateKey (const QSslKey &key);
	void setLocalCertificate (const QSslCertificate &cert);
#endif
	
	QTcpSocket *handleToSocket (qintptr handle);
	
protected:
	
	void incomingConnection (qintptr handle) override;
	
signals:
	
	void connectionRequested (qintptr handle);
	
private:
	
	bool m_ssl;
	
#ifndef NURIA_NO_SSL_HTTP
	QSslKey m_key;
	QSslCertificate m_cert;
#endif
	
};

}
}

#endif // NURIA_INTERNAL_TCPSERVER_HPP
