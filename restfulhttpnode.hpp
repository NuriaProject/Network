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

#ifndef NURIA_RESTFULHTTPNODE_HPP
#define NURIA_RESTFULHTTPNODE_HPP

#include "httpnode.hpp"

namespace Nuria {

class RestfulHttpNodePrivate;

/**
 * \brief The RestfulHttpNode class makes it easy to write RESTful APIs.
 * 
 * This class was designed to be as flexible as possible when it comes
 * to convenience from a remote API standpoint. You use this just like a
 * HttpNode, with the exception that slots are allowed to return something.
 * 
 * \par Usage of this class
 * As already noted you may use the methods known from HttpNode to register
 * slots. This has the advantage of a known flow, but has the big disadvantage
 * that you still need to convert the input arguments, which are in this case
 * expected to be transmitted as GET arguments. To overcome this, you can
 * also sub-class this class. All public slots whose names end in "Action"
 * will be considered to be invokable RESTful methods.
 * 
 * For this to work, the argument names of the slot have to match the GET query
 * names in the request. Additionally, all used types need to be convertable
 * from QString. If not all arguments are specified in the request, the call
 * will fail with a 400 Bad Request reply. The slot may expect a variable of
 * type \a Nuria::HttpClient* as last argument. If this is the case, then the
 * client will be passed to the slot, else there's no way for the slot to know
 * which client called it.
 * 
 * \sa Nuria::Variant::registerConversion
 * 
 * \note When the index of a node is requested a slot called \c indexAction
 * is tried to invoke. If it doesn't exist, the HttpNode mechanism is tried.
 * 
 * \par Sending replies to the client
 * You have many options zo send a reply to a client. First, you can send it
 * directly, which is the flow used in HttpNode. This is useful if you're
 * sending e.g. binary data. In this case the slot returns \c void.
 * 
 * Second, the slot may return something. This something is then converted
 * to a JSON or XML reply. The following return types are supported out of
 * the box:
 * 
 * - int, bool
 * - QByteArray (No conversion is done!)
 * - QVariantMap and QVariantList
 * 
 * Custom datatypes can be registered too using Nuria::Variant. When a custom
 * type is encountered, the implementation will try to convert it to a JSON or
 * XML reply using the following steps:
 * 
 * 1. Convert it to a QJsonDocument (Only on Qt5) for JSON.
 * 2. Convert it to a QByteArray.
 * 
 * If you don't want to (Or can't) use Nuria::Variant for the conversion, you
 * can also re-implement convertVariantToData, which is called internally, to
 * do the processing.
 * If conversion fails conversionFailure is called which usually replies with
 * a error code of 500.
 * 
 * It's also possible to return a type which is iteratable through
 * Nuria::Variant. In this case convertVariantToData is called for every
 * value in the result set.
 * 
 * \note For this to work you need to register both the type itself \b and the
 * container! See Nuria::Variant::registerIterators
 * 
 * \sa convertVariantToData conversionFailure
 *
 */
class NURIA_NETWORK_EXPORT RestfulHttpNode : public HttpNode {
	Q_OBJECT
	Q_ENUMS(ReplyFormat)
public:
	
	/**
	 * Describes the available reply formats. Defaults to JSON.
	 */
	enum ReplyFormat {
		Custom = 0,
		JSON,
//		XML
	};
	
	explicit RestfulHttpNode (ReplyFormat format, const QString &resourceName, HttpNode *parent = 0);
	explicit RestfulHttpNode (const QString &resourceName, HttpNode *parent = 0);
	explicit RestfulHttpNode (ReplyFormat format = JSON, QObject *parent = 0);
	~RestfulHttpNode ();
	
	/** Returns the reply format. Defaults to ReplyFormat::JSON. */
	ReplyFormat replyFormat () const;
	
	/** Sets the to-be-used reply format. */
	void setReplyFormat (ReplyFormat format);
	
signals:
	
public slots:
	
protected:
	
	/**
	 * Takes \a variant and tries to convert it to a \c QByteArray.
	 * When re-implementing this method call the default implementation if
	 * you don't handle this type:
	 * \codeline return Nuria::HttpNode::convertVariantToData (variant);
	 * 
	 * If a empty QByteArray is returned the conversion has failed.
	 */
	virtual QByteArray convertVariantToData (const QVariant &variant);
	
	/**
	 * Called if the conversion for \a variant failed. When this happens
	 * the reply is pretty much lost to the client. The default
	 * implementation loggs this as a error message and sends a 500 reply
	 * code to \a client.
	 */
	virtual void conversionFailure (const QVariant &variant, HttpClient *client);
	
	/**
	 * Internal helper method, converts \a result to a QByteArray. Also
	 * handles the case when \a result is a iteratable type.
	 */
	QByteArray processResultData (QVariant result, HttpClient *client);
	
	// 
	bool callSlotByName (const QString &name, HttpClient *client);
	
private:
	friend class RestfulHttpNodePrivate;
	
	/**
	 * Registers all slots which are defined in a sub-class. Slots of
	 * its superclasses are \b not registered.
	 */
	void registerActionSlots ();
	
	RestfulHttpNodePrivate *d_ptr;
	
};

}

#endif // NURIA_RESTFULHTTPNODE_HPP
