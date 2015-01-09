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

#ifndef NURIA_STANDARDFILTERS_HPP
#define NURIA_STANDARDFILTERS_HPP

#include "../nuria/httpfilter.hpp"

namespace Nuria {
namespace Internal {

class DeflateFilter : public HttpFilter {
	Q_OBJECT
public:
	
	static HttpFilter *instance ();
	
	explicit DeflateFilter (QObject *parent = 0);
	
	QByteArray filterName () const override;
	QByteArray filterBegin (HttpClient *client) override;
	bool filterData (HttpClient *client, QByteArray &data) override;
	QByteArray filterEnd (HttpClient *client) override;
	
};

class GzipFilter : public HttpFilter {
	Q_OBJECT
public:
	
	static HttpFilter *instance ();
	
	explicit GzipFilter (QObject *parent = 0);
	
	QByteArray filterName () const override;
	QByteArray filterBegin (HttpClient *) override;
	bool filterData (HttpClient *client, QByteArray &data) override;
	QByteArray filterEnd (HttpClient *client) override;
	
};

}
}

#endif // NURIA_STANDARDFILTERS_HPP
