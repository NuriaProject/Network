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
