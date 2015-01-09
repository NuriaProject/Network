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

#include "nuria/httppostbodyreader.hpp"

Nuria::HttpPostBodyReader::HttpPostBodyReader (QObject *parent)
	: QObject (parent)
{
	
}

Nuria::HttpPostBodyReader::~HttpPostBodyReader () {
	
}

bool Nuria::HttpPostBodyReader::hasField (const QString &field) const {
	return fieldNames ().contains (field);
}

bool Nuria::HttpPostBodyReader::isFieldComplete (const QString &field) const {
	if (!hasField (field)) {
		return false;
	}
	
	return (fieldLength (field) == fieldBytesTransferred (field));
}

QByteArray Nuria::HttpPostBodyReader::fieldValue (const QString &field) {
	QIODevice *device = fieldStream (field);
	if (!isFieldComplete (field) || !device) {
		return QByteArray ();
	}
	
	// 
	return device->readAll ();
}
