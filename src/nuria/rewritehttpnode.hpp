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

#ifndef NURIA_REWRITEHTTPNODE_HPP
#define NURIA_REWRITEHTTPNODE_HPP

#include <QRegularExpression>
#include "httpnode.hpp"

namespace Nuria {

class RewriteHttpNodePrivate;

/**
 * \brief Utility HttpNode for rewriting URLs
 * 
 * This HttpNode implementation uses regular expressions for URL rewrites.
 * 
 * \par Usage
 * 
 * After creating an instance, or sub-classing RewriteHttpNode, use addRewrite()
 * to add rewrite rules.
 * 
 * RewriteHttpNode supports two modes of operation. The default mode,
 * \c RewriteSubpath, only rewrites the sub-path of the request after the
 * part this node is responsible for.
 * For example, if the client requests "/stuff/rewriteNode/foo/bar", where
 * 'rewriteNode' is the RewriteNode itself, the regular expression will be
 * matched against "foo/bar". If this match is successful, the underlying
 * HttpNode implementation is invoked again with the new path. Note that in
 * this mode, the client URL is \b not changed.
 * 
 * 
 * The second mode is \c RewritePath. The regular expressions get passed the
 * whole requested path. On a successful match, the client URL will be
 * rewritten using HttpClient::invokePath().
 * 
 * When matching, the first working match is used, and no others are tried.
 * Rules are tried in the order they were inserted, meaning, the first one that
 * was inserted is the first one tried.
 * 
 * \par Compatibility
 * 
 * It's possible to use this node as usual HttpNode by adding nodes and slots
 * through HttpNode::addNode() or HttpNode::connectSlot(). Only if no node or
 * slot matches the given path, the rewrite rules are tried.
 * 
 * As the underlying HttpNode is invoked at least once or at most twice for
 * each request, the functions HttpNode::allowAccessToClient() and
 * HttpNode::callSlotByName() are called at least once or at most twice.
 * 
 * The first time the original path will be passed, the second time, if at all,
 * the rewritten path will be passed.
 * 
 * You can sub-class RewriteHttpNode to override these functions.
 */
class NURIA_NETWORK_EXPORT RewriteHttpNode : public HttpNode {
	Q_OBJECT
public:
	
	enum RewriteBehaviour {
		
		/** Rewrite only the sub-path. Default. */
		RewriteSubpath = 0,
		
		/** Rewrite the whole path. */
		RewritePath = 1
	};
	
	/** Constructor. */
	explicit RewriteHttpNode (const QString &resourceName, HttpNode *parent = nullptr);
	
	/** Default constructor. */
	explicit RewriteHttpNode (QObject *parent = nullptr);
	
	/** Destructor. */
	~RewriteHttpNode();
	
	/** Returns the current rewrite behaviour. */
	RewriteBehaviour rewriteBehaviour () const;
	
	/** Sets the rewriter behaviour. */
	void setRewriteBehaviour (RewriteBehaviour behaviour);
	
	/**
	 * Adds \a rx to the rewrite rules. \a replace may contain
	 * back-references to \a rx (See QString::replace()).
	 * 
	 * Returns \c false if \a rx is invalid or if \a replace references a
	 * capture not in \a rx.
	 */
	bool addRewrite (const QRegularExpression &rx, const QString &replace);
	
	/** Removes all added rules. */
	void clearRules ();
	
protected:
	
	bool invokePath (const QString &path, const QStringList &parts, int index, HttpClient *client);
	
private:
	
	bool rewriteSubpathAndInvoke (const QString &path, int length, int index, HttpClient *client);
	bool rewritePathAndInvoke (QString path, HttpClient *client);
	bool findMatchingRule (QString &path);
	
	RewriteHttpNodePrivate *d_ptr;
	
};

}

#endif // NURIA_REWRITEHTTPNODE_HPP
