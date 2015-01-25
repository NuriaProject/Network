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

#include "nuria/websocket.hpp"

#include "private/websocketreader.hpp"
#include "private/websocketwriter.hpp"
#include "nuria/httptransport.hpp"
#include "nuria/stringutils.hpp"
#include "nuria/httpclient.hpp"
#include "nuria/logger.hpp"

namespace Nuria {
class WebSocketRecvDevice;

class WebSocketPrivate {
public:
	
	WebSocketPrivate (WebSocket *q) : q_ptr (q) {}
	
	WebSocket *q_ptr;
	HttpClient *client;
	WebSocketRecvDevice *backend;
	
	// struct Stream {
	WebSocket::Mode mode = WebSocket::Frame;
	WebSocket::FrameType type = WebSocket::TextFrame;
	int curIncoming = -1; // WebSocket::FrameType
	int curOutgoing = -1; // WebSocket::FrameType
	bool usingReadBuffer = false;
	
	QList< QByteArray > frames; // Frame
	QByteArray buffer; // FrameStreaming and Streaming
	// }
	
	qint64 readFrame (char *data, qint64 maxlen);
	qint64 readStream (char *data, qint64 maxlen);
	void sendClose (int code, const QByteArray &msg);
	void sendPing (const QByteArray &challenge);
	void sendPong (const QByteArray &challenge);
	
	void processIncoming ();
	bool processPacket ();
	bool processFrame (Internal::WebSocketFrame frame, QByteArray payload);
	bool checkUtfValidity (const QByteArray &buffer, WebSocket::FrameType type, bool last);
	bool appendDataFrame (Internal::WebSocketFrame frame, QByteArray &payload);
	QByteArray &getFramePayload (bool append);
	bool processClose (QByteArray &payload);
	bool processPing (const QByteArray &payload);
	bool processPong (const QByteArray &payload);
	
};

class WebSocketRecvDevice : public QIODevice {
public:
	WebSocketRecvDevice (WebSocketPrivate *d, WebSocket *parent)
	        : QIODevice (parent), d_ptr (d)
	{
		setOpenMode (QIODevice::WriteOnly);
	}
	
	// 
	enum {
		BufferShrinkThresholdHuge = 1024 * 1024,
		BufferShrinkThreshold = 8 * 1024
	};
	WebSocketPrivate *d_ptr;
	QByteArray buffer;
	int bufferReadPos = 0;
	
	qint64 readData (char *data, qint64 maxlen) override;
	qint64 writeData (const char *data, qint64 len) override;
	void shrinkBuffer (bool fast);
};

qint64 WebSocketRecvDevice::readData (char *data, qint64 maxlen)
{ return -1; }

qint64 WebSocketRecvDevice::writeData (const char *data, qint64 len) {
	buffer.append (data, len);
	this->d_ptr->processIncoming ();
	return len;
}

void WebSocketRecvDevice::shrinkBuffer (bool fast) {
	int threshold = fast ? BufferShrinkThresholdHuge : BufferShrinkThreshold;
	
	if (this->bufferReadPos >= threshold) {
		this->buffer = this->buffer.mid (this->bufferReadPos);
		this->bufferReadPos = 0;
	}
	
}

}

Nuria::WebSocket::WebSocket (HttpClient *client)
	: QIODevice (client), d_ptr (new WebSocketPrivate (this))
{
	setOpenMode (ReadWrite);
	
	this->d_ptr->client = client;
	this->d_ptr->backend = new WebSocketRecvDevice (this->d_ptr, this);
	
	connect (client, &QIODevice::aboutToClose, this, &QIODevice::aboutToClose);
	connect (client, &QIODevice::aboutToClose, this, &WebSocket::connLostHandler);
	
}

Nuria::WebSocket::~WebSocket () {
	delete this->d_ptr;
}

Nuria::HttpClient *Nuria::WebSocket::httpClient () const {
	return this->d_ptr->client;
}

Nuria::WebSocket::Mode Nuria::WebSocket::mode () const {
	return this->d_ptr->mode;
}

void Nuria::WebSocket::setMode (Mode mode) {
	this->d_ptr->buffer.clear ();
	this->d_ptr->frames.clear ();
	this->d_ptr->mode = mode;
	
	this->d_ptr->usingReadBuffer = (mode != Frame);
	
}

