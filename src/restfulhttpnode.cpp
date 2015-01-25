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

#include "nuria/restfulhttpnode.hpp"

#include <nuria/serializer.hpp>
#include <nuria/bitutils.hpp>
#include <nuria/callback.hpp>
#include <QRegularExpression>
#include <nuria/logger.hpp>
#include <QJsonDocument>
#include <QDateTime>

enum { NumberOfHandlers = 5 };

struct InvokeInfo {
	Nuria::Callback callback;
	QStringList argNames;
	bool waitForRequestBody;
};

namespace Nuria {

namespace Internal {
struct RestfulHttpNodeSlotData {
	QRegularExpression path;
	InvokeInfo handlers[NumberOfHandlers];
};
}

class RestfulHttpNodePrivate {
public:
	
	bool loaded = false;
	MetaObject *metaObject = nullptr;
	void *object = nullptr;
	
	QMap< QString, Internal::RestfulHttpNodeSlotData > methods;
	
};

}

Nuria::RestfulHttpNode::RestfulHttpNode (void *object, MetaObject *metaObject, const QString &resourceName,
					 HttpNode *parent)
	: Nuria::HttpNode (resourceName, parent), d_ptr (new RestfulHttpNodePrivate)
{
	
	this->d_ptr->loaded = (metaObject != nullptr);
	this->d_ptr->metaObject = metaObject;
	this->d_ptr->object = object;
	
}

Nuria::RestfulHttpNode::RestfulHttpNode (const QString &resourceName, Nuria::HttpNode *parent)
	: RestfulHttpNode (this, nullptr, resourceName, parent)
{
	
}

Nuria::RestfulHttpNode::~RestfulHttpNode () {
	delete this->d_ptr;
}

void Nuria::RestfulHttpNode::setRestfulHandler (Nuria::HttpClient::HttpVerbs verbs, const QString &path,
						const QStringList &argumentNames, const Nuria::Callback &callback,
                                                bool waitForRequestPostBody) {
	QString compiled = compilePathRegEx (path);
	
	// Find control structure
	auto it = this->d_ptr->methods.find (compiled);
	if (it == this->d_ptr->methods.end ()) {
		Internal::RestfulHttpNodeSlotData data;
		data.path.setPattern (compiled);
		
		it = this->d_ptr->methods.insert (compiled, data);
	}
	
	// Prepare
	InvokeInfo info;
	info.callback = callback;
	info.argNames = argumentNames;
	info.waitForRequestBody = waitForRequestPostBody;
	
	// Store
	if (verbs & HttpClient::GET) it->handlers[0] = info;
	if (verbs & HttpClient::POST) it->handlers[1] = info;
	if (verbs & HttpClient::HEAD) it->handlers[2] = info;
	if (verbs & HttpClient::PUT) it->handlers[3] = info;
	if (verbs & HttpClient::DELETE) it->handlers[4] = info;
	
}

void Nuria::RestfulHttpNode::setRestfulHandler (const QString &path, const QStringList &argumentNames,
						const Nuria::Callback &callback, bool waitForRequestPostBody) {
	setRestfulHandler (HttpClient::AllVerbs, path, argumentNames, callback, waitForRequestPostBody);
}

QVariant Nuria::RestfulHttpNode::serializeVariant (const QVariant &variant) {
	int type = variant.userType ();
	
	// Qt Types
	switch (type) {
	case QMetaType::QByteArray:
	case QMetaType::QString:
	        return variant;
	case QMetaType::QDateTime:
		return variant.toDateTime ().toString (Qt::ISODate);
	case QMetaType::QDate:
		return variant.toDate ().toString (Qt::ISODate);
	case QMetaType::QTime:
		return variant.toTime ().toString (Qt::ISODate);
	}
	
	// POD types
	if ((type >= QMetaType::Bool && type <= QMetaType::Double) ||
	    (type >= QMetaType::Long && type <= QMetaType::Float)) {
		return variant;
	}
	
	// Lists and maps
	if (variant.canConvert< QVariantList > ()) {
		return deepConvertList (variant.toList ());
	} else if (variant.canConvert< QVariantMap > ()) {
		return deepConvertMap (variant.toMap ());
	}
	
	// Custom structures
	return serializeUserStructure (variant);
}

QVariant Nuria::RestfulHttpNode::convertArgumentToVariant (const QString &argumentData, int targetType) {
	QVariant variant = argumentData;
	
	if (targetType == QMetaType::QVariant) {
		return variant;
	}
	
	// 
	variant.convert (targetType);
	return variant;
}

void Nuria::RestfulHttpNode::conversionFailure (const QVariant &variant, Nuria::HttpClient *client) {
	nError() << "Failed to convert type" << variant.typeName ()
		 << "for URL" << client->path ().toString ();
	
	client->killConnection (500);
}

QByteArray Nuria::RestfulHttpNode::generateResultData (const QVariant &result, Nuria::HttpClient *client) {
	switch (result.userType ()) {
	case QMetaType::QByteArray:
		return result.toByteArray ();
	case QMetaType::QString:
		return result.toString ().toUtf8 ();
	case QMetaType::QJsonDocument:
		addJsonContentTypeHeaderToResponse (client);
		return result.value< QJsonDocument > ().toJson (QJsonDocument::Compact);
	}
	
	// 
	return sendVariantAsJson (serializeVariant (result), client);
}

