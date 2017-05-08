#pragma once
#include <string>
#include <WinSock2.h>

namespace WebSocketHelper
{
    enum FrameOpCode
    {
        Continuation = 0,
        Text = 1,
        Binary = 2,
        ConnectionClosed = 8,
        Ping = 9,
        Pong = 10,
    };

    static std::string mask(const std::string& text, FrameOpCode opCode = FrameOpCode::Text)
    {
        uint64_t length = text.size();
        std::string header;
        header += 0x80 | opCode & 0x0f;

        if (length <= 125)
        {
            header += static_cast<unsigned char>(length);
        }
        else if (length > 125 && length < 65536)
        {
            uint16_t extraLength = htons(length);

            header += static_cast<unsigned char>(126);
            header.append((char*) &extraLength, 2);
        }
        else if (length >= 65536)
        {
            uint64_t extraLength = htonl(length);

            header += static_cast<unsigned char>(127);
            header.append((char*) &extraLength, 8);
        }

        return header + text;
    }

    static std::string unmask(const std::string& mask, const std::string& data)
    {
        std::string text(data.size(), '\0');

        for (int i = 0; i < data.size(); i++)
        {
            text[i] = data[i] ^ mask[i % 4];
        }

        return text;
    }
}