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

#ifndef NURIA_HTTPPOSTBODYREADER_HPP
#define NURIA_HTTPPOSTBODYREADER_HPP

#include "network_global.hpp"
#include <QStringList>
#include <QObject>

class QIODevice;

namespace Nuria {

/**
 * \brief Abstract class for readers of the body of HTTP POST requests.
 * 
 * When a client sends a POST request, the body may be in various formats.
 * This abstract class offers a abstraction layer around this, so
 * implementations don't have to deal with the differences.
 * 
 * Implementations of this abstract class will most likely use a QIODevice
 * as input. Please be aware that the device is thought to be sequential,
 * thus large amounts of data may be copied.
 * 
 */
class NURIA_NETWORK_EXPORT HttpPostBodyReader : public QObject {
	Q_OBJECT
public:
	
	/** Constructor. */
	explicit HttpPostBodyReader (QObject *parent = 0);
	
	/** Destructor. */
	~HttpPostBodyReader ();
	
	/**
	 * Returns \c true if the body has been read completely.
	 * Also returns \c true if parsing has failed.
	 * \sa completed hasFailed
	 */
	virtual bool isComplete () const = 0;
	
	/**
	 * Returns \c true if parsing has failed.
	 */
	virtual bool hasFailed () const = 0;
	
	/**
	 * Returns \c true if \a field exists. The default implementation
	 * looks for \a field in the result of fieldNames().
	 */
	virtual bool hasField (const QString &field) const;
	
	/** Returns the list of known field names. */
	virtual QStringList fieldNames () const = 0;
	
	/**
	 * Returns the MIME-Type of \a field.
	 * If \a field does not exist or the MIME-Type is unknown, returns an
	 * empty string.
	 */
	virtual QString fieldMimeType (const QString &field) const = 0;
	
	/**
	 * Returns the total length of \a field.
	 * If \a field is unknown or the length is unknown, returns \c -1.
	 */
	virtual qint64 fieldLength (const QString &field) const = 0;
	
	/** Returns the amount of bytes transferred of \a field. */
	virtual qint64 fieldBytesTransferred (const QString &field) const = 0;
	
	/**
	 * Returns \c true if transfer of \a field is complete. If it is not,
	 * or if \a field does not exist, \c false is returned.
	 * 
	 * The default implementation compares the result of fieldLength()
	 * to the one of fieldBytesTransferred().
	 */
	virtual bool isFieldComplete (const QString &field) const;
	
	/**
	 * Returns the value of \a field. If the field has not been received
	 * completely yet or there's no \a field, then a empty QByteArray is
	 * returned.
	 * 
	 * \warning If you're expecting large amount of data, use fieldStream()
	 * instead.
	 * 
	 * The default implementation uses infoStream() to obtain the device
	 * and read all of its content, if isFieldComplete() returns \c true.
	 */
	virtual QByteArray fieldValue (const QString &field);
	
	/**
	 * Returns a QIODevice which reads contents from \a field in a
	 * streaming fashion. If \a field has not been found, \c nullptr
	 * is returned instead.
	 * 
	 * \note The returned device is expected to be sequential. This means
	 * that returning random-access devices is ok, but not required.
	 * \note There is only one device per \a field. Subsequent calls return
	 * the same device.
	 * 
	 * If \a field is a known field but is currently been transferred,
	 * then the returned stream will operate upon the already received
	 * data and new received data is put into it.
	 * 
	 * \note Ownership of the device belongs to the reader.
	 */
	virtual QIODevice *fieldStream (const QString &field) = 0;
	
signals:
	
	/**
	 * Emitted when a field called \a fieldName has been started to be
	 * transferred. Information about it may not be complete yet.
	 */
	void fieldFound (const QString &fieldName);
	
	/**
	 * Emitted when a field called \a fieldName has been completely
	 * received.
	 */
	void fieldCompleted (const QString &fieldName);
	
	/**
	 * Emitted when the body has been completely processed.
	 * If \a success is \c true, parsing was successful. Else it failed.
	 * \sa isComplete
	 */
	void completed (bool success);
	
};

}

#endif // NURIA_HTTPPOSTBODYREADER_HPP
