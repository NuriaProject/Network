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

#include "restfulhttpnode.hpp"

#include <QRegularExpression>
#include <serializer.hpp>
#include <QJsonDocument>
#include <callback.hpp>
#include <debug.hpp>

enum { NumberOfHandlers = 5 };

struct InvokeInfo {
	Nuria::Callback callback;
	QStringList argNames;
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
						const QStringList &argumentNames,
						const Nuria::Callback &callback) {
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
	
	// Store
	if (verbs & HttpClient::GET) it->handlers[0] = info;
	if (verbs & HttpClient::POST) it->handlers[1] = info;
	if (verbs & HttpClient::HEAD) it->handlers[2] = info;
	if (verbs & HttpClient::PUT) it->handlers[3] = info;
	if (verbs & HttpClient::DELETE) it->handlers[4] = info;
	
}

void Nuria::RestfulHttpNode::setRestfulHandler (const QString &path, const QStringList &argumentNames,
						const Nuria::Callback &callback) {
	setRestfulHandler (HttpClient::AllVerbs, path, argumentNames, callback);
}

QByteArray Nuria::RestfulHttpNode::convertVariantToData (const QVariant &variant) {
	
	// Static mappings
	switch (variant.userType ()) {
	case QMetaType::QString:
		return variant.toString ().toUtf8 ();
	case QMetaType::QByteArray:
		return variant.toByteArray ();
	}
	
	// 
	QVariant tryToString = Variant::convert< QString > (variant);
	if (tryToString.isValid ()) {
		return tryToString.toString ().toUtf8 ();
	}
	
	// 
	QVariant tryToByteArray = Variant::convert< QByteArray > (variant);
	return tryToByteArray.toByteArray ();
	
}

QVariant Nuria::RestfulHttpNode::convertArgumentToVariant (const QString &argumentData, int targetType) {
	if (targetType == QMetaType::QVariant) {
		return argumentData;
	}
	
	// 
	QVariant variant = Variant::convert (argumentData, targetType);
	return variant;
}

void Nuria::RestfulHttpNode::conversionFailure (const QVariant &variant, Nuria::HttpClient *client) {
	nError() << "Failed to convert type" << variant.typeName ()
		 << "for URL" << client->path ().toString ();
	
	client->killConnection (500);
}

QByteArray Nuria::RestfulHttpNode::generateResultData (QVariant result, Nuria::HttpClient *client) {
	Q_UNUSED(client);
	
	// 
	switch (result.userType ()) {
	case QMetaType::QByteArray:
		return result.toByteArray ();
	case QMetaType::QString:
		return result.toString ().toUtf8 ();
	case QMetaType::QVariantMap:
	case QMetaType::QVariantList: {
		return QJsonDocument::fromVariant (result).toJson (QJsonDocument::Compact);
	} break;
	}
	
	// Try serializing the structure
	Serializer serializer;
	void *objectPtr = const_cast< void * > (result.constData ());
	QVariantMap serialized = serializer.serialize (objectPtr, result.typeName ()); 
	
	// 
	if (!serializer.failedFields ().isEmpty ()) {
		return QByteArray ();
	}
	
	// Serialize map to JSON.
	return QJsonDocument::fromVariant (serialized).toJson (QJsonDocument::Compact);
}


#ifndef Q_OS_UNIX
#if defined(Q_CC_MINGW) || defined (Q_CC_GNU)
static inline int ffs (int i) { return __builtin_ffs (i); }
#elif defined(Q_CC_MSVC)
static inline int ffs (int i) { return _BitScanForward (i); }
#else
#error "Please provide a ffs (Find First Set Bit) function for your platform/compiler"
#endif
#endif

static InvokeInfo &findInvokeInfo (Nuria::Internal::RestfulHttpNodeSlotData &data, Nuria::HttpClient *client) {
	int verbIdx = ::ffs (client->verb ()) - 1;
	return data.handlers[verbIdx % NumberOfHandlers];
}

bool Nuria::RestfulHttpNode::invokePath (const QString &path, const QStringList &parts,
					 int index, Nuria::HttpClient *client) {
	delayedRegisterMetaObject ();
	
	// 
	if (!allowAccessToClient (path, parts, index, client)) {
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
	
	// Invoke
	int resultType = info.callback.returnType ();
	QVariant result = info.callback.invoke (arguments);
	
	if ((resultType == QMetaType::QVariant && !result.isValid ()) ||
	    (resultType != QMetaType::Void && resultType != 0 &&
	     resultType != result.userType ())) {
		return false;
	}
	
	// Send response
	return writeResponse (result, client);
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

void Nuria::RestfulHttpNode::registerMetaMethod (MetaMethod &method) {
	QString path = pathOfMetaMethod (method);
	
	if (!path.isEmpty ()) {
		registerRestfulHandlerFromMethod (path, method);
	}
	
}
