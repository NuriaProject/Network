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

#ifndef NURIA_HTTPPARSER_HPP
#define NURIA_HTTPPARSER_HPP

#include "httpclient.hpp"
#include <QByteArray>

class QIODevice;

namespace Nuria {

/**
 * \brief Parser functions for the HyperText Transfer Protocol.
 * 
 * \warning non-const references used to return data from parser methods point
 * to data from the ingoing QByteArray.
 */
class HttpParser {
public:
	/** Constructor. */
	HttpParser ();
	
	/** Destructor. */
	~HttpParser ();
	
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
};

}

#endif // NURIA_HTTPPARSER_HPP
