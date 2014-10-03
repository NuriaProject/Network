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
	
private:
	
	HttpClient *createClient (const QByteArray &request) {
		HttpMemoryTransport *transport = new HttpMemoryTransport (this);
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
	HttpMemoryTransport *transport = new HttpMemoryTransport (this);
	HttpClient *clientA = new HttpClient (transport, server);
	HttpClient *clientB = new HttpClient (transport, server);
	
	// Do two requests
	transport->setMaxRequests (2);
	input = transport->process (clientA, input);
	transport->process (clientB, input);
	
	// 
//	qDebug() << expected;
//	qDebug() << transport->outData;
	QCOMPARE(transport->outData, expected);
	
}

QTEST_MAIN(HttpClientTest)
#include "tst_httpclient.moc"
