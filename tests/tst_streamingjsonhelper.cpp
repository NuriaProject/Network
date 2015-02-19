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

#include <private/streamingjsonhelper.hpp>
#include <QtTest/QtTest>

using namespace Nuria::Internal;

class StreamingJsonHelperTest : public QObject {
	Q_OBJECT
private slots:
	
	void verifyOneElement ();
	void verifyTwoElements ();
	
	void rootMayContainAnyValue_data ();
	void rootMayContainAnyValue ();
	
	void prematureForIncompleteElement ();
	void jsonErrorForBrokenJsonData ();
	
	void stringMatching_data ();
	void stringMatching ();
	
	void streamData_data ();
	void streamData ();
	
	void benchmark ();
	
};

void StreamingJsonHelperTest::verifyOneElement () {
	QByteArray data = "{\"foo\": { \"inner\\\\\": [1,2,3]}}";
	
	StreamingJsonHelper h;
	QCOMPARE(h.appendData (data), StreamingJsonHelper::ElementComplete);
	QVERIFY(h.hasWaitingElement ());
	QCOMPARE(h.nextWaitingElement (), data);
	QVERIFY(!h.hasWaitingElement ());
	
}

void StreamingJsonHelperTest::verifyTwoElements () {
	QByteArray first = "{\"foo\": { \"inner\\\\\": [1,2,3]}}";
	QByteArray second = "[42,1337,\"\",{}]";
	QByteArray data = first + second;
	
	StreamingJsonHelper h;
	QCOMPARE(h.appendData (data), StreamingJsonHelper::ElementComplete);
	QVERIFY(h.hasWaitingElement ());
	QCOMPARE(h.nextWaitingElement (), first);
	QVERIFY(h.hasWaitingElement ());
	QCOMPARE(h.nextWaitingElement (), second);
	QVERIFY(!h.hasWaitingElement ());
	
}

void StreamingJsonHelperTest::rootMayContainAnyValue_data () {
	QTest::addColumn< QString > ("data");
	QTest::newRow ("string") << "\"foo\"";
	QTest::newRow ("number") << "12.34e5";
	QTest::newRow ("object") << "{}";
	QTest::newRow ("array") << "[]";
	QTest::newRow ("true") << "true";
	QTest::newRow ("false") << "false";
	QTest::newRow ("null") << "null";
}

void StreamingJsonHelperTest::rootMayContainAnyValue () {
	QFETCH(QString, data);
	QByteArray json = data.toLatin1 ();
	
	StreamingJsonHelper h;
	QCOMPARE(h.appendData (json), StreamingJsonHelper::ElementComplete);
	QVERIFY(h.hasWaitingElement ());
	QCOMPARE(h.nextWaitingElement (), json);
	QVERIFY(!h.hasWaitingElement ());
	
}

void StreamingJsonHelperTest::prematureForIncompleteElement () {
	QByteArray data = "{\"foo\": { ";
	
	StreamingJsonHelper h;
	QCOMPARE(h.appendData (data), StreamingJsonHelper::Premature);
	QVERIFY(!h.hasWaitingElement ());
	
}

void StreamingJsonHelperTest::jsonErrorForBrokenJsonData () {
	QByteArray data = "{ \"[\": ] ";
	
	StreamingJsonHelper h;
	QCOMPARE(h.appendData (data), StreamingJsonHelper::JsonError);
	QVERIFY(!h.hasWaitingElement ());
	
}

void StreamingJsonHelperTest::stringMatching_data () {
	QTest::addColumn< QString > ("data");
	
	QTest::newRow ("simple") << "abc123";
	QTest::newRow ("escaped-quote") << "\\\"";
	QTest::newRow ("escaped-backslash") << "\\\\";
	QTest::newRow ("escaped-backslash-quote") << "\\\\\\\"";
	QTest::newRow ("square-bracket-open") << "[";
	QTest::newRow ("square-bracket-close") << "]";
	QTest::newRow ("curly-bracket-open") << "{";
	QTest::newRow ("curly-bracket-close") << "}";
	
}

void StreamingJsonHelperTest::stringMatching () {
	QFETCH(QString, data);
	
	QByteArray str = data.toLatin1 ();
	str.prepend ('"');
	str.append ('"');
	
	StreamingJsonHelper h;
	QCOMPARE(h.appendData (str), StreamingJsonHelper::ElementComplete);
	QVERIFY(h.hasWaitingElement ());
	QCOMPARE(h.nextWaitingElement (), str);
	QVERIFY(!h.hasWaitingElement ());
}

void StreamingJsonHelperTest::streamData_data () {
	QTest::addColumn< int > ("chunkSize");
	
	QTest::newRow ("100") << 100;
	QTest::newRow ("50") << 50;
	QTest::newRow ("25") << 25;
	QTest::newRow ("12") << 12;
	QTest::newRow ("10") << 10;
	QTest::newRow ("9") << 9;
	QTest::newRow ("8") << 8;
	QTest::newRow ("7") << 7;
	QTest::newRow ("6") << 6;
	QTest::newRow ("5") << 5;
	QTest::newRow ("4") << 4;
	QTest::newRow ("3") << 3;
	QTest::newRow ("2") << 2;
	QTest::newRow ("1") << 1;
	
}

void StreamingJsonHelperTest::streamData () {
	QByteArray first = "{\"stuff\": [\"foo\",\"bar\",\"baz\",\"nuria\",\"project\"]}";
	QByteArray second = "[\"foo\\\"}\", {\"bar[\": [123,123,123]},\"asd\":\"\\\\}]\\\"\"]";
	QByteArray elem = first + second;
	
	QFETCH(int, chunkSize);
	
	StreamingJsonHelper h;
	while (!elem.isEmpty ()) {
		h.appendData (elem.left (chunkSize));
		elem = elem.mid (chunkSize);
	}
	
	// 
	QVERIFY(h.hasWaitingElement ());
	QCOMPARE(h.nextWaitingElement (), first);
	QVERIFY(h.hasWaitingElement ());
	QCOMPARE(h.nextWaitingElement (), second);
	QVERIFY(!h.hasWaitingElement ());
	
}

void StreamingJsonHelperTest::benchmark () {
	QByteArray elem = "[{\"stuff\": [\"foo\",\"bar\",\"baz\",\"nuria\",\"project\"]},"
	                  "{\"foo\\\"}\": {\"bar[\": [123,123,123]},\"asd\":\"\\\\}]\\\"\"}]";
	QByteArray json = elem + elem + elem + elem;
	json = json + json + json + json;
	json = json + json + json + json; // Creates ca. 7000 Bytes of data.
	
	QBENCHMARK {
		StreamingJsonHelper h;
		h.appendData (json);
	}
	
}

QTEST_MAIN(StreamingJsonHelperTest)
#include "tst_streamingjsonhelper.moc"
