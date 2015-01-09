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
