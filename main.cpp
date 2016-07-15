#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

#include <iostream>
#include <thread>
#include "Sockets.hpp"
#include "HttpHelper.hpp"
#include "Sha1.hpp"
#include "Base64Helper.hpp"
#include <sstream>

//void client();
void server();

int main(int argc, char** argv)
{
	/*std::thread tServer(server);
	tServer.join();*/

	server();
	
	std::cout << "Finished" << std::endl;
	std::cin.get();

	return 0;
}

/*void client()
{
	TCPClient* client = new TCPClient();
	client->connect("127.0.0.1", 66453);
	client->setBlocking(true);

	client->send("Hola");

	while (true)
	{
		string buffer = client->recv();
		std::cout << "Client >> " << buffer << std::endl;
	}

	delete client;
}*/

void server()
{
	TCPRawServer* server = new TCPRawServer();
	if (!server->start(80))
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
		client->connect(client1.sock, client1.ip, 80);
		std::cout << "Client connected!" << std::endl;

		bool a = false;
		while (client->isConnected()) 
		{
			std::string buffer = client->recv();
			if (a && buffer.size() > 1)
			{
				std::cout << buffer << std::endl;
			}
			if (!a) {
				a = true;
				std::string websocketKey = HttpHelper::getHeaderValue(buffer, "Sec-WebSocket-Key");
				std::cout << "Handshake Key: " << websocketKey << std::endl;
				std::string sha1Key = sha1UnsignedChar(websocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
				std::string finalKey = Base64Helper::encode(sha1Key.c_str());

				client->send("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: chat\r\nSec-WebSocket-Accept: " + finalKey + "\r\n\r\n");
				std::cout << "HandShake done" << std::endl;
			}
		}
	}

	delete server;
}