#ifndef INCLUDE_COMMON_LOGGER_HXX_INCLUDED
#define INCLUDE_COMMON_LOGGER_HXX_INCLUDED

#include <iostream>
#include <string_view>

class Logger
{
public:
	Logger(std::string_view app_name)
		: name(app_name)
	{
	}

	void operator()(std::string_view msg, std::string_view type = "INFO")
	{
		std::clog << name << ": " << type << ": " << msg << "\n";
	}

private:
	const std::string name;
};

#endif