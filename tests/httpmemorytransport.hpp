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

#ifndef NURIA_HTTPMEMORYTRANSPORT_HPP
#define NURIA_HTTPMEMORYTRANSPORT_HPP

#include <nuria/httptransport.hpp>
#include <nuria/httpbackend.hpp>
#include <QBuffer>

namespace Nuria {

class TestBackend : public HttpBackend {
public:
	int listenPort;
	bool secure;
	
	TestBackend (HttpServer *server, int port = 80, bool isSecure = false)
	        : HttpBackend (server), listenPort (port), secure (isSecure)
	{ }
	
	bool isListening () const override
	{ return true; }
	
	int port () const override
	{ return listenPort; }
	
	bool isSecure () const
	{ return secure; }
};

/**
 * \brief HttpTransport which uses a in-memory buffer for testing purposes.
 */
class HttpMemoryTransport : public HttpTransport {
	Q_OBJECT
public:
	QByteArray outData;
	bool secure = false;
	TestBackend *testBackend;
	
	/** Constructor. */
	explicit HttpMemoryTransport (HttpServer *server);
	
	QByteArray process (HttpClient *client, QByteArray data) {
		readFromRemote (client, data);
		setCurrentRequestCount (currentRequestCount () + 1);
		return data;
	}
	
	bool isSecure () const
	{ return secure; }
	
	// 
	bool isOpen () const;
	
public slots:
	void forceClose ();
	
protected:
	void close (HttpClient *client);
	bool sendToRemote (HttpClient *client, const QByteArray &data);
};

}

#endif // NURIA_HTTPMEMORYTRANSPORT_HPP
