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

#include "httpmemorytransport.hpp"

Nuria::HttpMemoryTransport::HttpMemoryTransport (QObject *parent)
	: HttpTransport (parent)
{
	
	this->ingoing = new QBuffer (this);
	this->outgoing = new QBuffer (this);
	
	this->ingoing->open (ReadWrite);
	this->outgoing->open (ReadWrite);
	setOpenMode (ReadWrite);
	
}

void Nuria::HttpMemoryTransport::close () {
	setOpenMode (NotOpen);
	emit aboutToClose ();
}

qint64 Nuria::HttpMemoryTransport::pos () const {
	return this->ingoing->pos ();
}

qint64 Nuria::HttpMemoryTransport::size () const {
	return this->ingoing->size ();
}

bool Nuria::HttpMemoryTransport::seek (qint64 pos) {
	return this->ingoing->seek (pos);
}

bool Nuria::HttpMemoryTransport::atEnd () const {
	return this->ingoing->atEnd ();
}

qint64 Nuria::HttpMemoryTransport::bytesAvailable () const {
	return this->ingoing->bytesAvailable ();
}

qint64 Nuria::HttpMemoryTransport::readData (char *data, qint64 maxlen) {
	return this->ingoing->read (data, maxlen);
}

qint64 Nuria::HttpMemoryTransport::writeData (const char *data, qint64 len) {
	QIODevice::seek (0);
	return this->outgoing->write (data, len);
}

bool Nuria::HttpMemoryTransport::reset () {
	return this->ingoing->reset ();
}

bool Nuria::HttpMemoryTransport::canReadLine () const {
	return this->ingoing->canReadLine ();
}

qint64 Nuria::HttpMemoryTransport::readLineData (char *data, qint64 maxlen) {
	return this->ingoing->readLine (data, maxlen);
}
