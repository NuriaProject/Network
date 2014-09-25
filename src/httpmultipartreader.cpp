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

#include "nuria/httpmultipartreader.hpp"

#include <nuria/temporarybufferdevice.hpp>
#include "nuria/httpclient.hpp"
#include "nuria/httpparser.hpp"
#include <nuria/debug.hpp>
#include <QIODevice>
#include <QMap>

struct FieldInfo {
	qint64 totalLength = -1;
	qint64 transferred = 0;
	QString mimeType;
	Nuria::TemporaryBufferDevice *buffer = nullptr;
};

enum State {
	FirstLine,
	Headers,
	Body,
	Complete,
	Error
};

namespace Nuria {
class HttpMultiPartReaderPrivate {
public:
	
	QIODevice *device;
	QByteArray boundary;
	
	QMap< QString, FieldInfo > fields;
	HttpClient::HeaderMap currentHeaders;
	QByteArray chunkBuffer;
	
	State state = FirstLine;
	bool lastWasNewline = false; // For processContent()
	TemporaryBufferDevice *currentBuffer = nullptr;
	qint64 currentBufferWritePos = 0;
	QString currentField;
};
}

Nuria::HttpMultiPartReader::HttpMultiPartReader (QIODevice *device, const QByteArray &boundary, QObject *parent)
	: HttpPostBodyReader (parent), d_ptr (new HttpMultiPartReaderPrivate)
{
	
	this->d_ptr->device = device;
	this->d_ptr->boundary = boundary;
	this->d_ptr->boundary.prepend ("--");
	
	// 
	connect (device, &QIODevice::readyRead, this, &HttpMultiPartReader::processData);
	if (device->bytesAvailable () > 0) {
		processData ();
	}
	
}

Nuria::HttpMultiPartReader::~HttpMultiPartReader() {
	delete this->d_ptr;
}

bool Nuria::HttpMultiPartReader::isComplete () const {
	return (this->d_ptr->state >= Complete);
}

bool Nuria::HttpMultiPartReader::hasFailed () const {
	return (this->d_ptr->state == Error);
}

bool Nuria::HttpMultiPartReader::hasField (const QString &field) const {
	return this->d_ptr->fields.contains (field);
}

QStringList Nuria::HttpMultiPartReader::fieldNames () const {
	return this->d_ptr->fields.keys ();
}

QString Nuria::HttpMultiPartReader::fieldMimeType (const QString &field) const {
	return this->d_ptr->fields.value (field).mimeType;
}

qint64 Nuria::HttpMultiPartReader::fieldLength (const QString &field) const {
	return this->d_ptr->fields.value (field).totalLength;
}

qint64 Nuria::HttpMultiPartReader::fieldBytesTransferred (const QString &field) const {
	return this->d_ptr->fields.value (field).transferred;
}

bool Nuria::HttpMultiPartReader::isFieldComplete (const QString &field) const {
	FieldInfo info = this->d_ptr->fields.value (field);
	return (info.totalLength == info.transferred);
}

QIODevice *Nuria::HttpMultiPartReader::fieldStream (const QString &field) {
	return this->d_ptr->fields.value (field).buffer;
}

void Nuria::HttpMultiPartReader::setState (int state) {
	this->d_ptr->state = State (state);
	
	if (state >= Complete) {
		emit completed (state == Complete);
	}
	
}

void Nuria::HttpMultiPartReader::processData () {
	if (this->d_ptr->state >= Complete) {
		return;
	}
	
	// 
	while (this->d_ptr->device->bytesAvailable () > 0) {
		processChunk ();
	}
	
	// Premature end?
	if (this->d_ptr->state != Complete && this->d_ptr->device->atEnd ()) {
		setState (Error);
	}
	
}

void Nuria::HttpMultiPartReader::processChunk () {
	QByteArray &chunk = this->d_ptr->chunkBuffer;
	chunk.append (this->d_ptr->device->read (4096));
	
	while (chunk.length () > 1 && processChunkStep ());
	
}

