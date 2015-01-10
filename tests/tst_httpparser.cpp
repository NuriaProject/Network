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

#include <nuria/httpparser.hpp>

#include <QtTest/QtTest>
#include <QObject>

using namespace Nuria;

class HttpParserTest : public QObject {
	Q_OBJECT
private slots:
	
	void removeTrailingNewline ();
	
	void parseFirstLineHappyPath ();
	void parseFirstLineBadData_data ();
	void parseFirstLineBadData ();
	
	void parseHeaderLineHappyPath_data ();
	void parseHeaderLineHappyPath ();
	void parseHeaderLineBadData_data ();
	void parseHeaderLineBadData ();
	
	void parseVersion_data ();
	void parseVersion ();
	
	void parseVerb_data ();
	void parseVerb ();
	
	void parseRangeHeaderValueHappyPath ();
	void parseRangeHeaderValueOpenEnd ();
	void parseRangeHeaderValueBadData_data ();
	void parseRangeHeaderValueBadData ();
	
	void correctHeaderKeyCase_data ();
	void correctHeaderKeyCase ();
	
	void parseFirstLineFullHappyPath ();
	void parseFirstLineFullBadData_data ();
	void parseFirstLineFullBadData ();
	
	void parseCookie_data ();
	void parseCookie ();
	
	void parseCookieFails_data ();
	void parseCookieFails ();
};

void HttpParserTest::removeTrailingNewline () {
	HttpParser parser;
	
	QByteArray valueRn ("bla\r\n");
	QByteArray valueN ("bla\n");
	QByteArray value ("bla");
	
	QVERIFY(parser.removeTrailingNewline (valueRn));
	QVERIFY(parser.removeTrailingNewline (valueN));
	QVERIFY(!parser.removeTrailingNewline (value));
	QCOMPARE(valueRn, QByteArray ("bla"));
	QCOMPARE(valueN, QByteArray ("bla"));
	QCOMPARE(value, QByteArray ("bla"));
}

void HttpParserTest::parseFirstLineHappyPath () {
	HttpParser parser;
	QByteArray data = "GET foo/bar HTTP/1.1";
	
	QByteArray verb;
	QByteArray path;
	QByteArray version;
	QVERIFY(parser.parseFirstLine (data, verb, path, version));
	
	QCOMPARE(verb, QByteArray ("GET"));
	QCOMPARE(path, QByteArray ("foo/bar"));
	QCOMPARE(version, QByteArray ("1.1"));
}

void HttpParserTest::parseFirstLineBadData_data () {
	QTest::addColumn< QString > ("line");
	
	QTest::newRow ("empty") << "";
	QTest::newRow ("verb") << " foo/bar HTTP/1.0";
	QTest::newRow ("empty path") << "GET  HTTP/1.0";
	QTest::newRow ("empty version") << "GET foo/bar ";
	QTest::newRow ("version") << "GET foo/bar";
	QTest::newRow ("broken version") << "GET foo/bar 1.1";
	
}

void HttpParserTest::parseFirstLineBadData () {
	HttpParser parser;
	QFETCH(QString, line);
	QByteArray data = line.toLatin1 ();
	
	QByteArray verb;
	QByteArray path;
	QByteArray version;
	QVERIFY(!parser.parseFirstLine (data, verb, path, version));
}

void HttpParserTest::parseHeaderLineHappyPath_data () {
	QTest::addColumn< QString > ("line");
	
	QTest::newRow ("colon") << "Foo: Bar: Baz";
	QTest::newRow ("colon space") << "Foo:Bar: Baz";
}

void HttpParserTest::parseHeaderLineHappyPath () {
	HttpParser parser;
	QFETCH(QString, line);
	QByteArray data = line.toLatin1 ();
	
	QByteArray name;
	QByteArray value;
	QVERIFY(parser.parseHeaderLine (data, name, value));
	
	QCOMPARE(name, QByteArray("Foo"));
	QCOMPARE(value, QByteArray("Bar: Baz"));
}

void HttpParserTest::parseHeaderLineBadData_data () {
	QTest::addColumn< QString > ("line");
	
	QTest::newRow ("empty") << "";
	QTest::newRow ("no colon") << "Foo=Bar";
	QTest::newRow ("empty name") << ": ";
	QTest::newRow ("empty value") << "Foo: ";
}

