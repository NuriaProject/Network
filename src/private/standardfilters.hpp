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
