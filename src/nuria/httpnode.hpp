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

#ifndef NURIA_HTTPNODE_HPP
#define NURIA_HTTPNODE_HPP

#include "network_global.hpp"
#include "httpclient.hpp"
#include <nuria/callback.hpp>
#include <QSharedData>
#include <QObject>
#include <QDir>

namespace Nuria {

class HttpNodePrivate;
class SlotInfoPrivate;
class HttpServer;
class HttpNode;

/**
 * Helper class for HttpNode. An instance is returned by HttpNode::connectSlot.
 * You can use this class to fine-tune some settings for each slot.
 */
class NURIA_NETWORK_EXPORT SlotInfo {
public:
	
	/** Constructor. */
	SlotInfo (const Callback &callback = Callback ());
	
	/** Copy constructor. */
	SlotInfo (const SlotInfo &other);
	
	/** Assignment operator. */
	SlotInfo &operator= (const SlotInfo &other);
	
	/** Destructor. */
	~SlotInfo ();
	
	/** Returns \c true if this instance is valid. */
	bool isValid () const;
	
	/** Returns the associated Callback. */
	Callback callback () const;
	
	/** 
	 * Returns \c true if the POST body should be streamed. Defaults to \c false.
	 * Streaming the POST body means that the received data will always be
	 * stored in memory and is useful if you want to process received data
	 * immediatly.
	 * \warning When using streaming mode, but you're not piping the data into
	 * a QProcess, you must close the connection yourself!
	 * \note If you're in non-streaming mode, your slot will be called \b after
	 * the POST body has been transferred.
	 */
	bool streamPostBody () const;
	
	/** \sa streamPostBody */
	void setStreamPostBody (bool value);
	
	/**
	 * Returns a bitmap of allowed verbs for this slot.
	 * By default all HTTP verbs are allowed.
	 */
	HttpClient::HttpVerbs allowedVerbs () const;
	
	/** \sa allowedVerbs */
	void setAllowedVerbs (HttpClient::HttpVerbs verbs);
	
	/**
	 * Returns the highest allowed POST body size.
	 * The default is 4MiB.
	 */
	qint64 maxBodyLength () const;
	
	/** \sa maxBodyLength */
	void setMaxBodyLength (qint64 length);
	
	/**
	 * Returns \c true if this slot can only be invoked using a secure
	 * connection (HTTPS).
	 * Defaults to \c false.
	 */
	bool forceEncrypted () const;
	
	/** \sa forceEncrypted */
	void setForceEncrypted (bool force);
	
private:
	friend class HttpNode;
	
	// 
	QSharedDataPointer< SlotInfoPrivate > d;
	
};

/** Convenience typedef for a QList of HttpNode* */
typedef QList< HttpNode * > HttpNodeList;

/**
 * \brief Virtual directory for HttpServer
 * 
 * A node is the equivalent to a directory of other HTTP servers and provides
 * a hierarchical system to manage resources. They may contain other nodes or
 * slots, all of which are named.
 * 
 * Slots may be invokable Callback's or static files.
 * 
 * \par Usage
 * 
 * To add a node as child node, just call addNode(). This will make the node
 * available to HTTP requests. If you want to invoke a C++ function when a
 * certain path was requested, use connectSlot(), which accepts a
 * Nuria::Callback and will work with any callback of this sort.
 * 
 * \note Slots are only invoked if it's the last element in the requested path.
 * 
 * For example, if you have a slot called "foo", it will \b only be called,
 * if "foo" is at the end of the path, like "/stuff/foo". It won't be invoked
 * if the request path would be "/foo/stuff".
 * 
 * \note It's possible to have a child node and a slot with the same name in a
 * single HttpNode instance.
 * 
 * It's also possible to easily serve static files from the file-system. See
 * setStaticResourceDir() and setStaticResourceMode() for more information.
 * 
 * \par How a path is invoked
 * 
 * Nodes are hierarchical, meaning you can put nodes into other nodes to build
 * paths. When a HTTP request was made and the request header has been parsed,
 * the HttpClient instance will try to "invoke" the path. It does this by
 * calling invokePath() on the HttpServer's root node. The root node will, in
 * the default implementation, look for a matching slot if it's at the last
 * part of the path, or else, try to find a child node with a matching name,
 * which if found will be invoked recursively.
 * 
 * Other HttpNode implementations may change this behaviour significantly by
 * re-implementing invokePath(). RestfulHttpNode is a good example, which allows
 * to write RESTful services easily by parsing the requested path for variables
 * passed to the call.
 * 
 * \par Advanced usage
 * 
 * It can happen that the default HttpNode doesn't satisfy your needs. In this
 * case you can subclass HttpNode and reimplement one of the protected virtual
 * methods.
 * 
 * If all you want to do is session management, re-implement
 * allowAccessToClient(). Want to enhance HttpNode's slot invocation mechanism?
 * Re-implement callSlotByName(). And if you want to have as much control over
 * everything as possible, you can also re-implement invokePath(). Please bear
 * in mind that doing this is more complex as it involves checking for
 * permissions, finding a slot to be called and handing the request over to
 * another HttpNode for processing.
 * 
 * Please note that if you want to use clients in a non-streamed fashion (Which
 * is the default), that you'll need to create a SlotInfo for each method and
 * use HttpClient::setSlotInfo to assign one to any client.
 * 
 * \sa HttpServer invokePath
 */
class NURIA_NETWORK_EXPORT HttpNode : public QObject {
	Q_OBJECT
public:
	
