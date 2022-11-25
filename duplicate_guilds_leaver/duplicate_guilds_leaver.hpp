#pragma once
#include <Windows.h>
#include <mutex>
#include <deque>
#include <string>

inline std::mutex g_DuplicateGuildsLeaverMutex;
struct DuplicateGuildsLeaverData_t {
	DuplicateGuildsLeaverData_t( std::string _token , std::deque< std::string > _guilds = { } ) {
		this->token = _token;
		this->guilds = _guilds;
	}

	std::string token;
	std::deque< std::string > guilds; // guilds (ids) to leave

	bool operator==( const DuplicateGuildsLeaverData_t& b ) {
		return this->token == b.token && !this->token.empty( );
	}

	bool operator!=( const DuplicateGuildsLeaverData_t& b ) {
		return this->token != b.token || this->token.empty( );
	}
};

inline std::deque< DuplicateGuildsLeaverData_t > g_DuplicateGuildsLeaverDatas = { };

class CDuplicateGuildsLeaver
{
private:
	std::atomic< bool > bInitialized = false;
	std::deque< HANDLE > vecThreads = { };

public:
	bool Init( );
	void Run( );
	void Shutdown( );

	std::deque< std::string > vecBlacklistGuilds = { };
	std::atomic< bool > bFinished = false;
	std::atomic< int > left_to_leave = 0;
};

inline CDuplicateGuildsLeaver g_DuplicateGuildsLeaver { };