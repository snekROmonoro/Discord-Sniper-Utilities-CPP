#pragma once
#include <Windows.h>
#include <mutex>
#include <deque>
#include <string>

inline std::mutex g_InviteScraperMutex;
struct InviteScraperData_t {
	std::deque< std::string > invites;
} inline g_InviteScraperData;

class CInviteScraper
{
private:
	std::atomic< bool > bInitialized = false;
	std::deque< HANDLE > vecThreads = { };
	bool bWroteScrapedInvites = false;

public:
	bool Init( );
	void Run( );
	void Shutdown( );

	std::atomic< bool > bFinished = false;
};

inline CInviteScraper g_InviteScraper { };
