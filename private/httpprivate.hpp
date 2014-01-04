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

#include "httpclient.hpp"

#include <QDateTime>

namespace Nuria {

// Private data structure of Nuria::HttpCookie
struct HttpCookiePrivate {
	
	QByteArray name;
	QByteArray value;
	
	QDateTime expires;
	qint64 maxAge;
	
	QString domain;
	QString path;
	int port;
	
	bool httpOnly;
	bool secure;
	
};

// Private data structure of Nuria::HttpClient
struct HttpClientPrivate {
	
	// 
	QTcpSocket *socket;
	HttpServer *server;
	
	//
	bool headerReady;
	bool headerSent;
	
	//
	Nuria::HttpClient::HttpVersion requestVersion;
	Nuria::HttpClient::HttpVerb requestType;
	QMultiMap< QByteArray, QByteArray > requestHeaders;
	QMultiMap< QByteArray, QByteArray > responseHeaders;
	Nuria::HttpClient::Cookies requestCookies;
	Nuria::HttpClient::Cookies responseCookies;
	
	QUrl path;
	
	int responseCode;
	QString responseName;
	
	int bufferMemorySize; // Defaults to 16kB
	QIODevice *bufferDevice;
	qint64 postBodyLength;
	qint64 postBodyTransferred;
	bool chunkedBodyTransfer; // For 100-continue stuff
	
	qint64 rangeStart;
	qint64 rangeEnd;
	qint64 contentLength;
	
	//
	QIODevice *pipeDevice;
	qint64 pipeMaxlen;
	
	// 
	SlotInfo *slotInfo;
	
	// 
	bool keepConnectionOpen;
	
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
	QMap< QString , SlotInfo * > mySlots;
	
};

class SlotInfoPrivate {
public:
	
	Callback callback;
	
	bool streamPostBody;
	qint64 maxBodyLength;
	HttpClient::HttpVerbs allowedVerbs;
	bool forceEncrypted;
	
};

}

#endif // NURIA_HTTPPRIVATE_HPP