static InvokeInfo &findInvokeInfo (Nuria::Internal::RestfulHttpNodeSlotData &data, Nuria::HttpClient *client) {
	int verbIdx = Nuria::ffs (int (client->verb ())) - 1;
	return data.handlers[verbIdx % NumberOfHandlers];
}

bool Nuria::RestfulHttpNode::invokePath (const QString &path, const QStringList &parts,
					 int index, Nuria::HttpClient *client) {
	delayedRegisterMetaObject ();
	
	// 
	if (client->verb () == HttpClient::InvalidVerb ||
	    !allowAccessToClient (path, parts, index, client)) {
		client->killConnection (403);
		return false;
	}
	
	// 
	QString interestingPart = QStringList (parts.mid (index)).join (QLatin1Char ('/'));
	
	// Reverse walk over the QMap, so we first check longer paths.
	auto it = this->d_ptr->methods.end ();
	auto end = this->d_ptr->methods.begin ();
	do {
		it--;
		Internal::RestfulHttpNodeSlotData &cur = *it;
		QRegularExpressionMatch match = cur.path.match (interestingPart);
		if (match.hasMatch ()) {
			return invokeMatch (cur, match, client);
		}
		
	} while (it != end);
	
	// 
	return HttpNode::invokePath (path, parts, index, client);
	
}

static Nuria::HttpClient::HttpVerbs verbsOfMetaMethod (Nuria::MetaMethod &method) {
	static const QByteArray annotation = QByteArrayLiteral("org.nuriaproject.network.restful.verbs");
	
	int idx = method.annotationLowerBound (annotation);
	if (idx == -1) {
		return Nuria::HttpClient::AllVerbs;
	}
	
	// 
	return Nuria::HttpClient::HttpVerbs (method.annotation (idx).value ().toInt ());
}

static QString pathOfMetaMethod (Nuria::MetaMethod &method) {
	static const QByteArray annotation = QByteArrayLiteral("org.nuriaproject.network.restful");
	
	int idx = method.annotationLowerBound (annotation);
	if (idx == -1) {
		return QString ();
	}
	
	// 
	return method.annotation (idx).value ().toString ();
}

void Nuria::RestfulHttpNode::registerAnnotatedHandlers () {
	this->d_ptr->loaded = true;
	
	if (!this->d_ptr->metaObject) {
		this->d_ptr->metaObject = MetaObject::byName (metaObject ()->className ());
		if (!this->d_ptr->metaObject) {
			nError() << "Failed to auto-find meta object of class" << metaObject ()->className ();
			return;
		}
		
	}
	
	// 
	MetaObject *meta = this->d_ptr->metaObject;
	for (int i = 0; i < meta->methodCount (); i++) {
		MetaMethod method = meta->method (i);
		registerMetaMethod (method);
	}
	
}

void Nuria::RestfulHttpNode::registerRestfulHandlerFromMethod (const QString &path, Nuria::MetaMethod &method) {
	HttpClient::HttpVerbs verbs = verbsOfMetaMethod (method);
	
	QStringList arguments = argumentNamesWithoutClient (method);
	setRestfulHandler (verbs, path, arguments, method.callback (this->d_ptr->object));
	
}

void Nuria::RestfulHttpNode::delayedRegisterMetaObject () {
	if (!this->d_ptr->loaded) {
		this->d_ptr->loaded = true;
		registerAnnotatedHandlers ();
	}
	
}

QStringList Nuria::RestfulHttpNode::argumentNamesWithoutClient (Nuria::MetaMethod &method) {
	QVector< QByteArray > names = method.argumentNames ();
	QVector< QByteArray > types = method.argumentTypes ();
	
	QStringList list;
	for (int i = 0; i < names.length (); i++) {
		if (types.at (i) != "Nuria::HttpClient*") {
			list.append (QString::fromLatin1 (names.at (i)));
		}
		
	}
	
	return list;
}

template< typename T >
static QString replaceInString (const QRegularExpression &rx, QString string, T func) {
	QRegularExpressionMatchIterator it = rx.globalMatch (string);
	
	int offset = 0;
	while (it.hasNext ()) {
		QRegularExpressionMatch match = it.next ();
		QString replaceWith = func (match);
		
		int length = match.capturedLength ();
		int begin = match.capturedStart () + offset;
		string.replace (begin, length, replaceWith);
		offset += replaceWith.length () - length;
	}
	
	return string;
}

static QString insertRegularExpression (QRegularExpressionMatch &match) {
	bool atEnd = match.capturedRef (2).isEmpty ();
	
	QString result = QStringLiteral("(?<");
	result.append (match.capturedRef (1));
	result.append (QStringLiteral(">"));
	
	if (atEnd) {
		result.append (QStringLiteral(".+)"));
	} else {
		result.append (QStringLiteral("[^"));
		result.append (match.capturedRef (2));
		result.append (QStringLiteral("]+)"));
		result.append (match.capturedRef (2));
	}
	
	return result;
}

