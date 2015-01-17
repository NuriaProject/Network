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

#include "private/websocketreader.hpp"
#include <QBuffer>

Q_DECLARE_METATYPE(Nuria::HttpClient::HeaderMap);
Q_DECLARE_METATYPE(Nuria::Internal::WebSocketFrame);

using namespace Nuria::Internal;
using namespace Nuria;

class WebSocketReaderTest : public QObject {
	Q_OBJECT
private slots:
	
	void verifyAreHttpRequirementsFulfilled ();
	
	void isWebSocketRequestHappyPath ();
	void isWebSocketRequestFails_data ();
	void isWebSocketRequestFails ();
	
	void verifyGenerateHandshakeKey ();
	
	void verifySizeOfFrame_data ();
	void verifySizeOfFrame ();
	
	void verifyReadFrameData_data ();
	void verifyReadFrameData ();
	void readFrameDataBitsAreReadInCorrectOrder ();
	
	void verifyPayloadLength ();
	
	void isLegalClientPacket_data ();
	void isLegalClientPacket ();
	
	void isLegalCloseCode_data ();
	void isLegalCloseCode ();
	
	void maskPayload ();
	
};

void WebSocketReaderTest::verifyAreHttpRequirementsFulfilled () {
	QVERIFY(WebSocketReader::areHttpRequirementsFulfilled (HttpClient::Http1_1, HttpClient::GET));
	
	QVERIFY(!WebSocketReader::areHttpRequirementsFulfilled (HttpClient::Http1_0, HttpClient::GET));
	QVERIFY(!WebSocketReader::areHttpRequirementsFulfilled (HttpClient::HttpUnknown, HttpClient::GET));
	QVERIFY(!WebSocketReader::areHttpRequirementsFulfilled (HttpClient::Http1_1, HttpClient::POST));
	QVERIFY(!WebSocketReader::areHttpRequirementsFulfilled (HttpClient::Http1_1, HttpClient::PUT));
	QVERIFY(!WebSocketReader::areHttpRequirementsFulfilled (HttpClient::Http1_1, HttpClient::DELETE));
	QVERIFY(!WebSocketReader::areHttpRequirementsFulfilled (HttpClient::Http1_1, HttpClient::InvalidVerb));
}

void WebSocketReaderTest::isWebSocketRequestHappyPath () {
	HttpClient::HeaderMap map {
		{ "Upgrade", "websocket-something" },
		{ "Connection", "Upgrade" },
		{ "Sec-WebSocket-Key", "notEmpty" },
		// { "Sec-WebSocket-Protocol", "notEmpty" }, // Not mandatory
		{ "Sec-WebSocket-Version", "13" }
	};
	
	QVERIFY(WebSocketReader::isWebSocketRequest (map));
}

void WebSocketReaderTest::isWebSocketRequestFails_data () {
	QTest::addColumn< HttpClient::HeaderMap > ("map");
	
	QTest::newRow ("bad-upgrade") << HttpClient::HeaderMap {
		{ "Upgrade", "foobar" }, { "Connection", "Upgrade" }, { "Sec-WebSocket-Key", "notEmpty" },
		{ "Sec-WebSocket-Protocol", "notEmpty" }, { "Sec-WebSocket-Version", "13" }
	};
	
	QTest::newRow ("bad-connection") << HttpClient::HeaderMap {
		{ "Upgrade", "websocket" }, { "Connection", "keep-alive" }, { "Sec-WebSocket-Key", "notEmpty" },
		{ "Sec-WebSocket-Protocol", "notEmpty" }, { "Sec-WebSocket-Version", "13" }
	};
	
	QTest::newRow ("empty-key") << HttpClient::HeaderMap {
		{ "Upgrade", "websocket" }, { "Connection", "Upgrade" }, { "Sec-WebSocket-Key", "" },
		{ "Sec-WebSocket-Protocol", "notEmpty" }, { "Sec-WebSocket-Version", "13" }
	};
	
	QTest::newRow ("bad-version") << HttpClient::HeaderMap {
		{ "Upgrade", "websocket" }, { "Connection", "Upgrade" }, { "Sec-WebSocket-Key", "notEmpty" },
		{ "Sec-WebSocket-Protocol", "notEmpty" }, { "Sec-WebSocket-Version", "14" }
	};
	
}

