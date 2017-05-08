#pragma once
#include <map>

namespace HttpHelper
{
    static bool tryParseHeaderField(std::string strField, std::pair<std::string, std::string>* outField)
    {
        size_t keyEndIndex = strField.find(":");

        if (keyEndIndex == std::string::npos || keyEndIndex == 0)
        {
            return false;
        }

        outField->first = strField.substr(0, keyEndIndex);

        size_t valueStartIndex = strField.find_first_not_of(" \t\r\n", keyEndIndex + 1);
        size_t valueEndIndex = strField.find_last_not_of(" \t\r\n");
        size_t valueLength = valueEndIndex - valueStartIndex + 1;

        if (valueStartIndex == std::string::npos || valueEndIndex == std::string::npos || valueStartIndex > valueEndIndex)
        {
            outField->second = "";
        }
        else
        {
            outField->second = strField.substr(valueStartIndex, valueLength);
        }

        return true;
    }

    static std::map<std::string, std::string> parseHeader(std::string fullHeader)
    {
        std::map<std::string, std::string> headers;
        std::pair<std::string, std::string> field;

        size_t fieldStartIndex = 0;
        size_t fieldEndIndex;

        do
        {
            fieldEndIndex = fullHeader.find("\r\n", fieldStartIndex);

            if (tryParseHeaderField(fullHeader.substr(fieldStartIndex, fieldEndIndex - fieldStartIndex), &field))
            {
                headers.insert(field);
            }

            fieldStartIndex = fieldEndIndex + 2;
        }
        while (fieldEndIndex != std::string::npos);

        return headers;
    }

    static std::string getHeaderValue(std::string allHeaders, std::string headerName)
    {
        std::string line;

        do
        {
            line = allHeaders.substr(0, allHeaders.find_first_of("\r\n"));

            if (line.find(headerName + ":") != std::string::npos)
            {
                std::string value = line.substr(headerName.size() + 1);

                while (value[0] == ' ' || value[0] == '\t' || value[0] == '\r' || value[0] == '\n')
                {
                    value = value.substr(1);
                }

                return value;
            }

            int newPos = allHeaders.find_first_of("\r\n");

            if (newPos != std::string::npos)
            {
                allHeaders = allHeaders.substr(newPos + 2);
            }
        }
        while (allHeaders.find("\r\n") != std::string::npos);

        return "";
    }
}
