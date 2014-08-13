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

#include <nuria/httpmultipartreader.hpp>
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

class HttpMultiPartReaderTest : public QObject {
	Q_OBJECT
private slots:
	
	void happyPath ();
	void fragmentedTransfer ();
	void bytePerByteTransfer ();
	void valueStreaming ();
	
private:
	
	// Used in happyPath() and bytePerByteTransfer()
	QByteArray input = "--asdasdasd\r\n"
			   "Content-Disposition: form-data; name=\"foo\"\r\n\r\n"
			   "Foo\r\n"
			   "--asdasdasd\r\n"
			   "Content-Disposition: form-data; name=\"bar\"\r\n"
			   "Content-Type: text/html\r\n\r\n"
			   "Bar\r\n"
			   "--asdasdasd--\r\n";
	
	// 
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

void HttpMultiPartReaderTest::happyPath () {
	HttpMultiPartReader reader (createBuffer (this->input), "--asdasdasd");
	
	QVERIFY(!reader.hasFailed ());
	QVERIFY(reader.isComplete ());
	QCOMPARE(reader.fieldNames (), QStringList ({ "bar", "foo" }));
	
	QVERIFY(reader.fieldStream ("foo"));
	QVERIFY(reader.fieldStream ("bar"));
	QVERIFY(!reader.fieldStream ("baz"));
	
	QCOMPARE(reader.fieldMimeType ("foo"), QString ("text/plain"));
	QCOMPARE(reader.fieldValue ("foo"), QByteArray ("Foo"));
	
	QCOMPARE(reader.fieldMimeType ("bar"), QString ("text/html"));
	QCOMPARE(reader.fieldValue ("bar"), QByteArray ("Bar"));
}


void HttpMultiPartReaderTest::fragmentedTransfer () {
	QByteArray part1 = "--asdas";
	QByteArray part2 = "dasd\r\nContent-Disposition: ";
	QByteArray part3 = "form-data; name=\"foo\"\r\n"
			   "Content-Type: text/plain\r\n\r\n";
	QByteArray part4 = "Foo\r\n";
	QByteArray part5 = "--asdasdasd\r\n"
			   "Content-Disposition: form-data; name=\"bar\"\r\n"
			   "Content-Type: text/html\r";
	QByteArray part6 = "\n\r\nBar\r\n";
	QByteArray part7 = "--asdasdasd--\r\n";
	
	// Part 1 - Half boundary
	TestBuffer *buffer = createBuffer (part1);
	buffer->isAtEnd = false;
	
	HttpMultiPartReader reader (buffer, "--asdasdasd");
	QSignalSpy fieldFoundSpy (&reader, SIGNAL(fieldFound(QString)));
	QSignalSpy fieldCompletedSpy (&reader, SIGNAL(fieldCompleted(QString)));
	QSignalSpy completedSpy (&reader, SIGNAL(completed(bool)));
	
	// Part 2 - Half header
	appendToBuffer (buffer, part2);
	qApp->processEvents ();
	
	QVERIFY(fieldFoundSpy.isEmpty ());
	QVERIFY(fieldCompletedSpy.isEmpty ());
	QVERIFY(completedSpy.isEmpty ());
	
	// Part 3 - Begin of 'foo'
	appendToBuffer (buffer, part3);
	qApp->processEvents ();
	
	QCOMPARE(fieldFoundSpy.length (), 1);
	QVERIFY(fieldCompletedSpy.isEmpty ());
	QVERIFY(completedSpy.isEmpty ());
	QVERIFY(reader.hasField ("foo"));
	QVERIFY(reader.fieldStream ("foo"));
	
	// Part 4 - Data for 'foo' before the boundary.
	appendToBuffer (buffer, part4);
	qApp->processEvents ();
	
	QCOMPARE(fieldFoundSpy.length (), 1);
	QCOMPARE(reader.fieldNames (), QStringList ({ "foo" }));
	QVERIFY(fieldCompletedSpy.isEmpty ());
	QVERIFY(completedSpy.isEmpty ());
	QVERIFY(reader.hasField ("foo"));
	QVERIFY(reader.fieldStream ("foo"));
	
	// Part 5 - Boundary for 'foo', complete header for 'bar' except for the last \n
	appendToBuffer (buffer, part5);
	qApp->processEvents ();
	
	QCOMPARE(fieldFoundSpy.length (), 1);
	QCOMPARE(fieldCompletedSpy.length (), 1);
	QVERIFY(completedSpy.isEmpty ());
	QCOMPARE(reader.fieldValue ("foo"), QByteArray ("Foo"));
	
	// Part 6 - Trailing \n and value for 'bar'
	appendToBuffer (buffer, part6);
	qApp->processEvents ();
	
	QCOMPARE(fieldFoundSpy.length (), 2);
	QCOMPARE(fieldCompletedSpy.length (), 1);
	QVERIFY(completedSpy.isEmpty ());
	QVERIFY(reader.hasField ("bar"));
	QVERIFY(reader.fieldStream ("bar"));
	
	// Part 7 - Ending boundary, transfer complete.
	appendToBuffer (buffer, part7);
	buffer->isAtEnd = true;
	qApp->processEvents ();
	
	QCOMPARE(reader.fieldNames (), QStringList ({ "bar", "foo" }));
	QCOMPARE(fieldFoundSpy.length (), 2);
	QCOMPARE(fieldCompletedSpy.length (), 2);
	QCOMPARE(completedSpy.length (), 1);
	QCOMPARE(reader.fieldValue ("bar"), QByteArray ("Bar"));
	
	// Verify various details
	QCOMPARE(fieldFoundSpy.at (0).first (), QVariant ("foo"));
	QCOMPARE(fieldFoundSpy.at (1).first (), QVariant ("bar"));
	QCOMPARE(fieldCompletedSpy.at (0).first (), QVariant ("foo"));
	QCOMPARE(fieldCompletedSpy.at (1).first (), QVariant ("bar"));
	QCOMPARE(completedSpy.first ().first (), QVariant (true));
	
	QVERIFY(!reader.hasFailed ());
	QVERIFY(reader.isComplete ());
	
}

void HttpMultiPartReaderTest::bytePerByteTransfer () {
	TestBuffer *buffer = createBuffer ("");
	buffer->isAtEnd = false;
	
	HttpMultiPartReader reader (buffer, "--asdasdasd");
	
	// 
	for (int i = 0; i < this->input.length (); i++) {
		appendToBuffer (buffer, QByteArray (1, this->input.at (i)));
		
		if (i + 1 >= this->input.length ()) {
			buffer->isAtEnd = true;
		}
		
		qApp->processEvents ();
	}
	
	// 
	QVERIFY(!reader.hasFailed ());
	QVERIFY(reader.isComplete ());
	QCOMPARE(reader.fieldNames (), QStringList ({ "bar", "foo" }));
	
	QVERIFY(reader.fieldStream ("foo"));
	QVERIFY(reader.fieldStream ("bar"));
	QVERIFY(!reader.fieldStream ("baz"));
	
	QCOMPARE(reader.fieldMimeType ("foo"), QString ("text/plain"));
	QCOMPARE(reader.fieldValue ("foo"), QByteArray ("Foo"));
	
	QCOMPARE(reader.fieldMimeType ("bar"), QString ("text/html"));
	QCOMPARE(reader.fieldValue ("bar"), QByteArray ("Bar"));
}


void HttpMultiPartReaderTest::valueStreaming () {
	TestBuffer *buffer = createBuffer ("--asdasdasd\r\n"
					   "Content-Disposition: form-data; name=\"foo\"\r\n"
					   "Content-Type: text/plain\r\n\r\n");
	buffer->isAtEnd = false;
	
	HttpMultiPartReader reader (buffer, "--asdasdasd");
	
	// 
	QIODevice *foo; 
	QVERIFY(!reader.isComplete ());
	QVERIFY(reader.hasField ("foo"));
	QVERIFY(foo = reader.fieldStream ("foo"));
	QCOMPARE(foo->bytesAvailable (), 0);
	
	// 
	QByteArray testData1 = "SomethingLongerThanTheBoundary";
	appendToBuffer (buffer, testData1);
	qApp->processEvents ();
	
	QVERIFY(!reader.isComplete ());
	QCOMPARE(foo->bytesAvailable (), testData1.length ());
	QCOMPARE(foo->readAll (), testData1);
	
	// 
	QByteArray testData2 = "\r\nNewline\r\n"; // Shorter than the boundary
	appendToBuffer (buffer, testData2);
	qApp->processEvents ();
	
	QVERIFY(!reader.isComplete ());
	QCOMPARE(foo->bytesAvailable (), 0);
	
	// 
	appendToBuffer (buffer, "--asdasdasd--\r\n");
	qApp->processEvents ();
	
	QVERIFY(reader.isComplete ());
	
	QCOMPARE(foo->bytesAvailable (), testData2.length () - 2);
	QCOMPARE(foo->readAll (), QByteArray ("\r\nNewline"));
	
}

QTEST_MAIN(HttpMultiPartReaderTest)
#include "tst_httpmultipartreader.moc"
