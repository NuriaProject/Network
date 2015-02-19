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

#include "streamingjsonhelper.hpp"

enum { ObjectElement = '{', ArrayElement = '[' };

Nuria::Internal::StreamingJsonHelper::StreamingJsonHelper () {
	
}

Nuria::Internal::StreamingJsonHelper::~StreamingJsonHelper () {
	
}

Nuria::Internal::StreamingJsonHelper::Status Nuria::Internal::StreamingJsonHelper::appendData (QByteArray data) {
	int i = this->m_skipFirst;
	this->m_skipFirst = 0;
	
	// Iterate over 'data'
	const char *raw = data.constData ();
	int length = data.length ();
	for (; i < length; i++) {
		char c = raw[i];
		
		if (c == '\\') { // Escape character
			if (i + 1 >= length) { // Continue in next chunk?
				this->m_skipFirst++;
			}
			
			// NOTE: On my CPU (AMD Phenom II X6 1090T) the following line makes this function 100x slower!
			// Wish this was a joke. The dissassembly shows that 'i++' only adds a ADDL instruction, but
			// for some reason this wreaks havoc with this functions performance. Weird.
			i++; // Skip next character
		} else if (c == '"') { // String begin/end
			this->m_inString = !this->m_inString;
		} else if (!this->m_inString) {
			Status status = Premature;
			
			if (c == '{' || c == '[') { // Count object and array openings/closings
				pushStack (c);
			} else if (c == '}' || c == ']') {
				status = popStack (c);
			}
			
			// 
			if (status == JsonError) {
				return status;
			} else if (status == ElementComplete) {
				appendElement (raw, i + 1);
				raw += i + 1;
				length -= i + 1;
				i = -1; // Because of 'i++'
			}
			
		}
		
	}
	
	// Store remaining data
	if (length > 0) {
		// Corner case: JSON value (not array or object) as root
		if (this->m_depthStack.empty () && !this->m_inString) {
			appendElement (raw, length);
		}
		
		this->m_buffer.append (raw, length);
	}
	
	// Done
	return this->m_elements.isEmpty () ? Premature : ElementComplete;
}

bool Nuria::Internal::StreamingJsonHelper::hasWaitingElement () const {
	return !this->m_elements.isEmpty ();
}

QByteArray Nuria::Internal::StreamingJsonHelper::nextWaitingElement () {
	return this->m_elements.takeFirst ();
}

inline void Nuria::Internal::StreamingJsonHelper::pushStack (char c) {
	this->m_depthStack.push_back (c);
}

inline Nuria::Internal::StreamingJsonHelper::Status Nuria::Internal::StreamingJsonHelper::popStack (char c) {
	if (this->m_depthStack.empty ()) {
		return JsonError;
	}
	
	// 
	uint8_t expected = (c == ']') ? ArrayElement : ObjectElement;
	uint8_t last = this->m_depthStack.back ();
	this->m_depthStack.pop_back ();
	
	if (last != expected) {
		return JsonError;
	}
	
	// Done
	return (this->m_depthStack.empty ()) ? ElementComplete : Premature;
}

void Nuria::Internal::StreamingJsonHelper::appendElement (const char *data, int end) {
	QByteArray left (data, end);
	
	// Push complete element
	this->m_elements.append (this->m_buffer + left);
	this->m_buffer.clear ();
}
