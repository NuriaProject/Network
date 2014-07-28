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

#ifndef NURIA_HTTPMEMORYTRANSPORT_HPP
#define NURIA_HTTPMEMORYTRANSPORT_HPP

#include <nuria/httptransport.hpp>
#include <QBuffer>

namespace Nuria {

/**
 * \brief HttpTransport which uses a in-memory buffer for testing purposes.
 */
class HttpMemoryTransport : public HttpTransport {
	Q_OBJECT
public:
	QBuffer *ingoing; // Transport -> HttpClient
	QBuffer *outgoing; // HttpClient -> Transport
	
	/** Constructor. */
	explicit HttpMemoryTransport (QObject *parent = 0);
	
	const QByteArray &inData ()
	{ return ingoing->data (); }
	
	const QByteArray &outData ()
	{ return outgoing->data (); }
	
	void clearOutgoing () {
		outgoing->close ();
		outgoing->setData (QByteArray ());
		outgoing->open (QIODevice::ReadWrite);
	}
	
	void setIncoming (const QByteArray &data) {
		ingoing->write (data);
		ingoing->reset ();
		emit readyRead ();
	}
	
	void close ();
	qint64 pos () const;
	qint64 size () const;
	bool seek (qint64 pos);
	bool atEnd () const;
	qint64 bytesAvailable () const;
	bool reset ();
	bool canReadLine () const;
	
protected:
	qint64 readData (char *data, qint64 maxlen);
	qint64 readLineData(char *data, qint64 maxlen);
	qint64 writeData (const char *data, qint64 len);
	
};

}

#endif // NURIA_HTTPMEMORYTRANSPORT_HPP