void HttpParserTest::parseHeaderLineBadData () {
	HttpParser parser;
	QFETCH(QString, line);
	QByteArray data = line.toLatin1 ();
	
	QByteArray name;
	QByteArray value;
	QVERIFY(!parser.parseHeaderLine (data, name, value));
}

void HttpParserTest::parseVersion_data () {
	QTest::addColumn< QString > ("version");
	QTest::addColumn< HttpClient::HttpVersion > ("result");
	
	QTest::newRow ("0.9") << "0.9" << HttpClient::HttpUnknown;
	QTest::newRow ("1.0") << "1.0" << HttpClient::Http1_0;
	QTest::newRow ("1.1") << "1.1" << HttpClient::Http1_1;
	QTest::newRow ("2.0") << "2.0" << HttpClient::HttpUnknown;
}

void HttpParserTest::parseVersion () {
	HttpParser parser;
	QFETCH(QString, version);
	QFETCH(HttpClient::HttpVersion, result);
	QByteArray data = version.toLatin1 ();
	
	QCOMPARE(parser.parseVersion (data), result);
	
}

void HttpParserTest::parseVerb_data () {
	QTest::addColumn< QString > ("verb");
	QTest::addColumn< HttpClient::HttpVerb > ("result");
	
	QTest::newRow ("GET") << "GET" << HttpClient::GET;
	QTest::newRow ("POST") << "POST" << HttpClient::POST;
	QTest::newRow ("HEAD") << "HEAD" << HttpClient::HEAD;
	QTest::newRow ("PUT") << "PUT" << HttpClient::PUT;
	QTest::newRow ("DELETE") << "DELETE" << HttpClient::DELETE;
	QTest::newRow ("invalid") << "get" << HttpClient::InvalidVerb;
}

void HttpParserTest::parseVerb () {
	HttpParser parser;
	QFETCH(QString, verb);
	QFETCH(HttpClient::HttpVerb, result);
	QByteArray data = verb.toLatin1 ();
	
	QCOMPARE(parser.parseVerb (data), result);
	
}

void HttpParserTest::parseRangeHeaderValueHappyPath () {
	HttpParser parser;
	QByteArray data = "bytes=50-60";
	
	qint64 begin = 0;
	qint64 end = 0;
	QVERIFY(parser.parseRangeHeaderValue (data, begin, end));
	
	QCOMPARE(begin, 50);
	QCOMPARE(end, 60);
}

void HttpParserTest::parseRangeHeaderValueOpenEnd () {
	HttpParser parser;
	QByteArray data = "bytes=50-";
	
	qint64 begin = 0;
	qint64 end = 0;
	QVERIFY(parser.parseRangeHeaderValue (data, begin, end));
	
	QCOMPARE(begin, 50);
	QCOMPARE(end, -1);
}

void HttpParserTest::parseRangeHeaderValueBadData_data () {
	QTest::addColumn< QString > ("range");
	
	QTest::newRow ("empty") << "";
	QTest::newRow ("no bytes=") << "12-34";
	QTest::newRow ("missing minus") << "bytes=1234";
	QTest::newRow ("missing begin") << "bytes=-34";
	QTest::newRow ("bad begin") << "bytes=0xaf-500";
	QTest::newRow ("bad end") << "bytes=12-0xaf";
	QTest::newRow ("begin < end") << "bytes=34-12";
	QTest::newRow ("begin == end") << "bytes=12-12";
	QTest::newRow ("end is negative") << "bytes=0--12";
	
}

void HttpParserTest::parseRangeHeaderValueBadData () {
	HttpParser parser;
	QFETCH(QString, range);
	QByteArray data = range.toLatin1 ();
	
	qint64 begin = 0;
	qint64 end = 0;
	QVERIFY(!parser.parseRangeHeaderValue (data, begin, end));
}

void HttpParserTest::correctHeaderKeyCase_data () {
	QTest::addColumn< QString > ("header");
	QTest::addColumn< QString > ("result");
	
	QTest::newRow ("no change") << "Content-Length" << "Content-Length";
	QTest::newRow ("lowercase") << "content-length" << "Content-Length";
	QTest::newRow ("all upper") << "CONTENT-LENGTH" << "CONTENT-LENGTH";
	
}

void HttpParserTest::correctHeaderKeyCase () {
	HttpParser parser;
	QFETCH(QString, header);
	QFETCH(QString, result);
	
	QCOMPARE(parser.correctHeaderKeyCase (header.toLatin1 ()), result.toLatin1 ());
}

