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

#ifndef NURIA_INTERNAL_TCPSERVER_HPP
#define NURIA_INTERNAL_TCPSERVER_HPP

#include <QSslCertificate>
#include <QTcpServer>
#include <QSslError>
#include <QSslKey>

namespace Nuria {

namespace Internal {

class HttpTcpBackend;

class TcpServer : public QTcpServer {
	Q_OBJECT
public:
	explicit TcpServer (bool useSsl, QObject *parent = 0);
	
	const QSslKey &privateKey () const;
	const QSslCertificate &localCertificate () const;
	
	bool useEncryption () const;
	
	void setPrivateKey (const QSslKey &key);
	void setLocalCertificate (const QSslCertificate &cert);
	
	QTcpSocket *handleToSocket (qintptr handle);
	
protected:
	
	void incomingConnection (qintptr handle) override;
	
signals:
	
	void connectionRequested (qintptr handle);
	
private:
	
	bool m_ssl;
	QSslKey m_key;
	QSslCertificate m_cert;
	
};
}

}

#endif // NURIA_INTERNAL_TCPSERVER_HPP
