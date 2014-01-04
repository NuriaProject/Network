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

#include <callback.hpp>
#include <QMetaObject>
#include <QMetaMethod>
#include <debug.hpp>

#ifdef NURIA_USING_QT5
#include <QJsonDocument>
#include <QUrlQuery>
#endif

struct SlotData {
	Nuria::Callback callback;
	QStringList argNames;
	QByteArray signature;
};

namespace Nuria {

class RestfulHttpNodePrivate {
public:
	
	RestfulHttpNodePrivate () : loaded (false) {}
	
	RestfulHttpNode::ReplyFormat format;
	QMap< QString, SlotData > methods;
	bool loaded;
	
	void invokeQtSlot (RestfulHttpNode *p, const SlotData &slot, Nuria::HttpClient *client);
	
};

}

Nuria::RestfulHttpNode::RestfulHttpNode (Nuria::RestfulHttpNode::ReplyFormat format,
					const QString &resourceName, Nuria::HttpNode *parent)
	: Nuria::HttpNode (resourceName, parent), d_ptr (new RestfulHttpNodePrivate)
{
	
	this->d_ptr->format = format;
	
}

Nuria::RestfulHttpNode::RestfulHttpNode (const QString &resourceName, Nuria::HttpNode *parent)
	: Nuria::HttpNode (resourceName, parent), d_ptr (new RestfulHttpNodePrivate)
{
	
	this->d_ptr->format = JSON;
}

Nuria::RestfulHttpNode::RestfulHttpNode (Nuria::RestfulHttpNode::ReplyFormat format, QObject *parent)
	: Nuria::HttpNode (parent), d_ptr (new RestfulHttpNodePrivate)
{
	
	this->d_ptr->format = format;
}

Nuria::RestfulHttpNode::~RestfulHttpNode () {
	delete this->d_ptr;
}

Nuria::RestfulHttpNode::ReplyFormat Nuria::RestfulHttpNode::replyFormat () const {
	return this->d_ptr->format;
}

void Nuria::RestfulHttpNode::setReplyFormat (Nuria::RestfulHttpNode::ReplyFormat format) {
	this->d_ptr->format = format;
}

QByteArray Nuria::RestfulHttpNode::convertVariantToData (const QVariant &variant) {
	QVariant r;
	
	// Static mappings
	switch (variant.userType ()) {
	case QMetaType::QByteArray:
		return variant.toByteArray ();
	case QMetaType::Int:
		return QByteArray::number (variant.toInt ());
	} 
	
#ifdef NURIA_USING_QT5
	// 1. Try to convert variant to a QJsonDocument
	r = Variant::convert< QJsonDocument > (variant);
	
	if (r.isValid ()) {
		return r.value< QJsonDocument > ().toJson ();
	}
#endif
	
	// 2. Try to convert variant to a QByteArray
	r = Variant::convert< QByteArray > (variant);
	return r.toByteArray ();
	
}

void Nuria::RestfulHttpNode::conversionFailure (const QVariant &variant, Nuria::HttpClient *client) {
	nError() << "Failed to convert type" << variant.typeName ()
		 << "for URL" << client->path ().toString ();
	
	client->killConnection (500);
}

QByteArray Nuria::RestfulHttpNode::processResultData (QVariant result, Nuria::HttpClient *client) {
	static const QByteArray itemSep (",");
	static const QByteArray listStart ("[");
	static const QByteArray listEnd ("]");
	
	static const QByteArray mapStart ("{");
	static const QByteArray mapNameLeft ("\"");
	static const QByteArray mapNameRight ("\":");
	static const QByteArray mapEnd ("}");
	
	// 
	Nuria::Variant::Iterator it = Nuria::Variant::begin (result);
	Nuria::Variant::Iterator end = Nuria::Variant::end (result);
	
	// 
	bool isList = Nuria::Variant::isList (result);
	bool isMap = Nuria::Variant::isMap (result);
	
	// 
	QByteArray data;
	
	// 
	if (isList) data.append (listStart);
	else if (isMap) data.append (mapStart);
	
	// 
	for (; it != end; ++it) {
		if (isMap) {
			QString keyName = it.key ().toString ()
					  .replace (QLatin1Char ('"'), QLatin1String ("\\\""));
			data.append (mapNameLeft);
			data.append (keyName);
			data.append (mapNameRight);
		}
		
		// 
		QVariant body = it.value ();
		QByteArray itemData;
		
		// Handle QVariantList and QVariantMap
		if (body.userType () == QMetaType::QVariantList ||
		    body.userType () == QMetaType::QVariantMap) {
			itemData = processResultData (body, client);
		} else {
			itemData = convertVariantToData (body);
		}
		
		if (itemData.isEmpty ()) {
			if (!client->responseHeaderSent ()) {
				conversionFailure (*it, client);
			}
			
			return QByteArray ();
		}
		
		data.append (itemData);
		data.append (itemSep);
		
	}
	
	// 
	data.chop (1);
	
	// 
	if (isList) data.append (listEnd);
	else if (isMap) data.append (mapEnd);
	
	// 
	return data;
}

