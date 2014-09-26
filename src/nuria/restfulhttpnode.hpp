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

#include "httpclient.hpp"
#include "httpnode.hpp"

/**
 * Annotation to set the RESTful path pattern of a method.
 * \note When a POST/PUT requests is received, the annotated method will be
 * invoked when the whole body has been received.
 */
#define NURIA_RESTFUL(Path) NURIA_ANNOTATE(org.nuriaproject.network.restful, Path)

/**
 * Annotation to only allow certain HttpVerb(s) for the tagged method.
 * When you want to allow multiple verbs simply OR them together.
 * Defaults to \c Nuria::HttpClient::AllVerbs.
 */
#define NURIA_RESTFUL_VERBS(Verbs) NURIA_ANNOTATE(org.nuriaproject.network.restful.verbs, int(Verbs))

namespace Nuria {

namespace Internal { struct RestfulHttpNodeSlotData; }

class RestfulHttpNodePrivate;
class MetaMethod;
class MetaObject;

/**
 * \brief The RestfulHttpNode class makes it easy to write RESTful APIs.
 * 
 * This class was designed to be as flexible as possible when it comes
 * to convenience from a remote API standpoint. You use this just like a
 * HttpNode, with the exception that slots are allowed to return something.
 * 
 * \par Usage of this class
 * To expose your own service in a RESTful manner, all you need to do is to
 * sub-class this class and tag it with NURIA_INTROSPECT as usual for Tria
 * to pick up annotations.
 * 
 * If you don't want to sub-class RestfulHttpNode, you can use the constructor
 * which takes a object pointer and a MetaObject instance.
 * 
 * Regardless which option you choose, you can then annotate methods you want
 * to expose using NURIA_RESTFUL() (and optionally with NURIA_RESTFUL_VERBS()).
 * When using this route all handlers (i.e. NURIA_RESTFUL methods) are
 * registered automatically when the first invocation on the node happens.
 * 
 * \sa setRestfulHandler()
 * 
 * \par Request types
 * By default, a handler is invoked regardless of the HTTP verb used by the
 * client. You can register multiple methods on the same path while
 * distinguishing between the different verbs using NURIA_RESTFUL_VERBS().
 * The same is possible using setRestfulHandler().
 * 
 * \par RESTful arguments
 * The path string may contain names enclosed in curly braces ("{}") to mark
 * variable parts. These names are expected to directly match those in the
 * parameter list of the method. The value extracted from the invoked path
 * must be compatible to the argument type - This means that the value must
 * be convertible from a QString to the type.
 * 
 * If the method has a argument of type 'Nuria::HttpClient*', the HttpClient
 * behind the request will be passed as value for this parameter.
 * 
 * To validate arguments prior calling, use the generic NURIA_REQUIRE()
 * approach.
 * 
 * You can also completely alter this behaviour by reimplementing 
 * 
 * \note When using Tria, suitable conversion facilities are already in place
 * if your type offers conversion methods of some kind.
 * 
 * \sa Variant::registerConversion
 * 
 * \par Sending replies to the client
 * To send a response, you can choose to either directly write() something
 * to the client (See previous paragraph on how to obtain the HttpClient
 * instance), or your method can return the result. The result type must be
 * convertible to QByteArray or QString. If a QVariantMap or QVariantList is
 * returned, then it will be serialized to JSON formatted data and sent as
 * response.
 * 
 * It's also possible to return a custom type if Nuria::Serializer is able
 * to serialize it to a QVariantMap.
 * 
 * You can change the behaviour of this by reimplementing
 * convertVariantToData(). If you want to customize the behaviour when
 * conversion fails, reimplement conversionFailure().
 * 
 * \sa convertVariantToData conversionFailure
 *
 */
class NURIA_NETWORK_EXPORT RestfulHttpNode : public HttpNode {
	Q_OBJECT
public:
	
