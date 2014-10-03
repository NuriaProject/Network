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

#include "nuria/httpwriter.hpp"

#include <QDateTime>

#include "nuria/httpclient.hpp"

Nuria::HttpWriter::HttpWriter () {
	
}

QByteArray Nuria::HttpWriter::httpVersionToString (Nuria::HttpClient::HttpVersion version) {
	switch (version) {
	case HttpClient::HttpUnknown: return QByteArray ();
	case HttpClient::Http1_0: return QByteArrayLiteral("HTTP/1.0");
	case HttpClient::Http1_1: return QByteArrayLiteral("HTTP/1.1");
	}
	
	return QByteArray ();
}

QByteArray Nuria::HttpWriter::writeResponseLine (HttpClient::HttpVersion version, int statusCode,
						 const QByteArray &message) {
	QByteArray line = httpVersionToString (version);
	line.append (' ');
	line.append (QByteArray::number (statusCode));
	line.append (' ');
	
	if (message.isEmpty ()) {
		line.append (HttpClient::httpStatusCodeName (statusCode));
	} else {
		line.append (message);
	}
	
	line.append ("\r\n");
	return line;
}

QByteArray Nuria::HttpWriter::writeSetCookies (const Nuria::HttpClient::Cookies &cookies) {
	static const QByteArray linePrefix ("Set-Cookie: ");
	
	QByteArray data;
	for (const QNetworkCookie &cookie : cookies) {
		data.append (linePrefix);
		data.append (writeSetCookieValue (cookie));
		data.append ("\r\n");
	}
	
	return data;
}

QByteArray Nuria::HttpWriter::writeSetCookieValue (const QNetworkCookie &cookie) {
	// http://tools.ietf.org/html/rfc2109 Section 4.1
	QByteArray data = cookie.name ();
	data.append ('=');
	data.append (cookie.value ().toPercentEncoding ());
	
	// ; Domain=<Domain>
	if (!cookie.domain ().isEmpty ()) {
		data.append ("; Domain=");
		data.append (cookie.domain ().toLatin1 ());
	}
	
	// ; Path=<Path>
	if (!cookie.path ().isEmpty ()) {
		data.append ("; Path=");
		data.append (cookie.path ().toLatin1 ());
	}
	
	// ; Max-Age=<Lifetime>
	if (!cookie.isSessionCookie ()) {
		qint64 secsSinceEpoch = cookie.expirationDate ().toMSecsSinceEpoch () / 1000;
		qint64 curSecs = QDateTime::currentMSecsSinceEpoch () / 1000;
		qint64 lifetime = secsSinceEpoch - curSecs;
		
		if (lifetime < 0) {
			lifetime = 0;
		}
		
		data.append ("; Max-Age=");
		data.append (QByteArray::number (lifetime));
	}
	
	// ; Secure
	if (cookie.isSecure ()) {
		data.append ("; Secure");
	}
	
	// ; HttpOnly
	if (cookie.isHttpOnly ()) {
		data.append ("; HttpOnly");
	}
	
	// Done.
	return data;
}

QByteArray Nuria::HttpWriter::writeHttpHeaders (const Nuria::HttpClient::HeaderMap &headers) {
	auto it = headers.constBegin ();
	auto end = headers.constEnd ();
	
	QByteArray data;
	for (; it != end; ++it) {
		data.append (it.key ());
		data.append (": ");
		data.append (it.value ());
		data.append ("\r\n");
	}
	
	return data;
}

QByteArray Nuria::HttpWriter::dateTimeToHttpDateHeader (const QDateTime &dateTime) {
	static const char *days[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
				        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };	
	
	QDate date = dateTime.date ();
        QTime time = dateTime.time ();
	
	QByteArray dateString;
	dateString.resize (40);
	
	int len = qsnprintf (dateString.data (), dateString.size (),
			     "%s, %i %s %i %02i:%02i:%02i GMT",
			     days[(date.dayOfWeek () - 1) % 7], date.day (),
			     months[(date.month () - 1) % 12], date.year (),
			     time.hour (), time.minute (), time.second ());
	dateString.resize (len);
	return dateString;
}

QByteArray Nuria::HttpWriter::buildRangeHeader (qint64 begin, qint64 end, qint64 totalLength) {
	QByteArray range = "bytes " + QByteArray::number (begin) +
			   "-" + QByteArray::number (end) + 
			   "/";
	
	// Content-Length known?
	if (totalLength > -1) {
		range.append (QByteArray::number (totalLength));
	} else {
		range.append ('*');
	}
	
	return range;
}

void Nuria::HttpWriter::addComplianceHeaders (Nuria::HttpClient::HttpVersion version,
					      Nuria::HttpClient::HeaderMap &headers) {
	if (version != HttpClient::Http1_1) {
		return;
	}
	
	// HTTP1.1 wants a Date header.
	QByteArray dateHeader = HttpClient::httpHeaderName (HttpClient::HeaderDate);
	
	if (!headers.contains (dateHeader)) {
		QByteArray dateString = dateTimeToHttpDateHeader (QDateTime::currentDateTimeUtc ());
		headers.insert (dateHeader, dateString);
	}
	
}

void Nuria::HttpWriter::applyRangeHeaders (qint64 begin, qint64 end, qint64 totalLength,
					   Nuria::HttpClient::HeaderMap &headers) {
	QByteArray rangeHeader = HttpClient::httpHeaderName (HttpClient::HeaderContentRange);
	QByteArray lengthHeader = HttpClient::httpHeaderName (HttpClient::HeaderContentLength);
	
	if (begin != -1 && end != -1) {
		if (!headers.contains (rangeHeader)) {
			QByteArray rangeValue = buildRangeHeader (begin, end, totalLength);
			headers.insert (rangeHeader, rangeValue);
		}
		
		if (!headers.contains (lengthHeader)) {
			QByteArray lengthValue = QByteArray::number (end - begin);
			headers.insert (lengthHeader, lengthValue);
		}
		
	} else if (totalLength != -1) {
		if (!headers.contains (lengthHeader)) {
			QByteArray lengthValue = QByteArray::number (totalLength);
			headers.insert (lengthHeader, lengthValue);
		}
		
	}
	
}

void Nuria::HttpWriter::addTransferEncodingHeader (HttpClient::TransferMode mode, HttpClient::HeaderMap &headers) {
	QByteArray transEncodingHeader = HttpClient::httpHeaderName (HttpClient::HeaderTransferEncoding);
	static const QByteArray chunked = QByteArrayLiteral("chunked");
	
	if (mode != HttpClient::ChunkedStreaming) {
		return;
	}
	
	// 
	QByteArray headerValue = headers.value (transEncodingHeader);
	if (headerValue.isEmpty ()) {
		headerValue = chunked;
	} else if (!headerValue.contains (chunked)) {
		headerValue.append (", chunked");
	}
	
	// Store
	headers.insert (transEncodingHeader, headerValue);
	
}

void Nuria::HttpWriter::addConnectionHeader (HttpClient::ConnectionMode mode, int count, int max,
                                             HttpClient::HeaderMap &headers) {
	QByteArray connectionHeader = HttpClient::httpHeaderName (HttpClient::HeaderConnection);
	static const QByteArray close = QByteArrayLiteral("close");
	static const QByteArray keepAlive = QByteArrayLiteral("keep-alive");
	
	if (count < max || max == -1) {
		if (mode == HttpClient::ConnectionKeepAlive) {
			headers.insert (connectionHeader, keepAlive);
		} else {
			headers.insert (connectionHeader, close);
		}
		
	} else {
		headers.insert (connectionHeader, close);
	}
	
}
