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

#include <QtTest/QtTest>
#include <QObject>

#include "private/httptcptransport.hpp"
#include "private/httptcpbackend.hpp"
#include "private/tcpserver.hpp"
#include <nuria/httpserver.hpp>
#include <nuria/httpclient.hpp>
#include <nuria/httpnode.hpp>
#include <QTcpSocket>
#include <QThread>

using namespace Nuria;

enum { Timeout = 500 };

class TestNode : public HttpNode {
	Q_OBJECT
public:
	
	TestNode (QObject *parent) : HttpNode (parent) {}
	
	bool invokePath (const QString &path, const QStringList &, int, HttpClient *client);
};

bool TestNode::invokePath (const QString &path, const QStringList &, int, HttpClient *client) {
	if (path == "/get") {
		client->write ("Works.");
	} else if (path == "/post") {
		auto func = [](HttpClient *client) {
			QByteArray data = client->readAll ();
			std::reverse (data.begin (), data.end ());
			client->write (data);
		};
		
		client->setSlotInfo (SlotInfo (Callback::fromLambda (func)));
	}
	
	// 
	return true;
}

// 
class HttpTcpTransportTest : public QObject {
	Q_OBJECT
private slots:
	
	void initTestCase ();
	void cleanupTestCase ();
	
	void verifyGetRequest ();
	void verifyPostRequest ();
	
	void testConnectTimeout ();
	void testDataTimeout ();
	void testKeepAliveTimeout ();
	
private:
	/*
	HttpClient *createClient (const QByteArray &request) {
		HttpMemoryTransport *transport = new HttpMemoryTransport (server);
		HttpClient *client = new HttpClient (transport, server);
		transport->setMaxRequests (1);
		transport->process (client, request);
		
		return client;
	}
	*/
	
	Internal::HttpTcpTransport *getTransport (HttpClient *client) {
		return qobject_cast< Internal::HttpTcpTransport * > (client->transport ());
	}
	
	// 
	QThread *thread = new QThread (this);
	HttpServer *server = new HttpServer;
	Internal::HttpTcpBackend *backend;
	TestNode *node = new TestNode (this);
	quint16 port = 0;
	
};

void HttpTcpTransportTest::initTestCase () {
	Internal::TcpServer *listener = new Internal::TcpServer (false);
	this->backend = new Internal::HttpTcpBackend (listener, server);
	
	// Listen on some free port.
	if (!listener->listen (QHostAddress::LocalHost)) {
		qFatal("Failed to create TCP listen socket on localhost. This test requires one though.");
	}
	
	// 
	this->port = listener->serverPort ();
	this->server->setFqdn ("unit.test");
	this->server->addBackend (backend);
	this->server->setRoot (this->node);
	
	this->server->setTimeout (HttpTransport::ConnectTimeout, Timeout);
	this->server->setTimeout (HttpTransport::DataTimeout, Timeout / 2);
	this->server->setTimeout (HttpTransport::KeepAliveTimeout, Timeout);
	this->server->setMinimalBytesReceived (4);
	
	// 
	this->thread->start ();
	this->server->moveToThread (this->thread);
	
}

void HttpTcpTransportTest::cleanupTestCase () {
	this->thread->quit ();
	this->thread->wait ();
}

void HttpTcpTransportTest::verifyGetRequest () {
	QTcpSocket socket;
	socket.connectToHost (QHostAddress::LocalHost, this->port);
	QVERIFY(socket.waitForConnected (Timeout));
	socket.write ("GET /get HTTP/1.0\r\n\r\n");
	
	QVERIFY(socket.waitForBytesWritten (Timeout));
	QVERIFY(socket.waitForReadyRead (Timeout));
	QCOMPARE(socket.readAll (), QByteArray("HTTP/1.0 200 OK\r\nConnection: close\r\n\r\nWorks."));
}

void HttpTcpTransportTest::verifyPostRequest () {
	QTcpSocket socket;
	socket.connectToHost (QHostAddress::LocalHost, this->port);
	QVERIFY(socket.waitForConnected (Timeout));
	socket.write ("POST /post HTTP/1.0\r\nContent-Length: 5\r\n\r\n12345");
	
	QVERIFY(socket.waitForBytesWritten (Timeout));
	QVERIFY(socket.waitForReadyRead (Timeout));
	QCOMPARE(socket.readAll (), QByteArray("HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n54321"));
	
}

void HttpTcpTransportTest::testConnectTimeout () {
	QTcpSocket socket;
	socket.connectToHost (QHostAddress::LocalHost, this->port);
	QVERIFY(socket.waitForConnected (Timeout));
	
	// Wait ...
	QSignalSpy spy (this->server, SIGNAL(connectionTimedout(Nuria::HttpTransport*,Nuria::HttpTransport::Timeout)));
	QThread::msleep (Timeout + 100);
	
	// 
	QCOMPARE(spy.length (), 1);
	QCOMPARE(spy.at (0).at (1), QVariant::fromValue (HttpTransport::ConnectTimeout));
	
}

void HttpTcpTransportTest::testDataTimeout () {
	QTcpSocket socket;
	socket.connectToHost (QHostAddress::LocalHost, this->port);
	QVERIFY(socket.waitForConnected (Timeout));
	socket.write ("POST /post HTTP/1.0\r\nContent-Length: 5\r\n\r\n123");
	
	QVERIFY(socket.waitForBytesWritten (Timeout));
	
	// Wait ...
	QSignalSpy spy (this->server, SIGNAL(connectionTimedout(Nuria::HttpTransport*,Nuria::HttpTransport::Timeout)));
	QThread::msleep (Timeout * 2);
	
	// 
	QCOMPARE(spy.length (), 1);
	QCOMPARE(spy.at (0).at (1), QVariant::fromValue (HttpTransport::DataTimeout));
}

void HttpTcpTransportTest::testKeepAliveTimeout () {
	QTcpSocket socket;
	socket.connectToHost (QHostAddress::LocalHost, this->port);
	QVERIFY(socket.waitForConnected (Timeout));
	
	QByteArray response = "Transfer-Encoding: chunked\r\n"
	                      "\r\n6\r\nWorks.\r\n0\r\n\r\n";
	
	// Do two keep-alive requests
	for (int i = 0; i < 2; i++) {
		socket.write ("GET /get HTTP/1.1\r\nHost: unit.test\r\nConnection: keep-alive\r\n\r\n");
		QVERIFY(socket.waitForBytesWritten ());
		QVERIFY(socket.waitForReadyRead (Timeout));
		QVERIFY(socket.readAll ().endsWith (response));
	}
	
	// Wait ...
	QSignalSpy spy (this->server, SIGNAL(connectionTimedout(Nuria::HttpTransport*,Nuria::HttpTransport::Timeout)));
	QThread::msleep (Timeout + 100);
	
	// 
	QCOMPARE(spy.length (), 1);
	QCOMPARE(spy.at (0).at (1), QVariant::fromValue (HttpTransport::KeepAliveTimeout));
}

QTEST_MAIN(HttpTcpTransportTest)
#include "tst_httptcptransport.moc"
