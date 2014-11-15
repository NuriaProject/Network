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

#ifndef NURIA_INTERNAL_FASTCGIWRITER_HPP
#define NURIA_INTERNAL_FASTCGIWRITER_HPP

#include "fastcgistructures.hpp"
#include <QByteArray>
#include <QMap>
class QIODevice;

namespace Nuria {
namespace Internal {

/**
 * \brief Internal writer for the FastCGI protocol.
 */
class FastCgiWriter {
public:
	
	static void writeNameValuePair (QByteArray &result, const QByteArray &name, const QByteArray &value);
	static QByteArray valueMapToBody (const NameValueMap &values);
	static void writeGetValuesResult (QIODevice *device, const NameValueMap &values);
	static void writeUnknownTypeResult (QIODevice *device, FastCgiType type);
	static void writeEndRequest (QIODevice *device, uint16_t requestId, uint32_t status,
	                             FastCgiProtocolStatus protoStatus);
	
	static bool writeStreamMessage (QIODevice *device, FastCgiType type,
	                                uint16_t requestId, const QByteArray &body);
	static void writeMultiPartStream (QIODevice *device, FastCgiType type,
	                                  uint16_t requestId, const QByteArray &body);
	
};

}
}

#endif // NURIA_INTERNAL_FASTCGIWRITER_HPP
