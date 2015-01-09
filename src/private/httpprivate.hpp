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

#ifndef NURIA_HTTPPRIVATE_HPP
#define NURIA_HTTPPRIVATE_HPP

#include "../nuria/httpclient.hpp"

#include <QDateTime>

namespace Nuria {

class TemporaryBufferDevice;

// Private data structure of Nuria::HttpClient
struct HttpClientPrivate {
	
	// 
	TemporaryBufferDevice *outBuffer = nullptr;
	HttpTransport *transport;
	HttpServer *server;
	
	//
	bool headerReady = false;
	bool headerSent = false;
	
	//
	HttpClient::HttpVersion requestVersion = HttpClient::HttpUnknown;
	HttpClient::HttpVerb requestType = HttpClient::InvalidVerb;
	HttpClient::HeaderMap requestHeaders;
	HttpClient::HeaderMap responseHeaders;
	HttpClient::Cookies requestCookies;
	HttpClient::Cookies responseCookies;
	HttpClient::TransferMode transferMode = HttpClient::Streaming;
	HttpClient::ConnectionMode connectionMode = HttpClient::ConnectionClose;
	
	QUrl path;
	
	int responseCode = 200;
	QString responseName;
	
	QIODevice *bufferDevice = nullptr;
	HttpPostBodyReader *bodyReader = nullptr;
	qint64 postBodyLength = -1;
	qint64 postBodyTransferred = 0;
	
	qint64 rangeStart = -1;
	qint64 rangeEnd = -1;
	qint64 contentLength = -1;
	
	//
	QIODevice *pipeDevice = nullptr;
	qint64 pipeMaxlen = -1;
	
	// 
	SlotInfo slotInfo;
	QVector< HttpFilter * > filters;
	
	// 
	bool keepConnectionOpen = false;
	bool connectionClosed = false;
};

class HttpNodePrivate {
public:
	
	HttpNode *parent;
	QString resourceName;
	QList< HttpNode * > nodes;
	
	QDir resourceDir;
	HttpNode::StaticResourcesMode resourceMode;
	
	// Named 'mySlots' instead of simply 'slots' to circuvement the #define
	// of 'slots' to '' in qobjectdefs.h
	QMap< QString , SlotInfo > mySlots;
	
};

class SlotInfoPrivate : public QSharedData {
public:
	
	Callback callback;
	
	bool streamPostBody = false;
	qint64 maxBodyLength = 4096 * 1024; // 4MiB
	HttpClient::HttpVerbs allowedVerbs = HttpClient::HttpVerbs (0xFF);
	bool forceEncrypted = false;
	
};

}

#endif // NURIA_HTTPPRIVATE_HPP
