#pragma once
#include <iostream>

class HttpHelper
{
    static bool parseHeaderField(std::string strField, std::pair<std::string,std::string>* field){
        size_t pos = strField.find(":");
        if(pos==std::string::npos || pos==0)
            return false;
        field->first = strField.substr(0, pos);
        size_t p1 = strField.find_first_not_of(" \t\r\n", pos+1),
               p2 = strField.find_last_not_of(" \t\r\n");
        if(p1 == std::string::npos || p2 == std::string::npos || p1>p2)
            field->second = "";
        else
            field->second = strField.substr(p1, p2-p1+1);
        return true;
    }

public:
    static std::map<std::string,std::string> parseHeader(std::string fullHeader){
        std::map<std::string,std::string> headers;
        size_t pos = 0,
               last = 0;
        std::pair<std::string,std::string> field;
        do{
            pos = fullHeader.find("\r\n", last);
            if(parseHeaderField(fullHeader.substr(last, pos-last), &field)){
                headers.insert(field);
            }
            last = pos+2;
        }while(pos != std::string::npos);
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
				while(value[0] == ' ' || value[0] == '\t' || value[0] == '\r' || value[0] == '\n')
				{
					value = value.substr(1);
				}
				return value;
			}

			int newPos = allHeaders.find_first_of("\r\n");
			if (newPos != std::string::npos)
			allHeaders = allHeaders.substr(newPos + 2);
		}
		while (allHeaders.find("\r\n") != std::string::npos);

		return "";
	}
};
