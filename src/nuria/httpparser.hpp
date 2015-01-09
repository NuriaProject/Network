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

#ifndef NURIA_HTTPPARSER_HPP
#define NURIA_HTTPPARSER_HPP

#include "httpclient.hpp"
#include <QByteArray>

class QIODevice;

namespace Nuria {

/**
 * \brief Parser functions for the HyperText Transfer Protocol.
 */
class NURIA_NETWORK_EXPORT HttpParser {
public:
	/** Constructor. */
	HttpParser ();
	
	/** Destructor. */
	~HttpParser ();
	
	/**
	 * Removes trailing newline characters according to HTTP.
	 * Returns \c false if there were no newline characters.
	 */
	bool removeTrailingNewline (QByteArray &data);
	
	/**
	 * Parses the first line from a HTTP request header from \a data and
	 * lets \a verb, \a path and \a version point to the specific parts in
	 * \a data. Returns \c true on success.
	 */
	bool parseFirstLine (const QByteArray &data, QByteArray &verb, QByteArray &path,
			     QByteArray &version);
	/**
	 * Parses a single HTTP header line from \a data and puts the result
	 * into \a name and \a value. Returns \c true on success.
	 */
	bool parseHeaderLine (const QByteArray &data, QByteArray &name, QByteArray &value);
	
	/**
	 * Tries to parse \a version. Returns \c HttpClient::HttpUnknown if
	 * \a version didn't match any known version string.
	 */
	HttpClient::HttpVersion parseVersion (const QByteArray &version);
	
	/**
	 * Tries to parse \a verb. Returns \c HttpClient::InvalidVerb if
	 * \a verb didn't match any known HTTP verb.
	 */
	HttpClient::HttpVerb parseVerb (const QByteArray &verb);
	
	/**
	 * Returns the appropriate HttpClient::TransferMode for
	 * \a connectionHeader. Defaults to HttpClient::Streaming.
	 */
	HttpClient::TransferMode decideTransferMode (HttpClient::HttpVersion version,
	                                             const QByteArray &connectionHeader);
	
	/**
	 * Parses \a value which contains the value of a "Range" HTTP header.
	 * On success, it sets \a begin and \a end to the given values and
	 * returns \c true.
	 */
	bool parseRangeHeaderValue (const QByteArray &value, qint64 &begin, qint64 &end);
	
	/**
	 * Takes a key name of a http header and 'corrects' the case,
	 * so 'content-length' becomes 'Content-Length'.
	 */
	QByteArray correctHeaderKeyCase (QByteArray key);
	
	/**
	 * Parses \a data from a 'Cookie' header sent by the client and puts the
	 * result into \a target. Returns \c true on success.
	 */
	bool parseCookies (const QByteArray &data, HttpClient::Cookies &target);
	
	/**
	 * Parses the first line of a HTTP header in \a line. The results are
	 * put into \a verb, \a path and \a version. If any parser step fails,
	 * \c false is returned.
	 */ 
	bool parseFirstLineFull (const QByteArray &line, HttpClient::HttpVerb &verb,
				 QByteArray &path, HttpClient::HttpVersion &version);
};


}

#endif // NURIA_HTTPPARSER_HPP
