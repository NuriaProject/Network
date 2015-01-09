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

#include "nuria/httpparser.hpp"

Nuria::HttpParser::HttpParser () {
	
}

Nuria::HttpParser::~HttpParser () {
	
}

bool Nuria::HttpParser::removeTrailingNewline (QByteArray &data) {
	if (data.endsWith ("\r\n")) {
		data.chop (2);
	} else if (data.endsWith ('\n')) {
		data.chop (1);
	} else {
		return false;
	}
	
	return true;
}

bool Nuria::HttpParser::parseFirstLine (const QByteArray &data, QByteArray &verb,
					QByteArray &path, QByteArray &version) {
	// Format: "<VERB> <Path> HTTP/<Version>"
	int endOfVerb = data.indexOf (' ');
	int endOfPath = data.indexOf (' ', endOfVerb + 1);
	
	// Indexes
	int beginOfPath = endOfVerb + 1;
	int beginOfVersion = endOfPath + 1;
	int pathLength = endOfPath - beginOfPath;
	int versionLength = data.length () - beginOfVersion;
	
	// Sanity check
	if (endOfVerb == -1 || endOfPath == -1) {
		return false;
	}
	
	// Shallow copy fields
	verb = QByteArray (data.constData (), endOfVerb);
	path = QByteArray (data.constData () + beginOfPath, pathLength);
	QByteArray rawVersion = QByteArray (data.constData () + beginOfVersion, versionLength);
	
	// Verify fields
	if (verb.isEmpty () || path.isEmpty () || !rawVersion.startsWith ("HTTP/")) {
		return false;
	}
	
	// Shallow copy version string
	beginOfVersion += 5;
	versionLength -= 5;
	version = QByteArray (data.constData () + beginOfVersion, versionLength);
	
	// Done.
	return !version.isEmpty ();
}

bool Nuria::HttpParser::parseHeaderLine (const QByteArray &data, QByteArray &name,
					 QByteArray &value) {
	// Format: "<Name>: <Value>"
	// Note: It's fine for us if there's no space after the colon.
	int endOfName = data.indexOf (':');
	int beginOfValue = endOfName + 1;
	
	// Sanity check
	if (endOfName == -1 || data.length () - beginOfValue < 2) {
		return false;
	}
	
	// Optional space after the colon
	if (data.at (beginOfValue) == ' ') {
		beginOfValue++;
	}
	
	// Shallow copy
	int valueLength = data.length () - beginOfValue;
	name = QByteArray (data.constData (), endOfName);
        value = QByteArray (data.constData () + beginOfValue, valueLength);
	
	// Done.
	return (!name.isEmpty () && !value.isEmpty ());
}

Nuria::HttpClient::HttpVersion Nuria::HttpParser::parseVersion (const QByteArray &version) {
	
	if (version == "1.1") {
		return HttpClient::Http1_1;
	}
	
	if (version == "1.0") {
		return HttpClient::Http1_0;
	}
	
	return HttpClient::HttpUnknown;
}

Nuria::HttpClient::HttpVerb Nuria::HttpParser::parseVerb (const QByteArray &verb) {
	if (verb == "GET") {
		return HttpClient::GET;
	}
	
	if (verb == "POST") {
		return HttpClient::POST;
	}
	
	if (verb == "HEAD") {
		return HttpClient::HEAD;
	}
	
	if (verb == "PUT") {
		return HttpClient::PUT;
	}
	
	if (verb == "DELETE") {
		return HttpClient::DELETE;
	}
	
	return HttpClient::InvalidVerb;
}

Nuria::HttpClient::TransferMode Nuria::HttpParser::decideTransferMode (HttpClient::HttpVersion version,
                                                                       const QByteArray &connectionHeader) {
	HttpClient::TransferMode default10 = HttpClient::Streaming;
	HttpClient::TransferMode default11 = HttpClient::ChunkedStreaming;
	
	if (!qstricmp (connectionHeader.constData (), "close")) {
		return HttpClient::Streaming;
	}
	
	if (!qstricmp (connectionHeader.constData (), "keep-alive")) {
		return HttpClient::ChunkedStreaming;
	}
	
	return (version == HttpClient::Http1_0) ? default10 : default11;
}

