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

#ifndef NURIA_INTERNAL_HTTPTHREAD_HPP
#define NURIA_INTERNAL_HTTPTHREAD_HPP

#include "../nuria/httptransport.hpp"
#include <QThread>

namespace Nuria {
class HttpTransport;
class HttpServer;

namespace Internal {

class HttpThread : public QThread {
	Q_OBJECT
public:
	
	explicit HttpThread (HttpServer *server = 0);
	~HttpThread () override;
	
	void incrementRunning (HttpTransport *transport);
	void transportDestroyed ();
	
public slots:
	
	// Waits for the currently processed request to be completed and
	// deletes the thread itself afterwards.
	void stopGraceful ();
	
private slots:
	void forwardTimeout (Nuria::HttpTransport::Timeout mode);
	
private:
	QAtomicInt m_running;
	QAtomicInt m_stop;
	HttpServer *m_server;
	
};

}
}

#endif // NURIA_INTERNAL_HTTPTHREAD_HPP
