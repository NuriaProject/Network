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

#include <QtTest/QTest>

#include "private/jsonrpcutil.hpp"

using namespace Nuria::Internal;

Q_DECLARE_METATYPE(Nuria::Internal::JsonRpcRequest);
Q_DECLARE_METATYPE(Nuria::Internal::JsonRpcResponse);
Q_DECLARE_METATYPE(Nuria::Internal::JsonRpcVersion);

class JsonRpcUtilTest : public QObject {
	Q_OBJECT
private slots:
	
	void jsonRpcErrorToString_data ();
	void jsonRpcErrorToString ();
	
	void dissectRequestObject_data ();
	void dissectRequestObject ();
	
	void dissectRequestArray_data ();
	void dissectRequestArray ();
	
	void getSuccessResponse ();
	void getErrorResponseWithData ();
	void getErrorResponseWithoutData ();
	
	void serializeResponse_data ();
	void serializeResponse ();


void JsonRpcUtilTest::jsonRpcErrorToString_data () {
	QTest::addColumn< int > ("error");
	QTest::addColumn< QString > ("expected");
	
	QTest::newRow ("unknown") << 0 << "";
	QTest::newRow ("ParseError") << int (ParseError) << "Parse error";
	QTest::newRow ("InvalidRequest") << int (InvalidRequest) << "Invalid Request";
	QTest::newRow ("MethodNotFound") << int (MethodNotFound) << "Method not found";
	QTest::newRow ("InvalidParams") << int (InvalidParams) << "Invalid params";
	QTest::newRow ("InternalError") << int (InternalError) << "Internal error";
	QTest::newRow ("ServerError-upper") << -32000 << "Server error";
	QTest::newRow ("ServerError") << -32050 << "Server error";
	QTest::newRow ("ServerError-lower") << -32099 << "Server error";
	
}

void JsonRpcUtilTest::jsonRpcErrorToString () {
	QFETCH(int, error);
	QFETCH(QString, expected);
	
	QCOMPARE(JsonRpcUtil::jsonRpcErrorToString (error), expected);
}

static JsonRpcRequest getJsonRpcRequest (JsonRpcVersion version, QString method = QString (), QString path = QString (),
                                         QVariantMap params = QVariantMap (), QJsonValue id = QJsonValue ()) {
	JsonRpcRequest r;
	r.version = version;
	r.method = method;
	r.path = path;
	r.params = params;
	r.id = id;
	
	return r;
}

void JsonRpcUtilTest::dissectRequestObject_data () {
	QTest::addColumn< QVariantMap > ("map");
	QTest::addColumn< JsonRpcRequest > ("expected");
	
	// Valid
	QTest::newRow ("valid")
	                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { { "1", "2" } } }, { "id", 123 } }
	                << getJsonRpcRequest (JsonRpc2_0, "foo", "", { { "1", "2" } }, 123);
	
	QTest::newRow ("valid-null-id")
	                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { { "1", "2" } } }, { "id", QVariant () } }
	                << getJsonRpcRequest (JsonRpc2_0, "foo", "", { { "1", "2" } }, QJsonValue ());
	
	QTest::newRow ("valid-no-id")
	                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { { "1", "2" } } } }
	                << getJsonRpcRequest (JsonRpc2_0, "foo", "", { { "1", "2" } },
	                                      QJsonValue (QJsonValue::Undefined));
	
	QTest::newRow ("valid-string-id")
	                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { { "1", "2" } } }, { "id", "asd" } }
	                << getJsonRpcRequest (JsonRpc2_0, "foo", "", { { "1", "2" } }, "asd");
	
	QTest::newRow ("valid-no-params")
	                << QVariantMap { { "jsonrpc", "2.0" }, { "method", "foo" }, { "id", 123 } }
	                << getJsonRpcRequest (JsonRpc2_0, "foo", "", { }, 123);
	
	QTest::newRow ("valid-uses-resource-extension")
                        << QVariantMap {
        { "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { { "1", "2" } } },
	{ "id", 123 }, { "path", "/nuria/project" } }
                        << getJsonRpcRequest (JsonRpc2_0_Resource, "foo", "/nuria/project", { { "1", "2" } }, 123);
	
	// Invalid
	QTest::newRow ("invalid-value-jsonrpc")
	                << QVariantMap {
	{ "jsonrpc", "1.0a" }, { "method", "foo" }, { "params", QVariantMap { } }, { "id", 123 } }
	                << getJsonRpcRequest (InvalidRequest);
	
	QTest::newRow ("invalid-no-jsonrpc")
	                << QVariantMap { { "method", "foo" }, { "params", QVariantMap { } }, { "id", 123 } }
	                << getJsonRpcRequest (InvalidRequest);
	
	QTest::newRow ("invalid-method-not-string")
                        << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", 123 }, { "params", QVariantMap { } }, { "id", 123 } }
                        << getJsonRpcRequest (InvalidRequest);
	
	QTest::newRow ("invalid-params-not-object")
	                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantList { } }, { "id", 123 } }
	                << getJsonRpcRequest (InvalidParams);
	
	QTest::newRow ("invalid-id-is-array")
	                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { } }, { "id", QVariantList { } } }
	                << getJsonRpcRequest (InvalidRequest);
	
	QTest::newRow ("invalid-id-is-object")
	                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { } }, { "id", QVariantMap { } } }
	                << getJsonRpcRequest (InvalidRequest);
	
	QTest::newRow ("invalid-path-not-string")
                        << QVariantMap {
        { "jsonrpc", "2.0" }, { "method", "foo" }, { "params", QVariantMap { { "1", "2" } } },
	{ "id", 123 }, { "path", 1337 } }
                        << getJsonRpcRequest (InvalidRequest);
        
}

