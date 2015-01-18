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

#ifndef NURIA_INTERNAL_WEBSOCKETREADER_HPP
#define NURIA_INTERNAL_WEBSOCKETREADER_HPP

#include "../nuria/httpclient.hpp"
#include "websocketstructures.hpp"

namespace Nuria {
namespace Internal {

class WebSocketReader {
        WebSocketReader () = delete;
public:
        
	// The payload must not exceed 64MiB.
	enum { PayloadHardLimit = 64 << 20 };
	
	static bool areHttpRequirementsFulfilled (HttpClient::HttpVersion version, HttpClient::HttpVerb verb);
        static bool isWebSocketRequest (const HttpClient::HeaderMap &headers);
        static QByteArray generateHandshakeKey (QByteArray requestKey);
	
	static int sizeOfFrame (WebSocketFrame frame);
	static bool readFrameData (const QByteArray &data, WebSocketFrame &frame, int pos = 0);
	static qint64 payloadLength (WebSocketFrame frame);
	
	static bool isLegalClientPacket (WebSocketFrame frame);
	static void maskPayload (uint32_t mask, QByteArray &payload);
	
	static bool readClose (const QByteArray &payload, int &code, QByteArray &message);
	static bool isLegalCloseCode (int code);
	
};

}
}

#endif // NURIA_INTERNAL_WEBSOCKETREADER_HPP
