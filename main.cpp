#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wsock32.lib")

#include <iostream>
#include <sstream>

#include "Sockets.hpp"
#include "HttpHelper.hpp"
#include "Sha1.hpp"
#include "Base64Helper.hpp"
#include "WebSocket.hpp"

void server();
std::string mask(std::string text);
std::string unmask(std::string packet);

int main(int argc, char** argv)
{
	server();

	std::cout << "Finished" << std::endl;
	std::cin.get();

	return 0;
}

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
				std::cout << data << std::endl;
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
					client->send(mask("\x03KEYMESSAGE"));
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

std::string mask(std::string text)
{
	int length = text.size();

	std::string header;
	if (length <= 125)
	{
		header += 0x81;
		header += static_cast<unsigned char>(length);
	}
	else if (length > 125 && length < 65536)
	{
		header += 0x81;
		header += static_cast<unsigned char>(126);
		header += static_cast<unsigned short>(length);
	}
	else if (length >= 65536)
	{
		header += 0x81;
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
