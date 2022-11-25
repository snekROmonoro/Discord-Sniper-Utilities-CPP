#pragma once
#include <string>
#include <mutex>
#include <time.h>

#include "nlohmann/json.hpp"

inline volatile bool g_exit = false;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

template<typename ... Args>
__forceinline std::string FormatString( const std::string& format , const Args&... args )
{
	char buf [ 260 ];
	sprintf_s( buf , format.data( ) , args... );
	return std::string( buf );
}

// https://stackoverflow.com/questions/42854679/c-convert-given-utc-time-string-to-local-time-zone
__forceinline std::time_t getEpochTime( const std::string& dateTime )
{
	static const std::string dateTimeFormat { "%Y-%m-%dT%H:%M:%SZ" };

	std::istringstream ss { dateTime };
	std::tm dt;
	ss >> std::get_time( &dt , dateTimeFormat.c_str( ) );

	/* Convert the tm structure to time_t value and return Epoch. */
	return _mkgmtime( &dt );
}
