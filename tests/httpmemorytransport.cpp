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

#include "httpmemorytransport.hpp"

Nuria::HttpMemoryTransport::HttpMemoryTransport (HttpServer *server)
	: HttpTransport (new TestBackend (server), server)
{
	this->testBackend = static_cast< TestBackend * > (backend ());
	
	setCurrentRequestCount (1);
	setMaxRequests (1);
}

bool Nuria::HttpMemoryTransport::isOpen () const {
	return true;
}

void Nuria::HttpMemoryTransport::forceClose () {
	qDebug("forceClose()");
}

void Nuria::HttpMemoryTransport::close (HttpClient *client) {
	Q_UNUSED(client)
	qDebug("close()");
}

bool Nuria::HttpMemoryTransport::sendToRemote (HttpClient *client, const QByteArray &data) {
	Q_UNUSED(client)
	this->outData.append (data);
	bytesSent (client, data.length ());
	return true;
}