Nuria::WebSocket::FrameType Nuria::WebSocket::frameType () const {
	return this->d_ptr->type;
}

void Nuria::WebSocket::setFrameType (FrameType type) {
	this->d_ptr->type = type;
}

bool Nuria::WebSocket::isUsingReadBuffer () {
	return this->d_ptr->usingReadBuffer;
}

void Nuria::WebSocket::setUseReadBuffer (bool useBuffer) {
	this->d_ptr->usingReadBuffer = useBuffer;
	if (useBuffer) {
		setOpenMode (QIODevice::ReadWrite);
	} else {
		setOpenMode (QIODevice::WriteOnly);
		this->d_ptr->buffer.clear ();
		this->d_ptr->frames.clear ();
	}
	
}

void Nuria::WebSocket::sendFrame (FrameType type, const QByteArray &data, bool isLast) {
	sendFrame (type, data.constData (), data.length (), isLast);
}

void Nuria::WebSocket::sendFrame (FrameType type, const char *data, int length, bool isLast) {
	Internal::WebSocketOpcode op;
	if (this->d_ptr->curOutgoing < 0) {
		op = (type == TextFrame) ? Internal::TextFrame : Internal::BinaryFrame;
		this->d_ptr->curOutgoing = type;
	} else {
		op = Internal::ContinuationFrame;
	}
	
	// 
	if (isLast) {
		this->d_ptr->curOutgoing = -1;
	}
	
	// 
	Internal::WebSocketWriter::sendToClient (this->d_ptr->client, isLast, op, data, length);
}

void Nuria::WebSocket::sendBinaryFrame(const QByteArray &data, bool isLast) {
	sendFrame (BinaryFrame, data.constData (), data.length (), isLast);
}

void Nuria::WebSocket::sendBinaryFrame (const char *data, int length, bool isLast) {
	sendFrame (BinaryFrame, data, length, isLast);
}

void Nuria::WebSocket::sendTextFrame (const QString &data, bool isLast) {
	sendFrame (TextFrame, data.toUtf8 (), isLast);
}

void Nuria::WebSocket::sendTextFrame (const QByteArray &data, bool isLast) {
	sendFrame (TextFrame, data.constData (), data.length (), isLast);
}

bool Nuria::WebSocket::sendPing (const QByteArray &challenge) {
	if (challenge.length () >= Internal::PayloadLengthMagicNumbers::Length16Bit) {
		return false;
	}
	
	// 
	this->d_ptr->sendPing (challenge);
	return true;
}

bool Nuria::WebSocket::flush () const {
	return this->d_ptr->client->transport ()->flush (this->d_ptr->client);
}

bool Nuria::WebSocket::isSequential () const {
	return true;
}

bool Nuria::WebSocket::open (QIODevice::OpenMode mode) {
	if (mode == NotOpen) {
		close ();
		return true;
	}
	
	return false;
}

void Nuria::WebSocket::close () {
	close (StatusNormal, QByteArray ());
}

void Nuria::WebSocket::close (int code, const QByteArray &message) {
	if (openMode () != NotOpen) {
		this->d_ptr->sendClose (code, message);
	}
	
	setOpenMode (NotOpen);
	emit connectionLost (CloseRequest);
	this->d_ptr->client->close ();
}

qint64 Nuria::WebSocket::bytesAvailable () const {
	int base = QIODevice::bytesAvailable ();
	
	if (!this->d_ptr->usingReadBuffer) {
		return base;
	} else if (this->d_ptr->mode == Frame) {
		if (this->d_ptr->frames.isEmpty ()) {
			return base;
		}
		
		return base + this->d_ptr->frames.first ().length ();
	}
	
	return base + this->d_ptr->buffer.length ();
}

qint64 Nuria::WebSocket::readData (char *data, qint64 maxlen) {
	if (this->d_ptr->mode == Frame) {
		return this->d_ptr->readFrame (data, maxlen);
	}
	
	return this->d_ptr->readStream (data, maxlen);
}

qint64 Nuria::WebSocket::writeData (const char *data, qint64 len) {
	sendFrame (this->d_ptr->type, data, len, true);
	return len;
}

