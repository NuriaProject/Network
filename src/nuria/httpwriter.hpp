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
	
	/**
	 * Adds a Transfer-Encoding header to \a headers if \a mode and
	 * \a encoding indicate chunked transfer. If there's already a
	 * Transfer-Encoding header, it will be modified accordingly.
	 */
	void addTransferEncodingHeader (HttpClient::TransferMode mode, HttpClient::HeaderMap &headers);
	
	/**
	 * Adds \a connectionValue to \a headers, if it is not empty,
	 * \a count is less than \a max and \a max is not \c -1. If the that
	 * condition is false, the function will add "Connection: close"
	 * instead.
	 */
	void addConnectionHeader (HttpClient::ConnectionMode mode, int count, int max,
	                          HttpClient::HeaderMap &headers);
	
private:
	
};

}

#endif // NURIA_HTTPWRITER_HPP
