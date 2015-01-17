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

#include "websocketreader.hpp"

#include <QCryptographicHash>
#include <QtEndian>

bool Nuria::Internal::WebSocketReader::areHttpRequirementsFulfilled (HttpClient::HttpVersion version,
                                                                     HttpClient::HttpVerb verb) {
	return (version >= HttpClient::Http1_1 && verb == HttpClient::GET);
}

bool Nuria::Internal::WebSocketReader::isWebSocketRequest (const HttpClient::HeaderMap &headers) {
	
	// Upgrade: websocket
	QByteArray upgrade = headers.value (HttpClient::httpHeaderName (HttpClient::HeaderUpgrade));
	if (!upgrade.toLower ().contains ("websocket")) {
		return false;
	}
	
	// Connection: Upgrade
	QByteArray connection = headers.value (HttpClient::httpHeaderName (HttpClient::HeaderConnection));
	if (!connection.toLower ().contains ("upgrade")) {
		return false;
	}
	
	// Sec-WebSocket-Version: 13
	QByteArray version = headers.value (HttpClient::httpHeaderName (HttpClient::HeaderSecWebSocketVersion));
	if (version != "13") {
		return false;
	}
	
	// Check that Sec-WebSocket-Key isn't empty
	QByteArray key = headers.value (HttpClient::httpHeaderName (HttpClient::HeaderSecWebSocketKey));
	return !key.isEmpty ();
}

QByteArray Nuria::Internal::WebSocketReader::generateHandshakeKey (QByteArray requestKey) {
	requestKey.append ("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"); // Magic UUID
	return QCryptographicHash::hash (requestKey, QCryptographicHash::Sha1).toBase64 ();
}

int Nuria::Internal::WebSocketReader::sizeOfFrame (WebSocketFrame frame) {
	int size = sizeof(frame.base);
	
	// Add size of extra payload length
	if (frame.base.payloadLen == PayloadLengthMagicNumbers::Length16Bit) {
		size += sizeof(uint16_t);
	} else if (frame.base.payloadLen == PayloadLengthMagicNumbers::Length64Bit) {
		size += sizeof(uint64_t);
	}
	
	// Add size of mask
	if (frame.base.mask) {
		size += sizeof(uint32_t);
	}
	
	return size;
}

static Nuria::Internal::WebSocketBaseFrame readBaseFrame (uint16_t data) {
	Nuria::Internal::WebSocketBaseFrame f;
	uint8_t *d = reinterpret_cast< uint8_t * > (&data);
	
	f.fin = !!(d[0] & (1 << 7));
	f.rsv1 = !!(d[0] & (1 << 6));
	f.rsv2 = !!(d[0] & (1 << 5));
	f.rsv3 = !!(d[0] & (1 << 4));
	f.opcode = d[0] & 0x0F;
	f.mask = !!(d[1] & (1 << 7));
	f.payloadLen = d[1] & (~0x80);
	
	return f;
}

bool Nuria::Internal::WebSocketReader::readFrameData (const QByteArray &data, WebSocketFrame &frame) {
	qint64 avail = data.length ();
	
	// 
	if (avail < sizeof(frame.base)) {
		return false;
	}
	
	// Read frame data
	frame.base = readBaseFrame (*reinterpret_cast< const uint16_t * > (data.constData ()));
	frame.extPayloadLen = 0;
	frame.maskKey = 0;
	
	int offset = sizeof(frame.base);
	
	// Read extended payload length if needed
	if (frame.base.payloadLen == PayloadLengthMagicNumbers::Length16Bit) {
		if (avail < offset + sizeof(uint16_t)) {
			return false;
		}
		
		// 
		const quint16 *ptr = reinterpret_cast< const quint16 * > (data.constData () + offset);
		frame.extPayloadLen = qFromBigEndian (*ptr);
		offset += sizeof(uint16_t);
	} else if (frame.base.payloadLen == PayloadLengthMagicNumbers::Length64Bit) {
		if (avail < offset + sizeof(uint64_t)) {
			return false;
		}
		
		// 
		const quint64 *ptr = reinterpret_cast< const quint64 * > (data.constData () + offset);
		frame.extPayloadLen = qFromBigEndian (*ptr);
		offset += sizeof(uint64_t);
	} else {
		frame.extPayloadLen = frame.base.payloadLen;
	}
	
	// Read mask key
	if (frame.base.mask) {
		if (avail < offset + sizeof(uint32_t)) {
			return false;
		}
		
		// 
		const quint32 *ptr = reinterpret_cast< const quint32 * > (data.constData () + offset);
		frame.maskKey = *ptr; // No endian conversion, as it's actually a bytearray!
	}
	
	// Done.
	return true;
}

