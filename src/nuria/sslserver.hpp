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

#ifndef NURIA_SSLSERVER_HPP
#define NURIA_SSLSERVER_HPP

#include "network_global.hpp"
#include <QSslCertificate>
#include <QTcpServer>
#include <QSslError>
#include <QSslKey>

namespace Nuria {
class SslServerPrivate;

/**
 * Internal helper class for HttpServer. It's essentially the same like a
 * QTcpServer, but instead of using plain tcp connection this class uses
 * only SSL secured connections.
 */
class NURIA_NETWORK_EXPORT SslServer : public QTcpServer {
	Q_OBJECT
public:
	explicit SslServer (QObject *parent = 0);
	~SslServer ();
	
	const QSslKey &privateKey () const;
	const QSslCertificate &localCertificate () const;
	
	void setPrivateKey (const QSslKey &key);
	void setLocalCertificate (const QSslCertificate &cert);
	
private slots:
	
	void connectionEncrypted ();
	void sslError (QList< QSslError > errors);
	
protected:
	
	virtual void incomingConnection (int handle);
	
private:
	
	// 
	SslServerPrivate *d_ptr;
	
};
}

#endif // NURIA_SSLSERVER_HPP
