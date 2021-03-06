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

#include <nuria/httpclient.hpp>

#include <QtTest/QtTest>
#include <QObject>

#include "httpmemorytransport.hpp"
#include <nuria/httpserver.hpp>
#include <nuria/httpwriter.hpp>
#include <nuria/restfulhttpnode.hpp>
#include "testnode.hpp"

using namespace Nuria;

// 
class RestfulHttpNodeTest : public QObject {
	Q_OBJECT
private slots:
	
	void initTestCase ();
	void registeringAHandler ();
	void returnFalseWhenPathNotFound ();
	void allowAccessToClientsIsCalled ();
	void invokeAnnotatedHandler ();
	
	void invokeAnnotatedSpecificHandler_data ();
	void invokeAnnotatedSpecificHandler ();
	
	void invokeNonStreamingPostHandlerLater ();
	
private:
	HttpClient *createClient (const QByteArray &method, bool withBody = false) {
		HttpMemoryTransport *transport = new HttpMemoryTransport (this->server);
		HttpClient *client = new HttpClient (transport, this->server);
		
		if (!withBody) {
			transport->process (client, method + " /api/annotate/42 HTTP/1.0\r\n\r\n");
		} else {
			transport->process (client, method + " /api/annotate/42 HTTP/1.0\r\n"
						"Content-Length: 10\r\n\r\n1234567890");
		}
		
		return client;
	}
	
	HttpMemoryTransport *getTransport (HttpClient *client) {
		return qobject_cast< HttpMemoryTransport * > (client->transport ());
	}
	
	void reopenTransport () {
		transport->outData.clear ();
		client->open (QIODevice::ReadWrite);
	}
	
	HttpServer *server = new HttpServer (this);
	HttpMemoryTransport *transport = new HttpMemoryTransport (server);
	HttpClient *client = new HttpClient (transport, server);
	TestNode *node = new TestNode;
	
};

void RestfulHttpNodeTest::initTestCase () {
	server->root ()->addNode (node);
	
	transport->process (client, "GET / HTTP/1.0\r\n\r\n");
	reopenTransport ();
}

void RestfulHttpNodeTest::registeringAHandler () {
	int one = 0;
	int two = 0;
	int three = 0;
	
	Callback cb = Callback::fromLambda ([&](int one_, int two_, int three_) {
		one = one_;
		two = two_;
		three = three_;
	});
	
	// 
	node->setRestfulHandler ("handle/{one}-{two}/{three}", { "one", "two", "three" }, cb);
	QVERIFY(node->invokeTest ("handle/1-2/3", client));
	
	// 
	QCOMPARE(one, 1);
	QCOMPARE(two, 2);
	QCOMPARE(three, 3);
}

void RestfulHttpNodeTest::returnFalseWhenPathNotFound () {
	reopenTransport ();
	QVERIFY(!node->invokeTest ("does/not/exist", client));
}

void RestfulHttpNodeTest::allowAccessToClientsIsCalled () {
	QTest::ignoreMessage (QtDebugMsg, "Access denied");
	reopenTransport ();
	QVERIFY(!node->invokeTest ("foo/deny", client));
}

void RestfulHttpNodeTest::invokeAnnotatedHandler () {
	client->setProperty ("someProperty", "abc");
	QTest::ignoreMessage (QtDebugMsg, "foo 123 1 abc");
	
	reopenTransport ();
	QVERIFY(node->invokeTest ("annotate/123/true/foo", client));
	QByteArray outData = transport->outData;
	outData.replace (' ', ""); // Remove spaces to be consistent across Qt versions.
	
	QCOMPARE(outData, QByteArray ("{\"boolean\":true,\"integer\":123,\"string\":\"foo\"}"));
}

void RestfulHttpNodeTest::invokeAnnotatedSpecificHandler_data () {
	QTest::addColumn< QString > ("method");
	
	QTest::newRow ("GET") << "GET";
	QTest::newRow ("HEAD") << "HEAD";
	QTest::newRow ("POST") << "POST";
	QTest::newRow ("PUT") << "PUT";
	QTest::newRow ("DELETE") << "DELETE";
}

void RestfulHttpNodeTest::invokeAnnotatedSpecificHandler () {
	QFETCH(QString, method);
	
	HttpClient *client = createClient (method.toLatin1 (), method.startsWith ('P'));
	HttpMemoryTransport *transport = getTransport (client);
	qApp->processEvents ();
	
	QByteArray result = transport->outData.split ('\n').last ();
	QByteArray expected = method.toLatin1 () + " 42";
	QCOMPARE(result, expected);
}

void RestfulHttpNodeTest::invokeNonStreamingPostHandlerLater () {
	QTest::ignoreMessage (QtDebugMsg, "1 2 3 1234567890");
	bool invoked = false;
	
	Callback cb = Callback::fromLambda ([&](int one, int two, int three, HttpClient *client) {
		qDebug("%i %i %i %s", one, two, three, client->readAll ().constData ());
		invoked = true;
	});
	
	// 
	node->setRestfulHandler ("nostream/{one}-{two}/{three}", { "one", "two", "three" }, cb, true);
	
	// Create client
	HttpMemoryTransport *transport = new HttpMemoryTransport (server);
	HttpClient *postClient = new HttpClient (transport, server);
	
	// Invoke
	transport->process (postClient, "POST /api/nostream/1-2/3 HTTP/1.0\r\n"
	                                "Content-Length: 10\r\n\r\n12345678");
	qApp->processEvents ();
	QVERIFY(postClient->slotInfo ().isValid ());
	
	// 
	QVERIFY(!invoked);
	transport->process (postClient, "90");
	QVERIFY(invoked);
	
}

QTEST_MAIN(RestfulHttpNodeTest)
#include "tst_restfulhttpnode.moc"
