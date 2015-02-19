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

#include "private/transportprivate.hpp"
#include "nuria/abstracttransport.hpp"
#include <nuria/logger.hpp>

#include <QMetaEnum>
#include <QTimer>

Nuria::AbstractTransport::AbstractTransport (QObject *parent)
        : AbstractTransport (new AbstractTransportPrivate, parent)
{
	
}

Nuria::AbstractTransport::AbstractTransport (AbstractTransportPrivate *d, QObject *parent)
        : QObject (parent), d_ptr (d)
{
	
	// Timeout timer
	this->d_ptr->timeoutTimer = new QTimer (this);
	connect (this->d_ptr->timeoutTimer, &QTimer::timeout, this, &AbstractTransport::triggerTimeout);
}

void Nuria::AbstractTransport::handleTimeout (Timeout timeout) {
	static const QMetaEnum e = staticMetaObject.enumerator (staticMetaObject.enumeratorOffset ());
	const char *name = e.valueToKey (timeout);
	
	nLog() << name << "triggered in transport" << this << "connected to remote peer"
	       << peerAddress () << "port" << peerPort () << "- Closing connection!";
	
	forceClose ();
}

Nuria::AbstractTransport::~AbstractTransport () {
	delete this->d_ptr;
}

bool Nuria::AbstractTransport::isSecure () const {
	return false;
}

QHostAddress Nuria::AbstractTransport::localAddress () const {
	return QHostAddress::Null;
}

quint16 Nuria::AbstractTransport::localPort () const {
	return 0;
}

QHostAddress Nuria::AbstractTransport::peerAddress () const {
	return QHostAddress::Null;
}

quint16 Nuria::AbstractTransport::peerPort () const {
	return 0;
}

int Nuria::AbstractTransport::currentRequestCount () const {
	return this->d_ptr->requestCount;
}

Nuria::AbstractTransport::Timeout Nuria::AbstractTransport::currentTimeoutMode () const {
	return this->d_ptr->currentTimeoutMode;
}

int Nuria::AbstractTransport::timeout (AbstractTransport::Timeout which) {
	switch (which) {
	case Disabled: return -1;
	case ConnectTimeout: return this->d_ptr->timeoutConnect;
	case DataTimeout: return this->d_ptr->timeoutData;
	case KeepAliveTimeout: return this->d_ptr->timeoutKeepAlive;
	}
	
	return 0;
}

void Nuria::AbstractTransport::setTimeout (AbstractTransport::Timeout which, int msec) {
	switch (which) {
	case ConnectTimeout:
		this->d_ptr->timeoutConnect = msec;
		break;
	case DataTimeout:
		this->d_ptr->timeoutData = msec;
		break;
	case KeepAliveTimeout:
		this->d_ptr->timeoutKeepAlive = msec;
		break;
	case Disabled:
		return;
	}
	
	emit timeoutChanged (which, msec);
}

int Nuria::AbstractTransport::minimumBytesReceived () const {
	return this->d_ptr->minBytesReceived;
}

void Nuria::AbstractTransport::setMinimumBytesReceived (int bytes) {
	this->d_ptr->minBytesReceived = bytes;
}

quint64 Nuria::AbstractTransport::bytesReceived () const {
	return this->d_ptr->trafficReceived;
}

quint64 Nuria::AbstractTransport::bytesSent () const {
	return this->d_ptr->trafficSent;
}

void Nuria::AbstractTransport::init () {
	// 
}

void Nuria::AbstractTransport::setCurrentRequestCount (int count) {
	this->d_ptr->requestCount = count;
}

void Nuria::AbstractTransport::incrementRequestCount () {
	this->d_ptr->requestCount++;
}

void Nuria::AbstractTransport::addBytesReceived (uint64_t bytes) {
	this->d_ptr->trafficReceived += bytes;
}

void Nuria::AbstractTransport::addBytesSent (uint64_t bytes) {
	this->d_ptr->trafficSent += bytes;
}

void Nuria::AbstractTransport::startTimeout (Timeout mode) {
	this->d_ptr->trafficReceivedLast = this->d_ptr->trafficReceived;
	this->d_ptr->currentTimeoutMode = mode;
	int msec = timeout (mode);
	
	if (msec < 0) {
		this->d_ptr->timeoutTimer->stop ();
	} else {
		this->d_ptr->timeoutTimer->start (msec);
	}
	
}

void Nuria::AbstractTransport::disableTimeout () {
	this->d_ptr->currentTimeoutMode = Disabled;
	this->d_ptr->timeoutTimer->stop ();
}

void Nuria::AbstractTransport::triggerTimeout () {
	if (this->d_ptr->currentTimeoutMode == DataTimeout) {
		uint64_t delta = this->d_ptr->trafficReceived - this->d_ptr->trafficReceivedLast;
		this->d_ptr->trafficReceivedLast = this->d_ptr->trafficReceived;
		
		// Make sure we received the minimal amount of data in this timeframe.
		if (this->d_ptr->minBytesReceived <= delta) {
			return;
		}
		
	}
	
	// Timeout!
	handleTimeout (this->d_ptr->currentTimeoutMode);
	emit connectionTimedout (this->d_ptr->currentTimeoutMode);
}
