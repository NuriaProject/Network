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

#include "httpthread.hpp"

#include "../nuria/httpserver.hpp"
#include <nuria/logger.hpp>

Nuria::Internal::HttpThread::HttpThread (HttpServer *server)
        : QThread (server), m_server (server)
{
	
}

Nuria::Internal::HttpThread::~HttpThread () {
	if (this->m_running.load () != 0) {
		nError() << "Destroying thread with running HttpTransports!";
	}
	
}

void Nuria::Internal::HttpThread::incrementRunning (HttpTransport *transport) {
	this->m_running.fetchAndAddRelaxed (1);
	
	connect (transport, SIGNAL(connectionTimedout(Nuria::HttpTransport::Timeout)),
	         this, SLOT(forwardTimeout(Nuria::HttpTransport::Timeout)));
	
}

void Nuria::Internal::HttpThread::transportDestroyed () {
	if (!this->m_running.deref () && this->m_stop.load () == 1) {
		exit ();
		deleteLater ();
	}
	
}

void Nuria::Internal::HttpThread::stopGraceful () {
	this->m_stop.store (1);
	if (this->m_running.load () == 0) {
		deleteLater ();
	}
	
}

void Nuria::Internal::HttpThread::forwardTimeout (Nuria::HttpTransport::Timeout mode) {
	HttpTransport *transport = qobject_cast< HttpTransport * > (sender ());
	if (transport) {
		emit this->m_server->connectionTimedout (transport, mode);
	}
	
}
