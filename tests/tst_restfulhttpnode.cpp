/* This file is part of the NuriaProject Framework. Copyright 2012 NuriaProject
 * The NuriaProject Framework is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 * The NuriaProject Framework is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with the library.  If not, see <http://www.gnu.org/licenses/>.
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
	
private:
	HttpClient *createClient (const QByteArray &method, bool withBody = false) {
		HttpMemoryTransport *transport = new HttpMemoryTransport;
		
		if (!withBody) {
			transport->setIncoming (method + " /api/annotate/42 HTTP/1.0\r\n\r\n");
		} else {
			transport->setIncoming (method + " /api/annotate/42 HTTP/1.0\r\n"
						"Content-Length: 10\r\n\r\n1234567890");
		}
			
		return new HttpClient (transport, server);
	}
	
	HttpMemoryTransport *getTransport (HttpClient *client) {
		return qobject_cast< HttpMemoryTransport * > (client->transport ());
	}
	
	HttpMemoryTransport *transport = new HttpMemoryTransport;
	HttpServer *server = new HttpServer (this);
	HttpClient *client = new HttpClient (transport, server);
	TestNode *node = new TestNode;
	
};

void RestfulHttpNodeTest::initTestCase () {
	server->root ()->addNode (node);
	
	transport->setIncoming ("GET / HTTP/1.0\r\n\r\n");
	qApp->processEvents ();
	
	transport->blockSignals (true);
	transport->clearOutgoing ();
	transport->open (QIODevice::ReadWrite);
	client->open (QIODevice::ReadWrite);
	transport->blockSignals (false);
	
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
	QVERIFY(!node->invokeTest ("does/not/exist", client));
}

void RestfulHttpNodeTest::allowAccessToClientsIsCalled () {
	QTest::ignoreMessage (QtDebugMsg, "Access denied");
	QVERIFY(!node->invokeTest ("foo/deny", client));
}

void RestfulHttpNodeTest::invokeAnnotatedHandler () {
	client->setProperty ("someProperty", "abc");
	QTest::ignoreMessage (QtDebugMsg, "foo 123 1 abc");
	transport->clearOutgoing ();
	
	QVERIFY(node->invokeTest ("annotate/123/true/foo", client));
	QByteArray outData = transport->outData ();
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
	
	QByteArray result = transport->outData ().split ('\n').last ();
	QByteArray expected = method.toLatin1 () + " 42";
	QCOMPARE(result, expected);
}

QTEST_MAIN(RestfulHttpNodeTest)
#include "tst_restfulhttpnode.moc"
