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

#include "fastcgihttpserver.hpp"

#include <QLocalServer>
#include <QTcpServer>

#include "private/httpprivate.hpp"
#include <debug.hpp>

#define FCGI_VERSION 1

enum FcgiType {
	FcgiBeginRequest = 1,
	FcgiAbortRequest = 2,
	FcgiEndRequest= 3,
	FcgiParams = 4,
	FcgiStdin = 5,
	FcgiStdout = 6,
	FcgiStderr = 7,
	FcgiData = 8,
	FcgiGetValues = 9,
	FcgiGetValuesResult = 10,
	FcgiUnknownType = 11
};

enum FcgiRole {
	FcgiResponder = 1,
	FcgiAuthorizer = 2,
	FcgiFilter = 3
};

enum FcgiProtocolStatus {
	FcgiRequestComplete = 0,
	FcgiCantMpxConn = 1,
	FcgiOverloaded = 2,
	FcgiUnknownRole = 3
};

struct FcgiHeader {
	quint8 version;
	quint8 type;
	quint16 requestId;
	quint16 contentLength;
	quint8 paddingLength;
	quint8 reserved;
	/*
	quint8 contentData[contentLength];
	quint8 paddingData[paddingLength];
	*/
};

struct FcgiBeginRequestBody {
	quint16 rol;
	quint8 flags;
	quint8 reserved[5];
};

struct FcgiBeginRequestRecord {
	FcgiHeader header;
	FcgiBeginRequestBody body;
};

/*
 * Mask for flags component of FCGI_BeginRequestBody
 */
#define FCGI_KEEP_CONN  1

struct FcgiEndRequestBody {
	quint32 appStatus;
	quint8 protocolStatus;
	quint8 reserved[3];
};

struct FcgiEndRequestRecord {
	FcgiHeader header;
	FcgiEndRequestBody body;
};

/*
 * Variable names for FCGI_GET_VALUES / FCGI_GET_VALUES_RESULT records
 */
#define FCGI_MAX_CONNS  "FCGI_MAX_CONNS"
#define FCGI_MAX_REQS   "FCGI_MAX_REQS"
#define FCGI_MPXS_CONNS "FCGI_MPXS_CONNS"

struct FcgiUnknownTypeBody {
	quint8 type;
	quint8 reserved[7];
};

struct FcgiUnknownTypeRecord {
	FcgiHeader header;
	FcgiUnknownTypeBody body;
};

Nuria::FastCgiHttpServer::FastCgiHttpServer (QObject *parent)
	: AbstractHttpServer (new FastCgiHttpServerPrivate, parent)
{
	
	// 
	Q_D(FastCgiHttpServer);
	d->server = new QTcpServer;
	// d->server->setSocketDescriptor(123);
	
	QByteArray socketDesc = qgetenv ("FCGI_LISTENSOCK_FILENO");
	QByteArray bindAddr = qgetenv("FCGI_WEB_SERVER_ADDRS");
	
}
