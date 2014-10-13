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

#include "httpthread.hpp"

#include "../nuria/httpserver.hpp"
#include <nuria/debug.hpp>

Nuria::Internal::HttpThread::HttpThread (HttpServer *server)
        : QThread (server), m_server (server)
{
	
}

Nuria::Internal::HttpThread::~HttpThread () {
	if (this->m_running.load () != 0) {
		nError() << "Destroying thread with running HttpTransports!";
	}
	
}

void Nuria::Internal::HttpThread::incrementRunning () {
	this->m_running.fetchAndAddRelaxed (1);
}

void Nuria::Internal::HttpThread::transportDestroyed () {
	this->m_running.fetchAndSubRelaxed (1);
	
	if (this->m_stop.load () == 1) {
		deleteLater ();
	}
}

void Nuria::Internal::HttpThread::stopGraceful () {
	this->m_stop.store (1);
	if (this->m_running.load () == 0) {
		deleteLater ();
	}
	
}
