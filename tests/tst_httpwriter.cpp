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

#include <nuria/httpwriter.hpp>

#include <QtTest/QtTest>
#include <QObject>

using namespace Nuria;

class HttpWriterTest : public QObject {
	Q_OBJECT
private slots:
	
	void httpVersionToString ();
	
	void writeResponseLineDefaultMessage ();
	void writeResponseLineCustomMessage ();
	
	void writeSetCookieValue_data ();
	void writeSetCookieValue ();
	
	void writeSetCookies ();
	
	void writeHttpHeaders ();
	
	void dateTimeToHttpDateHeader ();
	
	void buildRangeHeader ();
	
	void addComplianceHeaders ();
	void addComplianceHeadersHttp1_0 ();
	void addComplianceHeadersDateGiven ();
	
	void applyRangeHeadersRangeAndLength ();
	void applyRangeHeadersRangeAndLengthLeftAlone ();
	void applyRangeHeadersRange ();
	void applyRangeHeadersLength ();
	void applyRangeHeadersLengthLeftAlone ();
	void applyRangeHeadersNothing ();
	
	void addTransferEncodingHeaderSetsChunked ();
	void addTransferEncodingHeaderAddsChunked ();
	void addTransferEncodingHeaderDoesNothing ();
	void addTransferEncodingHeaderContainsChunkedAlready ();
	
	void addConnectionHeader_data ();
	void addConnectionHeader ();
};

void HttpWriterTest::httpVersionToString () {
	HttpWriter writer;
	
	QCOMPARE(writer.httpVersionToString (HttpClient::HttpUnknown), QByteArray ());
	QCOMPARE(writer.httpVersionToString (HttpClient::Http1_0), QByteArray ("HTTP/1.0"));
	QCOMPARE(writer.httpVersionToString (HttpClient::Http1_1), QByteArray ("HTTP/1.1"));
}

void HttpWriterTest::writeResponseLineDefaultMessage () {
	HttpWriter writer;
	
	QByteArray result = writer.writeResponseLine (HttpClient::Http1_1, 200, QByteArray ());
	QCOMPARE(result, QByteArray ("HTTP/1.1 200 OK\r\n"));
}

void HttpWriterTest::writeResponseLineCustomMessage () {
	HttpWriter writer;
	
	QByteArray result = writer.writeResponseLine (HttpClient::Http1_0, 200, "Yay");
	QCOMPARE(result, QByteArray ("HTTP/1.0 200 Yay\r\n"));
}

void HttpWriterTest::writeSetCookieValue_data () {
	QTest::addColumn< QNetworkCookie > ("input");
	QTest::addColumn< QString > ("result");
	
	QNetworkCookie normal ("foo", "bar");
	QNetworkCookie encoded ("foo", "b a;r");
	QNetworkCookie maxAge ("foo", "bar");
	QNetworkCookie maxAgePast ("foo", "bar");
	QNetworkCookie domain ("foo", "bar");
	QNetworkCookie path ("foo", "bar");
	QNetworkCookie secure ("foo", "bar");
	QNetworkCookie httpOnly ("foo", "bar");
	QNetworkCookie complex ("foo", "b a;r");
	
	maxAge.setExpirationDate (QDateTime::currentDateTime ().addSecs (123));
	maxAgePast.setExpirationDate (QDateTime::currentDateTime ().addSecs (-100));
	domain.setDomain ("nuriaproject.org");
	path.setPath ("/foo/bar");
	secure.setSecure (true);
	httpOnly.setHttpOnly (true);
	
	complex.setExpirationDate (QDateTime::currentDateTime ().addSecs (123));
	complex.setDomain ("nuriaproject.org");
	complex.setPath ("/foo/bar");
	complex.setSecure (true);
	complex.setHttpOnly (true);
	
	QTest::newRow ("normal") << normal << "foo=bar";
	QTest::newRow ("encoded") << encoded << "foo=b%20a%3Br";
	QTest::newRow ("maxAge") << maxAge << "foo=bar; Max-Age=123";
	QTest::newRow ("maxAge in past") << maxAgePast << "foo=bar; Max-Age=0";
	QTest::newRow ("domain") << domain << "foo=bar; Domain=nuriaproject.org";
	QTest::newRow ("path") << path << "foo=bar; Path=/foo/bar";
	QTest::newRow ("secure") << secure << "foo=bar; Secure";
	QTest::newRow ("httpOnly") << httpOnly << "foo=bar; HttpOnly";
	QTest::newRow ("complex") << complex << "foo=b%20a%3Br; Domain=nuriaproject.org; "
				     "Path=/foo/bar; Max-Age=123; Secure; HttpOnly";
	
}

