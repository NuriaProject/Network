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
