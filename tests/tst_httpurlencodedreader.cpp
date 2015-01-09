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

#include <QCoreApplication>

#include <nuria/httpurlencodedreader.hpp>
#include <QtTest/QTest>
#include <QSignalSpy>
#include <QBuffer>
#include <QDebug>

using namespace Nuria;

class TestBuffer : public QBuffer {
	Q_OBJECT
public:
	using QBuffer::QBuffer;
	
	bool isAtEnd = true;
	
	bool atEnd () const override
	{ return this->isAtEnd; }
	
};

class HttpUrlEncodedReaderTest : public QObject {
	Q_OBJECT
private slots:
	
	void happyPath ();
	void bytePerByteTransfer ();
	void errorOnPrematureEnd ();
	
private:
	
	QByteArray input = "foo=bar&nuria=project";
	
	TestBuffer *createBuffer (const QByteArray &data) {
		TestBuffer *buffer = new TestBuffer (this);
		buffer->setData (data);
		buffer->open (QIODevice::ReadWrite);
		return buffer;
	}
	
	void appendToBuffer (QBuffer *buffer, const QByteArray &data) {
		buffer->blockSignals (true);
		qint64 pos = buffer->pos ();
		
		buffer->close ();
		buffer->buffer ().append (data);
		buffer->open (QIODevice::ReadWrite);
		buffer->seek (pos);
		
		buffer->blockSignals (false);
		emit buffer->readyRead ();
	}
	
};

void HttpUrlEncodedReaderTest::happyPath () {
	HttpUrlEncodedReader reader (createBuffer (this->input), "utf-8");
	
	QVERIFY(!reader.hasFailed ());
	QVERIFY(reader.isComplete ());
	QCOMPARE(reader.fieldNames (), QStringList ({ "foo", "nuria" }));
	
	QVERIFY(reader.fieldStream ("foo"));
	QVERIFY(reader.fieldStream ("nuria"));
	QVERIFY(!reader.fieldStream ("baz"));
	
	QCOMPARE(reader.fieldMimeType ("foo"), QString ("text/plain; charset=utf-8"));
	QCOMPARE(reader.fieldValue ("foo"), QByteArray ("bar"));
	QCOMPARE(reader.fieldStream ("foo")->readAll (), QByteArray ("bar"));
	
	QCOMPARE(reader.fieldMimeType ("nuria"), QString ("text/plain; charset=utf-8"));
	QCOMPARE(reader.fieldValue ("nuria"), QByteArray ("project"));
	QCOMPARE(reader.fieldStream ("nuria")->readAll (), QByteArray ("project"));
}

void HttpUrlEncodedReaderTest::bytePerByteTransfer () {
	TestBuffer *buffer = createBuffer ("");
	buffer->isAtEnd = false;
	
	HttpUrlEncodedReader reader (buffer, "latin-1");
	
	// 
	for (int i = 0; i < this->input.length (); i++) {
		appendToBuffer (buffer, QByteArray (1, this->input.at (i)));
		
		if (i + 1 >= this->input.length ()) {
			buffer->isAtEnd = true;
			emit buffer->readyRead ();
		}
		
		qApp->processEvents ();
	}
	
	// 
	QVERIFY(!reader.hasFailed ());
	QVERIFY(reader.isComplete ());
	QCOMPARE(reader.fieldNames (), QStringList ({ "foo", "nuria" }));
	
	QVERIFY(reader.fieldStream ("foo"));
	QVERIFY(reader.fieldStream ("nuria"));
	QVERIFY(!reader.fieldStream ("baz"));
	
	QCOMPARE(reader.fieldMimeType ("foo"), QString ("text/plain; charset=latin-1"));
	QCOMPARE(reader.fieldValue ("foo"), QByteArray ("bar"));
	QCOMPARE(reader.fieldStream ("foo")->readAll (), QByteArray ("bar"));
	
	QCOMPARE(reader.fieldMimeType ("nuria"), QString ("text/plain; charset=latin-1"));
	QCOMPARE(reader.fieldValue ("nuria"), QByteArray ("project"));
	QCOMPARE(reader.fieldStream ("nuria")->readAll (), QByteArray ("project"));
}

void HttpUrlEncodedReaderTest::errorOnPrematureEnd () {
	HttpUrlEncodedReader reader (createBuffer ("foo=bar&nuria"), "utf-8");
	
	QVERIFY(reader.isComplete ());
	QVERIFY(reader.hasFailed ());
	
}

QTEST_MAIN(HttpUrlEncodedReaderTest)
#include "tst_httpurlencodedreader.moc"
