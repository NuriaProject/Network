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