void WebSocketReaderTest::isWebSocketRequestFails () {
	QFETCH(Nuria::HttpClient::HeaderMap, map);
	QVERIFY(!WebSocketReader::isWebSocketRequest (map));
}

void WebSocketReaderTest::verifyGenerateHandshakeKey () {
	QCOMPARE(WebSocketReader::generateHandshakeKey ("NuriaProject"),
	         QByteArray ("7BXe1ry4ymrcX4pKIcw/NOvfHNI="));
}

static WebSocketFrame getFrame (bool mask, uint8_t size, uint64_t extended = 0) {
	WebSocketFrame f;
	
	f.base.mask = mask;
	f.base.payloadLen = size;
	f.extPayloadLen = extended;
	
	return f;
}

void WebSocketReaderTest::verifySizeOfFrame_data () {
	QTest::addColumn< WebSocketFrame > ("frame");
	QTest::addColumn< int > ("size");
	
	QTest::newRow ("base") << getFrame (false, 1) << 2;
	QTest::newRow ("mask") << getFrame (true, 1) << 6;
	QTest::newRow ("16bit") << getFrame (false, 126) << 4;
	QTest::newRow ("16bit-mask") << getFrame (true, 126) << 8;
	QTest::newRow ("64bit") << getFrame (false, 127) << 10;
	QTest::newRow ("64bit-mask") << getFrame (true, 127) << 14;
	
}

void WebSocketReaderTest::verifySizeOfFrame () {
	QFETCH(Nuria::Internal::WebSocketFrame, frame);
	QFETCH(int, size);
	
	QCOMPARE(WebSocketReader::sizeOfFrame (frame), size);
}

void WebSocketReaderTest::verifyReadFrameData_data () {
	QTest::addColumn< bool > ("success");
	QTest::addColumn< QByteArray > ("data");
	QTest::addColumn< ulong > ("mask");
	QTest::addColumn< int > ("len");
	QTest::addColumn< qulonglong > ("extLen");
	
	QTest::newRow ("empty") << false << QByteArray () << 0UL << 0 << 0ULL;
	QTest::newRow ("1byte") << false << QByteArray ("A") << 0UL << 0 << 0ULL;
	QTest::newRow ("small") << true << QByteArray ("\xFF\x05") << 0UL << 5 << 5ULL;
	QTest::newRow ("16bit") << true << QByteArray ("\xFF\x7E\x12\x34") << 0UL << 126 << 0x1234ULL;
	QTest::newRow ("16bit-short") << false << QByteArray ("\xFF\x7E\x12") << 0UL << 5 << 0ULL;
	QTest::newRow ("64bit") << true << QByteArray ("\xFF\x7F\x12\x34\x56\x78\x9A\xBC\xDE\xF0")
	                        << 0UL << 127 << 0x123456789ABCDEF0ULL;
	QTest::newRow ("64bit-short") << false << QByteArray ("\xFF\x7F\x12\x34\x56\x78\x9A\xBC\xDE")
	                              << 0UL << 127 << 0ULL;
	QTest::newRow ("mask") << true << QByteArray ("\xFF\x85\x89\xAB\xCD\xEF") << 0xEFCDAB89UL << 5 << 5ULL;
	QTest::newRow ("mask-short") << false << QByteArray ("\xFF\x85\x89\xAB\xCD") << 0UL << 5 << 0ULL;
	QTest::newRow ("16bit-mask") << true << QByteArray ("\xFF\xFE\x12\x34\x89\xAB\xCD\xEF")
	                             << 0xEFCDAB89UL << 126 << 0x1234ULL;
	QTest::newRow ("16bit-mask-short") << false << QByteArray ("\xFF\xFE\x12\x34\x89\xAB\xCD") << 0UL << 0 << 0ULL;
	QTest::newRow ("64bit-mask") << true << QByteArray ("\xFF\xFF\x12\x34\x56\x78\x9A\xBC\xDE\xF0\x89\xAB\xCD\xEF")
	                             << 0xEFCDAB89UL << 127 << 0x123456789ABCDEF0ULL;
	QTest::newRow ("64bit-mask-short")
	                << false << QByteArray ("\xFF\xFF\x12\x34\x56\x78\x9A\xBC\xDE\xF0\x89\xAB\xCD")
	                << 0UL << 0 << 0ULL;
	
}

