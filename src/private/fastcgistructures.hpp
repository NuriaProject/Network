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

#ifndef FASTCGISTRUCTURES_HPP
#define FASTCGISTRUCTURES_HPP

#include <qcompilerdetection.h>
#include <QByteArray>
#include <QMap>

// FastCGI uses Big-Endian
#if Q_LITTLE_ENDIAN == Q_BYTE_ORDER
#include <QtEndian>
// Swap bytes for LE <-> BE
#define SWAP_BYTES(Var) Var = qbswap(Var)
#define SWAPPED(Value) qbswap(Value)
#else
// Do nothing on LE machines
#define SWAP_BYTES(Var) 
#define SWAPPED(Value) Value
#endif

enum { NameValueCharLimit = 127 };
typedef QMap< QByteArray, QByteArray > NameValueMap;

/*
  Structures taken from:
  http://www.fastcgi.com/drupal/node/6?q=node/22
*/

enum class FastCgiType : uint8_t {
	BeginRequest = 1,
	AbortRequest = 2,
	EndRequest = 3,
	Params = 4,
	StdIn = 5,
	StdOut = 6,
	StdErr = 7,
	Data = 8,
	GetValues = 9,
	GetValuesResult = 10,
	UnknownType = 11,
	MaxType = UnknownType
};

enum class FastCgiRole : uint16_t {
	Responder = 1,
	Authorizer = 2,
	Filter = 3
};

enum class FastCgiProtocolStatus : uint8_t {
	RequestComplete = 0,
	CantMultiplexConn = 1,
	Overloaded = 2,
	UnknownRole = 3
};

// 
struct FastCgiRecord {
	enum { Version = 1 };
	
	uint8_t version;
	FastCgiType type;
	uint16_t requestId;
	uint16_t contentLength;
	uint8_t paddingLength;
	uint8_t reserved;
	
	// uint8_t contentData[contentLength];
	// uint8_t paddingData[paddingLength];
	
} Q_PACKED;

// 
struct FastCgiBeginRequestBody {
	FastCgiRole role;
	uint8_t flags;
	uint8_t reserved[5];
} Q_PACKED;

struct FastCgiEndRequestBody {
	uint32_t appStatus;
	FastCgiProtocolStatus protocolStatus;
	uint8_t reserved[3];
} Q_PACKED;

struct FastCgiUnknownTypeBody {
	FastCgiType type;
	uint8_t reserved[7];
} Q_PACKED;

#endif // FASTCGISTRUCTURES_HPP