void HttpWriterTest::writeSetCookieValue () {
	QFETCH(QNetworkCookie, input);
	QFETCH(QString, result);
	QByteArray expected = result.toLatin1 ();
	
	HttpWriter writer;
	QByteArray output = writer.writeSetCookieValue (input);
	
	QCOMPARE(output, expected);
}

void HttpWriterTest::writeSetCookies () {
	HttpWriter writer;
	
	// Data
	QByteArray expected = "Set-Cookie: expires=b%20a%3Br; Max-Age=0; Secure; HttpOnly\r\n"
			      "Set-Cookie: session=foo\r\n";
	
	QNetworkCookie expires ("expires", "b a;r");
	expires.setExpirationDate (QDateTime::fromMSecsSinceEpoch (123));
	expires.setSecure (true);
	expires.setHttpOnly (true);
	
	HttpClient::Cookies cookies {
		{ "session", QNetworkCookie ("session", "foo") },
		{ "expires", expires }
	};
	
	// 
	QByteArray result = writer.writeSetCookies (cookies);
	QCOMPARE(result, expected);
}

void HttpWriterTest::writeHttpHeaders () {
	HttpWriter writer;
	
	HttpClient::HeaderMap map {
		{ "Foo", "Two" },
		{ "Foo", "One" },
		{ "Nuria", "Project" }
	};
	
	QByteArray expected = "Foo: One\r\n"
			      "Foo: Two\r\n"
			      "Nuria: Project\r\n";
	
	// 
	QByteArray result = writer.writeHttpHeaders (map);
	QCOMPARE(result, expected);
}

void HttpWriterTest::dateTimeToHttpDateHeader () {
	HttpWriter writer;
	QDateTime dateTime (QDate (2012, 11, 10), QTime (1, 2, 3));
	
	QByteArray result = writer.dateTimeToHttpDateHeader (dateTime);
	QCOMPARE(result, QByteArray ("Sat, 10 Nov 2012 01:02:03 GMT"));
	
}

void HttpWriterTest::buildRangeHeader () {
	HttpWriter writer;
	
	QCOMPARE(writer.buildRangeHeader (10, 30, 100), QByteArray ("bytes 10-30/100"));
	QCOMPARE(writer.buildRangeHeader (10, 30, -1), QByteArray ("bytes 10-30/*"));
}

void HttpWriterTest::addComplianceHeaders () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.addComplianceHeaders (HttpClient::Http1_1, map);
	QVERIFY(map.contains ("Date"));
}

void HttpWriterTest::addComplianceHeadersHttp1_0 () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.addComplianceHeaders (HttpClient::Http1_0, map);
	QVERIFY(map.isEmpty ());
}

void HttpWriterTest::addComplianceHeadersDateGiven () {
	HttpWriter writer;
	HttpClient::HeaderMap map { { "Date", "foo" } };
	
	writer.addComplianceHeaders (HttpClient::Http1_1, map);
	QCOMPARE(map.value ("Date"), QByteArray ("foo"));
	
}

void HttpWriterTest::applyRangeHeadersRangeAndLength () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.applyRangeHeaders (10, 30, 100, map);
	QCOMPARE(map.value ("Content-Range"), QByteArray ("bytes 10-30/100"));
	QCOMPARE(map.value ("Content-Length"), QByteArray ("20"));
}

