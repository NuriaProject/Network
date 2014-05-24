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

#ifndef NURIA_HTTPTCPTRANSPORT_HPP
#define NURIA_HTTPTCPTRANSPORT_HPP

#include "httptransport.hpp"

class QTcpSocket;

namespace Nuria {

class HttpTcpTransportPrivate;

/**
 * \brief HttpClient transport for TCP with or without SSL connections.
 */
class HttpTcpTransport : public HttpTransport {
	Q_OBJECT
public:
	/** Constructor. */
	explicit HttpTcpTransport (QTcpSocket *socket, QObject *parent = 0);
	
	/** Destructor. */
	~HttpTcpTransport () override;
	
	// 
	Type type () const override;
	bool isSecure () const override;
	QHostAddress localAddress () const override;
	quint16 localPort () const override;
	QHostAddress peerAddress () const override;
	quint16 peerPort () const override;
	
	bool isSequential () const;
	bool open (OpenMode mode);
	void close ();
	qint64 pos () const;
	qint64 size () const;
	bool seek (qint64 pos);
	bool atEnd () const;
	bool reset ();
	qint64 bytesAvailable () const;
	qint64 bytesToWrite () const;
	bool canReadLine () const;
	bool waitForReadyRead (int msecs);
	bool waitForBytesWritten (int msecs);

public slots:
	bool flush () override;
	void forceClose () override;
	
private slots:
	bool closeSocketWhenBytesWereWritten ();
	
protected:
	qint64 readData (char *data, qint64 maxlen) override;
	qint64 readLineData(char *data, qint64 maxlen);
	qint64 writeData (const char *data, qint64 len) override;
	
private:
	HttpTcpTransportPrivate *d_ptr;
	
};

}

#endif // NURIA_HTTPTCPTRANSPORT_HPP
