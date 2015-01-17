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
#include <nuria/websocket.hpp>

#include <QtTest/QtTest>
#include <QObject>

#include "private/websocketreader.hpp"
#include "httpmemorytransport.hpp"
#include <nuria/httpclient.hpp>
#include <nuria/httpserver.hpp>
#include <nuria/httpwriter.hpp>
#include <nuria/callback.hpp>
#include <nuria/httpnode.hpp>

Q_DECLARE_METATYPE(Nuria::WebSocket::Mode);

using namespace Nuria;

// 
class WebSocketTest : public QObject {
	Q_OBJECT
private slots:
	
	void initTestCase ();
	void init ();
	
	void connectToWebSocket ();
	
	void clientClosingEmitsConnectionClosed_data ();
	void clientClosingEmitsConnectionClosed ();
	
	void qiodeviceCloseSendsClosePacket ();
	void qiodeviceOpenOnlySupportsClosing ();
	
	void verifyClose_data ();
	void verifyClose ();
	
	void receiveTextFrameThroughRead_data ();
	void receiveTextFrameThroughRead ();
	
	void receiveFrameThroughSignal_data ();
	void receiveFrameThroughSignal ();
	
	void receivePartialFrames_data ();
	void receivePartialFrames ();
	
	void illegalPacketDropsConnection_data ();
	void illegalPacketDropsConnection ();
	
	void clientSendingPingEmitsSignalAndRepliesWithPong_data ();
	void clientSendingPingEmitsSignalAndRepliesWithPong ();
	
	void emitPongReceivedWhenPongReceived_data ();
	void emitPongReceivedWhenPongReceived ();
	
	void verifySendPing_data ();
	void verifySendPing ();
	void sendPingFailsForTooLargeChallenge ();
	
	void verifySendFrameTypes_data ();
	void verifySendFrameTypes ();
	
private:
	
	HttpClient *createClient (const QByteArray &request) {
		HttpMemoryTransport *transport = new HttpMemoryTransport (server);
		HttpClient *client = new HttpClient (transport, server);
		transport->setMaxRequests (1);
		transport->process (client, request);
		return client;
	}
	
	HttpMemoryTransport *getTransport (HttpClient *client)
	{ return qobject_cast< HttpMemoryTransport * > (client->transport ()); }
	
	HttpMemoryTransport *getTransport (WebSocket *socket)
	{ return qobject_cast< HttpMemoryTransport * > (socket->httpClient ()->transport ()); }
	
	void sendData (WebSocket *socket, const QByteArray &data)
	{ getTransport (socket)->process (socket->httpClient (), data); }
	
	WebSocket *createWebSocket ();
	QByteArray createFrame (bool fin, int opcode, QByteArray payload);
	
	HttpServer *server = new HttpServer (this);
	
};

WebSocket *WebSocketTest::createWebSocket() {
	WebSocket *socket = nullptr;
	QByteArray input ("GET /websocket HTTP/1.1\r\n"
	                  "Host: unit.test\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
	                  "Sec-WebSocket-Key: MDEyMzQ1Njc4OUFCQ0RFRg==\r\nSec-WebSocket-Version: 13\r\n\r\n");
	
	auto slot = [&](HttpClient *client) { socket = client->acceptWebSocketConnection (); };
	
	server->root ()->connectSlot ("websocket", Callback::fromLambda (slot));
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
        
	// 
	transport->outData.clear ();
	return socket;
}

QByteArray WebSocketTest::createFrame (bool fin, int opcode, QByteArray payload) {
	QByteArray data (6, Qt::Uninitialized);
	uint8_t *ptr = reinterpret_cast< uint8_t * > (data.data ());
	
	Q_ASSERT(payload.length () < 224);
	ptr[0] = (fin << 7) | opcode;
	
	if (!payload.isEmpty ()) {
		ptr[1] = (1 << 7) | payload.length (); // Masked data
		*reinterpret_cast< uint32_t * > (ptr + 2) = qToBigEndian (quint32 (0x11223344));
		
		Internal::WebSocketReader::maskPayload (0x44332211, payload);
		data.append (payload);
	} else {
		ptr[1] = 0;
		data.resize (2);
	}
	
	return data;
}

