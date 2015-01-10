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
#include <nuria/httpfilter.hpp>
#include <nuria/httpserver.hpp>
#include <nuria/httpwriter.hpp>
#include <nuria/httpnode.hpp>

using namespace Nuria;

// HttpNode
class TestNode : public HttpNode {
	Q_OBJECT
public:
	QBuffer *fromClientBuffer;
	QByteArray readerClassName;
	
	TestNode (QObject *parent) : HttpNode (parent) {
		fromClientBuffer = new QBuffer (this);
		fromClientBuffer->open (QIODevice::ReadWrite);
	}
	
	bool invokePath (const QString &path, const QStringList &parts, int index, HttpClient *client);
	
	static void dummy (HttpClient *client)
	{ client->write (client->readAll ()); }
};

class RotFilter : public HttpFilter {
public:
	RotFilter (QObject *p) : HttpFilter (p) {}
	
	QByteArray filterName () const override
	{ return "rot"; }
	
	bool filterHeaders (HttpClient *, HttpClient::HeaderMap &headers) override
	{ headers.insert ("Foo", "bar"); return true; }
	
	QByteArray filterBegin (HttpClient *) override
	{ return "begin\r\n"; }
	
	bool filterData (HttpClient *, QByteArray &data) override {
		for (int i = 0; i < data.length (); i++)
			data[i] = data[i] + 1;
		return true;
	}
	
	QByteArray filterEnd (HttpClient *) override
	{ return "\r\nend"; }
};

class UnnamedFilter : public HttpFilter {
public:
	UnnamedFilter (QObject *p) : HttpFilter (p) {}
	
	bool filterHeaders (HttpClient *, HttpClient::HeaderMap &headers) override
	{ headers.insert ("Nuria", "project"); return true; }
	
	QByteArray filterBegin (HttpClient *) override
	{ return "rev\r\n"; }
	
	bool filterData (HttpClient *, QByteArray &data) override {
		std::reverse (data.begin (), data.end ());
		return true;
	}
	
	QByteArray filterEnd (HttpClient *) override
	{ return "\r\nver"; }
	
};

class FailingFilter : public HttpFilter {
public:
	FailingFilter (QObject *p) : HttpFilter (p) {}
	
	bool failHeaders = false;
	bool failData = false;
	
	bool filterHeaders (HttpClient *, HttpClient::HeaderMap &)
	{ return !failHeaders; }
	
	bool filterData (HttpClient *, QByteArray &)
	{ return !failData; }
	
};

