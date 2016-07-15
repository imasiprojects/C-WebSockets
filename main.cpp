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
std::string mask(std::string text);
std::string unmask(std::string packet);

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
		std::cout << "Client connected!" << std::endl;

		bool handShakeDone = false;
		while (client->isConnected()) 
		{
			std::string buffer = client->recv();
			if (buffer.size() > 0)
			{
				if (handShakeDone)
				{
					std::cout << "Unmasked >> " << unmask(buffer) << std::endl;
					client->send(mask("MASKEEED MESSAGE!"));
				}
				else
				{
					std::string websocketKey = HttpHelper::getHeaderValue(buffer, "Sec-WebSocket-Key");
					std::cout << "Handshake Key: " << websocketKey << std::endl;
					std::string sha1Key = sha1UnsignedChar(websocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
					std::string finalKey = Base64Helper::encode(sha1Key.c_str());

					client->send("HTTP/1.1 101 Web Socket Protocol Handshake\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: chat\r\nSec-WebSocket-Accept: " + finalKey + "\r\n\r\n");
					handShakeDone = true;

					std::cout << "HandShake done" << std::endl;
				}
			}
		}

		delete client;
	}

	delete server;
}

std::string mask(std::string text)
{
	unsigned char magicNumber = 0x80 | 0x1 & 0x0f;
	int length = text.size();

	std::string header;
	if (length <= 125)
	{
		header += magicNumber;
		header += static_cast<unsigned char>(length);
	}
	else if (length > 125 && length < 65536)
	{
		header += magicNumber;
		header += static_cast<unsigned char>(126);
		header += static_cast<unsigned short>(length);
	}
	else if (length >= 65536)
	{
		header += magicNumber;
		header += static_cast<unsigned char>(127);
		header += static_cast<unsigned long>(length);
	}

	return header + text;
}

std::string unmask(std::string packet)
{
	int packetSize = packet[1] & 127;

	std::string mask, data;

	switch (packetSize)
	{
		case 126:
		{
			mask = packet.substr(4, 4);
			data = packet.substr(8);
			break;
		}
		case 127:
		{
			mask = packet.substr(10, 4);
			data = packet.substr(14);
			break;
		}
		default:
		{
			mask = packet.substr(2, 4);
			data = packet.substr(6);
			break;
		}
	}

	std::string text;
	for (int i = 0; i < data.size(); i++)
	{
		text += data[i] ^ mask[i % 4];
	}
	return text;
}