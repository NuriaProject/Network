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

#include <QtTest/QtTest>

#include <nuria/abstracttransport.hpp>

using namespace Nuria;

enum { Timeout = 500 };

class AbstractTransportTest : public QObject {
	Q_OBJECT
private slots:
	
	void verifyDocumentedBehaviourOfDefaultImplementations ();
	
	void timeoutCategoriesAreSeparate ();
	
	void verifyTimeoutBehaviour ();
	void dataTimeoutTriggersForNotEnoughTraffic ();
	void dataTimeoutDoesntTriggerForEnoughTraffic ();
};

class TestTransport : public AbstractTransport {
	Q_OBJECT
public:
	
	Timeout lastTimeout;
	
	bool isOpen () const
	{ qDebug("isOpen"); return false; }
	
	void forceClose ()
	{ qDebug("forceClose"); }
	
	void handleTimeout (Timeout timeout) {
		this->lastTimeout = timeout;
	}
	
	// Make these public
	using AbstractTransport::startTimeout;
	using AbstractTransport::disableTimeout;
	using AbstractTransport::addBytesReceived;
	
};

void AbstractTransportTest::verifyDocumentedBehaviourOfDefaultImplementations () {
	TestTransport t;
	
	QCOMPARE(t.isSecure (), false);
	QCOMPARE(t.localAddress (), QHostAddress (QHostAddress::Null));
	QCOMPARE(t.localPort (), quint16 (0));
	QCOMPARE(t.peerAddress (), QHostAddress (QHostAddress::Null));
	QCOMPARE(t.peerPort (), quint16 (0));
	QCOMPARE(t.currentRequestCount (), 0);
	
	QCOMPARE(t.currentTimeoutMode (), AbstractTransport::Disabled);
	QCOMPARE(t.timeout (AbstractTransport::Disabled), -1);
	QCOMPARE(t.timeout (AbstractTransport::ConnectTimeout), int (AbstractTransport::DefaultConnectTimeout));
	QCOMPARE(t.timeout (AbstractTransport::DataTimeout), int (AbstractTransport::DefaultDataTimeout));
	QCOMPARE(t.timeout (AbstractTransport::KeepAliveTimeout), int (AbstractTransport::DefaultKeepAliveTimeout));
	QCOMPARE(t.minimumBytesReceived (), int (AbstractTransport::DefaultMinimumBytesReceived));
	
	t.startTimeout (AbstractTransport::ConnectTimeout);
	QCOMPARE(t.currentTimeoutMode (), AbstractTransport::ConnectTimeout);
	t.disableTimeout ();
	QCOMPARE(t.currentTimeoutMode (), AbstractTransport::Disabled);
	
}

void AbstractTransportTest::timeoutCategoriesAreSeparate () {
	TestTransport t;
	
	t.setTimeout (AbstractTransport::ConnectTimeout, 100);
	t.setTimeout (AbstractTransport::DataTimeout, 200);
	t.setTimeout (AbstractTransport::KeepAliveTimeout, 300);
	
	QCOMPARE(t.timeout (AbstractTransport::ConnectTimeout), 100);
	QCOMPARE(t.timeout (AbstractTransport::DataTimeout), 200);
	QCOMPARE(t.timeout (AbstractTransport::KeepAliveTimeout), 300);
	
}

void eventLoopSleep (int msec, QObject *sender = nullptr, const char *signal = nullptr) {
	QEventLoop loop;
	QTimer timer;
	
	QObject::connect (&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
	if (sender) QObject::connect (sender, signal, &loop, SLOT(quit()));
	
	timer.start (msec);
	loop.exec ();
}

void AbstractTransportTest::verifyTimeoutBehaviour () {
	TestTransport t;
	QSignalSpy connectionTimedout (&t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	
	// Start timeout
	t.setTimeout (AbstractTransport::ConnectTimeout, 100);
	t.startTimeout (AbstractTransport::ConnectTimeout);
	
	// Wait 50ms -> should not have been emitted.
	eventLoopSleep (50, &t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	QCOMPARE(connectionTimedout.length (), 0);
	
	// Wait some more time -> should have been emitted
	eventLoopSleep (100, &t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	
	QCOMPARE(connectionTimedout.length (), 1);
	QCOMPARE(connectionTimedout.at (0).first ().value< AbstractTransport::Timeout > (),
	         AbstractTransport::ConnectTimeout);
	
}

void AbstractTransportTest::dataTimeoutTriggersForNotEnoughTraffic () {
	TestTransport t;
	QSignalSpy connectionTimedout (&t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	
	// Start timeout
	t.setTimeout (AbstractTransport::DataTimeout, 100);
	t.startTimeout (AbstractTransport::DataTimeout);
	t.setMinimumBytesReceived (10);
	
	// 
	eventLoopSleep (50, &t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	t.addBytesReceived (9);
	
	eventLoopSleep (100, &t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	
	QCOMPARE(connectionTimedout.length (), 1);
	QCOMPARE(connectionTimedout.at (0).first ().value< AbstractTransport::Timeout > (),
	         AbstractTransport::DataTimeout);
	QCOMPARE(t.lastTimeout, AbstractTransport::DataTimeout);
	
}

void AbstractTransportTest::dataTimeoutDoesntTriggerForEnoughTraffic () {
	TestTransport t;
	QSignalSpy connectionTimedout (&t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	
	// Start timeout
	t.setTimeout (AbstractTransport::DataTimeout, 100);
	t.startTimeout (AbstractTransport::DataTimeout);
	t.setMinimumBytesReceived (10);
	
	// 
	eventLoopSleep (50, &t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	t.addBytesReceived (10); // Needed amount.
	
	eventLoopSleep (100, &t, SIGNAL(connectionTimedout(Nuria::AbstractTransport::Timeout)));
	
	// Timeout didn't trigger
	QCOMPARE(connectionTimedout.length (), 0);
	
}

QTEST_MAIN(AbstractTransportTest)
#include "tst_abstracttransport.moc"
