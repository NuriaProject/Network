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