	/**
	 * This enum describes the mode of static resource handling in a node.
	 * The default is \c NoStaticResources.
	 */
	enum StaticResourcesMode {
		
		/** No static resources will be served by this node. */
		NoStaticResources = 0,
		
		/**
		 * This node will serve static resources, but won't go deeper
		 * in the directory hierarchy after the the set resource
		 * directory.
		 */
		UseStaticResources = 1,
		
		/**
		 * This node will server static resources and will follow
		 * the directory hierarchy.
		 */
		UseNestedStaticResources = 2
		
	};
	
	/**
	 * Constructs a node. If \a parent points to a HttpNode, this
	 * node will be added to \a parent. You can check if this was
	 * successful by calling isChild().
	 */
	HttpNode (const QString &resourceName, HttpNode *parent = 0);
	
	/**
	 * Constructs a node.
	 */
	HttpNode (QObject *parent = 0);
	
	/** Destructor. */
	~HttpNode ();
	
	/**
	 * Returns \c true when this node has a valid parent node.
	 */
	bool isChild () const;
	
	/**
	 * Returns the parent node this node is associated to. If this node
	 * has no parent, \c 0 is returned.
	 */
	HttpNode *parentNode () const;
	
	/**
	 * Connects a slot to this resource. The slot is callable by requesting
	 * <ip or domain of server>/<resource path>/<name>.
	 * Returns a SlotInfo instance on success. The call fails if either
	 * \a name is already a registered slot or node or \a callback is
	 * invalid. In this case \c 0 is returned.
	 * 
	 * If you want to deliver static resource files take a look at
	 * setStaticResourceDir.
	 * \sa SlotInfo setStaticResourceDir
	 * 
	 * \note The prototype for a slot is 
	 * <tt>void method (HttpClient *client);</tt>
	 * where \a client points to the connected client.
	 * 
	 * \note The slot 'index' is called when the user attempts to access this
	 * node directly.
	 */
	SlotInfo connectSlot (const QString &name, const Callback &callback);
	
	/**
	 * Connects a slot to this resource. The slot is callable by requesting
	 * <ip or domain of server>/<resource path>/<name>.
	 * Returns a SlotInfo instance on success. The call fails if either
	 * \a name is already a registered slot or node, \a receiver is 0 or
	 * \a slot points to a non-existing slot. In this case \c 0 is returned.
	 * 
	 * If you want to deliver static resource files take a look at
	 * setStaticResourceDir.
	 * \sa SlotInfo setStaticResourceDir
	 * 
	 * \note The prototype for a slot is 
	 * <tt>void method (HttpClient *client);</tt>
	 * where \a client points to the connected client.
	 * 
	 * \note The slot 'index' is called when the user attempts to access
	 * this node directly.
	 */
	SlotInfo connectSlot (const QString &name, QObject *receiver, const char *slot);
	
	/**
	 * Disconnects the slot \a name. If there was a slot with this name
	 * \a true is returned. On failure \a false is returned.
	 */
	bool disconnectSlot (const QString &name);
	
	/**
	 * Adds a subnode to this node. Returns \a true on success. If there is
	 * already a node with the same resource name of \a node, \a node is 0
	 * or \a node doesn't have a resource name the call will fail and return
	 * \a false. The call succeeds if \a node is already a subnode of this
	 * node. On success \a node will be reparented.
	 */
	bool addNode (HttpNode *node);
	
	/**
	 * Returns the resource name of this node.
	 */
	const QString &resourceName () const;
	
	/** 
	 * Returns the static resource file directory of this node.
	 */
	const QDir &staticResourceDir () const;
	
	/**
	 * Returns the current static resource mode.
	 */
	StaticResourcesMode staticResourceMode () const;
	
	/**
	 * Sets the resource name of this node. If this node is a subnode,
	 * the call fails if there is already a node with this name or if
	 * \name is a empty string.
	 */
	bool setResourceName (const QString &name);
	