bool TestNode::invokePath (const QString &path, const QStringList &parts,
			   int index, HttpClient *client) {
	Q_UNUSED(parts)
	Q_UNUSED(index)
	Q_UNUSED(client)
	
	// Transfer mode tests
	if (path == "/default") {
		client->write ("default");
		return true;
		
	} else if (path == "/chunked") {
		client->setTransferMode (HttpClient::ChunkedStreaming);
		client->write ("0123456789");
		client->write ("abc");
		return true;
		
	} else if (path == "/buffered") {
		client->setTransferMode (HttpClient::Buffered);
		client->write ("abc");
		client->write ("defg");
		return true;
	}
	
	// 
	client->setTransferMode (HttpClient::Buffered);
	if (path == "/to_buffer") {
		client->pipeFromPostBody (fromClientBuffer);
		client->setKeepConnectionOpen (true);
		
	} else if (path == "/to_process") {
		QProcess *process = new QProcess;
		process->start ("tac", QStringList ());
		process->setReadChannel (QProcess::StandardOutput);
		client->pipeFromPostBody (process);
		client->pipeToClient (process);
		
	} else if (path == "/reader") {
		HttpPostBodyReader *reader = client->postBodyReader ();
		readerClassName.clear ();
		
		if (client->hasReadablePostBody () && reader) {
			readerClassName = reader->metaObject ()->className ();
		}
		
	} else  if (client->verb () == HttpClient::POST) {
		SlotInfo info (&dummy);
		client->setSlotInfo (info);
		
	} else if (path == "/buffer") {
		QBuffer *buffer = new QBuffer;
		buffer->setData ("0123456789");
		buffer->open (QIODevice::ReadOnly);
		client->pipeToClient (buffer);
		
	} else if (path == "/file") {
		QTemporaryFile *file = new QTemporaryFile;
		file->open ();
		file->write ("0123456789");
		file->reset ();
		client->pipeToClient (file);
		
	} else if (path == "/process") {
		QProcess *process = new QProcess;
		process->start ("echo", { "hello" });
		process->setReadChannel (QProcess::StandardOutput);
		client->pipeToClient (process);
		
	} else if (path == "/gzip") {
		client->addFilter (HttpClient::GzipFilter);
		client->write ("NuriaProject");
		
	} else if (path == "/deflate") {
		client->addFilter (HttpClient::DeflateFilter);
		client->write ("NuriaProject");
		
	} else if (path == "/filter") {
		client->addFilter (new UnnamedFilter (client));
		client->addFilter (new RotFilter (client));
		client->write ("abcdef");
		
	} else if (path == "/rewrite") {
		return client->invokePath ("/rewritten");
		
	} else if (path == "/redirect/keep") {
		client->redirectClient ("/nuria", HttpClient::RedirectMode::Keep);
		
	} else if (path == "/redirect/http") {
		client->redirectClient ("/nuria", HttpClient::RedirectMode::ForceUnsecure);
		
	} else if (path == "/redirect/https") {
		client->redirectClient ("/nuria", HttpClient::RedirectMode::ForceSecure);
		
	} else if (path == "/redirect/302") {
		client->redirectClient ("/nuria", HttpClient::RedirectMode::Keep, 302);
		
	} else if (path == "/redirect/remote") {
		client->redirectClient (QUrl ("http://nuriaproject.org/"));
		
	} else {
		qDebug("%s", qPrintable(path));
		client->write (path.toLatin1 ());
	}
	
	return true;
}

// 
class HttpClientTest : public QObject {
	Q_OBJECT
private slots:
	
	void initTestCase ();
	void getHttp10 ();
	void getHttp11Compliant ();
	void getHttp11NotCompliantKillsConnection ();
	void getWithPostBodyKillsConnection ();
	void postWithoutContentLengthKillsConnection ();
	void postWithTooMuchData ();
	void postWithout100Continue ();
	void postWith100Continue ();
	void pipeToClientBuffer ();
	void pipeToClientFile ();
	void pipeToClientProcess ();
	void pipeFromClientBuffer ();
	void pipeFromClientProcess ();
	void bodyReaderForMultipart ();
	void bodyReaderForMultipartNoBoundaryGiven ();
	void bodyReaderForUrlEncoded ();
	void parseCookiesFromHeader ();
	void verifyChunkedTransfer ();
	void verifyBuffered ();
	void verifyKeepAliveBehaviour ();
	void filterIsAddedToContentEncoding ();
	void verifyGzipFilter ();
	void verifyDeflateFilter ();
	
	void verifyClientPath_data ();
	void verifyClientPath ();
	
	void testInvokePath ();
	
	void redirectClientLocal_data ();
	void redirectClientLocal ();
	void redirectClientLocalCustomResponseCode ();
	void redirectClientRemote ();
	
private:
	
	HttpClient *createClient (const QByteArray &request) {
		HttpMemoryTransport *transport = new HttpMemoryTransport (server);
		HttpClient *client = new HttpClient (transport, server);
		transport->setMaxRequests (1);
		transport->process (client, request);
		
		return client;
	}
	
	HttpMemoryTransport *getTransport (HttpClient *client) {
		return qobject_cast< HttpMemoryTransport * > (client->transport ());
	}
	
	HttpServer *server = new HttpServer (this);
	TestNode *node = new TestNode (this);
	
};

void HttpClientTest::initTestCase () {
	server->setRoot (node);
	server->setFqdn ("unit.test");
	
	server->addBackend (new TestBackend (server, 80, false));
	server->addBackend (new TestBackend (server, 443, true));
}

