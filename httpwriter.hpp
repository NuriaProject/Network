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

#ifndef NURIA_HTTPWRITER_HPP
#define NURIA_HTTPWRITER_HPP

#include "httpclient.hpp"
#include <QByteArray>

namespace Nuria {

/**
 * \brief Writer for HTTP data.
 */
class NURIA_NETWORK_EXPORT HttpWriter {
public:
	
	/** Constructor. */
	HttpWriter ();
	
	/**
	 * Returns the full HTTP name of \a version.
	 */
	QByteArray httpVersionToString (HttpClient::HttpVersion version);
	
	/**
	 * Returns the first line for a HTTP response header.
	 * If \a message is empty, HttpClient::httpStatusCodeName() will be used
	 * to generate an appropriate message.
	 */
	QByteArray writeResponseLine (HttpClient::HttpVersion version, int statusCode,
				      const QByteArray &message);
	
	/**
	 * Returns Set-Cookie header linedata from \a cookies.
	 */
	QByteArray writeSetCookies (const HttpClient::Cookies &cookies);
	
	/**
	 * Writes the value for a Set-Cookie HTTP header from \a cookie.
	 * \note This method is compliant with RFC 2109
	 * \note As of Qt5.3, QNetworkCookie::toRawForm() is buggy which is the
	 * reason we're doing it in here manually.
	 */
	QByteArray writeSetCookieValue (const QNetworkCookie &cookie);
	
	/**
	 * Takes \a headers and turns it into a HTTP header string.
	 */
	QByteArray writeHttpHeaders (const HttpClient::HeaderMap &headers);
	
	/**
	 * Returns the formatted value for a Date header based on \a dateTime.
	 * \note \a dateTime is assumed to be in UTC.
	 */
	QByteArray dateTimeToHttpDateHeader (const QDateTime &dateTime);
	
	/**
	 * Returns the value for a Range header.
	 */
	QByteArray buildRangeHeader (qint64 begin, qint64 end, qint64 totalLength);
	
	/**
	 * Adds missing HTTP headers to \a headers to make it compliant to
	 * \a version.
	 */
	void addComplianceHeaders (HttpClient::HttpVersion version, HttpClient::HeaderMap &headers);
	
	/**
	 * If \a begin is \c -1 and \a totalLength is not \c -1, then a
	 * \c Content-Length will be put into \a headers. If \a begin and
	 * \a end are not \c -1, a \c Content-Range header is stored in
	 * \a headers. If no conditions are met, \a headers is left untouched.
	 * 
	 * Existing headers are not changed.
	 */
	void applyRangeHeaders (qint64 begin, qint64 end, qint64 totalLength,
				HttpClient::HeaderMap &headers);
private:
	
};

}

#endif // NURIA_HTTPWRITER_HPP
