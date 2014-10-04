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