void HttpParserTest::parseFirstLineFullHappyPath () {
	HttpParser parser;
	QByteArray line = "PUT /foo/bar HTTP/1.1";
	
	QByteArray path;
	HttpClient::HttpVerb verb;
	HttpClient::HttpVersion version;
	QVERIFY(parser.parseFirstLineFull (line, verb, path, version));
	
	QCOMPARE(path, QByteArray ("/foo/bar"));
	QCOMPARE(verb, HttpClient::PUT);
	QCOMPARE(version, HttpClient::Http1_1);
}

void HttpParserTest::parseFirstLineFullBadData_data () {
	QTest::addColumn< QString > ("line");
	
	QTest::newRow ("empty") << "";
	QTest::newRow ("bad verb") << "YAY /foo/bar HTTP/1.1";
	QTest::newRow ("bad path") << "GET  HTTP/1.1";
	QTest::newRow ("bad version") << "GET /foo/bar HTTP/2.0";
}

void HttpParserTest::parseFirstLineFullBadData () {
	HttpParser parser;
	QFETCH(QString, line);
	QByteArray lineData = line.toLatin1 ();
	
	QByteArray path;
	HttpClient::HttpVerb verb;
	HttpClient::HttpVersion version;
	QVERIFY(!parser.parseFirstLineFull (lineData, verb, path, version));
	
}

void HttpParserTest::parseCookie_data () {
	QTest::addColumn< QString > ("data");
	QTest::addColumn< QStringList > ("names");
	QTest::addColumn< QStringList > ("values");
	
	QString withoutDquotes1 = "foo=bar";
	QString withoutDquotes2 = "bar=baz";
	QString withDquotes1 = "nuria=\"project;\"";
	QString withDquotes2 = "some=\"thing;\"";
	
	QTest::newRow ("no-dquote") << withoutDquotes1 << QStringList { "foo" } << QStringList { "bar" };
	QTest::newRow ("2x no-dquote") << withoutDquotes1 + ";" + withoutDquotes2 << QStringList { "foo", "bar" }
	                               << QStringList { "bar", "baz" };
	QTest::newRow ("dquote") << withDquotes1 << QStringList { "nuria" } << QStringList { "project;" };
	QTest::newRow ("2x dquote") << withDquotes1 + ";" + withDquotes2 << QStringList { "nuria", "some" }
	                            << QStringList { "project;", "thing;" };
	QTest::newRow ("no-dquote dquote") << withoutDquotes1 + ";" + withDquotes1 << QStringList { "foo", "nuria" }
	                                   << QStringList { "bar", "project;" };
	QTest::newRow ("dquote no-dquote") << withDquotes1 + ";" + withoutDquotes1 << QStringList { "foo", "nuria" }
	                                   << QStringList { "bar", "project;" };
	QTest::newRow ("no-dquote encoded") << "foo=ba%20r" << QStringList { "foo" } << QStringList { "ba r" };
	QTest::newRow ("dquote encoded") << "foo=\"ba%20r\"" << QStringList { "foo" } << QStringList { "ba r" };
	
}

void HttpParserTest::parseCookie () {
	QFETCH(QString, data);
	QFETCH(QStringList, names);
	QFETCH(QStringList, values);
	
	HttpParser parser;
	HttpClient::Cookies map;
	QVERIFY(parser.parseCookies (data.toLatin1 (), map));
	QCOMPARE(map.size (), names.length ());
	
	for (int i = 0; i < names.length (); i++) {
		QNetworkCookie cookie = map.value (names.at (i).toLatin1 ());
		QCOMPARE(cookie.name (), names.at (i).toLatin1 ());
		QCOMPARE(cookie.value (), values.at (i).toLatin1 ());
	}
	
}

void HttpParserTest::parseCookieFails_data () {
	QTest::addColumn< QString > ("data");
	
	QTest::newRow ("no ending dquote") << "foo=\"bar";
	QTest::newRow ("no ; after dquote") << "foo=\"bar\"nuria=project";
	QTest::newRow ("no value end") << "foo=bar;nuria";
	QTest::newRow ("no value front") << "foo;nuria=project";
	
}

void HttpParserTest::parseCookieFails () {
	QFETCH(QString, data);
	
	HttpParser parser;
	HttpClient::Cookies map;
	QVERIFY(!parser.parseCookies (data.toLatin1 (), map));
}

QTEST_MAIN(HttpParserTest)
#include "tst_httpparser.moc"
