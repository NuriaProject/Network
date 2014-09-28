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

#include "nuria/httpnode.hpp"
#include <QMimeDatabase>
#include <QDir>

#include "private/httpprivate.hpp"
#include "nuria/httpserver.hpp"
#include <nuria/debug.hpp>

// Note: HttpNodePrivate and SlotInfoPrivate are defined in private/httpprivate.hpp!

Nuria::SlotInfo::SlotInfo (const Callback &callback)
	: d (new SlotInfoPrivate)
{
	
	this->d->callback = callback;
	
}

Nuria::SlotInfo::SlotInfo (const Nuria::SlotInfo &other)
	: d (other.d)
{
	
}

Nuria::SlotInfo &Nuria::SlotInfo::operator= (const Nuria::SlotInfo &other) {
	this->d = other.d;
	return *this;
}

Nuria::SlotInfo::~SlotInfo () {
	// 
}

bool Nuria::SlotInfo::isValid () const {
	return this->d->callback.isValid ();
}

Nuria::Callback Nuria::SlotInfo::callback () const {
	return this->d->callback;
}

bool Nuria::SlotInfo::streamPostBody () const {
	return this->d->streamPostBody;
}

void Nuria::SlotInfo::setStreamPostBody (bool value) {
	this->d->streamPostBody = value;
}

Nuria::HttpClient::HttpVerbs Nuria::SlotInfo::allowedVerbs () const {
	return this->d->allowedVerbs;
}

void Nuria::SlotInfo::setAllowedVerbs (Nuria::HttpClient::HttpVerbs verbs) {
	this->d->allowedVerbs = verbs;
}

qint64 Nuria::SlotInfo::maxBodyLength () const {
	return this->d->maxBodyLength;
}

void Nuria::SlotInfo::setMaxBodyLength (qint64 length) {
	this->d->maxBodyLength = length;
}

bool Nuria::SlotInfo::forceEncrypted () const {
	return this->d->forceEncrypted;
}

void Nuria::SlotInfo::setForceEncrypted (bool force) {
	this->d->forceEncrypted = force;
}

Nuria::HttpNode::HttpNode (const QString &resourceName, HttpNode *parent)
	: QObject (parent), d_ptr (new HttpNodePrivate)
{
	
	this->d_ptr->parent = 0;
	this->d_ptr->resourceName = resourceName;
	this->d_ptr->resourceMode = NoStaticResources;
	
	// 
	if (parent) {
		parent->addNode (this);
	}
	
}

Nuria::HttpNode::HttpNode (QObject *parent)
	: QObject (parent), d_ptr (new HttpNodePrivate)
{
	
	// 
	this->d_ptr->parent = 0;
	this->d_ptr->resourceMode = NoStaticResources;
	
}

Nuria::HttpNode::~HttpNode () {
	delete this->d_ptr;
	
}

bool Nuria::HttpNode::isChild () const {
	return (this->d_ptr->parent != 0);
}

Nuria::HttpNode *Nuria::HttpNode::parentNode () const {
	return this->d_ptr->parent;
}

Nuria::SlotInfo Nuria::HttpNode::connectSlot (const QString &name, const Nuria::Callback &callback) {
	// Sanity check
	if (name.isEmpty () || !callback.isValid () || this->d_ptr->mySlots.contains (name)) {
		return SlotInfo ();
	}
	
	// Create binding
	SlotInfo info (callback);
	this->d_ptr->mySlots.insert (name, info);
	
	return info;
}

Nuria::SlotInfo Nuria::HttpNode::connectSlot (const QString &name, QObject *receiver, const char *slot) {
	
	// Sanity check
	if (name.isEmpty () || !receiver || !slot || this->d_ptr->mySlots.contains (name)) {
		return SlotInfo ();
	}
	
	// Check for slot
	const QMetaObject *meta = receiver->metaObject ();
	int index = meta->indexOfMethod (slot + 1);
	if (index == -1) {
		return SlotInfo ();
	}
	
	// Create binding
	SlotInfo info (Callback (receiver, slot));
	this->d_ptr->mySlots.insert (name, info);
	
	return info;
}

bool Nuria::HttpNode::disconnectSlot (const QString &name) {
	
	// Find slot info
	auto it = this->d_ptr->mySlots.find (name);
	
	// Break if not found
	if (it == this->d_ptr->mySlots.end ())
		return false;
	
	// Free internal data and erase the entry
	this->d_ptr->mySlots.erase (it);
	
	return true;
	
}

