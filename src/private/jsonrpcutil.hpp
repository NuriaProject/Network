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

#ifndef NURIA_INTERNAL_JSONRPCUTIL_HPP
#define NURIA_INTERNAL_JSONRPCUTIL_HPP

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QVector>

namespace Nuria {
namespace Internal {

/** Possible JSON-RPC versions */
enum JsonRpcVersion {
	
	// JSON-RPC specification error codes
	ParseError = -32700,
	InvalidRequest = -32600,
	MethodNotFound = -32601,
	InvalidParams = -32602,
	InternalError = -32603,
	
	// 
	InvalidElement = 0,
	JsonRpc2_0, /// Plain JSON-RPC 2.0
	JsonRpc2_0_Resource /// JSON-RPC 2.0 with Nuria::Resource extension
};

struct JsonRpcRequest {
	JsonRpcVersion version = InvalidElement;
	QString method;
	QString path; // Resource extension
	QVariantMap params; // Only maps are allowed
	QJsonValue id;
};

struct JsonRpcResponse {
	bool error = false; // Is the response an error?
	QJsonValue response; // "response" or "error" value
	QJsonValue id;
};

class JsonRpcUtil {
	JsonRpcUtil () = delete;
public:
	
	/**
	 * Returns a human readable string of \a error, or an empty string
	 * if \a error is greater-equal to \c InvalidElement. Returns
	 * "Server error" for any error codes in the range of [-32000, -32099].
	 */
	static QString jsonRpcErrorToString (int error);
	
	/**
	 * Looks at \a object and returns the used JSON-RPC version.
	 */
	static JsonRpcRequest dissectRequestObject (const QJsonObject &object);
	
	/** Constructs a JSON-RPC error response. */
	static JsonRpcResponse getSuccessResponse (const QJsonValue &id, const QVariant &result);
	
	/** Constructs a JSON-RPC error response. */
	static JsonRpcResponse getErrorResponse (const QJsonValue &id, int code, const QVariant &data = QVariant ());
	
	/** Returns the serialized form of \a response. */
	static QJsonObject serializeResponse (const JsonRpcResponse &response);
	
};

} // namespace Internal
} // namespace Nuria

#endif // NURIA_INTERNAL_JSONRPCUTIL_HPP