void HttpWriterTest::applyRangeHeadersRangeAndLengthLeftAlone () {
	HttpWriter writer;
	HttpClient::HeaderMap map { { "Content-Range", "a" }, { "Content-Length", "b" } };
	
	writer.applyRangeHeaders (10, 30, 100, map);
	QCOMPARE(map.value ("Content-Range"), QByteArray ("a"));
	QCOMPARE(map.value ("Content-Length"), QByteArray ("b"));
}

void HttpWriterTest::applyRangeHeadersRange () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.applyRangeHeaders (10, 30, -1, map);
	QCOMPARE(map.value ("Content-Range"), QByteArray ("bytes 10-30/*"));
	QCOMPARE(map.value ("Content-Length"), QByteArray ("20"));
}

void HttpWriterTest::applyRangeHeadersLength () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.applyRangeHeaders (-1, -1, 100, map);
	QVERIFY(!map.contains ("Content-Range"));
	QCOMPARE(map.value ("Content-Length"), QByteArray ("100"));
}

void HttpWriterTest::applyRangeHeadersLengthLeftAlone () {
	HttpWriter writer;
	HttpClient::HeaderMap map { { "Content-Length", "a" } };
	
	writer.applyRangeHeaders (-1, -1, 100, map);
	QVERIFY(!map.contains ("Content-Range"));
	QCOMPARE(map.value ("Content-Length"), QByteArray ("a"));
	
}

void HttpWriterTest::applyRangeHeadersNothing () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.applyRangeHeaders (-1, -1, -1, map);
	QVERIFY(!map.contains ("Content-Range"));
	QVERIFY(!map.contains ("Content-Length"));
}

void HttpWriterTest::addTransferEncodingHeaderSetsChunked () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.addTransferEncodingHeader (HttpClient::ChunkedStreaming, map);
	QCOMPARE(map.value ("Transfer-Encoding"), QByteArray ("chunked"));
	
}

void HttpWriterTest::addTransferEncodingHeaderAddsChunked () {
	HttpWriter writer;
	HttpClient::HeaderMap map = { { "Transfer-Encoding", "gzip" } };
	
	writer.addTransferEncodingHeader (HttpClient::ChunkedStreaming, map);
	QCOMPARE(map.value ("Transfer-Encoding"), QByteArray ("gzip, chunked"));
}

void HttpWriterTest::addTransferEncodingHeaderDoesNothing () {
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.addTransferEncodingHeader (HttpClient::Buffered, map);
	writer.addTransferEncodingHeader (HttpClient::Streaming, map);
	QVERIFY(map.isEmpty ());
}

void HttpWriterTest::addTransferEncodingHeaderContainsChunkedAlready () {
	HttpWriter writer;
	HttpClient::HeaderMap map = { { "Transfer-Encoding", "chunked, gzip" } };
	
	writer.addTransferEncodingHeader (HttpClient::ChunkedStreaming, map);
	QCOMPARE(map.value ("Transfer-Encoding"), QByteArray ("chunked, gzip"));
}

void HttpWriterTest::addConnectionHeader_data () {
	QTest::addColumn< int > ("mode");
	QTest::addColumn< int > ("current");
	QTest::addColumn< int > ("max");
	QTest::addColumn< QString > ("result");
	
	QTest::newRow ("not last request") << 1 << 1 << 2 << "keep-alive";
	QTest::newRow ("unlimited requests") << 1 << 2 << -1 << "keep-alive";
	QTest::newRow ("last request") << 1 << 2 << 2 << "close";
	QTest::newRow ("close") << 0 << 2 << 2 << "close";
	
}

void HttpWriterTest::addConnectionHeader () {
	QFETCH(int, mode);
	QFETCH(int, current);
	QFETCH(int, max);
	QFETCH(QString, result);
	
	HttpWriter writer;
	HttpClient::HeaderMap map;
	
	writer.addConnectionHeader (HttpClient::ConnectionMode (mode), current, max, map);
	QCOMPARE(map.value ("Connection"), result.toLatin1 ());
	
}

QTEST_MAIN(HttpWriterTest)
#include "tst_httpwriter.moc"
