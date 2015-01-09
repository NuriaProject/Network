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

#ifndef TESTNODE_HPP
#define TESTNODE_HPP

#include <nuria/restfulhttpnode.hpp>

// Structure we're returning.
struct NURIA_INTROSPECT TestStruct {
	QString string;
	int integer;
	bool boolean;
};

class NURIA_INTROSPECT TestNode : public Nuria::RestfulHttpNode {
	Q_OBJECT
public:
	
	// Constructor.
	explicit TestNode ()
		: Nuria::RestfulHttpNode ("api", nullptr)
	{ }
	
	// 
	bool invokeTest (const QString &path, Nuria::HttpClient *client, int index = 0) {
		return invokePath (path, path.split ('/'), index, client);
	}
	
	// Annotation based registration. Responds to HttpVerb::AllVerbs
	NURIA_RESTFUL("annotate/{anInt}/{aBool}/{aString}")
	TestStruct viaAnnotation (QString aString, int anInt, bool aBool, Nuria::HttpClient *client) {
		Q_CHECK_PTR(client);
		QString prop = client->property ("someProperty").toString ();
		qDebug("%s %i %i %s", qPrintable(aString), anInt, aBool, qPrintable(prop));
		TestStruct result;
		result.string = aString;
		result.integer = anInt;
		result.boolean = aBool;
		return result;
	}
	
	// Annotations which only care for a specific HttpVerb.
	// Also note that all URLs overlap with the one from the method above.
	NURIA_RESTFUL("annotate/{anInt}")
	NURIA_RESTFUL_VERBS(Nuria::HttpClient::GET)
	QString getAnnotation (int anInt) { return QString ("GET %1").arg (anInt); }
	
	NURIA_RESTFUL("annotate/{anInt}")
	NURIA_RESTFUL_VERBS(Nuria::HttpClient::HEAD)
	QString headAnnotation (int anInt) { return QString ("HEAD %1").arg (anInt); }
	
	NURIA_RESTFUL("annotate/{anInt}")
	NURIA_RESTFUL_VERBS(Nuria::HttpClient::POST)
	QString postAnnotation (int anInt) { return QString ("POST %1").arg (anInt); }
	
	NURIA_RESTFUL("annotate/{anInt}")
	NURIA_RESTFUL_VERBS(Nuria::HttpClient::PUT)
	QString putAnnotation (int anInt) { return QString ("PUT %1").arg (anInt); }
	
	NURIA_RESTFUL("annotate/{anInt}")
	NURIA_RESTFUL_VERBS(Nuria::HttpClient::DELETE)
	QString deleteAnnotation (int anInt) { return QString ("DELETE %1").arg (anInt); }
	
protected:
	
	// Deny access if the URL ends with 'deny'.
	bool allowAccessToClient (const QString &, const QStringList &parts, int, Nuria::HttpClient *) {
		if (!parts.isEmpty () && parts.last () == "deny") {
			qDebug("Access denied");
			return false;
		}
		
		return true;
	}
	
	void conversionFailure (const QVariant &variant, Nuria::HttpClient *) {
		qDebug("Failed to convert %s", qPrintable(variant.toString ()));
	}
};

#endif // TESTNODE_HPP
