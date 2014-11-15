/* Copyright (c) 2014, The Nuria Project
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *    1. The origin of this software must not be misrepresented; you must not
 *       claim that you wrote the original software. If you use this software
 *       in a product, an acknowledgment in the product documentation would be
 *       appreciated but is not required.
 *    2. Altered source versions must be plainly marked as such, and must not be
 *       misrepresented as being the original software.
 *    3. This notice may not be removed or altered from any source
 *       distribution.
 */

#include <QtTest/QTest>

#include "private/fastcgireader.hpp"
#include <QBuffer>

using namespace Nuria::Internal;

class FastCgiReaderTest : public QObject {
	Q_OBJECT
private slots:
	
	void readRecordHappyPath ();
	void readRecordFails ();
	
	void readNameValuePair_data ();
	void readNameValuePair ();
	
	void readNameValuePairFails_data ();
	void readNameValuePairFails ();
	
	void readAllNameValuePairsHappyPath ();
	void readAllNameValuePairsFails ();
	
	void readBeginRequestBodyHappyPath ();
	void readBeginRequestBodyFails ();
	
};

void FastCgiReaderTest::readRecordHappyPath () {
	FastCgiRecord record;
	QByteArray data ("\x01\x02\x03\x04\x05\x06\x07\x08", 8);
	
	QBuffer buffer (&data);
	buffer.open (QIODevice::ReadOnly);
	
	QVERIFY(FastCgiReader::readRecord (&buffer, record));
	QCOMPARE(record.version, uint8_t (1));
	QCOMPARE(record.type, FastCgiType::AbortRequest);
	QCOMPARE(record.requestId, uint16_t (0x0304));
	QCOMPARE(record.contentLength, uint16_t (0x0506));
	QCOMPARE(record.paddingLength, uint8_t (0x07));
	QCOMPARE(record.reserved, uint8_t (0x08));
	
}


void FastCgiReaderTest::readRecordFails () {
	FastCgiRecord record;
	QByteArray data ("\x01\x02\x03\x04\x05\x06\x07\x08", 7);
	
	QBuffer buffer (&data);
	buffer.open (QIODevice::ReadOnly);
	
	QVERIFY(!FastCgiReader::readRecord (&buffer, record));
}

void FastCgiReaderTest::readNameValuePair_data () {
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

void FastCgiReaderTest::readNameValuePair () {
	QFETCH(QByteArray, header);
	QFETCH(QByteArray, name);
	QFETCH(QByteArray, value);
	
	QByteArray data = "abc" + header + name + value;
	QByteArray outName, outValue;
	int offset = 3;
	
	QVERIFY(FastCgiReader::readNameValuePair (data, outName, outValue, offset));
	QCOMPARE(offset, data.length ());
	QCOMPARE(outName, name);
	QCOMPARE(outValue, value);
}

void FastCgiReaderTest::readNameValuePairFails_data () {
	QTest::addColumn< QByteArray > ("data");
	
	QTest::newRow("empty") << QByteArray ();
	QTest::newRow("premature no value length") << QByteArray ("\x1F", 1);
	QTest::newRow("premature short") << QByteArray ("\x05\x04abc", 5);
	QTest::newRow("premature long") << QByteArray ("\x80\x00\x00\x81\x04abc", 8);
	
}

void FastCgiReaderTest::readNameValuePairFails () {
	QFETCH(QByteArray, data);
	
	QByteArray outName, outValue;
	int offset = 0;
	
	QVERIFY(!FastCgiReader::readNameValuePair (data, outName, outValue, offset));
}

void FastCgiReaderTest::readAllNameValuePairsHappyPath () {
	QByteArray longName1 (129, 'l');
	QByteArray longValue1 (129, 'L');
	QByteArray longName2 (129, 'a');
	QByteArray longValue2 (129, 'b');
	const unsigned char input[] = {
	        0x03, 0x04, '1', '1', '1', 'd', 'e', 'f', 'g',
		0x03, 0x80, 0x00, 0x00, 0x81, '2', '2', '2', 'v', 'v',
		0x80, 0x00, 0x00, 0x81, 0x03, 'n', 'n', 'A', 'B', 'C',
		0x80, 0x00, 0x00, 0x81, 0x80, 0x00, 0x00, 0x81, 'N', 'N', 'V', 'V'
	};
	
	QByteArray data (reinterpret_cast< const char * > (input), sizeof(input));
	data.replace ("vv", longValue1);
	data.replace ("nn", longName1);
	data.replace ("VV", longValue2);
	data.replace ("NN", longName2);
	
	NameValueMap values;
	QVERIFY(FastCgiReader::readAllNameValuePairs (data, values));
	QCOMPARE(values.size (), 4);
	QCOMPARE(values.value ("111"), QByteArray ("defg"));
	QCOMPARE(values.value ("222"), longValue1);
	QCOMPARE(values.value (longName1), QByteArray ("ABC"));
	QCOMPARE(values.value (longName2), longValue2);
	
}

void FastCgiReaderTest::readAllNameValuePairsFails () {
	QByteArray data ("\x03\x03abcdefg", 9); // Trailing byte
	NameValueMap values;
	
	QVERIFY(!FastCgiReader::readAllNameValuePairs (data, values));
}

void FastCgiReaderTest::readBeginRequestBodyHappyPath () {
	QByteArray data ("\x01\x02\x03\x04\x05\x06\x07\x08", 8);
	FastCgiBeginRequestBody body;
	
	QVERIFY(FastCgiReader::readBeginRequestBody (data, body));
	QCOMPARE(body.role, FastCgiRole (0x0102));
	QCOMPARE(body.flags, uint8_t (0x03));
	
}


void FastCgiReaderTest::readBeginRequestBodyFails () {
	QByteArray data ("\x01\x02\x03\x04\x05\x06\x07\x08", 7);
	FastCgiBeginRequestBody body;
	
	QVERIFY(!FastCgiReader::readBeginRequestBody (data, body));
}

QTEST_MAIN(FastCgiReaderTest)
#include "tst_fastcgireader.moc"
