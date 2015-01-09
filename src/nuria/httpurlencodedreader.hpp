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
