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

#include "nuria/httpfilter.hpp"


Nuria::HttpFilter::HttpFilter (QObject *parent)
        : QObject (parent)
{
	
}

Nuria::HttpFilter::~HttpFilter () {
	// Nothing.
}

QByteArray Nuria::HttpFilter::filterName () const {
	return QByteArray ();
}

bool Nuria::HttpFilter::filterHeaders (HttpClient *client, HttpClient::HeaderMap &headers) {
	Q_UNUSED(client)
	Q_UNUSED(headers)
	return true;
}

QByteArray Nuria::HttpFilter::filterBegin (HttpClient *client) {
	Q_UNUSED(client)
	return QByteArray ();
}

bool Nuria::HttpFilter::filterData (HttpClient *client, QByteArray &data) {
	Q_UNUSED(client)
	Q_UNUSED(data)
	return true;
}

QByteArray Nuria::HttpFilter::filterEnd (HttpClient *client) {
	Q_UNUSED(client)
	return QByteArray ();
}