	/**
	 * Constructor.
	 * The node will operate on \a object of type \a metaObject. Ownership
	 * of \a object is \b not transferred. If you want to do this, you could
	 * e.g. connect to the destroyed() signal of this instance to
	 * deleteLater() of \a object, if \a object is a QObject. Another option
	 * would be connecting a lambda which destroys \a object to destroyed().
	 */
	explicit RestfulHttpNode (void *object, MetaObject *metaObject, const QString &resourceName,
				  HttpNode *parent = 0);
	
	/** Constructor. */
	explicit RestfulHttpNode (const QString &resourceName = QString (), HttpNode *parent = 0);
	
	/** Destructor. */
	~RestfulHttpNode ();
	
	/**
	 * Manually registers a RESTful handler which will be invoked when
	 * \a path is requested with a HTTP verb in \a verbs. In this case,
	 * arguments are read from the requested path as defined in \a path and
	 * are passed to \a callback in the order defined by \a argumentNames.
	 * 
	 * When dealing with POST/PUT bodies, the default is to wait for the
	 * whole body to be received and then invoke the handler. This can be
	 * overriden by passing \c false for \a waitForRequestPostBody.
	 * 
	 * \note When \a callback expects a argument of type
	 * 'Nuria::HttpClient*', the name of this argument is expected to be
	 * \b omitted in \a argumentNames!
	 */
	void setRestfulHandler (HttpClient::HttpVerbs verbs, const QString &path,
				const QStringList &argumentNames, const Callback &callback,
	                        bool waitForRequestPostBody = true);
	
	/**
	 * \overload
	 * Does the same as the other setRestfulHandler(), but assumes a value
	 * of HttpClient::AllVerbs for \a verbs.
	 */
	void setRestfulHandler (const QString &path, const QStringList &argumentNames,
				const Callback &callback, bool waitForRequestPostBody = true);
	
protected:
	
	/**
	 * Takes \a variant and tries to convert it to a \c QByteArray.
	 * When re-implementing this method call the default implementation if
	 * you don't handle this type:
	 * \codeline return HttpNode::convertVariantToData (variant);
	 * 
	 * If a empty QByteArray is returned the conversion has failed.
	 */
	virtual QByteArray convertVariantToData (const QVariant &variant);
	
	/**
	 * Takes \a argumentData and returns a QVariant of type \a targetType.
	 * Returns a invalid QVariant on failure. The behaviour of the default
	 * implementation is outlined in the class documentation above.
	 */
	virtual QVariant convertArgumentToVariant (const QString &argumentData, int targetType);
	
	/**
	 * Called if the conversion for \a variant failed. When this happens
	 * the reply is pretty much lost to the client. The default
	 * implementation loggs this as a error message and sends a 500 reply
	 * code to \a client.
	 */
	virtual void conversionFailure (const QVariant &variant, HttpClient *client);
	
	/**
	 * Converts \a result to a QByteArray. A empty QByteArray is treated as
	 * error.
	 */
	QByteArray generateResultData (QVariant result, HttpClient *client);
	
	/** \reimp Does the invocation. */
	bool invokePath (const QString &path, const QStringList &parts,
			 int index, HttpClient *client) override;
	
private:
	friend class RestfulHttpNodePrivate;
	
	void registerAnnotatedHandlers ();
	void registerMetaMethod (MetaMethod &method);
	void registerRestfulHandlerFromMethod (const QString &path, MetaMethod &method);
	void delayedRegisterMetaObject ();
	QStringList argumentNamesWithoutClient (MetaMethod &method);
	QString compilePathRegEx (QString path);
	bool invokeMatchLater (Callback callback, const QVariantList &arguments, HttpClient *client);
	bool invokeMatchNow (Callback callback, const QVariantList &arguments, HttpClient *client);
	bool invokeMatch (Internal::RestfulHttpNodeSlotData &slotData,
			  QRegularExpressionMatch &match, HttpClient *client);
	QVariantList argumentValues (const QStringList &names, const QList<int> &types,
				     QRegularExpressionMatch &match, HttpClient *client);
	bool writeResponse (const QVariant &response, HttpClient *client);
	void addJsonContentTypeHeaderToResponse (HttpClient *client);
	
	RestfulHttpNodePrivate *d_ptr;
	
};

}

#endif // NURIA_RESTFULHTTPNODE_HPP
