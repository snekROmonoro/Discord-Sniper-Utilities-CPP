#pragma once
#include <Windows.h>
#include <mutex>
#include <deque>
#include <string>

struct DuplicateGuildsGuildData_t {
	DuplicateGuildsGuildData_t( std::string _guild_id , std::string _guild_name , std::deque< std::string > _tokens = { } ) {
		this->guild_id = _guild_id;
		this->guild_name = _guild_name;
		this->tokens = _tokens;
	}

	std::string guild_id;
	std::string guild_name;
	std::deque< std::string > tokens;

	bool operator==( const DuplicateGuildsGuildData_t& b ) {
		return this->guild_id == b.guild_id && !this->guild_id.empty( );
	}

	bool operator!=( const DuplicateGuildsGuildData_t& b ) {
		return this->guild_id != b.guild_id || this->guild_id.empty( );
	}
};

inline std::mutex g_DuplicateGuildsCheckerMutex;
struct DuplicateGuildsCheckerData_t {
	std::deque< DuplicateGuildsGuildData_t > guilds;
	std::deque< std::string > tokens;
} inline g_DuplicateGuildsCheckerData;

class CDuplicateGuildsChecker
{
private:
	std::atomic< bool > bInitialized = false;
	std::deque< HANDLE > vecThreads = { };
	bool bWroteDuplicatedGuilds = false;

	bool ParseTokens( );
	void WriteAllGuildsToAFile( );
public:
	bool Init( );
	void Run( );
	void Shutdown( );

	// this is made for duplicate guilds leaver in general
	void RunThreadsOnly( );

	std::atomic< bool > bFinished = false;
	std::atomic< int > left_to_check = 0;
};

inline CDuplicateGuildsChecker g_DuplicateGuildsChecker { };
