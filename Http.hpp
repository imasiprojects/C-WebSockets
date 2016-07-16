#pragma once
#include <string>

namespace Http
{
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