void WebSocketReaderTest::verifyReadFrameData () {
	QFETCH(bool, success);
	QFETCH(QByteArray, data);
	QFETCH(ulong, mask);
	QFETCH(int, len);
	QFETCH(qulonglong, extLen);
	
	// 
	WebSocketFrame frame;
	QCOMPARE(WebSocketReader::readFrameData (data, frame), success);
	
	if (success) {
		QCOMPARE(frame.maskKey, uint32_t (mask));
		QCOMPARE(frame.base.payloadLen, uint8_t (len));
		QCOMPARE(frame.extPayloadLen, uint64_t (extLen));
	}
	
}

static bool operator== (const WebSocketFrame &l, const WebSocketFrame &r) {
	return reinterpret_cast< const uint16_t & > (l) == reinterpret_cast< const uint16_t & > (r);
}

void WebSocketReaderTest::readFrameDataBitsAreReadInCorrectOrder () {
	WebSocketFrame frame;
	WebSocketFrame odd { { 1, 0, 1, 0, 1, 0, 1 }, 1, 0 };
	WebSocketFrame even { { 0, 1, 0, 1, 0, 1, 1 }, 1, 0x7A7A7A7A };
	
	QVERIFY(WebSocketReader::readFrameData (QByteArray ("\xA1\x01"), frame));
	QCOMPARE(frame, odd);
	
	// Extra bytes for the mask
	QVERIFY(WebSocketReader::readFrameData (QByteArray ("\x50\x81zzzz"), frame));
	QCOMPARE(frame, even);
}

void WebSocketReaderTest::verifyPayloadLength () {
	QCOMPARE(WebSocketReader::payloadLength (getFrame (false, 125, 0)), 125);
	QCOMPARE(WebSocketReader::payloadLength (getFrame (false, 126, 1024)), 1024);
	QCOMPARE(WebSocketReader::payloadLength (getFrame (false, 127, 2048)), 2048);
}