void WebSocketTest::initTestCase () {
	server->setFqdn ("unit.test");
	server->addBackend (new TestBackend (server, 80, false));
}

void WebSocketTest::init () {
	// Clean environment
	server->setRoot (new HttpNode);
}

void WebSocketTest::connectToWebSocket () {
	WebSocket *socket = nullptr;
	
	auto slot = [&](HttpClient *client) {
		QVERIFY(client->isWebSocketHandshake ());
		socket = client->acceptWebSocketConnection ();
	};
	
	// 
	QByteArray input ("GET /websocket HTTP/1.1\r\n"
	                  "Host: unit.test\r\n"
	                  "Upgrade: websocket\r\n"
	                  "Connection: Upgrade\r\n"
	                  "Sec-WebSocket-Key: MDEyMzQ1Njc4OUFCQ0RFRg==\r\n"
	                  "Sec-WebSocket-Version: 13\r\n\r\n");
	
	QByteArray expected ("HTTP/1.1 101 Switching Protocols\r\n"
	                     "Connection: Upgrade\r\n"
	                     "Date: %%\r\n"
	                     "Sec-WebSocket-Accept: pnL6omb3MSKYnUzHgi0MFLCWfLc=\r\n"
	                     "Upgrade: websocket\r\n\r\n");
	
	expected.replace ("%%", HttpWriter ().dateTimeToHttpDateHeader (QDateTime::currentDateTimeUtc ()));
	
	// 
	server->root ()->connectSlot ("websocket", Callback::fromLambda (slot));
	HttpClient *client = createClient (input);
	HttpMemoryTransport *transport = getTransport (client);
	
	// 
	QVERIFY(socket);
	QCOMPARE(transport->outData, expected);
	
}

void WebSocketTest::clientClosingEmitsConnectionClosed_data () {
	QTest::addColumn< int > ("code");
	QTest::addColumn< QByteArray > ("msg");
	QTest::addColumn< QByteArray > ("data");
	
	QTest::newRow ("empty") << -1 << QByteArray () << QByteArray ("\x88\x00", 2);
	QTest::newRow ("code") << 1000 << QByteArray () << QByteArray ("\x88\x82\x00\x00\x00\x00\x00\x10", 8);
	QTest::newRow ("message") << 1000 << QByteArray ("Nuria")
	                          << QByteArray ("\x88\x87\x00\x00\x00\x00\x00\x10Nuria", 13);
	
}

void WebSocketTest::clientClosingEmitsConnectionClosed () {
	QFETCH(int, code);
	QFETCH(QByteArray, msg);
	QFETCH(QByteArray, data);
	
	// 
	QByteArray body;
	if (code > -1) {
		body.resize (2);
		*reinterpret_cast< uint16_t * > (body.data ()) = qToBigEndian (quint16 (code));
		body.append (msg);
	}
	
	// 
	WebSocket *socket = createWebSocket ();
	QSignalSpy aboutToClose (socket, SIGNAL(aboutToClose()));
	QSignalSpy connectionLost (socket, SIGNAL(connectionLost(Nuria::WebSocket::CloseReason)));
	QSignalSpy connectionClosed (socket, SIGNAL(connectionClosed(int,QByteArray)));
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	sendData (socket, createFrame (true, 8, body));
	
	QCOMPARE(socket->openMode (), QIODevice::NotOpen);
	QCOMPARE(aboutToClose.length (), 1);
	QCOMPARE(connectionLost.length (), 1);
	QCOMPARE(connectionClosed.length (), 1);
	QCOMPARE(connectionLost.at (0).at (0).value< WebSocket::CloseReason > (), WebSocket::CloseRequest);
	QCOMPARE(connectionClosed.at (0).at (0).toInt (), code);
	QCOMPARE(connectionClosed.at (0).at (1).toByteArray (), msg);
}