static QVariantList parseClientArguments (const SlotData &data, Nuria::HttpClient *client, bool &error) {
	QList< int > types = data.callback.argumentTypes ();
	bool lastIsClient = types.isEmpty ()
			    ? false
			    : (types.last () == qMetaTypeId< Nuria::HttpClient * > ());
	
	// 
	QVariantList arguments;
	
#ifdef NURIA_USING_QT5
	QUrlQuery query (client->path ());
#else
	QUrl query (client->path ());
#endif
	
	int len = types.length () - lastIsClient;
	for (int i = 0; i < len; i++) {
		int type = types.at (i);
		const QString &name = data.argNames.at (i);
		
		// 
		if (!query.hasQueryItem (name)) {
			nError() << "No value for argument" << name
				 << "in request" << client->path ().toString ();
			error = true;
			break;
		}
		
		// 
		QString rawValue = query.queryItemValue (name);
		
		// Convert!
		QVariant value = Nuria::Variant::convert (rawValue, type);
		
		// Sanity check
		if (!value.isValid ()) {
			nError() << "bad value" << rawValue << "for argument" << name
				 << "of type" << QMetaType::typeName (type)
				 << "in request" << client->path ().toString ();
			error = true;
			break;
		}
		
		// Store, next
		arguments.append (value);
		
	}
	
	// 
	if (lastIsClient) {
		arguments.append (QVariant::fromValue (client));
	}
	
	// 
	return arguments;
}

static QVariant invokeMetaMethod (const SlotData &data, Nuria::HttpClient *client) {
	bool error = false;
        QVariantList arguments = parseClientArguments (data, client, error);
	nDebug() << "Invoking method" << data.signature << arguments << "fail" << error;
	
	if (error) {
		client->killConnection (400);
		return QVariant ();
	}
	
	// Invoke!
	Nuria::Callback cb = data.callback;
	QVariant result = cb.invoke (arguments);
	
	return result;
}

void Nuria::RestfulHttpNodePrivate::invokeQtSlot (Nuria::RestfulHttpNode *p,
						  const SlotData &slot,
						  HttpClient *client) {
	QVariant result = invokeMetaMethod (slot, client);
	
	if (!result.isValid ()) {
		return;
	}
	
	// 
	QByteArray reply = p->processResultData (result, client);
	
	if (reply.isEmpty ()) {
		return;
	}
	
	// 
//	client->setResponseHeader (HttpClient::HeaderContentType
	client->setContentLength (reply.length ());
	client->write (reply);
	
}


bool Nuria::RestfulHttpNode::callSlotByName (const QString &name, Nuria::HttpClient *client) {
	
	// 
	if (!this->d_ptr->loaded) {
		this->d_ptr->loaded = true;
		registerActionSlots ();
	}
	
	// Do we have a Qt slot for this?
	auto it = this->d_ptr->methods.constFind (name);
	if (it != this->d_ptr->methods.constEnd ()) {
		this->d_ptr->invokeQtSlot (this, *it, client);
		return true;
	}
	
	// 
	return HttpNode::callSlotByName (name, client);
}

void Nuria::RestfulHttpNode::registerActionSlots () {
	
	const QMetaObject *meta = static_cast< QObject * > (this)->metaObject ();
	
	int i = RestfulHttpNode::staticMetaObject.methodCount ();
	
	for (; i < meta->methodCount (); i++) {
		QMetaMethod m = meta->method (i);
		
		// We only want public slots.
		if (m.access () != QMetaMethod::Public ||
		    m.methodType () != QMetaMethod::Slot) {
			continue;
		}
		
		// 
		QByteArray signature ("1");
		
#ifdef NURIA_USING_QT5
		signature.append (m.methodSignature ());
		QByteArray methodName = m.name ();
#else
		signature.append (m.signature ());
		QByteArray methodName (m.signature ());
		methodName.chop (methodName.indexOf ('(') - methodName.length ());
#endif
		
		// Skip all which don't end in "Action".
		if (!methodName.endsWith ("Action")) {
			continue;
		}
		
		// 
		SlotData data;
		data.signature = signature;
		
		foreach (const QByteArray &name, m.parameterNames ()) {
			data.argNames.append (name);
		}
		
		data.callback.setCallback (this, data.signature.constData ());
		
		// Store
		methodName.chop (6); // Remove trailing "Action"
		
		this->d_ptr->methods.insert (QString::fromLatin1 (methodName), data);
		
	}
	
}
