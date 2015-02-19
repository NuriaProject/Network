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

#ifndef NURIA_INTERNAL_STREAMINGJSONHELPER_HPP
#define NURIA_INTERNAL_STREAMINGJSONHELPER_HPP

#include <QByteArray>
#include <QVector>

namespace Nuria {
namespace Internal {

/** \internal */
class StreamingJsonHelper {
public:
	
	enum Status {
		ElementComplete = 1,
		Premature = 2,
		JsonError = 3,
	};
	
	StreamingJsonHelper ();
	~StreamingJsonHelper ();
	
	Status appendData (QByteArray data);
	bool hasWaitingElement () const;
	QByteArray nextWaitingElement ();
	
private:
	
	void pushStack (char c);
	Status popStack (char c);
	void appendElement (const char *data, int end);
	
	QByteArray m_buffer;
	QVector< QByteArray > m_elements;
	
	std::vector< uint8_t > m_depthStack;
	bool m_inString = false;
	int m_skipFirst = 0;
	
};

} // namespace Internal
} // namespace Nuria

#endif // NURIA_INTERNAL_STREAMINGJSONHELPER_HPP