void WebSocketTest::qiodeviceCloseSendsClosePacket () {
	WebSocket *socket = createWebSocket ();
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	socket->close ();
	
	QCOMPARE(socket->openMode (), QIODevice::NotOpen);
	QCOMPARE(getTransport (socket)->outData, QByteArray ("\x88\02\x03\xE8", 4));
}

void WebSocketTest::qiodeviceOpenOnlySupportsClosing () {
	WebSocket *socket = createWebSocket ();
	
	// open() shall fail for anything but NotOpen
	QVERIFY(!socket->open (QIODevice::ReadWrite));
	QCOMPARE(socket->openMode (), QIODevice::ReadWrite);
	QVERIFY(!socket->open (QIODevice::ReadOnly));
	QCOMPARE(socket->openMode (), QIODevice::ReadWrite);
	QVERIFY(!socket->open (QIODevice::WriteOnly));
	QCOMPARE(socket->openMode (), QIODevice::ReadWrite);
	
	// 
	QTest::ignoreMessage (QtDebugMsg, "close()");
	QVERIFY(socket->open (QIODevice::NotOpen));
	QCOMPARE(socket->openMode (), QIODevice::NotOpen);
	QCOMPARE(getTransport (socket)->outData, QByteArray ("\x88\02\x03\xE8", 4));
	
}

void WebSocketTest::verifyClose_data () {
	QTest::addColumn< int > ("code");
	QTest::addColumn< QByteArray > ("msg");
	QTest::addColumn< QByteArray > ("expected");
	
	QTest::newRow ("empty") << -1 << QByteArray () << QByteArray ("\x88\x00", 2);
	QTest::newRow ("code") << 16 << QByteArray () << QByteArray ("\x88\x02\x00\x10", 4);
	QTest::newRow ("message") << 16 << QByteArray ("Nuria") << QByteArray ("\x88\x07\x00\x10Nuria", 9);
	
}

void WebSocketTest::verifyClose () {
	QFETCH(int, code);
	QFETCH(QByteArray, msg);
	QFETCH(QByteArray, expected);
	
	WebSocket *socket = createWebSocket ();
	QSignalSpy aboutToClose (socket, SIGNAL(aboutToClose()));
	QSignalSpy connectionLost (socket, SIGNAL(connectionLost(Nuria::WebSocket::CloseReason)));
	QSignalSpy connectionClosed (socket, SIGNAL(connectionClosed(int,QByteArray)));
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	socket->close (code, msg);
	
	QCOMPARE(socket->openMode (), QIODevice::NotOpen);
	QCOMPARE(aboutToClose.length (), 1);
	QCOMPARE(connectionLost.length (), 1);
	QCOMPARE(connectionClosed.length (), 0);
	QCOMPARE(connectionLost.at (0).at (0).value< WebSocket::CloseReason > (), WebSocket::CloseRequest);
	QCOMPARE(getTransport (socket)->outData, expected);
	
}

void WebSocketTest::receiveTextFrameThroughRead_data () {
	QTest::addColumn< WebSocket::Mode > ("mode");
	
	QTest::newRow ("Frame") << WebSocket::Frame;
	QTest::newRow ("FrameStreaming") << WebSocket::FrameStreaming;
	QTest::newRow ("Streaming") << WebSocket::Streaming;
}

void WebSocketTest::receiveTextFrameThroughRead () {
	QFETCH(Nuria::WebSocket::Mode, mode);
	WebSocket *socket = createWebSocket ();
	socket->setMode (mode);
	socket->setUseReadBuffer (true);
	
	QSignalSpy readyRead (socket, SIGNAL(readyRead()));
	sendData (socket, createFrame (true, 1, "Foobar"));
	sendData (socket, createFrame (true, 1, "NuriaProject"));
	
	QCOMPARE(readyRead.length (), 2);
	
	if (mode == WebSocket::Frame) {
		QCOMPARE(socket->bytesAvailable (), 6);
		QCOMPARE(socket->read (socket->bytesAvailable ()), QByteArray ("Foobar"));
		QCOMPARE(socket->bytesAvailable (), 12);
		QCOMPARE(socket->read (socket->bytesAvailable ()), QByteArray ("NuriaProject"));
	} else {
		QCOMPARE(socket->bytesAvailable (), 18);
		QCOMPARE(socket->read (18), QByteArray ("FoobarNuriaProject"));
	}
	
	QCOMPARE(socket->bytesAvailable (), 0);
}

