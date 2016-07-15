#pragma once
#include <string>

namespace WebSocket
{
	static std::string mask(std::string text)
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

	static std::string unmask(std::string packet)
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
};