bool Nuria::HttpNode::addNode (HttpNode *node) {
	
	// Already a subnode?
	if (this->d_ptr->nodes.contains (node)) {
		return true;
	}
	
	// Resource name of node. If empty, return false
	const QString &res = node->resourceName ();
	
	if (res.isEmpty ())
		return false;
	
	// Do we already have a node or slot with this name?
	if (hasNode (res) || hasSlot (res))
		return false;
	
	// Add as subnode
	this->d_ptr->nodes.append (node);
	
	// Reparent node
	node->setParent (this);
	node->d_ptr->parent = this;
	
	return true;
}

const QString &Nuria::HttpNode::resourceName () const {
	return this->d_ptr->resourceName;
}

const QDir &Nuria::HttpNode::staticResourceDir () const {
	return this->d_ptr->resourceDir;
}

Nuria::HttpNode::StaticResourcesMode Nuria::HttpNode::staticResourceMode () const {
	return this->d_ptr->resourceMode;
}

bool Nuria::HttpNode::setResourceName (const QString &name) {
	
	// Sanity check
	if (name.isEmpty ())
		return false;
	
	// Check if parent is happy with the new name
	if (this->d_ptr->parent) {
		
		// name already a node or slot?
		if (this->d_ptr->parent->hasNode (name) ||
		    this->d_ptr->parent->hasSlot (name))
			return false;
		
	}
	
	// 
	this->d_ptr->resourceName = name;
	return true;
}

void Nuria::HttpNode::setStaticResourceDir (QDir path) {
	
	if (this->d_ptr->resourceMode == NoStaticResources) {
		this->d_ptr->resourceMode = UseStaticResources;
	}
	
	this->d_ptr->resourceDir = path;
	
}

void Nuria::HttpNode::setStaticResourceMode (Nuria::HttpNode::StaticResourcesMode mode) {
	this->d_ptr->resourceMode = mode;
}

bool Nuria::HttpNode::hasNode (const QString &name) {
	
	for (int i = 0; i < this->d_ptr->nodes.length (); i++) {
		if (this->d_ptr->nodes.at (i)->d_ptr->resourceName == name)
			return true;
	}
	
	return false;
}

bool Nuria::HttpNode::hasSlot (const QString &name) {
	return this->d_ptr->mySlots.contains (name);
}

Nuria::HttpNode *Nuria::HttpNode::findNode (const QString &name) const {
	
	// Iterate over subnodes. Return it if the resource name matches
	// If nothing is found, return 0.
	for (int i = 0; i < this->d_ptr->nodes.length (); i++) {
		if (this->d_ptr->nodes.at (i)->resourceName () == name) {
			return this->d_ptr->nodes.at (i);
		}
		
	}
	
	return 0;
}

bool Nuria::HttpNode::invokePath (const QString &path, const QStringList &parts,
				  int index, Nuria::HttpClient *client) {
	
	// 
	if (!allowAccessToClient (path, parts, index, client)) {
		client->killConnection (403);
		return false;
	}
	
	// 
	static const QString indexSlot (QLatin1String ("index"));
	
	// Client requested page index.
	if (parts.isEmpty () || index >= parts.length ()) {
		return callSlotByName (indexSlot, client) || sendStaticResource (QStringList(), 0, client);
	}
	
	const QString &cur = parts.at (index);
	
	// Try to find child node
	HttpNode *child = findNode (cur);
	
	// Not found?
	if (!child) {
		
		// If no child with this name can be found, it must be a slot,
		// but only if index points to the last item of parts.
		// Try to invoke.
		if (index == parts.length () - 1 && callSlotByName (cur, client)) {
			return true;
		}
		
		// Still nothing. Try to find a static resource.
		return sendStaticResource (parts, index, client);
		
	}
	
	// Matching child-node found. Call invokePath() on child
	return child->invokePath (path, parts, index + 1, client);
	
}

bool Nuria::HttpNode::allowAccessToClient (const QString &path, const QStringList &parts,
					   int index, Nuria::HttpClient *client) {
	Q_UNUSED(path)
	Q_UNUSED(parts)
	Q_UNUSED(index)
	Q_UNUSED(client)
	return true;
}

