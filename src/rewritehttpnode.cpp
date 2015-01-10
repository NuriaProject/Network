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

#include "nuria/rewritehttpnode.hpp"

#include <QVector>

struct Replacement {
	int offset;
	int len;
	int idx;
};

struct Rule {
	QRegularExpression rx;
	QString replace;
	QVector< Replacement > refs;
};

namespace Nuria {
class RewriteHttpNodePrivate {
public:
	
	RewriteHttpNode::RewriteBehaviour behaviour = RewriteHttpNode::RewriteSubpath;
	QVector< Rule > rules;
	
};
}

Nuria::RewriteHttpNode::RewriteHttpNode (const QString &resourceName, Nuria::HttpNode *parent)
        : HttpNode (resourceName, parent), d_ptr (new RewriteHttpNodePrivate)
{
	
}

Nuria::RewriteHttpNode::RewriteHttpNode (QObject *parent)
	: HttpNode (parent), d_ptr (new RewriteHttpNodePrivate)
{
	
}

Nuria::RewriteHttpNode::~RewriteHttpNode () {
	delete this->d_ptr;
}

Nuria::RewriteHttpNode::RewriteBehaviour Nuria::RewriteHttpNode::rewriteBehaviour () const {
	return this->d_ptr->behaviour;
}

void Nuria::RewriteHttpNode::setRewriteBehaviour (RewriteBehaviour behaviour) {
	this->d_ptr->behaviour = behaviour;
}

static QVector< Replacement > replaceReferences (const QString &str, int &highestRef) {
	QVector< Replacement > list;
	int idx = 0;
	int length = str.length ();
	
	while ((idx = str.indexOf ('\\', idx) + 1) > 0) {
		int offset = idx - 1;
		int len = 1;
		int ref = 0;
		
		// Get reference number (0 - 99)
		if (length - idx > 0 && str.at (idx).isDigit ()) {
			ref = str.at (idx).toLatin1 () - '0';
			len++;
			
			if (length - idx > 1 && str.at (idx + 1).isDigit ()) {
				ref = (ref * 10) + (str.at (idx + 1).toLatin1 () - '0');
				len++;
			}
			
		}
		
		// Reference found?
		if (len > 1) {
			list.append (Replacement { offset, len, ref });
			highestRef = std::max (highestRef, ref);
		}
		
	}
	
	return list;
}

bool Nuria::RewriteHttpNode::addRewrite (const QRegularExpression &rx, const QString &replace) {
	int highest = 0;
	QVector< Replacement > refs = replaceReferences (replace, highest);
	
	// Sanity check
	if (rx.captureCount () == -1 || highest > rx.captureCount ()) {
		return false;
	}
	
	// Store and done.
	this->d_ptr->rules.append (Rule { rx, replace, refs });
	return true;
}

void Nuria::RewriteHttpNode::clearRules () {
	this->d_ptr->rules.clear ();
}

bool Nuria::RewriteHttpNode::invokePath (const QString &path, const QStringList &parts, int index, HttpClient *client) {
	if (HttpNode::invokePath (path, parts, index, client)) {
		return true;
	}
	
	// See if something was already sent, in which case we're done.
	if (client->responseHeaderSent ()) {
		return false;
	}
	
	// Rewrite and invoke
	if (this->d_ptr->behaviour == RewriteSubpath) {
		return rewriteSubpathAndInvoke (path, parts.length (), index, client);
	}
	
	return rewritePathAndInvoke (path, client);
}

bool Nuria::RewriteHttpNode::rewriteSubpathAndInvoke (const QString &path, int length, int index, HttpClient *client) {
	QString subPath;
	QString parentPath;
	
	// Get path parts
	if (length > 0) {
		parentPath = path.section ('/', 0, index - 1, QString::SectionSkipEmpty);
		subPath = path.section ('/', index, -1, QString::SectionSkipEmpty);
	}
	
	// Find match
	if (!findMatchingRule (subPath)) {
		return false;
	}
	
	// Construct new path
	if (!parentPath.startsWith ('/')) {
		parentPath.prepend ('/');
	}
	
	if (!parentPath.endsWith ('/')) {
		parentPath.append ('/');
	}
	
	// Invoke
	QString newPath = parentPath + subPath;
	QStringList parts = newPath.split ('/', QString::SkipEmptyParts);
	
	return HttpNode::invokePath (newPath, parts, index, client);
}

bool Nuria::RewriteHttpNode::rewritePathAndInvoke (QString path, HttpClient *client) {
	if (!findMatchingRule (path)) {
		return false;
	}
	
	// Invoke
	return client->invokePath (path);
}

static QString replaceMatch (const QRegularExpressionMatch &match, const QString &repl,
                             const QVector< Replacement > &refs) {
	QString result = repl;
	int offset = 0;
	
	// Replace references with the caught groups
	for (int i = 0, count = refs.length (); i < count; i++) {
		Replacement r = refs.at (i);
		QString insert  = match.captured (r.idx);
		
		result.replace (r.offset + offset, r.len, insert);
		offset += insert.length () - r.len;
	}
	
	return result;
}

bool Nuria::RewriteHttpNode::findMatchingRule (QString &path) {
	int i = 0;
	int count = this->d_ptr->rules.length ();
	
	for (; i < count; i++) {
		const Rule &r = this->d_ptr->rules.at (i);
		QRegularExpressionMatch m = r.rx.match (path);
		
		if (m.hasMatch ()) {
			path = replaceMatch (m, r.replace, r.refs);
			return true;
		}
		
	}
	
	return false;
}
