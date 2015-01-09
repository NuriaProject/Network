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

#include "localserver.hpp"

#include <QLocalSocket>

Nuria::Internal::LocalServer::LocalServer (QObject *parent)
	: QLocalServer (parent)
{
	// 
}

Nuria::Internal::LocalServer::~LocalServer () {
	// 
}

QLocalSocket *Nuria::Internal::LocalServer::handleToSocket (qintptr handle) {
	QLocalSocket *socket = new QLocalSocket;
	socket->setSocketDescriptor (handle);
	return socket;
	
}

void Nuria::Internal::LocalServer::incomingConnection (quintptr handle) {
	emit connectionRequested (handle);
}
