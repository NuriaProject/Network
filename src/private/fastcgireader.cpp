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

#include "fastcgireader.hpp"

#include <QIODevice>

bool Nuria::Internal::FastCgiReader::readRecord (QIODevice *device, FastCgiRecord &record) {
	QByteArray headerData = device->peek (sizeof(FastCgiRecord));
	if (headerData.length () != sizeof(FastCgiRecord)) {
		return false;
	}
	
	// 
	record = *reinterpret_cast< const FastCgiRecord * > (headerData.constData ());
	SWAP_BYTES(record.contentLength);
	SWAP_BYTES(record.requestId);
	
	// 
	return true;
	
}

static int readVariableLength (const QByteArray &source, int &offset) {
	if (source.length () - offset < int (sizeof(uint8_t))) {
		return -1;
	}
	
	// Short?
	uint8_t shortLength = *reinterpret_cast< const uint8_t * > (source.constData () + offset);
	if (shortLength <= NameValueCharLimit) {
		offset++;
		return shortLength;
	}
	
	// Long variable
	if (source.length () - offset < sizeof(uint32_t)) {
		return -1;
	}
	
	uint32_t longLength = *reinterpret_cast< const uint32_t * > (source.constData () + offset);
	offset += sizeof(uint32_t);
	return int (SWAPPED(longLength) & 0x7FFFFFFF);
}

bool Nuria::Internal::FastCgiReader::readNameValuePair (const QByteArray &source, QByteArray &name,
                                                        QByteArray &value, int &offset) {
	int nameLen = readVariableLength (source, offset);
	int valueLen = readVariableLength (source, offset);
	
	// Verify
	if (nameLen < 0 || valueLen < 0 || source.length () - offset < (nameLen + valueLen)) {
		return false;
	}
	
	// Read.
	name = source.mid (offset, nameLen);
	value = source.mid (offset + nameLen, valueLen);
	offset += nameLen + valueLen;
	
	return true;
}

bool Nuria::Internal::FastCgiReader::readAllNameValuePairs (const QByteArray &source,
                                                            NameValueMap &values) {
	int offset = 0;
	QByteArray name;
	QByteArray value;
	
	while (offset != source.length () && readNameValuePair (source, name, value, offset)) {
		values.insert (name, value);
	}
	
	// Used all bytes?
	return (offset == source.length ());
}

template< typename T >
static bool genericReader (const QByteArray &source, T &body) {
	if (source.length () < sizeof(T)) {
		return false;
	}
	
	// 
	body = *reinterpret_cast< const T * > (source.constData ());
	return true;
}

bool Nuria::Internal::FastCgiReader::readBeginRequestBody (const QByteArray &source, FastCgiBeginRequestBody &body) {
	bool r = genericReader (source, body);
	body.role = FastCgiRole (SWAPPED(uint16_t (body.role)));
	return r;
}
