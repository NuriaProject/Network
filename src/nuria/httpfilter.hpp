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

#ifndef NURIA_HTTPFILTER_HPP
#define NURIA_HTTPFILTER_HPP

#include "network_global.hpp"
#include "httpclient.hpp"
#include <QObject>

namespace Nuria {

class HttpClient;

/**
 * \brief A filter can modify a HttpClients outgoing stream
 * 
 * Filters allow you to modify the data a HttpClient sends back. This can be
 * used to implement compression algorithms or on-the-fly "minifiers".
 * 
 * \warning Filters are expected to be thread-safe.
 */
class NURIA_NETWORK_EXPORT HttpFilter : public QObject {
	Q_OBJECT
public:
	
	/** Constructor. */
	explicit HttpFilter (QObject *parent = 0);
	
	/** Destructor. */
	~HttpFilter () override;
	
	/**
	 * Returns the name of the filter. If the filter returns a non-empty
	 * QByteArray, it will only be triggered if the client mentions it
	 * in the "Accept-Encoding" HTTP header.
	 * 
	 * Also, if a non-empty QByteArray is returned, it will automatically
	 * be added to the "Content-Encoding" header upon sending.
	 * 
	 * \note The correct header would be "Transfer-Encoding" for HTTP/1.1
	 * requests, but the world doesn't care so we use "Content-Encoding" to
	 * be compliant with everyone else.
	 * 
	 * The default implementation returns an empty QByteArray.
	 */
	virtual QByteArray filterName () const;
	
	/**
	 * Called before the HTTP header is sent by \a client. The
	 * implementation can change \a headers at will. Returning \c false
	 * indicates an error and will instruct \a client to not send anything.
	 * 
	 * \note To add this filter to the "Transfer-Encoding" header
	 * automatically, see filterName().
	 * 
	 * The default implementation returns \c true.
	 */
	virtual bool filterHeaders (HttpClient *client, HttpClient::HeaderMap &headers);
	
	/**
	 * Called before the response body is sent. Implementations can return a
	 * QByteArray which, if not empty, will be sent to \a client. The
	 * default implementation returns an empty QByteArray.
	 */
	virtual QByteArray filterBegin (HttpClient *client);
	
	/**
	 * Called before \a data is sent to the client. Implementations can
	 * modify \a data at will or even empty it. Returning \c false indicates
	 * an error and will instruct \a client to not send anything.
	 * 
	 * The default implementation returns \c true.
	 */
	virtual bool filterData (HttpClient *client, QByteArray &data);
	
	/**
	 * Called after the response body has been sent. Implementations can
	 * return a QByteArray which will be sent to the remote \a client.
	 * The default implementation returns an empty QByteArray.
	 */
	virtual QByteArray filterEnd (HttpClient *client);
	
};

}

#endif // NURIA_HTTPFILTER_HPP
