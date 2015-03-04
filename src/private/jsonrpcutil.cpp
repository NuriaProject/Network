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

#include "jsonrpcutil.hpp"

QString Nuria::Internal::JsonRpcUtil::jsonRpcErrorToString (int error) {
	if (error >= -32099 && error <= -32000) {
		return QStringLiteral("Server error");
	}
	
	switch (error) {
	case ParseError: return QStringLiteral("Parse error");
	case InvalidRequest: return QStringLiteral("Invalid Request");
	case MethodNotFound: return QStringLiteral("Method not found");
	case InvalidParams: return QStringLiteral("Invalid params");
	case InternalError: return QStringLiteral("Internal error");
	}
	
	return QString ();
}

Nuria::Internal::JsonRpcRequest Nuria::Internal::JsonRpcUtil::dissectRequestObject (const QJsonObject &object) {
	static const QString jsonrpc2_0 = QStringLiteral("2.0");
	static const QString jsonrpcField = QStringLiteral("jsonrpc");
	static const QString methodField = QStringLiteral("method");
	static const QString paramsField = QStringLiteral("params");
	static const QString idField = QStringLiteral("id");
	static const QString pathField = QStringLiteral("path"); // Resource extension
	
	// Get values
	JsonRpcRequest result;
	QJsonValue jsonrpcValue = object.value (jsonrpcField);
	QJsonValue methodValue = object.value (methodField);
	QJsonValue paramsValue = object.value (paramsField);
	QJsonValue idValue = object.value (idField);
	QJsonValue pathValue = object.value (pathField);
	
	// Json-Rpc 2.0 sanity check
	if (jsonrpcValue.type () != QJsonValue::String ||
	    idValue.type () == QJsonValue::Bool ||
	    idValue.type () == QJsonValue::Array ||
	    idValue.type () == QJsonValue::Object ||
	    methodValue.type () != QJsonValue::String ||
	    jsonrpcValue.toString () != jsonrpc2_0) {
		result.version = InvalidRequest;
		return result;
	}
	
	// This implementation needs the "params" field to be an object (if it exists)
	if (!paramsValue.isUndefined () && paramsValue.type () != QJsonValue::Object) {
		result.version = InvalidParams;
		return result;
	}
	
	// The request is at least a 2.0 request.
	result.version = JsonRpc2_0;
	
	// Check that if "path" exists it's a string. In this case, it's a
	// request with the Resource extension.
	if (!pathValue.isUndefined ()) {
		if (pathValue.isString ()) {
			result.version = JsonRpc2_0_Resource;
			result.path = pathValue.toString (); // Store 'path'
		} else {
			result.version = InvalidRequest;
			return result;
		}
		
	}
	
	// Fill 'result'
	result.method = methodValue.toString ();
	result.params = paramsValue.toObject ().toVariantMap ();
	result.id = idValue;
	return result;
}

Nuria::Internal::JsonRpcResponse
Nuria::Internal::JsonRpcUtil::getSuccessResponse (const QJsonValue &id, const QVariant &result) {
	JsonRpcResponse response;
	response.error = false;
	response.id = id;
	response.response = QJsonValue::fromVariant (result);
	
	return response;
}

Nuria::Internal::JsonRpcResponse Nuria::Internal::JsonRpcUtil::getErrorResponse (const QJsonValue &id, int code,
                                                                                 const QVariant &data) {
	static const QString codeField = QStringLiteral("code");
	static const QString messageField = QStringLiteral("message");
	static const QString dataField = QStringLiteral("data");
	
	// Build 'error' object
	QJsonObject error;
	error.insert (codeField, code);
	error.insert (messageField, jsonRpcErrorToString (code));
	
	if (data.isValid ()) {
		error.insert (dataField, QJsonValue::fromVariant (data));
	}
	
	// 
	JsonRpcResponse response;
	response.error = true;
	response.id = id;
	response.response = error;
	
	// Done
	return response;
}

QJsonObject Nuria::Internal::JsonRpcUtil::serializeResponse (const JsonRpcResponse &response) {
	static const QString jsonrpc2_0 = QStringLiteral("2.0");
	static const QString jsonrpcField = QStringLiteral("jsonrpc");
	static const QString idField = QStringLiteral("id");
	static const QString resultField = QStringLiteral("result");
	static const QString errorField = QStringLiteral("error");
	
	QJsonObject object;
	object.insert (jsonrpcField, jsonrpc2_0);
	object.insert (idField, response.id.isUndefined () ? QJsonValue () : response.id);
	object.insert (response.error ? errorField : resultField, response.response);
	
	return object;
}

QJsonArray Nuria::Internal::JsonRpcUtil::serializeResponseList (const QVector< JsonRpcResponse > &list) {
	QJsonArray array;
	
	for (int i = 0, count = list.length (); i < count; i++) {
		array.append (serializeResponse (list.at (i)));
	}
	
	return array;
}
