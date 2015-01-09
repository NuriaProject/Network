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

#include <QtTest/QTest>

#include "private/fastcgiwriter.hpp"
#include <QBuffer>

using namespace Nuria::Internal;

class FastCgiWriterTest : public QObject {
	Q_OBJECT
private slots:
	
	void writeNameValuePair_data ();
	void writeNameValuePair ();
	
	void valueMapToBody ();
	
	void writeGetValuesResult ();
	void writeUnknownTypeResult ();
	void writeEndRequest ();
	void writeStreamMessageHappyPath ();
	void writeStreamMessageFails ();
	void writeMultiPartStream ();
};

void FastCgiWriterTest::writeNameValuePair_data () {
	QByteArray shortName ("abc");
	QByteArray shortValue ("def");
	QByteArray longName (129, 'l');
	QByteArray longValue (129, 'L');
	
	QTest::addColumn< QByteArray > ("header");
	QTest::addColumn< QByteArray > ("name");
	QTest::addColumn< QByteArray > ("value");
	
	QTest::newRow("11") << QByteArray ("\x03\x03", 2) << shortName << shortValue;
	QTest::newRow("14") << QByteArray ("\x03\x80\x00\x00\x81", 5) << shortName << longValue;
	QTest::newRow("41") << QByteArray ("\x80\x00\x00\x81\x03", 5) << longName << shortValue;
	QTest::newRow("44") << QByteArray ("\x80\x00\x00\x81\x80\x00\x00\x81", 8) << longName << longValue;
	
}

void FastCgiWriterTest::writeNameValuePair () {
	QFETCH(QByteArray, header);
	QFETCH(QByteArray, name);
	QFETCH(QByteArray, value);
	
	QByteArray expected = header + name + value;
	QByteArray data;
	
	FastCgiWriter::writeNameValuePair (data, name, value);
	QCOMPARE(data, expected);
}

void FastCgiWriterTest::valueMapToBody () {
	QByteArray longName1 (129, 'l');
	QByteArray longValue1 (129, 'L');
	QByteArray longName2 (129, 'a');
	QByteArray longValue2 (129, 'b');
	const unsigned char expectedRaw[] = {
	        0x03, 0x04, '1', '1', '1', 'd', 'e', 'f', 'g',
		0x03, 0x80, 0x00, 0x00, 0x81, '2', '2', '2', 'v', 'v',
		0x80, 0x00, 0x00, 0x81, 0x80, 0x00, 0x00, 0x81, 'N', 'N', 'V', 'V',
	        0x80, 0x00, 0x00, 0x81, 0x03, 'n', 'n', 'A', 'B', 'C'
	};
	
	QByteArray expected (reinterpret_cast< const char * > (expectedRaw), sizeof(expectedRaw));
	expected.replace ("vv", longValue1);
	expected.replace ("nn", longName1);
	expected.replace ("VV", longValue2);
	expected.replace ("NN", longName2);
	
	NameValueMap values { { "111", "defg" }, { "222", longValue1 },
		              { longName1, "ABC" }, { longName2, longValue2 } };
	
	QByteArray result = FastCgiWriter::valueMapToBody (values);
	QCOMPARE(result, expected);
}

void FastCgiWriterTest::writeGetValuesResult () {
	NameValueMap values { { "a", "b" } };
	QByteArray result;
	QBuffer buf (&result);
	buf.open (QIODevice::WriteOnly);
	
	const unsigned char expectedRaw[] = {
	        0x01, 0x0A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, /* record */
	        0x01, 0x01, 'a', 'b' /* data */
	};
	
	// 
	QByteArray expected (reinterpret_cast< const char * > (expectedRaw), sizeof(expectedRaw));
	FastCgiWriter::writeGetValuesResult (&buf, values);
	QCOMPARE(result, expected);
	
}

void FastCgiWriterTest::writeUnknownTypeResult () {
	QByteArray result;
	QBuffer buf (&result);
	buf.open (QIODevice::WriteOnly);
	
	const unsigned char expectedRaw[] = {
	        0x01, 0x0B, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, /* record */
	        0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* data */
	};
	
	// 
	QByteArray expected (reinterpret_cast< const char * > (expectedRaw), sizeof(expectedRaw));
	FastCgiWriter::writeUnknownTypeResult (&buf, FastCgiType::StdErr);
	QCOMPARE(result, expected);
	
}

void FastCgiWriterTest::writeEndRequest () {
	QByteArray result;
	QBuffer buf (&result);
	buf.open (QIODevice::WriteOnly);
	
	const unsigned char expectedRaw[] = {
	        0x01, 0x03, 0xAB, 0xCD, 0x00, 0x08, 0x00, 0x00, /* record */
	        0x12, 0x34, 0x56, 0x78, 0x02, 0x00, 0x00, 0x00  /* data */
	};
	
	// 
	QByteArray expected (reinterpret_cast< const char * > (expectedRaw), sizeof(expectedRaw));
	FastCgiWriter::writeEndRequest (&buf, 0xABCD, 0x12345678, FastCgiProtocolStatus::Overloaded);
	QCOMPARE(result, expected);
	
}

void FastCgiWriterTest::writeStreamMessageHappyPath () {
	QByteArray result;
	QBuffer buf (&result);
	buf.open (QIODevice::WriteOnly);
	
	const unsigned char expectedRaw[] = {
	        0x01, 0x06, 0x12, 0x34, 0x00, 0x03, 0x00, 0x00, /* record */
	        'a', 'b', 'c'  /* data */
	};
	
	// 
	QByteArray expected (reinterpret_cast< const char * > (expectedRaw), sizeof(expectedRaw));
	FastCgiWriter::writeStreamMessage (&buf, FastCgiType::StdOut, 0x1234, "abc");
	QCOMPARE(result, expected);
	
}

void FastCgiWriterTest::writeStreamMessageFails () {
	QByteArray data (0x10000, Qt::Uninitialized);
	QVERIFY(!FastCgiWriter::writeStreamMessage (nullptr, FastCgiType::StdOut, 0, data));
}

void FastCgiWriterTest::writeMultiPartStream () {
	QByteArray body = QByteArray (0xFFFF, 'a') + QByteArray (10, 'b');
	QByteArray result;
	QBuffer buf (&result);
	buf.open (QIODevice::WriteOnly);
	
	const unsigned char expectedRaw[] = {
	        0x01, 0x06, 0x12, 0x34, 0xFF, 0xFF, 0x00, 0x00, /* record */
	        'A', /* data 1 */
	        0x01, 0x06, 0x12, 0x34, 0x00, 0x0A, 0x00, 0x00, /* record */
	        'B' /* data 2 */
	};
	
	// 
	QByteArray expected (reinterpret_cast< const char * > (expectedRaw), sizeof(expectedRaw));
	expected.replace ('A', QByteArray (0xFFFF, 'a')).replace ('B', QByteArray (10, 'b'));
	
	FastCgiWriter::writeMultiPartStream (&buf, FastCgiType::StdOut, 0x1234, body);
	QCOMPARE(result, expected);
	
}

QTEST_MAIN(FastCgiWriterTest)
#include "tst_fastcgiwriter.moc"