bool Nuria::HttpParser::parseRangeHeaderValue (const QByteArray &value, qint64 &begin, qint64 &end) {
	// Format: "bytes=<Begin>-<End>"
	
	if (!value.startsWith ("bytes=")) {
		return false;
	}
	
	// Index
	int beginOfBegin = 6; // After 'bytes='
	int endOfBegin = value.indexOf ('-', beginOfBegin);
	int beginLength = endOfBegin - beginOfBegin;
	int beginOfEnd = endOfBegin + 1;
	int endLength = value.length () - beginOfEnd;
	
	// Sanity check
	if (endOfBegin == -1 || beginLength < 1 || endLength < 1) {
		return false;
	}
	
	// Read parts
	QByteArray beginData = QByteArray (value.constData () + beginOfBegin, beginLength);
	QByteArray endData = QByteArray (value.constData () + beginOfEnd, endLength);
	
	// Parse integers
	bool beginOk = false;
	bool endOk = false;
	begin = beginData.toLongLong (&beginOk);
	end = endData.toLongLong (&endOk);
	
	// Done.
	return (beginOk && endOk && begin >= 0 && end >= 0 && begin < end);
}

QByteArray Nuria::HttpParser::correctHeaderKeyCase (QByteArray key) {
	for (int i = 0; i < key.length (); i++) {
		if (islower (key.at (i)) && (i == 0 || key.at (i - 1) == '-')) {
			key[i] = toupper (key.at (i));
		}
		
	}
	
	return key;
}

static int readUnquotedCookieValue (QNetworkCookie &cookie, const QByteArray &data, int offset) {
	int end = data.indexOf (';', offset);
	if (end == -1) {
		end = data.length ();
	}
	
	// 
	cookie.setValue (QByteArray::fromPercentEncoding (data.mid (offset, end - offset)));
	return end + 1;
}

static int readQuotedCookieValue (QNetworkCookie &cookie, const QByteArray &data, int offset) {
	int end = data.indexOf ("\"", offset + 1);
	if (end == -1 || (end + 1 < data.length () && data.at (end + 1) != ';')) {
		return -1;
	}
	
	// 
	offset++;
	cookie.setValue (QByteArray::fromPercentEncoding (data.mid (offset, end - offset)));
	return end + 2;
	
}

static QNetworkCookie parseSingleCookie (const QByteArray &data, int &offset) {
	QNetworkCookie cookie;
	
	// 
	int equalSign = data.indexOf ('=', offset);
	if (equalSign < 1) {
		offset = -1;
		return cookie;
	}
	
	// 
	QByteArray name = data.mid (offset, equalSign - offset).trimmed ();
	cookie.setName (name);
	
	if (name.contains (';') || data.length () < equalSign + 1) {
		offset = -1;
	} else if (data.at (equalSign + 1) == '"') {
		offset = readQuotedCookieValue (cookie, data, equalSign + 1);
	} else {
		offset = readUnquotedCookieValue (cookie, data, equalSign + 1);
	}
	
	// 
	return cookie;
	
}

bool Nuria::HttpParser::parseCookies (const QByteArray &data, Nuria::HttpClient::Cookies &target) {
	int offset = 0;
	for (QNetworkCookie cookie; !(cookie = parseSingleCookie (data, offset)).name ().isEmpty () && offset != -1; ) {
		target.insert (cookie.name (), cookie);
		
		// 
		if (offset >= data.length ()) {
			break;
		}
	}
	
	return (offset != -1);
}

bool Nuria::HttpParser::parseFirstLineFull (const QByteArray &line, Nuria::HttpClient::HttpVerb &verb,
					    QByteArray &path, Nuria::HttpClient::HttpVersion &version) {
	QByteArray rawVerb;
	QByteArray rawVersion;
	
	// Parse first line ..
	if (!parseFirstLine (line, rawVerb, path, rawVersion)) {
		return false;
	}
	
	// Parse verb
	verb = parseVerb (rawVerb);
	if (verb == HttpClient::InvalidVerb) {
		return false;
	}
	
	// Parse HTTP version
	version = parseVersion (rawVersion);
	if (version == HttpClient::HttpUnknown) {
		return false;
	}
	
	// Done
	return true;
}
