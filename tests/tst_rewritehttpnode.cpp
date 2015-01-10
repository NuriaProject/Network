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

#include <nuria/rewritehttpnode.hpp>
#include "httpmemorytransport.hpp"
#include <nuria/httpserver.hpp>

using namespace Nuria;

// 
class TestNode : public RewriteHttpNode {
public:
	
	TestNode () : RewriteHttpNode ("r") {}
	
	bool invoke (const QString &path, int index, HttpClient *client) {
		return invokePath (path, path.split ('/'), index, client);
	}
	
};

class EchoNode : public HttpNode {
public:
	EchoNode () : HttpNode ("echo") {}
	
protected:
	bool invokePath (const QString &path, const QStringList &parts, int index, HttpClient *client) {
		qDebug("%s %s", qPrintable(client->path ().path ()), qPrintable(path));
		client->setResponseCode (200);
		return true;
	}
};

// 
class RewriteHttpNodeTest : public QObject {
	Q_OBJECT
private slots:
	
	void initTestCase ();
	
	void verifyUnderlyingNodeIsInvoked ();
	
	void verifySubpathRewrite_data ();
	void verifySubpathRewrite ();
	
	void verifyPathRewrite ();
	
	void addRewriteFailsForBadRegex ();
	void addRewriteFailsForUnknownCaptureGroupInReplacement ();
	
	void verifyMatchingOrder ();
	
private:
	void doRewriteTest (const QString &rx, const QString &rep, const QString &path, const QString &echo,
	                    RewriteHttpNode::RewriteBehaviour b);
	
	HttpClient *createClient () {
		HttpMemoryTransport *transport = new HttpMemoryTransport (this->server);
		HttpClient *client = new HttpClient (transport, this->server);
		return client;
	}
	
	HttpMemoryTransport *getTransport (HttpClient *client) {
		return qobject_cast< HttpMemoryTransport * > (client->transport ());
	}
	
	void reopenTransport () {
		transport->outData.clear ();
		client->open (QIODevice::ReadWrite);
	}
	
	// 
	HttpServer *server = new HttpServer (this);
	HttpMemoryTransport *transport = new HttpMemoryTransport (server);
	HttpClient *client = new HttpClient (transport, server);
	TestNode *node = new TestNode;
	
};

void RewriteHttpNodeTest::initTestCase () {
	server->root ()->addNode (node);
	server->root ()->addNode (new EchoNode);
	node->addNode (new EchoNode);
	
	transport->process (client, "GET / HTTP/1.0\r\n\r\n");
	reopenTransport ();
}

void RewriteHttpNodeTest::verifyUnderlyingNodeIsInvoked () {
	bool success = false;
	
	auto slot = [&success]() { success = true; };
	this->node->connectSlot ("slot", Callback::fromLambda (slot));
	
	// 
	QVERIFY(this->node->invoke ("r/slot", 1, client));
	QVERIFY(success);
	
}

void RewriteHttpNodeTest::verifySubpathRewrite_data () {
	QTest::addColumn< QString > ("rx");
	QTest::addColumn< QString > ("rep");
	QTest::addColumn< QString > ("path");
	QTest::addColumn< QString > ("echo");
	
	QTest::newRow ("simple") << "foo" << "echo/bar" << "/r/foo" << "/r/foo /r/echo/bar";
	QTest::newRow ("selfref") << ".*" << "echo/\\0" << "/r/foo" << "/r/foo /r/echo/foo";
	QTest::newRow ("refs") << "(a)(b)" << "echo/\\2\\1" << "/r/ab" << "/r/ab /r/echo/ba";
	QTest::newRow ("manyrefs") << "(0)(1)(2)(3)(4)(5)(6)(7)(8)(9)(a)"
	                           << "echo/\\11\\10\\9\\8\\7\\6\\5\\4\\3\\2\\1"
	                           << "/r/0123456789a" << "/r/0123456789a /r/echo/a9876543210";
	QTest::newRow ("longvalue") << "([a-z]+)-([0-9]+)" << "echo/\\1_\\2X"
	                            << "/r/abcdef-0123456" << "/r/abcdef-0123456 /r/echo/abcdef_0123456X";
}

void RewriteHttpNodeTest::doRewriteTest (const QString &rx, const QString &rep, const QString &path,
                                         const QString &echo, RewriteHttpNode::RewriteBehaviour b) {
	this->node->setRewriteBehaviour (b);
	QVERIFY(this->node->addRewrite (QRegularExpression (rx), rep));
	
	// 
	HttpClient *client = createClient ();
	HttpMemoryTransport *t = getTransport (client);
	QByteArray request = "GET __ HTTP/1.0\r\n\r\n";
	
	// 
	QByteArray ign = echo.toLatin1 ();
	QTest::ignoreMessage (QtDebugMsg, ign.constData ());
	QTest::ignoreMessage (QtDebugMsg, "close()");
	t->process (client, request.replace ("__", path.toLatin1 ()));
	QVERIFY(t->outData.startsWith ("HTTP/1.0 200 OK\r\n"));
	
	delete t;
	this->node->clearRules ();
}

void RewriteHttpNodeTest::verifySubpathRewrite () {
	QFETCH(QString, rx);
	QFETCH(QString, rep);
	QFETCH(QString, path);
	QFETCH(QString, echo);
	
	// 
	doRewriteTest (rx, rep, path, echo, RewriteHttpNode::RewriteSubpath);
}

void RewriteHttpNodeTest::verifyPathRewrite () {
	doRewriteTest ("/r/nuria", "/echo/nuria", "/r/nuria", "/echo/nuria /echo/nuria", RewriteHttpNode::RewritePath);
}

void RewriteHttpNodeTest::addRewriteFailsForBadRegex () {
	QVERIFY(!this->node->addRewrite (QRegularExpression ("["), ""));
}

void RewriteHttpNodeTest::addRewriteFailsForUnknownCaptureGroupInReplacement () {
	QVERIFY(!this->node->addRewrite (QRegularExpression ("foo(bar)"), "\\2"));
	
}

void RewriteHttpNodeTest::verifyMatchingOrder () {
	
	// The first one matches less, but first match wins
	QVERIFY(this->node->addRewrite (QRegularExpression ("^f"), "echo/correct"));
	doRewriteTest ("^foo", "echo/wrong", "/r/foo", "/r/foo /r/echo/correct",
	               RewriteHttpNode::RewriteSubpath);
	
}

QTEST_MAIN(RewriteHttpNodeTest)
#include "tst_rewritehttpnode.moc"