bool Nuria::HttpMultiPartReader::processChunkStep () {
	QByteArray &chunk = this->d_ptr->chunkBuffer;
//	chunk.append (this->d_ptr->device->read (4096));
	
	// 
	int result = 0;
	if (this->d_ptr->state == FirstLine) {
		result = processFirstLine ();
	} else if (this->d_ptr->state == Headers) {
		result = processAllHeaders ();
	} else {
		result = processContent ();
	}
	
	// 
	if (result < 0) {
		this->d_ptr->chunkBuffer.clear ();
	} else if (result > 0) {
		this->d_ptr->chunkBuffer = chunk.mid (result);
		return true;
	}
	
	return false;
}

int Nuria::HttpMultiPartReader::processFirstLine () {
	QByteArray &chunk = this->d_ptr->chunkBuffer;
	
	bool isLast = false;
	if (chunk.length () < boundingLineLength ()) {
		return 0;
	}
	
	// 
	int result = isBoundingLine (chunk, isLast, 0);
	if (result == 0) {
		setState (Error);
		return -1;
	}
	
	// 
	setState (Headers);
	return result;
	
}

int Nuria::HttpMultiPartReader::processAllHeaders () {
	int offset = 0;
	int curOffset = 0;
	
	while (this->d_ptr->state == Headers &&
	       (curOffset = processHeaders (offset)) > 0) {
		offset += curOffset;
	}
	
	// Error handling
	if (curOffset < 0) {
		return -1;
	}
	
	return offset;
}

int Nuria::HttpMultiPartReader::processHeaders (int offset) {
	QByteArray &chunk = this->d_ptr->chunkBuffer;
	int idx = chunk.indexOf ("\r\n", offset);
	
	// No end of line in sight?
	if (idx == -1) {
		return 0;
	}
	
	// Empty line?
	if (idx == offset) {
		return parseHeaders () ? 2 : -1;
	}
	
	// Read header
	int length = idx - offset;
	QByteArray line = QByteArray::fromRawData (chunk.constData () + offset, length);
	if (!processHeaderLine (line)) {
		return -1;
	}
	
	return length + 2;
}

bool Nuria::HttpMultiPartReader::processHeaderLine (const QByteArray &line) {
	HttpParser parser;
	
	// Parse
	QByteArray key;
	QByteArray value;
	if (!parser.parseHeaderLine (line, key, value)) {
		return false;
	}
	
	// Store.
	this->d_ptr->currentHeaders.insert (parser.correctHeaderKeyCase (key), value);
	return true;
}

bool Nuria::HttpMultiPartReader::parseHeaders () {
	FieldInfo info;
	
	// 
	QByteArray dispositionName = HttpClient::httpHeaderName (HttpClient::HeaderContentDisposition);
	QByteArray lengthName = HttpClient::httpHeaderName (HttpClient::HeaderContentLength);
	QByteArray typeName = HttpClient::httpHeaderName (HttpClient::HeaderContentType);
	
	// Find various headers
	QByteArray disposition = this->d_ptr->currentHeaders.value (dispositionName);
	QByteArray length = this->d_ptr->currentHeaders.value (lengthName);
	QByteArray type = this->d_ptr->currentHeaders.value (typeName);
	
	// Clear header cache
	this->d_ptr->currentHeaders.clear ();
	
	// Read name
	QString name = parseContentDispositionValue (disposition);
	if (disposition.isEmpty () || name.isEmpty ()) {
		setState (Error);
		return false;
	}
	
	// 
	info.totalLength = parseContentLength (length);
	info.mimeType = QString::fromLatin1 (type);
	info.buffer = new TemporaryBufferDevice (this);
	
	// RFC2388 Section 3
	if (info.mimeType.isEmpty ()) {
		info.mimeType = QStringLiteral("text/plain");
	}
	
	// 
	this->d_ptr->fields.insert (name, info);
	initCurrentBuffer (name, info.buffer);
	
	emit fieldFound (name);
	setState (Body);
	
	return true;
}

qint64 Nuria::HttpMultiPartReader::parseContentLength (const QByteArray &data) {
	bool ok = false;
	qint64 result = data.toLongLong (&ok);
	return (!ok || result < 1) ? -1 : result;
}

QString Nuria::HttpMultiPartReader::parseContentDispositionValue (const QByteArray &data) {
	if (!data.startsWith ("form-data;")) {
		return QString ();
	}
	
	return parseContentDispositionFormDataValue (data);
}