void WebSocketTest::receiveFrameThroughSignal_data () {
	QTest::addColumn< WebSocket::Mode > ("mode");
	
	QTest::newRow ("Frame") << WebSocket::Frame;
	QTest::newRow ("FrameStreaming") << WebSocket::FrameStreaming;
	QTest::newRow ("Streaming") << WebSocket::Streaming;
}

void WebSocketTest::receiveFrameThroughSignal () {
	QFETCH(Nuria::WebSocket::Mode, mode);
	WebSocket *socket = createWebSocket ();
	socket->setMode (mode);
	
	QSignalSpy readyRead (socket, SIGNAL(readyRead()));
	QSignalSpy frameReceived (socket, SIGNAL(frameReceived(Nuria::WebSocket::FrameType,QByteArray)));
	QSignalSpy partialFrameReceived (socket,
	                                 SIGNAL(partialFrameReceived(Nuria::WebSocket::FrameType,QByteArray,bool)));
	
	// 
	socket->setUseReadBuffer (false);
	sendData (socket, createFrame (true, 1, "Foobar"));
	sendData (socket, createFrame (true, 2, "NuriaProject"));
	
	QCOMPARE(readyRead.length (), 0);
	QCOMPARE(socket->bytesAvailable (), 0);
	
	if (mode == WebSocket::Frame) {
		QCOMPARE(frameReceived.length (), 2);
		QCOMPARE(partialFrameReceived.length (), 2);
		
		QCOMPARE(frameReceived.at (0).at (0).value< WebSocket::FrameType > (), WebSocket::TextFrame);
		QCOMPARE(frameReceived.at (0).at (1).toByteArray (), QByteArray ("Foobar"));
		QCOMPARE(frameReceived.at (1).at (0).value< WebSocket::FrameType > (), WebSocket::BinaryFrame);
		QCOMPARE(frameReceived.at (1).at (1).toByteArray (), QByteArray ("NuriaProject"));
		
		QCOMPARE(partialFrameReceived.at (0).at (0).value< WebSocket::FrameType > (), WebSocket::TextFrame);
		QCOMPARE(partialFrameReceived.at (0).at (1).toByteArray (), QByteArray ("Foobar"));
		QCOMPARE(partialFrameReceived.at (0).at (2).toBool (), true);
		QCOMPARE(partialFrameReceived.at (1).at (0).value< WebSocket::FrameType > (), WebSocket::BinaryFrame);
		QCOMPARE(partialFrameReceived.at (1).at (1).toByteArray (), QByteArray ("NuriaProject"));
		QCOMPARE(partialFrameReceived.at (1).at (2).toBool (), true);
	} else {
		QCOMPARE(frameReceived.length (), 0);
		QCOMPARE(partialFrameReceived.length (), 0);
		
	}
	
}

void WebSocketTest::receivePartialFrames_data () {
	QTest::addColumn< WebSocket::Mode > ("mode");
	
	QTest::newRow ("Frame") << WebSocket::Frame;
	QTest::newRow ("FrameStreaming") << WebSocket::FrameStreaming;
	QTest::newRow ("Streaming") << WebSocket::Streaming;
}