void JsonRpcUtilTest::dissectRequestObject () {
	QFETCH(QVariantMap, map);
	QFETCH(Nuria::Internal::JsonRpcRequest, expected);
	
	JsonRpcRequest result = JsonRpcUtil::dissectRequestObject (QJsonObject::fromVariantMap (map));
	
	QCOMPARE(int (result.version), int (expected.version));
	QCOMPARE(result.id, expected.id);
	QCOMPARE(result.method, expected.method);
	QCOMPARE(result.params, expected.params);
	QCOMPARE(result.path, expected.path);
}

typedef QVector< Nuria::Internal::JsonRpcRequest > RequestList;

void JsonRpcUtilTest::dissectRequestArray_data () {
	QTest::addColumn< QVariantList > ("list");
	QTest::addColumn< JsonRpcVersion > ("expected");
	QTest::addColumn< QVector< Nuria::Internal::JsonRpcRequest > > ("expectedVector");
	
	QVariantMap validA {
		{ "jsonrpc", "2.0" }, { "method", "a" }, { "params", QVariantMap { { "1", "2" } } }, { "id", 123 }
	};
	
	QVariantMap validB {
		{ "jsonrpc", "2.0" }, { "method", "b" }, { "path", "/foo/bar" }, { "id", 456 }
	};
	
	// Valid
	QTest::newRow ("valid-1") << QVariantList { validA } << JsonRpc2_0
	                          << RequestList { getJsonRpcRequest (JsonRpc2_0, "a", "", { { "1", "2" } }, 123) };
	
	QTest::newRow ("valid-2") << QVariantList { validA, validB } << JsonRpc2_0
	                          << RequestList {
	                             getJsonRpcRequest (JsonRpc2_0, "a", "", { { "1", "2" } }, 123),
	                             getJsonRpcRequest (JsonRpc2_0_Resource, "b", "/foo/bar", { }, 456) };
	
	// Invalid
	QTest::newRow ("invalid-1") << QVariantList { QVariantMap { { "jsonrpc", 123 } } } << InvalidRequest
	                            << RequestList { };
	
	QTest::newRow ("invalid-2") << QVariantList { validA, QVariantMap { { "jsonrpc", 123 } } } << InvalidRequest
	                            << RequestList { };
	
	QTest::newRow ("invalid-empty-array") << QVariantList { } << InvalidRequest << RequestList { };
	QTest::newRow ("invalid-non-object-1") << QVariantList { 123 } << InvalidRequest << RequestList { };
	QTest::newRow ("invalid-non-object-2") << QVariantList { validA, 123 } << InvalidRequest << RequestList { };
	
}

