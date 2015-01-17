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

#include "private/websocketwriter.hpp"
#include <QBuffer>

Q_DECLARE_METATYPE(Nuria::Internal::WebSocketFrame);

using namespace Nuria::Internal;
using namespace Nuria;

class WebSocketWriterTest : public QObject {
	Q_OBJECT
private slots:
	
	void serializeFrame_data ();
	void serializeFrame ();
	
	void verifySendToClient ();
	
	void createClosePayload_data ();
	void createClosePayload ();
	
};

void WebSocketWriterTest::serializeFrame_data () {
	QTest::addColumn< WebSocketFrame > ("frame");
	QTest::addColumn< QByteArray > ("expected");
	
	unsigned const char smallData[] = { 0x8C, 0x64 };
	QTest::newRow ("small") << WebSocketFrame { { true, 0, 0, 0, 12, 0, 0 }, 100, 0 }
	                        << QByteArray ((const char *)smallData, sizeof(smallData));
	
	unsigned const char bit16Data[] = { 0x8C, 0x7E, 0x01, 0x00 };
	QTest::newRow ("16bit") << WebSocketFrame { { true, 0, 0, 0, 12, 0, 0 }, 0x100, 0 }
	                        << QByteArray ((const char *)bit16Data, sizeof(bit16Data));
	
	unsigned const char bit64Data[] = { 0x8C, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF };
	QTest::newRow ("64bit") << WebSocketFrame { { true, 0, 0, 0, 12, 0, 0 }, 0x1FFFF, 0 }
	                        << QByteArray ((const char *)bit64Data, sizeof(bit64Data));
	QTest::newRow ("bits1") << WebSocketFrame { { 1, 0, 1, 0, 1, 0, 1 }, 1, 0 }
	                        << QByteArray ("\xA1\x01");
	QTest::newRow ("bits2") << WebSocketFrame { { 0, 1, 0, 1, 0, 1, 0 }, 1, 0 }
	                        << QByteArray ("\x50\x81");
	
}

void WebSocketWriterTest::serializeFrame () {
	QFETCH(Nuria::Internal::WebSocketFrame, frame);
	QFETCH(QByteArray, expected);
	
	QCOMPARE(WebSocketWriter::serializeFrame (frame), expected);
}

void WebSocketWriterTest::verifySendToClient () {
	QBuffer buf;
	buf.open (QIODevice::WriteOnly);
	
	QByteArray body = "nuria";
	WebSocketWriter::sendToClient (&buf, true, WebSocketOpcode::Pong, body.constData (), body.length ());
	buf.close ();
	
	QCOMPARE(buf.data (), QByteArray ("\x8A\x05nuria"));
}

void WebSocketWriterTest::createClosePayload_data () {
	QTest::addColumn< int > ("code");
	QTest::addColumn< QByteArray > ("message");
	QTest::addColumn< QByteArray > ("expected");
	
	QTest::newRow ("no-body") << -1 << QByteArray () << QByteArray ();
	QTest::newRow ("only-code") << 1 << QByteArray () << QByteArray ("\x00\x01", 2);
	QTest::newRow ("with-msg") << 10 << QByteArray ("test") << QByteArray ("\x00\x0Atest", 6);
}

void WebSocketWriterTest::createClosePayload () {
	QFETCH(int, code);
	QFETCH(QByteArray, message);
	QFETCH(QByteArray, expected);
	
	QCOMPARE(WebSocketWriter::createClosePayload (code, message), expected);
}

QTEST_MAIN(WebSocketWriterTest)
#include "tst_websocketwriter.moc"