QString Nuria::RestfulHttpNode::compilePathRegEx (QString path) {
	static const QRegularExpression rx ("{([^}]+)}(.|$)");
	path = replaceInString (rx, path, insertRegularExpression);
	
	path.prepend ("^");
	path.append ("$");
	
	return path;
}

bool Nuria::RestfulHttpNode::invokeMatchLater (Callback callback, const QVariantList &arguments, HttpClient *client) {
	Callback invoker (this, &RestfulHttpNode::invokeMatchNow);
	invoker.bind (callback, arguments);
	
	SlotInfo slotInfo (invoker);
	client->setSlotInfo (slotInfo);
	return true;
}

bool Nuria::RestfulHttpNode::invokeMatchNow (Callback callback, const QVariantList &arguments, HttpClient *client) {
	int resultType = callback.returnType ();
	QVariant result = callback.invoke (arguments);
	
	if ((resultType == QMetaType::QVariant && !result.isValid ()) ||
	    (resultType != QMetaType::Void && resultType != 0 &&
	     resultType != result.userType ())) {
	        return false;
	}
	
	// Send response
	return writeResponse (result, client);
}

bool Nuria::RestfulHttpNode::invokeMatch (Internal::RestfulHttpNodeSlotData &slotData,
					  QRegularExpressionMatch &match, HttpClient *client) {
	InvokeInfo &info = findInvokeInfo (slotData, client);
	
	// Prepare arguments
	QList< int > types = info.callback.argumentTypes ();
	QVariantList arguments = argumentValues (info.argNames, types, match, client);
	
	// Sanity check
	if (types.length () != arguments.length ()) {
		return false;
	}
	
	// Wait for POST body if we're not streaming.
	if (info.waitForRequestBody && client->postBodyLength () > 0 &&
	    client->postBodyLength () > client->postBodyTransferred ()) {
		return invokeMatchLater (info.callback, arguments, client);
	}
	
	// Invoke
	return invokeMatchNow (info.callback, arguments, client);
}

QVariantList Nuria::RestfulHttpNode::argumentValues (const QStringList &names, const QList< int > &types,
						     QRegularExpressionMatch &match, HttpClient *client) {
	QVariantList list;
	
	int i;
	for (i = 0; i < types.length (); i++) {
		if (types.at (i) == qMetaTypeId< HttpClient * > ()) {
			list.append (QVariant::fromValue (client));
			continue;
		}
		
		// 
		QString data = match.captured (names.at (i));
		if (data.isEmpty ()) {
			return QVariantList ();
		}
		
		QVariant value = convertArgumentToVariant (data, types.at (i));
		if (!value.isValid ()) {
			return QVariantList ();
		}
		
		list.append (value);
	}
	
	return list;
}

bool Nuria::RestfulHttpNode::writeResponse (const QVariant &response, Nuria::HttpClient *client) {
	if (!response.isValid ()) {
		return true;
	}
	
	// 
	QByteArray responseData = generateResultData (response, client);
	if (responseData.isEmpty ()) {
		return false;
	}
	
	// 
	client->write (responseData);
	return true;
}

void Nuria::RestfulHttpNode::addJsonContentTypeHeaderToResponse (HttpClient *client) {
	static const QByteArray json = QByteArrayLiteral("application/json");
	
	if (!client->hasResponseHeader (HttpClient::HeaderContentType)) {
		client->setResponseHeader (HttpClient::HeaderContentType, json);
	}
	
}

QVariantMap Nuria::RestfulHttpNode::deepConvertMap (QVariantMap map) {
	for (auto it = map.begin (), end = map.end (); it != end; ++it) {
		*it = serializeVariant (*it);
	}
	
	return map;
}

QVariantList Nuria::RestfulHttpNode::deepConvertList (QVariantList list) {
	for (auto it = list.begin (), end = list.end (); it != end; ++it) {
		*it = serializeVariant (*it);
	}
	
	return list;
	
}

QVariant Nuria::RestfulHttpNode::serializeUserStructure (const QVariant &variant) {
	Serializer serializer;
	void *objectPtr = const_cast< void * > (variant.constData ());
	QVariantMap serialized = serializer.serialize (objectPtr, variant.typeName ()); 
	
	// 
	if (!serializer.failedFields ().isEmpty ()) {
		return QVariant ();
	}
	
	return serialized;
	
}

QByteArray Nuria::RestfulHttpNode::sendVariantAsJson (const QVariant &variant, Nuria::HttpClient *client) {
	if (!variant.isValid ()) {
		return QByteArray ();
	}
	
	// Add JSON Content-Type and return serialized JSON data.
	addJsonContentTypeHeaderToResponse (client);
	return QJsonDocument::fromVariant (variant).toJson (QJsonDocument::Compact);
}

void Nuria::RestfulHttpNode::registerMetaMethod (MetaMethod &method) {
	QString path = pathOfMetaMethod (method);
	
	if (!path.isEmpty ()) {
		registerRestfulHandlerFromMethod (path, method);
	}
	
}
