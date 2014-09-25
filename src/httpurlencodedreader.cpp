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

#include "nuria/httpurlencodedreader.hpp"

#include "nuria/httpclient.hpp"
#include "nuria/httpparser.hpp"
#include <nuria/debug.hpp>
#include <QBuffer>
#include <QMap>

enum State {
	Key,
	Value,
	Complete,
	Error
};

namespace Nuria {
class HttpUrlEncodedReaderPrivate {
public:
	
	QByteArray mimeType;
	QIODevice *device;
	
	QMap< QString, QByteArray > fields;
	QMap< QString, QBuffer * > buffers;
	
	QString currentName;
	QByteArray chunkBuffer;
	State state = Key;
	
};
}

Nuria::HttpUrlEncodedReader::HttpUrlEncodedReader (QIODevice *device, const QByteArray &charset, QObject *parent)
	: HttpPostBodyReader (parent), d_ptr (new HttpUrlEncodedReaderPrivate)
{
	
	this->d_ptr->device = device;
	this->d_ptr->mimeType = QByteArrayLiteral ("text/plain; charset=") + charset;
	
	// 
	connect (device, &QIODevice::readyRead, this, &HttpUrlEncodedReader::processData);
	if (device->bytesAvailable () > 0) {
		processData ();
	}
	
}

Nuria::HttpUrlEncodedReader::~HttpUrlEncodedReader() {
	delete this->d_ptr;
}

bool Nuria::HttpUrlEncodedReader::isComplete () const {
	return (this->d_ptr->state >= Complete);
}

bool Nuria::HttpUrlEncodedReader::hasFailed () const {
	return (this->d_ptr->state == Error);
}

bool Nuria::HttpUrlEncodedReader::hasField (const QString &field) const {
	return this->d_ptr->fields.contains (field);
}

QStringList Nuria::HttpUrlEncodedReader::fieldNames () const {
	return this->d_ptr->fields.keys ();
}

QString Nuria::HttpUrlEncodedReader::fieldMimeType (const QString &) const {
	return this->d_ptr->mimeType;
}

qint64 Nuria::HttpUrlEncodedReader::fieldLength (const QString &field) const {
	return this->d_ptr->fields.value (field).length ();
}

qint64 Nuria::HttpUrlEncodedReader::fieldBytesTransferred (const QString &field) const {
	return this->d_ptr->fields.value (field).length ();
}

bool Nuria::HttpUrlEncodedReader::isFieldComplete (const QString &field) const {
	return this->d_ptr->fields.contains (field);
}

QIODevice *Nuria::HttpUrlEncodedReader::fieldStream (const QString &field) {
	QBuffer *device = this->d_ptr->buffers.value (field);
	if (!device && this->d_ptr->fields.contains (field)) {
		device = new QBuffer (this);
		device->setData (this->d_ptr->fields.value (field));
		device->open (QIODevice::ReadOnly);
		
		this->d_ptr->buffers.insert (field, device);
		
	}
	
	return device;
}

QByteArray Nuria::HttpUrlEncodedReader::fieldValue (const QString &field) {
	return this->d_ptr->fields.value (field);
}

void Nuria::HttpUrlEncodedReader::processData () {
	if (this->d_ptr->state >= Complete) {
		return;
	}
	
	// 
	do {
		processChunk ();
	} while (this->d_ptr->device->bytesAvailable () > 0);
	
	// Premature end?
	if (this->d_ptr->state != Complete && this->d_ptr->device->atEnd ()) {
		this->d_ptr->state = Error;
		emit completed (false);
	}
	
}

void Nuria::HttpUrlEncodedReader::processChunk () {
	this->d_ptr->chunkBuffer.append (this->d_ptr->device->read (4096));
	while (this->d_ptr->state < Complete && processChunkStep ());
	
}

bool Nuria::HttpUrlEncodedReader::processChunkStep () {
	State curState = this->d_ptr->state;
	
	switch (this->d_ptr->state) {
	case Key:
		processKey ();
		break;
	case Value:
		this->d_ptr->state = State (processValue ());
		break;
	default: ;
	}
	
	// Clean up when we're finished.
	if (this->d_ptr->state >= Complete) {
		this->d_ptr->currentName.clear ();
		this->d_ptr->chunkBuffer.clear ();
		emit completed (this->d_ptr->state == Complete);
		stopListening ();
	}
	
	// Wait for more data when the state hasn't changed.
	return (curState != this->d_ptr->state);
}

void Nuria::HttpUrlEncodedReader::processKey () {
	QByteArray &chunk = this->d_ptr->chunkBuffer;
	
	int idx = chunk.indexOf ('=');
	if (idx != -1) {
		QByteArray rawName = QByteArray::fromPercentEncoding (chunk.left (idx));
		this->d_ptr->currentName = QString::fromLatin1 (rawName);
		this->d_ptr->chunkBuffer = chunk.mid (idx + 1); // Skip '='
		
		this->d_ptr->state = Value;
		emit fieldFound (this->d_ptr->currentName);
	}
	
}

int Nuria::HttpUrlEncodedReader::processValue () {
	QByteArray &chunk = this->d_ptr->chunkBuffer;
	
	int idx = chunk.indexOf ('&');
	if (idx != -1 || this->d_ptr->device->atEnd ()) {
		QByteArray unparsedValue = (idx < 0) ? chunk : chunk.left (idx);
		QByteArray rawValue = QByteArray::fromPercentEncoding (unparsedValue);
		
		// Store
		this->d_ptr->fields.insert (this->d_ptr->currentName, rawValue);
		emit fieldCompleted (this->d_ptr->currentName);
		
		// Parsing completed?
		if (idx == -1) {
			return Complete;
		}
		
		// Looking for the next key
		this->d_ptr->chunkBuffer = chunk.mid (idx + 1); // Skip '&'
		return Key;
		
	}
	
	// Still value.
	return Value;
	
}

void Nuria::HttpUrlEncodedReader::stopListening () {
	disconnect (this->d_ptr->device, &QIODevice::readyRead, this, &HttpUrlEncodedReader::processData);
}
