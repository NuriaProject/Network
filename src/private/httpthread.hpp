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

#ifndef NURIA_INTERNAL_HTTPTHREAD_HPP
#define NURIA_INTERNAL_HTTPTHREAD_HPP

#include <QThread>

namespace Nuria {
class HttpServer;

namespace Internal {

class HttpThread : public QThread {
	Q_OBJECT
public:
	
	explicit HttpThread (HttpServer *server = 0);
	~HttpThread () override;
	
	void incrementRunning ();
	void transportDestroyed ();
	
public slots:
	
	// Waits for the currently processed request to be completed and
	// deletes the thread itself afterwards.
	void stopGraceful ();
	
private:
	QAtomicInt m_running;
	QAtomicInt m_stop;
	HttpServer *m_server;
	
};

}
}

#endif // NURIA_INTERNAL_HTTPTHREAD_HPP