	/**
	 * Sets the directory which hosts static resources like images.
	 * If a client requests a resource which is not a slot it will be
	 * searched for in \a path.
	 * The default is an invalid path, which points at no directory.
	 * If the current staticResourceMode is \c NoStaticResources it will
	 * be changed to \c UseStaticResources.
	 * \note The index file is called "index.html"
	 */
	void setStaticResourceDir (QDir path);
	
	/**
	 * Sets the static resource mode.
	 *\sa setStaticResourceDir StaticResourcesMode
	 */
	void setStaticResourceMode (StaticResourcesMode mode);
	
	/**
	 * Returns \c true if this node has a subnode called \a name.
	 */
	bool hasNode (const QString &name);
	
	/**
	 * Returns \c true if this node has a slot called \a name.
	 */
	bool hasSlot (const QString &name);
	
	/**
	 * Looks for a node \a name and returns it if found.
	 * Returns \c 0 if there is no subnode with this name.
	 */
	HttpNode *findNode (const QString &name) const;
	
protected:
	
	/**
	 * Recurses into the HttpNode structure to invoke the requested slot.
	 * The default implementation checks for the following:
	 * - First it tries to find a matching node. If it finds one it calls
	 *   invokePath() on it.
	 * - If that fails it tries to call a slot with the name if \a index
	 *   points to the last item in \a parts.
	 * - Last it tries to deliver a static resource file.
	 * - When everything fails it returns \c false.
	 * 
	 * If you need a more flexible approach than the default node/slot
	 * mechanism, subclass HttpNode and reimplement this method. Please note
	 * that if you're looking for a way to have session management, you can
	 * also re-implement allowAccessToClient() which was designed for this
	 * purpose.
	 * 
	 * \note Consider re-implementing callSlotByName() instead, as
	 * implementing invokePath() can be a complex task.
	 * 
	 * \note If \a index is equal to \c parts.length() the user requested
	 * the index page of this node.
	 * 
	 * \warning Special case: If \a index is \c 0 and \a parts is empty, the
	 * client requested the index page of the server
	 * (E.g. http://example.com/).
	 * 
	 * \param path The complete path as requested by the user
	 * \param parts The path split by '/'. Empty parts are ignored.
	 * \param index The current index in \a parts.
	 * \param client The client.
	 * \returns \c true on success, otherwise \c false.
	 * 
	 * \sa callSlotByName addNode connectSlot
	 */
	virtual bool invokePath (const QString &path, const QStringList &parts,
				 int index, HttpClient *client);
	
	/**
	 * This method is used by invokePath() to check if \a client is allowed
	 * to access the resource as specified by \a path or by \a parts and
	 * \a index (As they're used in invokePath()). If access is not granted,
	 * you may use HttpClient::killConnection() to use a custom status code.
	 * If you don't do this and disallow access, then the client will
	 * receive a 403 (Forbidden) reply. 
	 * 
	 * If access is allowed, this method must \b not send data to the
	 * client. This is the job of invokePath().
	 *
	 * The default implementation always returns \c true.
	 * 
	 * \note This method is not here to check if the path actually exists!
	 * Its purpose is to check whether or not the client is allowed to go
	 * through the current HttpNode.
	 * 
	 * \note As this method is called for every HttpNode the request
	 * recurses to, make sure to not do expensive calculations. If you
	 * need to do this, consider caching the result. This can be easily
	 * done by e.g. doing:
	 * \codeline client->setProperty("accessGranted", true);
	 * 
	 * \returns \c true if the access should be allowed, otherwise \c false.
	 */
	virtual bool allowAccessToClient (const QString &path, const QStringList &parts,
					  int index, HttpClient *client);
	
	/**
	 * Used by HttpServer::invokeByPath to invoke a slot. Returns \c true if
	 * the slot has been found. This does \b not indicate a failure while
	 * calling. Only return \c false if the slot \a name has not been found.
	 * 
	 * \note When this method is reached, allowAccessToClient has already
	 * been called.
	 */
	virtual bool callSlotByName (const QString &name, HttpClient *client);
	
	/**
	 * Tries to send a static resource file named \a name.
	 * If no static resource directory is set this method returns \c false
	 * immediately. If a resource file has been found, it will be opened
	 * by QFile and pipe()'d back to the client. In this case \c true is
	 * returned.
	 * \note This method supports range requests.
	 */
	bool sendStaticResource (const QString &name, HttpClient *client);
	
	/** \overload */
	bool sendStaticResource (const QStringList &path, int indexInPath,
				 HttpClient *client);
	
private:
	friend class HttpServer;
	friend class HttpClient;
	
	/**
	 * Calls the slot associated with \a info. Used by callSlotByName
	 * and HttpClient::receivedData to call a slot which has been delayed.
	 */
	static bool callSlot (const SlotInfo &info, HttpClient *client);
	
	// 
	HttpNodePrivate *d_ptr;
	
};
}

#endif // NURIA_HTTPNODE_HPP