void HttpClientTest::getHttp10 () {
	QByteArray input = "GET / HTTP/1.0\r\n\r\n";
	QByteArray expected = "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Length: 1\r\n\r\n/";
	
	QTest::ignoreMessage (QtDebugMsg, "/");
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

QByteArray &insertDateTime (QByteArray &data) {
	HttpWriter writer;
	data.replace ("%%", writer.dateTimeToHttpDateHeader (QDateTime::currentDateTimeUtc ()));
	return data;
}

void HttpClientTest::getHttp11Compliant () {
	QByteArray input = "GET / HTTP/1.1\r\n"
			   "Host: example.com\r\n"
	                   "Connection: close\r\n"
			   "\r\n";
	
	QByteArray expected = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 1\r\nDate: %%\r\n\r\n/";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "/");
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::getHttp11NotCompliantKillsConnection () {
	QByteArray input = "GET / HTTP/1.1\r\n\r\n";
	QByteArray expected = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nDate: %%\r\n\r\nBad Request";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::getWithPostBodyKillsConnection () {
	QByteArray input = "GET / HTTP/1.1\r\n"
			   "Host: example.com\r\n"
			   "Content-Length: 10\r\n"
			   "\r\n"
			   "0123456789";
	QByteArray expected = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nDate: %%\r\n\r\nBad Request";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::postWithoutContentLengthKillsConnection () {
	QByteArray input = "POST / HTTP/1.1\r\n"
			   "Host: example.com\r\n"
			   "\r\n"
			   "0123456789";
	QByteArray expected = "HTTP/1.1 400 Bad Request\r\n"
			      "Connection: close\r\nDate: %%\r\n\r\nBad Request";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::postWithTooMuchData () {
	QByteArray input = "POST / HTTP/1.1\r\n"
			   "Host: example.com\r\n"
			   "Content-Length: 9\r\n"
	                   "Connection: close\r\n"
			   "\r\n"
			   "0123456789";
	QByteArray expected = "HTTP/1.1 413 Request Entity Too Large\r\n"
			      "Connection: close\r\nDate: %%\r\n\r\nRequest Entity Too Large";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);

	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::postWithout100Continue () {
	QByteArray input = "POST / HTTP/1.0\r\n"
			   "Host: example.com\r\n"
			   "Content-Length: 10\r\n"
			   "\r\n"
			   "0123456789";
	QByteArray expected = "HTTP/1.0 200 OK\r\n"
	                      "Connection: close\r\n"
	                      "Content-Length: 10\r\n\r\n"
			      "0123456789";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::postWith100Continue () {
	QByteArray input = "POST / HTTP/1.0\r\n"
			   "Host: example.com\r\n"
			   "Content-Length: 10\r\n"
			   "Expect: 100-continue\r\n"
			   "\r\n"
			   "0123456789";
	QByteArray expected = "HTTP/1.0 100 Continue\r\n\r\n"
			      "HTTP/1.0 200 OK\r\n"
			      "Connection: close\r\n"
	                      "Content-Length: 10\r\n\r\n"
			      "0123456789";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::pipeToClientBuffer () {
	QByteArray input = "GET /buffer HTTP/1.1\r\n"
			   "Host: example.com\r\n"
	                   "Connection: close\r\n"
			   "\r\n";
	
	QByteArray expected = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 10\r\nDate: %%\r\n\r\n"
			      "0123456789";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::pipeToClientFile () {
	QByteArray input = "GET /file HTTP/1.1\r\n"
			   "Host: example.com\r\n"
	                   "Connection: close\r\n"
			   "\r\n";
	
	QByteArray expected = "HTTP/1.1 200 OK\r\nConnection: close\r\n"
			      "Content-Length: 10\r\nDate: %%\r\n\r\n"
			      "0123456789";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

static void runEventLoopUntil (QObject *object, const char *signal, int timeout = 1000) {
	QEventLoop loop;
	
	QTimer::singleShot (timeout, &loop, SLOT(quit()));
	QObject::connect (object, signal, &loop, SLOT(quit()));
	
	loop.exec ();
}

void HttpClientTest::pipeToClientProcess () {
#ifdef Q_OS_WIN
	QSKIP("No idea what process to call as test on windows :(");
#endif
	
	QByteArray input = "GET /process HTTP/1.1\r\n"
			   "Host: example.com\r\n"
	                   "Connection: close\r\n"
			   "\r\n";
	
	QByteArray expected = "HTTP/1.1 200 OK\r\nConnection: close\r\n"
			      "Date: %%\r\n\r\n"
			      "hello\n";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	runEventLoopUntil (client, SIGNAL(aboutToClose()));
	
	QCOMPARE(transport->outData, expected);
	QVERIFY(!client->isOpen ());
}

void HttpClientTest::pipeFromClientBuffer () {
	QByteArray input = "POST /to_buffer HTTP/1.1\r\n"
			   "Host: example.com\r\n"
			   "Content-Length: 10\r\n"
	                   "Connection: close\r\n"
			   "\r\n"
			   "0123456789";
	
	QByteArray expected = "HTTP/1.1 200 OK\r\n"
	                      "Connection: close\r\n"
	                      "Content-Length: 0\r\n"
	                      "Date: %%\r\n\r\n";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	client->close (); // We keep the connection open in the TestNode above.
	
	node->fromClientBuffer->close ();
	
	QCOMPARE(transport->outData, expected);
	QCOMPARE(node->fromClientBuffer->data (), QByteArray ("0123456789"));
	
	// 
	node->fromClientBuffer->setData (QByteArray ());
	node->fromClientBuffer->open (QIODevice::ReadWrite);
	
}

void HttpClientTest::pipeFromClientProcess () {
#ifdef Q_OS_WIN
	QSKIP("No idea what process to call as test on windows :(");
#endif
	
	QByteArray input = "POST /to_process HTTP/1.1\r\n"
			   "Host: example.com\r\n"
			   "Content-Length: 8\r\n"
	                   "Connection: close\r\n"
			   "\r\n"
			   "one\n"
			   "two\n";
	
	QByteArray expected = "HTTP/1.1 200 OK\r\n"
	                      "Connection: close\r\n"
			      "Date: %%\r\n\r\n"
			      "two\n"
			      "one\n";
	insertDateTime (expected);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	runEventLoopUntil (client, SIGNAL(aboutToClose()));
	
	QCOMPARE(transport->outData, expected);
	QVERIFY(!client->isOpen ());
}

void HttpClientTest::bodyReaderForMultipart () {
	QByteArray input = "POST /reader HTTP/1.0\r\n"
	                   "Content-Type: multipart/form-data; boundary=asdasdasd\r\n"
	                   "Content-Length: 10\r\n\r\n0123456789";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	QCOMPARE(node->readerClassName, QByteArray ("Nuria::HttpMultiPartReader"));
}

void HttpClientTest::bodyReaderForMultipartNoBoundaryGiven () {
	QByteArray input = "POST /reader HTTP/1.0\r\n"
	                   "Content-Type: multipart/form-data; \r\n"
	                   "Content-Length: 10\r\n\r\n0123456789";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	QVERIFY(node->readerClassName.isEmpty ());
	
}

void HttpClientTest::bodyReaderForUrlEncoded () {
	QByteArray input = "POST /reader HTTP/1.0\r\n"
	                   "Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
	                   "Content-Length: 10\r\n\r\n0123456789";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	QCOMPARE(node->readerClassName, QByteArray ("Nuria::HttpUrlEncodedReader"));
}

void HttpClientTest::parseCookiesFromHeader () {
	QByteArray input = "GET / HTTP/1.0\r\n"
	                   "Cookie: foo=bar; nuria=project\r\n\r\n";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	QTest::ignoreMessage (QtDebugMsg, "/");
	HttpClient *client = createClient (input);
	HttpClient::Cookies cookies = client->cookies ();
	
	QCOMPARE(cookies.size (), 2);
	QCOMPARE(cookies.value ("foo").value (), QByteArray ("bar"));
	QCOMPARE(cookies.value ("nuria").value (), QByteArray ("project"));
	
}

void HttpClientTest::verifyChunkedTransfer () {
	QByteArray input = "GET /chunked HTTP/1.0\r\n\r\n";
	QByteArray expected = "HTTP/1.0 200 OK\r\n"
	                      "Connection: close\r\n"
	                      "Transfer-Encoding: chunked\r\n\r\n"
	                      "a\r\n"
	                      "0123456789\r\n"
	                      "3\r\n"
	                      "abc\r\n"
	                      "0\r\n\r\n";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::verifyBuffered () {
	QByteArray input = "GET /buffered HTTP/1.0\r\n\r\n";
	QByteArray expected = "HTTP/1.0 200 OK\r\n"
	                      "Connection: close\r\n"
	                      "Content-Length: 7\r\n\r\n"
	                      "abcdefg";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::verifyKeepAliveBehaviour () {
	QByteArray input = "GET /default HTTP/1.0\r\n"
	                   "Connection: keep-alive\r\n\r\n"
	                   "GET /default HTTP/1.0\r\n"
	                   "Connection: keep-alive\r\n\r\n";
	QByteArray expected = "HTTP/1.0 200 OK\r\n"
	                      "Connection: keep-alive\r\n"
	                      "Transfer-Encoding: chunked\r\n\r\n"
	                      "7\r\ndefault\r\n"
	                      "0\r\n\r\n"
	                      "HTTP/1.0 200 OK\r\n"
	                      "Connection: close\r\n\r\n"
	                      "default";
	
	// 
	HttpMemoryTransport *transport = new HttpMemoryTransport (server);
	HttpClient *clientA = new HttpClient (transport, server);
	HttpClient *clientB = new HttpClient (transport, server);
	
	// Do two requests
	transport->setMaxRequests (2);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	input = transport->process (clientA, input);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	transport->process (clientB, input);
	
	// 
	QCOMPARE(transport->outData, expected);
	
}

void HttpClientTest::filterIsAddedToContentEncoding () {
	QByteArray input = "GET /filter HTTP/1.0\r\n\r\n";
	QByteArray expected = "HTTP/1.0 200 OK\r\n"
	                      "Connection: close\r\n"
	                      "Content-Encoding: rot\r\n"
	                      "Foo: bar\r\n"
	                      "Nuria: project\r\n\r\n"
	                      "rev\r\nbegin\r\ngfedcb\r\nver\r\nend";
	
	// 
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::verifyGzipFilter () {
	QByteArray input = "GET /gzip HTTP/1.0\r\n\r\n";
	static const char data[] = "HTTP/1.0 200 OK\r\n"
	                           "Connection: close\r\n"
	                           "Content-Encoding: gzip\r\n\r\n"
	                           "\x1f\x8b\x08\x00\x00\x00\x00\x00"
	                           "\x00\xff\xf3\x2b\x2d\xca\x4c\x0c"
	                           "\x28\xca\xcf\x4a\x4d\x2e\x01\x00"
	                           "\x43\xa8\xad\x4e\x0c\x00\x00\x00";
	QByteArray expected (data, sizeof(data) - 1);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::verifyDeflateFilter () {
	QByteArray input = "GET /deflate HTTP/1.0\r\n\r\n";
	static const char data[] = "HTTP/1.0 200 OK\r\n"
	                           "Connection: close\r\n"
	                           "Content-Encoding: deflate\r\n\r\n"
	                           "\x78\x9c\xf3\x2b\x2d\xca\x4c\x0c"
	                           "\x28\xca\xcf\x4a\x4d\x2e\x01\x00"
	                           "\x1f\x00\x04\xd7";
	QByteArray expected (data, sizeof(data) - 1);
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

void HttpClientTest::verifyClientPath_data () {
	QTest::addColumn< QString > ("host");
	QTest::addColumn< bool > ("secure");
	QTest::addColumn< QString > ("url");
	
	QTest::newRow ("http") << "foo.bar" << false << "http://foo.bar/nuria";
	QTest::newRow ("https") << "foo.bar" << true << "https://foo.bar/nuria";
	QTest::newRow ("http:81") << "foo.bar:81" << false << "http://foo.bar:81/nuria";
	QTest::newRow ("https:444") << "foo.bar:444" << true << "https://foo.bar:444/nuria";
	QTest::newRow ("http1.0") << "" << false << "http://unit.test/nuria";
	QTest::newRow ("https1.0") << "" << true << "https://unit.test/nuria";
//	QTest::newRow ("http1.0:81") << "" << 81 << false << "http://unit.test:81/nuria";
//	QTest::newRow ("https1.0:444") << "" << 444 << true << "https://unit.test:444/nuria";
}

void HttpClientTest::verifyClientPath () {
	QFETCH(QString, host);
	QFETCH(bool, secure);
	QFETCH(QString, url);
	
	// Build HTTP request
	QByteArray input = "GET /nuria HTTP/1.0\r\n";
	if (!host.isEmpty ()) {
		input.append ("Host: ");
		input.append (host);
		input.append ("\r\n");
	}
	
	input.append ("\r\n");
	
	//
	QTest::ignoreMessage (QtDebugMsg, "/nuria");
	QTest::ignoreMessage (QtDebugMsg, "close()");
	
	// 
	HttpMemoryTransport *transport = new HttpMemoryTransport (server);
	transport->testBackend->listenPort = (secure) ? 443 : 80;
	HttpClient *client = new HttpClient (transport, server);
	transport->secure = secure;
	transport->process (client, input);
	
	// 
	QCOMPARE(client->path ().toString (), url);
	
}

void HttpClientTest::testInvokePath () {
	QByteArray input = "GET /rewrite HTTP/1.0\r\n\r\n";
	QByteArray expected = "HTTP/1.0 200 OK\r\n"
	                      "Connection: close\r\n"
	                      "Content-Length: 10\r\n\r\n"
                              "/rewritten";
	
	QTest::ignoreMessage (QtDebugMsg, "/rewritten");
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
	QCOMPARE(client->path ().path (), QString ("/rewritten"));
}

void HttpClientTest::redirectClientLocal_data () {
	QTest::addColumn< QString > ("host");
	QTest::addColumn< int > ("port");
	QTest::addColumn< bool > ("secure");
	QTest::addColumn< QString > ("mode");
	QTest::addColumn< QString > ("location");
	QTest::addColumn< int > ("code");
	
	// RedirectMode::Keep
	QTest::newRow ("http keep") << "foo.bar" << 80 << false << "keep" << "/nuria" << 307;
	QTest::newRow ("https keep") << "foo.bar" << 443 << true << "keep" << "/nuria" << 307;
	QTest::newRow ("http:81 keep") << "foo.bar:81" << 80 << false << "keep" << "/nuria" << 307;
	QTest::newRow ("https:444 keep") << "foo.bar:444" << 443 << true << "keep" << "/nuria" << 307;
	QTest::newRow ("http1.0 keep") << "" << 80 << false << "keep" << "/nuria" << 301;
	QTest::newRow ("https1.0 keep") << "" << 443 << true << "keep" << "/nuria" << 301;
	QTest::newRow ("http1.0:81 keep") << "" << 81 << false << "keep" << "/nuria" << 301;
	QTest::newRow ("https1.0:444 keep") << "" << 444 << true << "keep" << "/nuria" << 301;
	
	// RedirectMode::ForceUnsecure
	QTest::newRow ("http unsecure") << "foo.bar" << 80 << false << "http" << "http://foo.bar/nuria" << 307;
	QTest::newRow ("https unsecure") << "foo.bar" << 443 << true << "http" << "http://foo.bar/nuria" << 307;
	QTest::newRow ("http:81 unsecure") << "foo.bar:81" << 80 << false << "http" << "http://foo.bar:81/nuria" << 307;
	QTest::newRow ("https:444 unsecure") << "foo.bar:444" << 443 << true << "http" << "http://foo.bar/nuria" << 307;
	QTest::newRow ("http1.0 unsecure") << "" << 80 << false << "http" << "http://unit.test/nuria" << 301;
	QTest::newRow ("https1.0 unsecure") << "" << 443 << true << "http" << "http://unit.test/nuria" << 301;
	QTest::newRow ("http1.0:81 unsecure") << "" << 81 << false << "http" << "http://unit.test:81/nuria" << 301;
	QTest::newRow ("https1.0:444 unsecure") << "" << 444 << true << "http" << "http://unit.test/nuria" << 301;
	
	// RedirectMode::ForceSecure
	QTest::newRow ("http secure") << "foo.bar" << 80 << false << "https" << "https://foo.bar/nuria" << 307;
	QTest::newRow ("https secure") << "foo.bar" << 443 << true << "https" << "https://foo.bar/nuria" << 307;
	QTest::newRow ("http:81 secure") << "foo.bar:81" << 80 << false << "https" << "https://foo.bar/nuria" << 307;
	QTest::newRow ("https:444 secure") << "foo.bar:444" << 443 << true
	                                   << "https" << "https://foo.bar:444/nuria" << 307;
	QTest::newRow ("http1.0 secure") << "" << 80 << false << "https" << "https://unit.test/nuria" << 301;
	QTest::newRow ("https1.0 secure") << "" << 443 << true << "https" << "https://unit.test/nuria" << 301;
	QTest::newRow ("http1.0:81 secure") << "" << 81 << false << "https" << "https://unit.test/nuria" << 301;
        QTest::newRow ("https1.0:444 secure") << "" << 444 << true << "https" << "https://unit.test:444/nuria" << 301;
	
}

void HttpClientTest::redirectClientLocal () {
	QFETCH(QString, host);
	QFETCH(int, port);
	QFETCH(bool, secure);
	QFETCH(QString, mode);
	QFETCH(QString, location);
	QFETCH(int, code);
	
	// 
	if (!host.isEmpty ()) {
		host.prepend ("Host: ");
		host.append ("\r\n");
	}
	
	// Build HTTP request
	static const QString templ = "GET /redirect/%1 HTTP/1.%2\r\n%3\r\n";
	QString input = templ.arg (mode, QString ((host.isEmpty ()) ? "0" : "1"), host);
	
	//
	QTest::ignoreMessage (QtDebugMsg, "close()");
	
	// 
	HttpServer server;
	server.setRoot (this->node);
	server.setFqdn (this->server->fqdn ());
	this->node->setParent (this->server);
	
	// Only used when switching secure <-> unsecure
	server.addBackend (new TestBackend (&server, (!secure) ? 443 : 80, !secure));
	server.addBackend (new TestBackend (&server, port, secure));
	
	// 
	HttpMemoryTransport *transport = new HttpMemoryTransport (&server);
	HttpClient *client = new HttpClient (transport, &server);
	transport->testBackend->secure = secure;
	transport->testBackend->listenPort = port;
	transport->secure = secure;
	transport->process (client, input.toLatin1 ());
	
	// 
	static const QRegularExpression locationRx ("Location: ([^\r]*)\r\n");
	static const QRegularExpression codeRx ("HTTP/1.[10] ([^ ]*)");
	QRegularExpressionMatch loc = locationRx.match (transport->outData);
	QRegularExpressionMatch response = codeRx.match (transport->outData);
	
	// 
	if (!loc.hasMatch ()) QFAIL("No 'Location' header");
	if (!response.hasMatch ()) QFAIL("No response code");
	if (loc.captured (1) != location) {
		qWarning() << "Expected:" << location;
		qWarning() << "Result  :" << loc.captured (1);
		QFAIL("Location header URL is different.");
	}
	
	if (response.captured (1).toInt () != code) {
		qWarning() << "Expected:" << code;
		qWarning() << "Result  :" << response.captured (1);
		QFAIL("HTTP response code is different.");
	}
	
}

void HttpClientTest::redirectClientLocalCustomResponseCode () {
	QByteArray input = "GET /redirect/302 HTTP/1.0\r\n\r\n";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	
	QCOMPARE(client->responseCode (), 302);
}

void HttpClientTest::redirectClientRemote () {
	QByteArray input = "GET /redirect/remote HTTP/1.0\r\n\r\n";
	QByteArray expected = "HTTP/1.0 301 Moved Permanently\r\n"
	                      "Connection: close\r\n"
	                      "Content-Length: 78\r\n"
	                      "Location: http://nuriaproject.org/\r\n\r\n"
	                      "Redirecting to <a href=\"http://nuriaproject.org/\">http://nuriaproject.org/</a>";
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	QCOMPARE(transport->outData, expected);
}

QTEST_MAIN(HttpClientTest)
#include "tst_httpclient.moc"
