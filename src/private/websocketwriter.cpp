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

#include "websocketwriter.hpp"

#include "websocketreader.hpp"
#include <QtEndian>

QByteArray Nuria::Internal::WebSocketWriter::serializeFrame (WebSocketFrame frame) {
	QByteArray data (sizeof(uint16_t) + sizeof(uint64_t), Qt::Uninitialized);
	int len = sizeof(uint16_t);
	
	// Set length
	if (frame.extPayloadLen > 0xFFFF) {
		frame.base.payloadLen = PayloadLengthMagicNumbers::Length64Bit;
		uint64_t *n = reinterpret_cast< uint64_t * > (data.data () + sizeof(uint16_t));
		*n = qToBigEndian (quint64 (frame.extPayloadLen));
		len += sizeof(*n);
	} else if (frame.extPayloadLen >= PayloadLengthMagicNumbers::Length16Bit) {
		frame.base.payloadLen = PayloadLengthMagicNumbers::Length16Bit;
		uint16_t *n = reinterpret_cast< uint16_t * > (data.data () + sizeof(uint16_t));
		*n = qToBigEndian (quint16 (frame.extPayloadLen));
		len += sizeof(*n);
	} else {
		frame.base.payloadLen = frame.extPayloadLen;
	}
	
	// Build bitmap. Akward but portable.
	WebSocketBaseFrame b = frame.base;
	uint8_t *f = reinterpret_cast< uint8_t * > (data.data ());
	f[0] = uint8_t (b.fin << 7 | b.rsv1 << 6 | b.rsv2 << 5 | b.rsv3 << 4 | b.opcode);
	f[1] = uint8_t (b.mask << 7 | b.payloadLen);
	
	// Done
	data.resize (len);
	return data;
}

void Nuria::Internal::WebSocketWriter::sendToClient (QIODevice *device, bool fin, WebSocketOpcode opcode,
                                                     const char *data, int len) {
	WebSocketFrame frame { { fin, 0, 0, 0, opcode, 0, 0 }, uint64_t (len), 0 };
	device->write (serializeFrame (frame));
	device->write (data, len);
}

QByteArray Nuria::Internal::WebSocketWriter::createClosePayload (int code, const QByteArray &message) {
	if (code < 0) {
		return QByteArray ();
	}
	
	// Create body
	QByteArray body (sizeof(uint16_t) + message.length (), Qt::Uninitialized);
	*reinterpret_cast< uint16_t * > (body.data ()) = qToBigEndian (quint16 (code));
	memcpy (body.data () + sizeof(uint16_t), message.constData (), message.length ());
	
	// Done
	return body;
	
}
