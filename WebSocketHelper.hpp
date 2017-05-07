#pragma once
#include <string>

namespace WebSocketHelper
{
    static std::string mask(const std::string& text, unsigned char opCode = 0x1)
    {
        int length = text.size();
        std::string header;
        header += 0x80 | opCode & 0x0f;

        if (length <= 125)
        {
            header += static_cast<unsigned char>(length);
        }
        else if (length > 125 && length < 65536)
        {
            header += static_cast<unsigned char>(126);
            header += static_cast<unsigned short>(length);
        }
        else if (length >= 65536)
        {
            header += static_cast<unsigned char>(127);
            header += static_cast<unsigned long>(length);
        }

        return header + text;
    }

    static std::string unmask(const std::string& mask, const std::string& data)
    {
        std::string text;

        for (int i = 0; i < data.size(); i++)
        {
            text += data[i] ^ mask[i % 4];
        }

        return text;
    }
}