QIODevice *Nuria::WebSocket::backendDevice () const {
	return this->d_ptr->backend;
}

void Nuria::WebSocket::connLostHandler () {
	if (openMode () != NotOpen) {
		setOpenMode (NotOpen);
		emit connectionLost (CloseRequest);
	}
	
}

static bool readFromByteArray (QByteArray &buffer, char *data, qint64 &maxlen) {
	
	// readAll() shortcut
	if (maxlen >= buffer.length ()) {
		maxlen = buffer.length ();
		memcpy (data, buffer.constData (), maxlen);
		return true;
	}
	
	// 
	memcpy (data, buffer.constData (), maxlen);
	buffer = buffer.mid (maxlen);
	return false;
	
}

qint64 Nuria::WebSocketPrivate::readFrame (char *data, qint64 maxlen) {
	if (this->frames.isEmpty ()) {
		return 0;
	}
	
	// 
	if (readFromByteArray (this->frames.first (), data, maxlen)) {
		this->frames.removeFirst ();
	}
	
	// 
	return maxlen;
}

qint64 Nuria::WebSocketPrivate::readStream (char *data, qint64 maxlen) {
	if (readFromByteArray (this->buffer, data, maxlen)) {
		this->buffer.clear ();
	}
	
	return maxlen;
}

void Nuria::WebSocketPrivate::sendClose (int code, const QByteArray &msg) {
	QByteArray payload = Internal::WebSocketWriter::createClosePayload (code, msg); 
	Internal::WebSocketWriter::sendToClient (this->client, true, Internal::ConnectionClose,
	                                         payload.constData (), payload.length ());
}

void Nuria::WebSocketPrivate::sendPing (const QByteArray &challenge) {
	Internal::WebSocketWriter::sendToClient (this->client, true, Internal::Ping,
	                                         challenge.constData (), challenge.length ());
}

void Nuria::WebSocketPrivate::sendPong (const QByteArray &challenge) {
	Internal::WebSocketWriter::sendToClient (this->client, true, Internal::Pong,
	                                         challenge.constData (), challenge.length ());
}

void Nuria::WebSocketPrivate::processIncoming () {
	if (!processPacket ()) {
		this->q_ptr->setOpenMode (QIODevice::NotOpen);
		sendClose (WebSocket::StatusProtocolError, QByteArray ());
		this->q_ptr->flush ();
		this->client->close ();
		emit this->q_ptr->connectionLost (WebSocket::IllegalFrameReceived);
		
	}
	
}

static inline QByteArray byteMidRef (const QByteArray &ba, int pos, int len) {
	return QByteArray::fromRawData (ba.constData () + pos, len);
}

bool Nuria::WebSocketPrivate::processPacket () {
	using namespace Nuria::Internal;
	
	QByteArray &buffer = this->backend->buffer;
	int &bufferReadPos = this->backend->bufferReadPos;
	WebSocketFrame frame;
	
	// Read frame
	while (WebSocketReader::readFrameData (buffer, frame, bufferReadPos)) {
		if (!WebSocketReader::isLegalClientPacket (frame)) {
			return false;
		}
		
		// Read payload
		qint64 dataLength = frame.extPayloadLen;
		qint64 frameSize = WebSocketReader::sizeOfFrame (frame);
		if ((frameSize + dataLength) > buffer.length () - bufferReadPos) { // Not enough data available?
			break;
		}
		
//		nDebug() << "Processing frame" << frame.base.opcode << "len" << frame.extPayloadLen
//		         << "in buffer:" << buffer.length () << "offset" << bufferReadPos;
		
		// Process frame
		if (!processFrame (frame, byteMidRef (buffer, bufferReadPos + frameSize, dataLength))) {
			return false;
		}
		
		// Remove data
		bufferReadPos += frameSize + dataLength;
		this->backend->shrinkBuffer (true); // Only shrink now if we have lots of garbage.
	}
	
	// Ok
	this->backend->shrinkBuffer (false);
	return true;
}

