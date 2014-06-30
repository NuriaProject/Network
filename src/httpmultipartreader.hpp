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