bool Nuria::HttpNode::callSlotByName (const QString &name, HttpClient *client) {
	
	// Find slot
	auto it = this->d_ptr->mySlots.constFind (name);
	
	// Not found?
	if (it == this->d_ptr->mySlots.constEnd ()) {
		return false;
	}
	
	// Host object invalid?
	const SlotInfo &info = *it;
	if (!info.d->callback.isValid ()) {
		return false;
	}
	
	// Illegal request method?
	if (!(info.d->allowedVerbs & client->verb ())) {
		
		// Kick client with 405
		client->killConnection (405);
		return false;
		
	}
	
	// Is encryption enforced?
	if (info.forceEncrypted () && !client->isConnectionSecure ()) {
		client->httpServer ()->redirectClientToUseEncryption (client);
		return false;
	}
	
	// Apply slot info
	client->d_ptr->slotInfo = info;
	
	// Call. If we're in non-streaming mode and the request
	// method was POST or PUT we delay the call until the
	// body has been completely received.
	if (!info.streamPostBody () &&
	    (client->verb () & (HttpClient::POST | HttpClient::PUT))) {
		
//		nDebug() << "Delaying call to" << info->slotName ();
		
		return true;
	}
	
	return callSlot (info, client);
}

bool Nuria::HttpNode::sendStaticResource (const QString &name, Nuria::HttpClient *client) {
	return sendStaticResource (name.split (QLatin1Char ('/')), 0, client);
}

static void setMimeTypeHeaderForStaticResource (Nuria::HttpClient *client, const QString &name, QIODevice *device) {
	if (client->hasResponseHeader (Nuria::HttpClient::HeaderContentType)) {
		return;
	}
	
	// Set Content-Type
	QMimeDatabase database;
	QMimeType mimeType = database.mimeTypeForFileNameAndData (name, device);
	client->setResponseHeader (Nuria::HttpClient::HeaderContentType, mimeType.name ().toLatin1 ());
	
}

bool Nuria::HttpNode::sendStaticResource (const QStringList &path, int indexInPath, HttpClient *client) {
	qint64 maxLen = -1; // For range requests
	
	// No static resource directory?
	if (this->d_ptr->resourceMode == NoStaticResources) {
		return false;
	}
	
	// Deliver 'index.html' if a empty name was requested
	QString file;
	
	if (path.isEmpty ()) {
		file = QLatin1String ("index.html");
	} else {
		
		// Make sure there are no '..' or '.' elements in path.
		if (path.contains (QLatin1String ("..")) || path.contains (QLatin1String ("."))) {
			return false;
		}
		
		// Make sure that path has a length of 1 if no nested resources
		// are allowed
		if (this->d_ptr->resourceMode == UseStaticResources &&
		    (path.length () - indexInPath) > 1) {
			return false;
		}
		
		// Construct path
		if (indexInPath == 0) {
			file = path.join (QLatin1String ("/"));
		} else {
			QStringList part = path.mid (indexInPath);
			file = part.join (QLatin1String ("/"));
		}
		
	}
	
	// Pipe the file if it exists
	QFile *handle = new QFile (this->d_ptr->resourceDir.filePath (file));
	
	// Open file
	if (!handle->open (QIODevice::ReadOnly)) {
		delete handle;
		return false;
	}
	
	// Determine mime type
	setMimeTypeHeaderForStaticResource (client, file, handle);
	
	// Is the client only interested in a specific range?
	if (client->rangeStart () != -1) {
		qint64 start = client->rangeStart ();
		qint64 end = client->rangeEnd ();
		
		// Failed, respond with 416 - Requested Range Not Satisfiable
		if (!handle->seek (start)) {
			client->killConnection (416);
			return false;
		}
		
		maxLen = end - start;
		
	}
	
	// Check if the client used a GET request. If not, respond with a 405.
	if (client->verb () == HttpClient::GET) {
		client->pipeToClient (handle, maxLen);
		return true;
	}
	
	// Failed.
	delete handle;
	client->killConnection (405); // Method Not Allowed
	
	return false;
}

bool Nuria::HttpNode::callSlot (const SlotInfo &info, HttpClient *client) {
	if (!info.isValid ()) {
		nError() << "Can't invoke a slot without a valid SlotInfo instance.";
		return false;
	}
	
	Callback cb = info.callback ();
	cb (client);
	
	return cb.isValid ();
}