qint64 Nuria::Internal::WebSocketReader::payloadLength (WebSocketFrame frame) {
	if (frame.base.payloadLen >= PayloadLengthMagicNumbers::Length16Bit) {
		return frame.extPayloadLen;
	}
	
	return frame.base.payloadLen;
}

bool Nuria::Internal::WebSocketReader::isLegalClientPacket (WebSocketFrame frame) {
	// The RSVx fields must be 0.
	if (frame.base.rsv1 || frame.base.rsv2 || frame.base.rsv3) {
		return false;
	}
	
	// Check for useless extended payload length. Also check the hard limit.
	if ((frame.base.payloadLen == PayloadLengthMagicNumbers::Length16Bit ||
	    frame.base.payloadLen == PayloadLengthMagicNumbers::Length64Bit) &&
	    (frame.extPayloadLen < PayloadLengthMagicNumbers::Length16Bit ||
	     frame.extPayloadLen > PayloadHardLimit)) {
		return false;
	}
	
	// The opcode isn't a reserved one
	if ((frame.base.opcode >= WebSocketOpcode::Reserved3 && frame.base.opcode <= WebSocketOpcode::Reserved7) ||
	    (frame.base.opcode >= WebSocketOpcode::ReservedB && frame.base.opcode <= WebSocketOpcode::ReservedF)) {
		return false;
	}
	
	// Control frames may only have a payload length of 125 or less and must not be fragmented
	if ((frame.base.opcode & WebSocketOpcode::ControlFrameMask) &&
	    (frame.base.payloadLen > 125 || !frame.base.fin)) {
		return false;
	}
	
	// The frame must be masked (for client -> server) if it contains payload
	if (!frame.base.mask && frame.base.payloadLen > 0) {
		return false;
	}
	
	// Legal frame
	return true;
}

// Generic implementation of the masking algorithm used in WebSockets.
template< typename T >
static void maskData (T mask, QByteArray &payload) {
	
	// Make sure payload's buffer is a multiple of 4 in length
	int origLen = payload.length ();
	if (origLen % sizeof(T) > 0) {
		payload.reserve (origLen + sizeof(T) - (origLen % sizeof(T)));
	}
	
	// Iterate over the payload, XORing each byte with the 'mask'.
	T *ptr = reinterpret_cast< T * > (payload.data ());
	for (int pos = 0; pos < origLen; pos += sizeof(T), ptr++) {
		*ptr ^= mask;
	}
	
}

void Nuria::Internal::WebSocketReader::maskPayload (uint32_t mask, QByteArray &payload) {
#if QT_POINTER_SIZE == 8 // 64Bit
	return maskData< uint64_t > ((uint64_t (mask) << 32) | mask, payload);
#else // 32Bit
	return maskData< uint32_t > (mask, payload);
#endif
}

bool Nuria::Internal::WebSocketReader::readClose (const QByteArray &payload, int &code, QByteArray &message) {
	if (payload.isEmpty ()) {
		code = -1;
		return true;
	}
	
	// Fail if the payload is too small
	if (payload.length () < sizeof(uint16_t)) {
		return false;
	}
	
	// Read error code
	code = qFromBigEndian (*reinterpret_cast< const quint16 * > (payload.constBegin ()));
	
	// Read message being the remainder of the payload
	message = payload.mid (sizeof(uint16_t));
	
	// 
	return true;
}

bool Nuria::Internal::WebSocketReader::isLegalCloseCode (int code) {
	if (code < 0) { // code = -1 as special case
		return true;
	}
	
	// Reserved codes
	if (code >= 1004 && code <= 1006) {
		return false;
	}
	
	// Codes that must be registered with the IANA
	if (code < 1000 || (code > 1011 && code < 3000)) {
		return false;
	}
	
	// All codes greater than the private section (4000-4999) are invalid.
	if (code > 4999) {
		return false;
	}
	
	// Valid.
	return true;
}