void WebSocketTest::receivePartialFrames () {
	QFETCH(Nuria::WebSocket::Mode, mode);
	WebSocket *socket = createWebSocket ();
	socket->setMode (mode);
	socket->setUseReadBuffer (true);
	
	QSignalSpy readyRead (socket, SIGNAL(readyRead()));
	QSignalSpy frameReceived (socket, SIGNAL(frameReceived(Nuria::WebSocket::FrameType,QByteArray)));
	QSignalSpy partialFrameReceived (socket,
	                                 SIGNAL(partialFrameReceived(Nuria::WebSocket::FrameType,QByteArray,bool)));
	
	// 
	sendData (socket, createFrame (false, 1, "Foo"));
	
	QCOMPARE(frameReceived.length (), 0);
	if (mode == WebSocket::Frame) {
		QCOMPARE(partialFrameReceived.length (), 1);
		QCOMPARE(readyRead.length (), 0);
		QCOMPARE(socket->bytesAvailable (), 0);
	} else {
		QCOMPARE(partialFrameReceived.length (), 0);
		QCOMPARE(readyRead.length (), (mode == WebSocket::Streaming ? 1 : 0));
		QCOMPARE(socket->bytesAvailable (), 3);
	}
	
	sendData (socket, createFrame (true, 0, "Bar"));
	
	QCOMPARE(socket->openMode (), QIODevice::ReadWrite);
	if (mode == WebSocket::Frame) {
		QCOMPARE(readyRead.length (), 1);
		QCOMPARE(frameReceived.length (), 1);
		QCOMPARE(partialFrameReceived.length (), 2);
		
		// 
		QCOMPARE(frameReceived.at (0).at (0).value< WebSocket::FrameType > (), WebSocket::TextFrame);
		QCOMPARE(frameReceived.at (0).at (1).toByteArray (), QByteArray ("FooBar"));
		QCOMPARE(partialFrameReceived.at (0).at (0).value< WebSocket::FrameType > (), WebSocket::TextFrame);
		QCOMPARE(partialFrameReceived.at (0).at (1).toByteArray (), QByteArray ("Foo"));
		QCOMPARE(partialFrameReceived.at (0).at (2).toBool (), false);
		QCOMPARE(partialFrameReceived.at (1).at (0).value< WebSocket::FrameType > (), WebSocket::TextFrame);
		QCOMPARE(partialFrameReceived.at (1).at (1).toByteArray (), QByteArray ("Bar"));
		QCOMPARE(partialFrameReceived.at (1).at (2).toBool (), true);
	} else {
		QCOMPARE(readyRead.length (), (mode == WebSocket::Streaming ? 2 : 1));
		QCOMPARE(frameReceived.length (), 0);
		QCOMPARE(partialFrameReceived.length (), 0);
	}
	
	QCOMPARE(socket->bytesAvailable (), 6);
	QCOMPARE(socket->readAll (), QByteArray ("FooBar"));
}

void WebSocketTest::illegalPacketDropsConnection_data () {
	QTest::addColumn< QByteArray > ("packet");
	
	QTest::newRow ("illegal-bits") << QByteArray ("\xFF\x80\x11\x11\x11\x11");
	QTest::newRow ("unknown-opcode") << QByteArray ("\x8B\x80\x11\x11\x11\x11");
	QTest::newRow ("broken-continue") << QByteArray ("\x80\x80\x11\x11\x11\x11");
	QTest::newRow ("bad-close") << QByteArray ("\x88\x81\x11\x11\x11\x11\x11");
}

void WebSocketTest::illegalPacketDropsConnection () {
	QFETCH(QByteArray, packet);
	
	WebSocket *socket = createWebSocket ();
	QCOMPARE(socket->openMode (), QIODevice::ReadWrite);
	
	QSignalSpy aboutToClose (socket, SIGNAL(aboutToClose()));
	QSignalSpy connectionLost (socket, SIGNAL(connectionLost(Nuria::WebSocket::CloseReason)));
	QSignalSpy connectionClosed (socket, SIGNAL(connectionClosed(int,QByteArray)));
	
	QTest::ignoreMessage (QtDebugMsg, "close()");
	sendData (socket, packet);
	
	QCOMPARE(socket->openMode (), QIODevice::NotOpen);
	QCOMPARE(aboutToClose.length (), 1);
	QCOMPARE(connectionLost.length (), 1);
	QCOMPARE(connectionClosed.length (), 0);
	QCOMPARE(connectionLost.at (0).at (0).value< WebSocket::CloseReason > (), WebSocket::IllegalFrameReceived);
	
}

