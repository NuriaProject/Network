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

#ifndef NURIA_TRANSPORTPRIVATE_HPP
#define NURIA_TRANSPORTPRIVATE_HPP

#include "../nuria/abstracttransport.hpp"

class QTimer;

namespace Nuria {

// Private data structure of Nuria::AbstractTransport
class AbstractTransportPrivate {
public:
	
	~AbstractTransportPrivate () {
		// 
	}
	
	QTimer *timeoutTimer = nullptr;
	
	int requestCount = 0;
	
	AbstractTransport::Timeout currentTimeoutMode = AbstractTransport::Disabled;
	
	int timeoutConnect = AbstractTransport::DefaultConnectTimeout;
	int timeoutData = AbstractTransport::DefaultDataTimeout;
	int timeoutKeepAlive = AbstractTransport::DefaultKeepAliveTimeout;
	
	int minBytesReceived = AbstractTransport::DefaultMinimumBytesReceived;
	
	uint64_t trafficReceivedLast = 0;
	uint64_t trafficReceived = 0;
	uint64_t trafficSent = 0;
	
};

}

#endif // NURIA_TRANSPORTPRIVATE_HPP
