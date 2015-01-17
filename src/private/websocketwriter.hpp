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

#ifndef NURIA_INTERNAL_WEBSOCKETWRITER_HPP
#define NURIA_INTERNAL_WEBSOCKETWRITER_HPP

#include "websocketstructures.hpp"
#include <QIODevice>

namespace Nuria {
namespace Internal {

class WebSocketWriter {
	WebSocketWriter () = delete;
public:
	
	/**
	 * Serializes \a frame into a byte array.
	 * For size, only  extPayloadLen in \a frame is used.
	 * The mask is ignored.
	 */
	static QByteArray serializeFrame (WebSocketFrame frame);
	
	/**
	 * Constructs and sends a WebSocket frame with frame data set to
	 * \a fin, \a opcode, and appends \a data of \a len, writing everything
	 * into \a device. No check is done if \a opcode is a reserved one or
	 * not.
	 */
	static void sendToClient (QIODevice *device, bool fin, WebSocketOpcode opcode, const char *data, int len);
	
	/**
	 * Creates the payload for a Close WebSocket packet. If \c code is
	 * \c -1, the resulting byte array is empty.
	 */
	static QByteArray createClosePayload (int code, const QByteArray &message);
	
};

} // namespace Internal
} // namespace Nuria

#endif // NURIA_INTERNAL_WEBSOCKETWRITER_HPP