void WebSocketReaderTest::isLegalClientPacket_data () {
	QTest::addColumn< bool > ("success");
	QTest::addColumn< WebSocketFrame > ("frame");
	
	QTest::newRow ("legal") << true << WebSocketFrame { { 0, 0, 0, 0, 0, 1, 5 }, 0, 0 };
	QTest::newRow ("legal-16bit") << true << WebSocketFrame { { 0, 0, 0, 0, 0, 1, 126 }, 500, 0 };
	QTest::newRow ("legal-64bit") << true << WebSocketFrame { { 0, 0, 0, 0, 0, 1, 127 }, 500, 0 };
	QTest::newRow ("legal-control") << true << WebSocketFrame { { 1, 0, 0, 0, 0xA, 1, 125 }, 0, 0 };
	QTest::newRow ("legal-empty") << true << WebSocketFrame { { 1, 0, 0, 0, 8, 0, 0 }, 0, 0 };
	QTest::newRow ("bad-rsv1") << false << WebSocketFrame { { 0, 1, 0, 0, 0, 1, 100 }, 0, 0 };
	QTest::newRow ("bad-rsv2") << false << WebSocketFrame { { 0, 0, 1, 0, 0, 1, 100 }, 0, 0 };
	QTest::newRow ("bad-rsv3") << false << WebSocketFrame { { 0, 0, 0, 1, 0, 1, 100 }, 0, 0 };
	QTest::newRow ("bad-hardlimit") << false << WebSocketFrame { { 0, 0, 0, 0, 0, 1, 127 }, (64 << 20) + 1, 0 };
	QTest::newRow ("bad-useless-extended") << false << WebSocketFrame { { 0, 0, 0, 0, 0, 1, 127 }, 100, 0 };
	QTest::newRow ("bad-reserved-3") << false << WebSocketFrame { { 0, 0, 0, 0, 3, 1, 120 }, 0, 0 };
	QTest::newRow ("bad-reserved-7") << false << WebSocketFrame { { 0, 0, 0, 0, 7, 1, 120 }, 0, 0 };
	QTest::newRow ("bad-reserved-b") << false << WebSocketFrame { { 0, 0, 0, 0, 0xB, 1, 120 }, 0, 0 };
	QTest::newRow ("bad-reserved-f") << false << WebSocketFrame { { 0, 0, 0, 0, 0xF, 1, 120 }, 0, 0 };
	QTest::newRow ("bad-control-length") << false << WebSocketFrame { { 1, 0, 0, 0, 0xA, 1, 126 }, 0, 0 };
	QTest::newRow ("bad-control-fin") << false << WebSocketFrame { { 0, 0, 0, 0, 0xA, 0, 0 }, 0, 0 };
	
}

void WebSocketReaderTest::isLegalClientPacket () {
	QFETCH(bool, success);
	QFETCH(Nuria::Internal::WebSocketFrame, frame);
	
	QCOMPARE(WebSocketReader::isLegalClientPacket (frame), success);
}

void WebSocketReaderTest::isLegalCloseCode_data () {
	QTest::addColumn< bool > ("legal");
	QTest::addColumn< int > ("code");
	
	QTest::newRow ("internal") << true << -1;
	QTest::newRow ("1000") << true << 1000;
	QTest::newRow ("1001") << true << 1001;
	QTest::newRow ("1002") << true << 1002;
	QTest::newRow ("1003") << true << 1003;
	QTest::newRow ("1004") << false << 1004;
	QTest::newRow ("1005") << false << 1005;
	QTest::newRow ("1006") << false << 1006;
	QTest::newRow ("1007") << true << 1007;
	QTest::newRow ("1008") << true << 1008;
	QTest::newRow ("1009") << true << 1009;
	QTest::newRow ("1010") << true << 1010;
	QTest::newRow ("1011") << true << 1011;
	QTest::newRow ("1012") << false << 1012;
	QTest::newRow ("2000") << false << 2000;
	QTest::newRow ("3000") << true << 3000;
	QTest::newRow ("3999") << true << 3999;
	QTest::newRow ("4000") << true << 4000;
	QTest::newRow ("4999") << true << 4999;
	QTest::newRow ("5000") << false << 5000;
	
}

void WebSocketReaderTest::isLegalCloseCode () {
	QFETCH(bool, legal);
	QFETCH(int, code);
	
	QCOMPARE(WebSocketReader::isLegalCloseCode (code), legal);
}

void WebSocketReaderTest::maskPayload () {
	QByteArray benchData = QByteArray ("ABCDEFGH").repeated (8); // 64 Bytes in length
	QByteArray data = "12345";
	
	// 
	WebSocketReader::maskPayload (0x11223344UL, data);
	WebSocketReader::maskPayload (0x11223344UL, data);
	QCOMPARE(data, QByteArray ("12345"));
	
	QBENCHMARK {
		WebSocketReader::maskPayload (0x11223344UL, benchData);
	}
	
}

QTEST_MAIN(WebSocketReaderTest)
#include "tst_websocketreader.moc"