QString Nuria::HttpMultiPartReader::parseContentDispositionFormDataValue (const QByteArray &data) {
	int idx = data.indexOf ("name=\"");
	if (idx == -1) {
		return QString ();
	}
	
	// 
	idx += 6; // "name=\""
	int end = data.indexOf ('"', idx);
	if (end == -1) {
		return QString ();
	}
	
	// Return name
	return QString::fromLatin1 (data.mid (idx, end - idx));
}

int Nuria::HttpMultiPartReader::processContent () {
	int result = 0;
	
	// End of content?
	int idx = this->d_ptr->chunkBuffer.indexOf ("\r\n");
	if (this->d_ptr->lastWasNewline) {
		
		// Buffer more data for now if the current buffer is shorter
		// than the bounding line length.
		if (this->d_ptr->chunkBuffer.length () < boundingLineLength ()) {
			return 0;
		}
		
		// 
		result = processContentBoundary (idx);
		if (result != 0) {
			return result;
		}
		
		// No boundary line, append '\r\n' to the buffer if this is not
		// the first invocation of this method.
		if (this->d_ptr->currentBufferWritePos > 0) {
			appendToCurrentBuffer (QByteArrayLiteral("\r\n"), 2);
		}
		
	}
	
	// Read until '\r\n' (Or till the end)
	if (idx == -1) {
		this->d_ptr->lastWasNewline = false;
		
		appendToCurrentBuffer (this->d_ptr->chunkBuffer, this->d_ptr->chunkBuffer.length ());
		this->d_ptr->chunkBuffer.clear ();
		
		result = 0;
	} else {
		this->d_ptr->lastWasNewline = true;
		
		result = appendToCurrentBuffer (this->d_ptr->chunkBuffer, idx);
		result += 2;
	}
	
	// 
	return result;
}

int Nuria::HttpMultiPartReader::isBoundingLine (const QByteArray &buffer, bool &last, int offset) {
	
	// Is 'buffer' too short?
	int boundingLength = boundingLineLength ();
	if (buffer.length () - offset < boundingLength) {
		return 0;
	}
	
	// Verify bounding
	if (::memcmp (buffer.constData () + offset,
		      this->d_ptr->boundary.constData (),
		      this->d_ptr->boundary.length ())) {
		return 0;
	}
	
	// Last bounding?
	boundingLength += offset;
	if (buffer.at (boundingLength - 2) == '-' && buffer.at (boundingLength - 1) == '-') {
		last = true;
	} else if (buffer.at (boundingLength - 2) != '\r' || buffer.at (boundingLength - 1) != '\n') {
		return 0;
	}
	
	// Bounding line found!
	return boundingLength;
}

int Nuria::HttpMultiPartReader::boundingLineLength () const {
	// Length of "<boundary>\r\n"
	return this->d_ptr->boundary.length () + 2;
}

void Nuria::HttpMultiPartReader::stopListening () {
	disconnect (this->d_ptr->device, &QIODevice::readyRead, this, &HttpMultiPartReader::processData);
}

int Nuria::HttpMultiPartReader::appendToCurrentBuffer (const QByteArray &data, int length) {
	
	// Seek to write position
	qint64 readPos = this->d_ptr->currentBuffer->pos ();
	this->d_ptr->currentBuffer->seek (this->d_ptr->currentBufferWritePos);
	
	// Append
	this->d_ptr->currentBuffer->write (data.constData (), length);
	this->d_ptr->currentBufferWritePos += length;
	
	// Reset buffer position
	this->d_ptr->currentBuffer->seek (readPos);
	return length;
}

int Nuria::HttpMultiPartReader::processContentBoundary (int idxOfNewline) {
	bool last = false;
	int boundaryOffset = (idxOfNewline == 0) ? 2 : 0;
	int offset = isBoundingLine (this->d_ptr->chunkBuffer, last, boundaryOffset);
	
	if (offset > 0) {
		FieldInfo &info = this->d_ptr->fields[this->d_ptr->currentField];
		info.totalLength = info.transferred = this->d_ptr->currentBuffer->size ();
		
		emit fieldCompleted (this->d_ptr->currentField);
		setState (last ? Complete : Headers);
	}
	
	return offset;
}

void Nuria::HttpMultiPartReader::initCurrentBuffer (const QString &name, TemporaryBufferDevice *device) {
	this->d_ptr->currentField = name;
	this->d_ptr->currentBuffer = device;
	
	this->d_ptr->lastWasNewline = true;
	this->d_ptr->currentBufferWritePos = 0;
	
}
