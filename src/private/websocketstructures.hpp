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

#ifndef NURIA_INTERNAL_WEBSOCKETSTRUCTURES_HPP
#define NURIA_INTERNAL_WEBSOCKETSTRUCTURES_HPP

#include <qcompilerdetection.h>

namespace Nuria {
namespace Internal {

enum WebSocketOpcode {
	ControlFrameMask = 0x8, // MSB is 1 for control frames
	
	ContinuationFrame = 0x0,
	TextFrame = 0x1,
	BinaryFrame = 0x2,
	Reserved3 = 0x3,
	Reserved4 = 0x4,
	Reserved5 = 0x5,
	Reserved6 = 0x6,
	Reserved7 = 0x7,
	ConnectionClose = 0x8,
	Ping = 0x9,
	Pong = 0xA,
	ReservedB = 0xB,
	ReservedC = 0xC,
	ReservedD = 0xD,
	ReservedE = 0xE,
	ReservedF = 0xF
};

enum PayloadLengthMagicNumbers {
	Length16Bit = 126,
	Length64Bit = 127
};

// Based on RFC 6455 Section 5.2
struct WebSocketBaseFrame {
        uint8_t fin:1;
	uint8_t rsv1:1;
	uint8_t rsv2:1;
	uint8_t rsv3:1;
	uint8_t opcode:4; // WebSocketOpcode
	
	uint8_t mask:1;
	uint8_t payloadLen:7; // PayloadLengthMagicNumbers
} Q_PACKED;

// 
struct WebSocketFrame {
	WebSocketBaseFrame base;
	uint64_t extPayloadLen;
	uint32_t maskKey;
	
};

}
}

#endif // NURIA_INTERNAL_WEBSOCKETSTRUCTURES_HPP

