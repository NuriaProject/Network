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

#include "standardfilters.hpp"

#include <QCoreApplication>
#include <QAtomicPointer>
#include <QtEndian>

namespace {
// Please see the files for information on their license.
#include "adler32.h"
#include "crc32.h"
}

namespace Nuria {
namespace Internal {

struct CompressFooter {
        quint32 hashsum = 0;
        quint32 uncompressedSize = 0;
} __attribute__((packed));

}
}

Q_DECLARE_METATYPE(Nuria::Internal::CompressFooter)

template< typename T >
static T *threadSafeGlobal () {
	static QAtomicPointer< T > container;
	T *inst = container.load ();
	
	if (!inst) {
		inst = new T (qApp);
		if (!container.testAndSetRelaxed (nullptr, inst)) {
			delete inst;
			return container.load ();
		}
		
	}
	
	return inst;
}

static inline QByteArray compress (const QByteArray &data) {
	// qCompress() prepends the stream length.
	// Also get rid of the DEFLATE header and footer.
	QByteArray result = qCompress (data).mid (6);
	result.chop (4);
	return result;
}

Nuria::HttpFilter *Nuria::Internal::DeflateFilter::instance () {
	return threadSafeGlobal< DeflateFilter > ();
}

Nuria::HttpFilter *Nuria::Internal::GzipFilter::instance () {
	return threadSafeGlobal< GzipFilter > ();
}

Nuria::Internal::DeflateFilter::DeflateFilter (QObject *parent)
        : HttpFilter (parent)
{
	
}

QByteArray Nuria::Internal::DeflateFilter::filterName () const {
        return QByteArrayLiteral("deflate");
}

QByteArray Nuria::Internal::DeflateFilter::filterBegin (HttpClient *client) {
	static const quint8 header[] = { // RFC 1950
	        0x78 /* cmf */, 0x9C /* flg */
	};
	
	CompressFooter footer;
	footer.hashsum = 1;
	client->setProperty ("_nuria_deflate", QVariant::fromValue (footer));
	return QByteArray::fromRawData (reinterpret_cast< const char * > (header), sizeof(header));
}

bool Nuria::Internal::DeflateFilter::filterData (HttpClient *client, QByteArray &data) {
	// According to RFC 1951
	
	// Update footer
	CompressFooter footer = client->property ("_nuria_deflate").value< CompressFooter > ();
	footer.hashsum = adler32 (footer.hashsum, data.constData (), data.length ());
	footer.uncompressedSize += data.length ();
	client->setProperty ("_nuria_deflate", QVariant::fromValue (footer));
	
	// Compress data.
	data = compress (data);
	return true;
}

QByteArray Nuria::Internal::DeflateFilter::filterEnd (HttpClient *client) {
	CompressFooter footer = client->property ("_nuria_deflate").value< CompressFooter > ();
	
	QByteArray result (4, Qt::Uninitialized);
	int *hash = reinterpret_cast< int * > (result.data ());
	*hash = qToBigEndian (footer.hashsum);
	
	return result;
}

Nuria::Internal::GzipFilter::GzipFilter (QObject *parent)
        : HttpFilter (parent)
{
	
}

QByteArray Nuria::Internal::GzipFilter::filterName () const {
	return QByteArrayLiteral("gzip");
}

QByteArray Nuria::Internal::GzipFilter::filterBegin (HttpClient *) {
	static const quint8 member[] = {
	        0x1F /* id1 */, 0x8B /* id2 */, 0x08 /* cm */, 0x0, /* flg */
	        0x0, 0x0, 0x0, 0x0 /* mtime */, 0x0 /* xfl */, 0xFF /* os */
	};
	
	return QByteArray::fromRawData (reinterpret_cast< const char * > (member), sizeof(member));
}

bool Nuria::Internal::GzipFilter::filterData (HttpClient *client, QByteArray &data) {
	// According to RFC 1952
	
	// Update footer
	CompressFooter footer = client->property ("_nuria_gzip").value< CompressFooter > ();
	footer.hashsum = crc32 (footer.hashsum, data.constData (), data.length ());
	footer.uncompressedSize += data.length ();
	client->setProperty ("_nuria_gzip", QVariant::fromValue (footer));
	
	// Compress data.
	data = compress (data);
	return true;
}

QByteArray Nuria::Internal::GzipFilter::filterEnd (HttpClient *client) {
	CompressFooter footer = client->property ("_nuria_gzip").value< CompressFooter > ();
	QByteArray result (reinterpret_cast< char * > (&footer), sizeof(footer));
	return result;
}