bool Nuria::WebSocketPrivate::processFrame (Internal::WebSocketFrame frame, QByteArray payload) {
	if (!payload.isEmpty ()) {
		Internal::WebSocketReader::maskPayload (frame.maskKey, payload);
	}
	
	switch (frame.base.opcode) {
	case Internal::ContinuationFrame:
	case Internal::TextFrame:
	case Internal::BinaryFrame:
		return appendDataFrame (frame, payload);
	case Internal::ConnectionClose:
		return processClose (payload);
	case Internal::Ping:
		return processPing (payload);
	case Internal::Pong:
		return processPong (payload);
	default:
		// Kill the connection.
		return false;
	}
	
}

bool Nuria::WebSocketPrivate::checkUtfValidity (const QByteArray &buffer, WebSocket::FrameType type, bool last) {
	if (type != WebSocket::TextFrame) { // Only affects text frames
		return true;
	}
	
	// 
	int pos = 0;
	const char *ptr = buffer.constData ();
	int len = buffer.length ();
	
	// Do check
	StringUtils::CheckState state = StringUtils::checkValidUtf8 (ptr, len, pos);
	
	// Accept if either the buffer is valid, or if the data has only been
	// received partially and thus is incomplete for the checker.
	return (state == StringUtils::Valid || (!last && state == StringUtils::Incomplete));
}

bool Nuria::WebSocketPrivate::appendDataFrame (Internal::WebSocketFrame frame, QByteArray &payload) {
	bool append = (frame.base.opcode == Internal::ContinuationFrame);
	WebSocket::FrameType type;
	if (append && this->curIncoming >= 0) {
		type = WebSocket::FrameType (this->curIncoming);
	} else if (!append && this->curIncoming < 0) {
		type = (frame.base.opcode == Internal::TextFrame) ? WebSocket::TextFrame : WebSocket::BinaryFrame;
		this->curIncoming = type;
	} else { // ContinuationFrame sent without being one. Kill connection.
		return false;
	}
	
	// Store data
	this->buffer.append (payload);
	
	// Emit signals
	if (this->mode == WebSocket::Frame) {
		emit this->q_ptr->partialFrameReceived (type, payload, frame.base.fin);
	
		if (frame.base.fin) {
			if (!checkUtfValidity (this->buffer, type, true)) {
				this->q_ptr->close (WebSocket::StatusBrokenData);
				return false; // Invalid UTF-8 sequence.
			}
			
			this->frames.append (this->buffer);
			emit this->q_ptr->frameReceived (type, this->buffer);
			this->buffer.clear ();
		}
		
	}
	
	// Throw away temporary read buffers if it's not used.
	if (frame.base.fin) {
		this->curIncoming = -1;
	}
	
	if (frame.base.fin || this->mode == WebSocket::Streaming) {
		if (!this->usingReadBuffer) {
			this->buffer.clear ();
		}
		
		if (this->usingReadBuffer) {
			emit this->q_ptr->readyRead ();
		}
		
	}
	
	// 
	return true;
}

QByteArray &Nuria::WebSocketPrivate::getFramePayload (bool append) {
	if (this->mode == WebSocket::Frame && this->usingReadBuffer) {
		if (!append || this->frames.isEmpty ()) {
			this->frames.append (QByteArray ());
		}
		
		return this->frames.last ();
	}
	
	return this->buffer;
}

bool Nuria::WebSocketPrivate::processClose (QByteArray &payload) {
	int error = -1;
	QByteArray message;
	
	// Read close payload and do a sanity check
	if (!Internal::WebSocketReader::readClose (payload, error, message) ||
	    !Internal::WebSocketReader::isLegalCloseCode (error) ||
	    !checkUtfValidity (message, WebSocket::TextFrame, true)) {
		return false;
	}
	
	// Send close response
	if (this->q_ptr->openMode () != QIODevice::NotOpen) {
		sendClose (WebSocket::StatusNormal, QByteArray ());
	}
	
	// 
	emit this->q_ptr->connectionClosed (error, message);
	this->client->close ();
	return true;
}

bool Nuria::WebSocketPrivate::processPing (const QByteArray &payload) {
	emit this->q_ptr->pingReceived (payload);
	sendPong (payload);
	return true;
}

bool Nuria::WebSocketPrivate::processPong (const QByteArray &payload) {
	emit this->q_ptr->pongReceived (payload);
	return true;
	
}
