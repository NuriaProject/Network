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

#include "fastcgiwriter.hpp"

#include <QByteArray>
#include <QIODevice>

// Convenience write methods
static inline void write (QIODevice *device, const QByteArray &data) {
	device->write (data);
}

template< typename T >
static inline void write (QIODevice *device, const T &t) {
	device->write (reinterpret_cast< const char * > (&t), sizeof(T));
}

template< typename T >
static inline void write (QByteArray &data, const T &t) {
	data.append (reinterpret_cast< const char * > (&t), sizeof(T));
}

// 
static inline void writeNameValueLength (QByteArray &data, int len) {
	if (len <= NameValueCharLimit) {
	        write (data, uint8_t (len));
	} else {
	        write (data, SWAPPED(uint32_t (len) | (1 << 31)));
	}
	
}

void Nuria::Internal::FastCgiWriter::writeNameValuePair (QByteArray &result, const QByteArray &name,
                                                         const QByteArray &value) {
	writeNameValueLength (result, name.length ());
	writeNameValueLength (result, value.length ());
	result.append (name);
	result.append (value);
}

QByteArray Nuria::Internal::FastCgiWriter::valueMapToBody (const NameValueMap &values) {
	QByteArray data;
	
	for (auto it = values.begin (), end = values.end (); it != end; ++it) {
		writeNameValuePair (data, it.key (), it.value ());
	}
	
	return data;
}

void Nuria::Internal::FastCgiWriter::writeGetValuesResult (QIODevice *device, const NameValueMap &values) {
	FastCgiRecord record;
	QByteArray body = valueMapToBody (values);
	
	record.version = FastCgiRecord::Version;
	record.type = FastCgiType::GetValuesResult;
	record.reserved = 0x0;
	record.requestId = 0;
	record.paddingLength = 0;
	record.contentLength = SWAPPED(uint16_t (body.length ()));
	
	write (device, record);
	write (device, body);
}

void Nuria::Internal::FastCgiWriter::writeUnknownTypeResult (QIODevice *device, FastCgiType type) {
	FastCgiRecord record;
	FastCgiUnknownTypeBody body;
	
	record.version = FastCgiRecord::Version;
	record.type = FastCgiType::UnknownType;
	record.reserved = 0x0;
	record.requestId = 0;
	record.paddingLength = 0;
	record.contentLength = SWAPPED(uint16_t (sizeof(body)));
	
	body.type = type;
	memset (body.reserved, 0x0, sizeof(body.reserved));
	
	write (device, record);
	write (device, body);
}


void Nuria::Internal::FastCgiWriter::writeEndRequest (QIODevice *device, uint16_t requestId, uint32_t status,
                                                      FastCgiProtocolStatus protoStatus) {
	FastCgiRecord record;
        FastCgiEndRequestBody body;
        
        record.version = FastCgiRecord::Version;
        record.type = FastCgiType::EndRequest;
        record.reserved = 0x0;
        record.requestId = SWAPPED(requestId);
        record.paddingLength = 0;
        record.contentLength = SWAPPED(uint16_t (sizeof(body)));
	body.appStatus = SWAPPED(status);
	body.protocolStatus = protoStatus;
	memset (body.reserved, 0x0, sizeof(body.reserved));
	
	write (device, record);
	write (device, body);
}

bool Nuria::Internal::FastCgiWriter::writeStreamMessage (QIODevice *device, FastCgiType type,
                                                         uint16_t requestId, const QByteArray &body) {
	if (body.length () > 0xFFFF) {
		return false;
	}
	
	// 
	FastCgiRecord record;
	record.version = FastCgiRecord::Version;
        record.type = type;
        record.reserved = 0x0;
        record.requestId = SWAPPED(requestId);
        record.paddingLength = 0;
        record.contentLength = SWAPPED(uint16_t (body.length ()));
	
	write (device, record);
	write (device, body);
	return true;
}

void Nuria::Internal::FastCgiWriter::writeMultiPartStream (QIODevice *device, FastCgiType type,
                                                           uint16_t requestId, const QByteArray &body) {
	enum { ChunkSize = 0xFFFF };
	int offset = 0;
	int len = body.length ();
	const char *raw = body.constData ();
	
	for (; offset < len; offset += ChunkSize) {
		int size = std::min (len - offset, int (ChunkSize));
		writeStreamMessage (device, type, requestId, QByteArray::fromRawData (raw + offset, size));
	}
	
}
