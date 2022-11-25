#pragma once
#include <Windows.h>
#include <mutex>
#include <deque>
#include <string>

struct InviteCodeData_t {
	InviteCodeData_t( std::string _code ) {
		this->code = _code;
		this->good = false;
		this->guild = "";
		this->guild_name = "";
		this->online_member_count = 0;
		this->member_count = 0;
	}

	InviteCodeData_t( std::string _code , bool _good , std::string _expires_at , std::string _guild , std::string _guild_name , int _online_member_count , int _member_count ) {
		this->code = _code;
		this->good = _good;
		this->expires_at = _expires_at;
		this->guild = _guild;
		this->guild_name = _guild_name;
		this->online_member_count = _online_member_count;
		this->member_count = _member_count;
	}

	std::string code;
	bool good;
	std::string expires_at;
	std::string guild;
	std::string guild_name;
	int online_member_count;
	int member_count;

	bool operator==( InviteCodeData_t b ) {
		return this->code == b.code || ( !this->guild.empty( ) && this->guild == b.guild );
	}

	bool operator!=( InviteCodeData_t b ) {
		return this->guild != b.guild || ( this->guild.empty( ) && this->code != b.code );
	}
};

inline std::mutex g_InviteCheckerMutex;
struct InviteCheckerData_t {
	std::atomic< int > bad_count = 0;
	std::atomic< int > good_count = 0;
	std::atomic< int > left_to_check = 0;
	std::deque< InviteCodeData_t > remainings;
	std::deque< InviteCodeData_t > checked;
	std::deque< std::string > blacklist_guilds;
} inline g_InviteCheckerData;

class CInviteChecker
{
private:
	std::atomic< bool > bInitialized = false;
	std::deque< HANDLE > vecThreads = { };
	bool bWroteFinishedCodes = false;

	bool ParseInviteCodes( );
	bool ParseBlacklistGuilds( );
public:
	bool Init( );
	void Run( );
	void Shutdown( );

	std::atomic< bool > bFinished = false;
};

inline CInviteChecker g_InviteChecker { };