static bool operator== (const JsonRpcRequest &left, const JsonRpcRequest &right) {
	return (left.version == right.version && left.method == right.method &&
	        left.params == right.params && left.path == right.path &&
	        left.id == right.id);
}

void JsonRpcUtilTest::dissectRequestArray () {
	QFETCH(QVariantList, list);
	QFETCH(Nuria::Internal::JsonRpcVersion, expected);
	QFETCH(QVector<Nuria::Internal::JsonRpcRequest>, expectedVector);
	
	QVector< JsonRpcRequest > vector;
	JsonRpcVersion result = JsonRpcUtil::dissectRequestArray (QJsonArray::fromVariantList (list), vector);
	
	QCOMPARE(result, expected);
	if (result > InvalidElement) {
		QCOMPARE(vector, expectedVector);
	}
	
}

void JsonRpcUtilTest::getSuccessResponse () {
	JsonRpcResponse resp = JsonRpcUtil::getSuccessResponse (42, "yay");
	
	QCOMPARE(resp.error, false);
	QCOMPARE(resp.id, QJsonValue (42));
	QCOMPARE(resp.response, QJsonValue ("yay"));
}

void JsonRpcUtilTest::getErrorResponseWithData () {
	QVariantMap response { { "code", -32600 }, { "message", "Invalid Request" }, { "data", "fail" } };
	
	JsonRpcResponse resp = JsonRpcUtil::getErrorResponse (42, -32600, "fail");
	
	QCOMPARE(resp.error, true);
	QCOMPARE(resp.id, QJsonValue (42));
	QCOMPARE(resp.response, QJsonValue (QJsonObject::fromVariantMap (response)));
	
}

void JsonRpcUtilTest::getErrorResponseWithoutData () {
	QVariantMap response { { "code", -32600 }, { "message", "Invalid Request" } };
	
	JsonRpcResponse resp = JsonRpcUtil::getErrorResponse (42, -32600);
	
	QCOMPARE(resp.error, true);
	QCOMPARE(resp.id, QJsonValue (42));
	QCOMPARE(resp.response, QJsonValue (QJsonObject::fromVariantMap (response)));
	
}

void JsonRpcUtilTest::serializeResponse_data () {
	QTest::addColumn< JsonRpcResponse > ("response");
	QTest::addColumn< QVariantMap > ("expected");
	
	QVariantMap error { { "code", -32600 }, { "message", "Invalid Request" } };
	
	QTest::newRow ("success") << JsonRpcUtil::getSuccessResponse (123, "yes.")
	                          << QVariantMap { { "jsonrpc", "2.0" }, { "result", "yes." }, { "id", 123 } };
	
	QTest::newRow ("success-no-id") << JsonRpcUtil::getSuccessResponse (QJsonValue::Undefined, "yes.")
	                                << QVariantMap {
	{ "jsonrpc", "2.0" }, { "result", "yes." }, { "id", QVariant () } };
	
	QTest::newRow ("error") << JsonRpcUtil::getErrorResponse (123, -32600)
	                        << QVariantMap { { "jsonrpc", "2.0" }, { "error", error }, { "id", 123 } };
	
	QTest::newRow ("error-no-id") << JsonRpcUtil::getErrorResponse (QJsonValue::Undefined, -32600)
	                              << QVariantMap {
	{ "jsonrpc", "2.0" }, { "error", error }, { "id", QVariant () } };
	
	
}

void JsonRpcUtilTest::serializeResponse () {
	QFETCH(Nuria::Internal::JsonRpcResponse, response);
	QFETCH(QVariantMap, expected);
	
	QVariantMap result = JsonRpcUtil::serializeResponse (response).toVariantMap ();
	QCOMPARE(result, expected);
	
}

QTEST_MAIN(JsonRpcUtilTest)
#include "tst_jsonrpcutil.moc"
