#pragma once
#include <Windows.h>
#include <mutex>
#include <deque>
#include <string>

struct TokenData_t {
	TokenData_t( ) {
		token = "";
		userId = "";
		userName = "";
		emailVerified = false;
		email = "";
		phone = "";
		premium_type = 0;
		bot = false;
		twoFactorAuthEnabled = false;
		language = "";
		guilds = { };
		successParsingGuilds = false;
	}

	std::string token = "";
	std::string userId = "";
	std::string userName = "";
	bool emailVerified = false;
	std::string email = "";
	std::string phone = "";
	int premium_type = 0;
	bool bot = false;
	bool twoFactorAuthEnabled = false;
	std::string language = "";
	std::deque< std::string > guilds = { };
	bool successParsingGuilds = false;
};

inline std::mutex g_TokenCheckerMutex;
struct TokenCheckerData_t {
	std::atomic< int > left_to_check = 0;
	std::deque< std::string > remainings;
	std::deque< std::string > invalid;
	std::deque< std::string > requires_verify;
	std::deque< TokenData_t > valid;
} inline g_TokenCheckerData;

class CTokenChecker
{
private:
	std::atomic< bool > bInitialized = false;
	std::deque< HANDLE > vecThreads = { };
	bool bWroteData = false;

	bool ParseTokens( );
	void WriteValid( );
	void WriteValidSimple( );
	void WriteInvalid( );
	void WriteRequiresVerify( );
public:
	bool Init( );
	void Run( );
	void Shutdown( );

	std::atomic< bool > bFinished = false;
};

inline CTokenChecker g_TokenChecker { };
