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

#ifndef NURIA_HTTPMULTIPARTREADER_HPP
#define NURIA_HTTPMULTIPARTREADER_HPP

#include "httppostbodyreader.hpp"

namespace Nuria {

class HttpMultiPartReaderPrivate;
class TemporaryBufferDevice;

/**
 * \brief Reader for HTTP multi-part formatted data.
 * 
 * HTTP User-agents use specific formats to transfer form-data to the server.
 * One of these formats is 'multi-part'.
 * 
 * \sa HttpClient::hasReadablePostBody() HttpClient::postBodyReader()
 * 
 * \par Limitations
 * The format itself does not require clients to tell the server beforehand
 * how much data a single field will contain. Same applies for the MIME-type.
 * 
 * For the user of this class this means, that:
 * - fieldLength() will return \c -1 as long the transfer is in process.
 * - fieldMimeType() may or may not return something useful.
 */
class NURIA_NETWORK_EXPORT HttpMultiPartReader : public HttpPostBodyReader {
	Q_OBJECT
public:
	
	/**
	 * Creates a multi-part reader which acts upon \a device.
	 */
	HttpMultiPartReader (QIODevice *device, const QByteArray &boundary, QObject *parent = nullptr);
	
	/** Destructor. */
	~HttpMultiPartReader ();
	
	bool isComplete () const override;
	bool hasFailed () const override;
	bool hasField (const QString &field) const override;
	QStringList fieldNames () const override;
	QString fieldMimeType (const QString &field) const override;
	qint64 fieldLength (const QString &field) const override;
	qint64 fieldBytesTransferred (const QString &field) const override;
	bool isFieldComplete (const QString &field) const override;
	QIODevice *fieldStream (const QString &field) override;
	
private:
	
	void setState (int state);
	void processData ();
	void processChunk ();
	bool processChunkStep ();
	int processFirstLine ();
	int processAllHeaders ();
	int processHeaders (int offset);
	bool processHeaderLine (const QByteArray &line);
	bool parseHeaders ();
	qint64 parseContentLength (const QByteArray &data);
	QString parseContentDispositionValue (const QByteArray &data);
	QString parseContentDispositionFormDataValue (const QByteArray &data);
	int processContent ();
	int isBoundingLine (const QByteArray &buffer, bool &last, int offset);
	int boundingLineLength () const;
	void stopListening ();
	int appendToCurrentBuffer (const QByteArray &data, int length);
	int processContentBoundary (int idxOfNewline);
	void initCurrentBuffer (const QString &name, TemporaryBufferDevice *device);
	
	HttpMultiPartReaderPrivate *d_ptr;
	
};

}

#endif // NURIA_HTTPMULTIPARTREADER_HPP
