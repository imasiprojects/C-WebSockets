#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

#include <iostream>
#include <sstream>

#include "Sockets.hpp"
#include "WebSocketServer.hpp"


class CustomConnection : public WebSocketConnection{
    static int nextId;
    int id;

public:

    CustomConnection(WebSocketServer* server, Connection conn)
    :WebSocketConnection(server, conn), id(0){}

    int getId() const{
        return id;
    }

    void setId(){
        if(id<=0)
            id = nextId++;
    }

};
int CustomConnection::nextId = 1;


void startServer()
{
    WebSocketServer server;

    server.setNewClientCallback([](WebSocketServer* server, WebSocketConnection* connection){
        CustomConnection* conn = (CustomConnection*)connection;
        conn->setId();
        std::cout << "New client entered with IP: " << connection->getIp() << ", ID: " << conn->getId() << std::endl;
		connection->send("saludo", "hola");
    });

    server.setClosedClientCallback([](WebSocketServer* server, WebSocketConnection* connection){
        CustomConnection* conn = (CustomConnection*)connection;
        std::cout << "Client with ID: " << conn->getId() << " disconnected." << std::endl;
    });

	server.setUnknownMessageCallback([](WebSocketServer* server, WebSocketConnection* connection, std::string key, std::string data)
	{
	    CustomConnection* conn = (CustomConnection*)connection;
		std::cout << "Id: " << conn->getId() << " -> Unknown: [" << key << "] = " << data << std::endl;
	});

	server.setDataCallback("Prueba", [](WebSocketServer* server, WebSocketConnection* connection, std::string key, std::string data)
	{
	    CustomConnection* conn = (CustomConnection*)connection;
		std::cout << "Id: " << conn->getId() << " -> [" << key << "] = " << data << std::endl;
		server->sendPing();
	});

	server.setServeFolder("c-websockets");
	server.setDefaultPage("client.html");

	server.setInstantiator([](WebSocketServer* server, Connection conn)->WebSocketConnection*{
        return new CustomConnection(server, conn);
    });

	if(!server.startAndWait(80)){
		std::cout << "Server couldn't be started" << std::endl;
	}
}

int main(int argc, char** argv)
{
	startServer();

	std::cout << "Finished" << std::endl;
	std::cin.get();

	return 0;
}

/*
void server()
{
	int serverPort = 80;
	TCPRawServer* server = new TCPRawServer();
	if (!server->start(serverPort))
	{
		std::cout << "Error" << std::endl;
		return;
	}
	std::cout << "Server started @ 80" << std::endl;
	server->setBlocking(true);

	while (true)
	{
		Connection client1 = server->newClient();
		TCPClient* client = new TCPClient();
		client->connect(client1.sock, client1.ip, serverPort);
		client->setBlocking(true);
		std::cout << "Client connected!" << std::endl;

		bool handShakeDone = false;
		std::string finBuffer;
		char finOpCode;
		while (client->isConnected())
		{
			std::string buffer;
			if (handShakeDone)
			{
				do
				{
					buffer += (*client).recv(2 - buffer.size());
				}
				while (buffer.size() < 2);

				bool fin = buffer[0] & 0x80;
				char opCode = buffer[0] & 0xF;
				bool hasMask = buffer[1] & 0x80;
				long packetSize = buffer[1] & 127;
				if (packetSize == 126)
				{
					do
					{
						buffer += (*client).recv(4 - buffer.size());
					}
					while (buffer.size() < 4);
					packetSize = *(short*)&buffer[2];
				}
				else if (packetSize == 127)
				{
					do
					{
						buffer += (*client).recv(10 - buffer.size());
					}
					while (buffer.size() < 10);
					packetSize = *(long long*)&buffer[2];
				}

				std::string mask;
				if (hasMask)
				{
					do
					{
						mask += (*client).recv(4 - mask.size());
					}
					while (mask.size() < 4);
				}

				buffer.clear();
				do
				{
					buffer += (*client).recv(packetSize - buffer.size());
				}
				while (buffer.size() < packetSize);

				std::string data = hasMask ? WebSocket::unmask(mask, buffer) : buffer;

				if (fin)
				{
					if (opCode == 0x0)
					{
						data = finBuffer + data;
						finBuffer.clear();
					}

					std::cout << data << std::endl;
				}
				else
				{
					if (opCode != 0x0)
					{
						finOpCode = opCode;
					}
					finBuffer += data;
				}
			}
			else
			{
				do
				{
					buffer += (*client).recv();
				}
				while (buffer.rfind("\r\n\r\n") == std::string::npos);

				std::string websocketKey = HttpHelper::getHeaderValue(buffer, "Sec-WebSocket-Key");
				if (websocketKey.size() > 0)
				{
					std::cout << "Handshake Key: " << websocketKey << std::endl;
					std::string sha1Key = sha1UnsignedChar(websocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
					std::string finalKey = Base64Helper::encode(sha1Key.c_str());

					(*client).send("HTTP/1.1 101 Web Socket Protocol Handshake\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: chat\r\nSec-WebSocket-Accept: " + finalKey + "\r\n\r\n");
					handShakeDone = true;
					buffer.clear();

					std::cout << "HandShake done" << std::endl;
					client->send(WebSocket::mask("\x03KEYMESSAGE"));
				}
				else
				{
					buffer.clear();
					handShakeDone = false;
				}
			}
		}

		delete client;
	}

	delete server;
}
*/