void WebSocketTest::clientSendingPingEmitsSignalAndRepliesWithPong_data () {
	QTest::addColumn< QByteArray > ("challenge");
	
	QTest::newRow ("empty") << QByteArray ();
	QTest::newRow ("challenge") << QByteArray ("foo");
}

void WebSocketTest::clientSendingPingEmitsSignalAndRepliesWithPong () {
	QFETCH(QByteArray, challenge);
	
	QByteArray response = "\x8A";
	response.append (char (challenge.length ()));
	response.append (challenge);
	
	WebSocket *socket = createWebSocket ();
	QSignalSpy pingReceived (socket, SIGNAL(pingReceived(QByteArray)));
	
	sendData (socket, createFrame (true, 9, challenge));
	
	QCOMPARE(pingReceived.length (), 1);
	QCOMPARE(pingReceived.at (0).at (0).toByteArray (), challenge);
	QCOMPARE(getTransport (socket)->outData, response);
}

void WebSocketTest::emitPongReceivedWhenPongReceived_data () {
	QTest::addColumn< QByteArray > ("challenge");
	
	QTest::newRow ("empty") << QByteArray ();
	QTest::newRow ("challenge") << QByteArray ("foo");
}

void WebSocketTest::emitPongReceivedWhenPongReceived () {
	QFETCH(QByteArray, challenge);
	
	WebSocket *socket = createWebSocket ();
	QSignalSpy pongReceived (socket, SIGNAL(pongReceived(QByteArray)));
	
	sendData (socket, createFrame (true, 10, challenge));
	
	QCOMPARE(pongReceived.length (), 1);
	QCOMPARE(pongReceived.at (0).at (0).toByteArray (), challenge);
	QVERIFY(getTransport (socket)->outData.isEmpty ());
	
}

void WebSocketTest::verifySendPing_data () {
	QTest::addColumn< QByteArray > ("challenge");
	
	QTest::newRow ("empty") << QByteArray ();
	QTest::newRow ("challenge") << QByteArray ("foo");
}

void WebSocketTest::verifySendPing () {
	QFETCH(QByteArray, challenge);
	
	QByteArray ping = "\x89";
	ping.append (char (challenge.length ()));
	ping.append (challenge);
	
	WebSocket *socket = createWebSocket ();
	QSignalSpy pingReceived (socket, SIGNAL(pingReceived(QByteArray)));
	QSignalSpy pongReceived (socket, SIGNAL(pongReceived(QByteArray)));
	
	QVERIFY(socket->sendPing (challenge));
	QCOMPARE(pingReceived.length (), 0);
	QCOMPARE(pongReceived.length (), 0);
	QCOMPARE(getTransport (socket)->outData, ping);
}

void WebSocketTest::sendPingFailsForTooLargeChallenge () {
	WebSocket *socket = createWebSocket ();
	QVERIFY(!socket->sendPing (QByteArray (126, 'x')));
}

void WebSocketTest::verifySendFrameTypes_data () {
	QTest::addColumn< WebSocket::FrameType > ("type");
	QTest::newRow ("text") << WebSocket::TextFrame;
	QTest::newRow ("binary") << WebSocket::BinaryFrame;
}

void WebSocketTest::verifySendFrameTypes () {
	QFETCH(Nuria::WebSocket::FrameType, type);
	
	QByteArray response1;
	response1.append (type == WebSocket::TextFrame ? '\x01' : '\x02');
	response1.append ("\x03xyz");
	
	WebSocket *socket = createWebSocket ();
	HttpMemoryTransport *transport = getTransport (socket);
	
	socket->sendFrame (type, QByteArray ("xyz"), false);
	QCOMPARE(transport->outData, response1);
	transport->outData.clear ();
	
	socket->sendFrame (type, QByteArray ("uvw"), true);
	QCOMPARE(transport->outData, QByteArray ("\x80\x03uvw"));
	transport->outData.clear ();
	
}

QTEST_MAIN(WebSocketTest)
#include "tst_websocket.moc"
