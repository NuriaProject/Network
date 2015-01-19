/* Copyright (c) 2014-2015, The Nuria Project
 * The NuriaProject Framework is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 * 
 * The NuriaProject Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with The NuriaProject Framework.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <QCoreApplication>

#include <nuria/httpclient.hpp>
#include <nuria/httpserver.hpp>
#include <nuria/websocket.hpp>
#include <nuria/httpnode.hpp>
#include <cstdio>

//#define VERBOSE

#ifdef VERBOSE
int connectionCount = 0;
#endif

void handleWebSocket (Nuria::HttpClient *client) {
	using namespace Nuria;
	
	WebSocket *ws = client->acceptWebSocketConnection ();
	if (!ws) { return; }
	
	
#ifdef VERBOSE
	connectionCount++;
	fprintf (stderr, "-> New connection from %s (%i connections)\n",
	         qPrintable(client->peerAddress ().toString ()), connectionCount);
#endif
	
	ws->setUseReadBuffer (false);
	QObject::connect (ws, &WebSocket::frameReceived,
	                  [ws](WebSocket::FrameType t, const QByteArray &data) {
#ifdef VERBOSE
		fprintf (stderr, "  Echoing %i bytes [%s]\n", data.length (), data.mid (0, 10).constData ());
#endif
		ws->sendFrame (t, data);
	});
	
#ifdef VERBOSE
	QObject::connect (ws, &QIODevice::aboutToClose, []() {
		connectionCount--;
		fprintf (stderr, "<- Connection lost (%i connections)\n", connectionCount);
	});
#endif
}

int main (int argc, char *argv[]) {
	QCoreApplication a(argc, argv);
	
	Nuria::HttpServer server;
	if (!server.listen (QHostAddress::LocalHost, 9001)) {
		fprintf (stderr, "Failed to listen on localhost:9001\n");
		return 1;
	}
	
	// 
	server.root ()->connectSlot ("index", handleWebSocket);
	fprintf (stderr, "Now run: $ wstest -m fuzzingclient\n");
	
	return a.exec ();
}
