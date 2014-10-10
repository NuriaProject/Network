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
