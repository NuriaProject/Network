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

#ifndef NURIA_HTTPURLENCODEDREADER_HPP
#define NURIA_HTTPURLENCODEDREADER_HPP

#include "httppostbodyreader.hpp"

namespace Nuria {

class HttpUrlEncodedReaderPrivate;

/**
 * \brief Reader for HTTP url-encoded formatted data.
 * 
 * \par Limitations
 * The format itself does not require clients to tell the server beforehand
 * how much data a single field will contain. On top of that, the format doesn't
 * support MIME types.
 * 
 * Further, this class does not have support for streaming fields, as URL
 * encoded data is generally short.
 * 
 * For the user of this class this means, that:
 * - fieldLength() will return \c -1 as long the transfer is in process.
 * - fieldMimeType() returns "text/plain; charset=<Charset>"
 */
class NURIA_NETWORK_EXPORT HttpUrlEncodedReader : public HttpPostBodyReader {
	Q_OBJECT
public:
	
	/**
	 * Creates a url-encoded reader which acts upon \a device.
	 */
	HttpUrlEncodedReader (QIODevice *device, const QByteArray &charset, QObject *parent = nullptr);
	
	/** Destructor. */
	~HttpUrlEncodedReader ();
	
	bool isComplete () const override;
	bool hasFailed () const override;
	bool hasField (const QString &field) const override;
	QStringList fieldNames () const override;
	QString fieldMimeType (const QString &) const override;
	qint64 fieldLength (const QString &field) const override;
	qint64 fieldBytesTransferred (const QString &field) const override;
	bool isFieldComplete (const QString &field) const override;
	QIODevice *fieldStream (const QString &field) override;
	QByteArray fieldValue (const QString &field) override;
	
private:
	
	void processData ();
	void processChunk ();
	bool processChunkStep ();
	void processKey ();
	int processValue ();
	void stopListening ();
	
	HttpUrlEncodedReaderPrivate *d_ptr;
	
};

}

#endif // NURIA_HTTPURLENCODEDREADER_HPP
