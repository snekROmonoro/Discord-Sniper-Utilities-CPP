#include <curl/curl.h>
#include <iostream>
#include <string>

#include "nlohmann/json.hpp"

#include "config/config.hpp"
#include "http/http.hpp"
#include "proxy/proxy.hpp"

#include "utils.hpp"

#include "duplicate_guilds_checker/duplicate_guilds_checker.hpp"
#include "duplicate_guilds_leaver/duplicate_guilds_leaver.hpp"
#include "invite_checker/invite_checker.hpp"
#include "invite_scraper/invite_scraper.hpp"
#include "token_checker/token_checker.hpp"

// https://www.asawicki.info/news_1465_handling_ctrlc_in_windows_console_application
// to terminate on CTRL+C or more shortcuts

static BOOL WINAPI console_ctrl_handler( DWORD dwCtrlType )
{
	switch ( dwCtrlType )
	{
	case CTRL_C_EVENT: // Ctrl+C
	case CTRL_BREAK_EVENT: // Ctrl+Break
	case CTRL_CLOSE_EVENT: // Closing the console window
	case CTRL_LOGOFF_EVENT: // User logs off. Passed only to services!
	case CTRL_SHUTDOWN_EVENT: // System is shutting down. Passed only to services!
		g_exit = true;
		return TRUE;
		break;
	}

	// Return TRUE if handled this message, further handler functions won't be called.
	// Return FALSE to pass this message to further handlers until default handler calls ExitProcess().
	return FALSE;
}

int main( )
{
	SetConsoleTitleA( "Welcome to discord sniper helper" );
	SetConsoleCtrlHandler( console_ctrl_handler , TRUE );

	if ( !g_Config.Init( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to read/parse config.toml. please re-check config.toml and restart app\n" );
		while ( !g_exit ) { }

		return 0;
	}

	if ( !g_ProxyHelper.Init( ) ) {
		printf( ANSI_COLOR_YELLOW "[warn]" ANSI_COLOR_RESET " no proxies\n" );
	}

	if ( g_exit ) { return 0; }

	g_ProxyHelper.RunScraperAsync( );

	int runtime_mode = -1;

	printf( ANSI_COLOR_GREEN "[select]" ANSI_COLOR_RESET " 1 = invite checker\n" );
	printf( ANSI_COLOR_GREEN "[select]" ANSI_COLOR_RESET " 2 = check tokens\n" );
	printf( ANSI_COLOR_GREEN "[select]" ANSI_COLOR_RESET " 3 = get duplicated guilds\n" );
	printf( ANSI_COLOR_GREEN "[select]" ANSI_COLOR_RESET " 4 = leave duplicated guilds\n" );
	printf( ANSI_COLOR_GREEN "[select]" ANSI_COLOR_RESET " 5 = invite scraper\n" );
	std::cin >> runtime_mode;

	if ( !( runtime_mode >= 1 && runtime_mode <= 5 ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " wrong selection\n" );
		while ( !g_exit ) { }

		g_ProxyHelper.Quit( );
		return 0;
	}

	switch ( runtime_mode )
	{
	case 1: {
		if ( !g_InviteChecker.Init( ) ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to initialize invite checker\n" );
			while ( !g_exit ) { }

			g_ProxyHelper.Quit( );
			return 0;
		}

	} break;
	case 2: {
		if ( !g_TokenChecker.Init( ) ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to initialize token checker\n" );
			while ( !g_exit ) { }

			g_ProxyHelper.Quit( );
			return 0;
		}

	} break;
	case 3: {
		if ( !g_DuplicateGuildsChecker.Init( ) ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to initialize duplicate guilds checker\n" );
			while ( !g_exit ) { }

			g_ProxyHelper.Quit( );
			return 0;
		}

	} break;
	case 4: {
		if ( !g_DuplicateGuildsLeaver.Init( ) ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to initialize duplicate guilds leaver\n" );
			while ( !g_exit ) { }

			g_ProxyHelper.Quit( );
			return 0;
		}

	} break;
	case 5: {
		if ( !g_InviteScraper.Init( ) ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to initialize invite scraper\n" );
			while ( !g_exit ) { }

			g_ProxyHelper.Quit( );
			return 0;
		}

	} break;
	}

	curl_global_init( CURL_GLOBAL_DEFAULT );

	switch ( runtime_mode )
	{
	case 1: {
		g_InviteChecker.Run( );
	} break;
	case 2: {
		g_TokenChecker.Run( );
	} break;
	case 3: {
		g_DuplicateGuildsChecker.Run( );
	} break;
	case 4: {
		g_DuplicateGuildsLeaver.Run( );
	} break;
	case 5: {
		g_InviteScraper.Run( );
	} break;
	}

	//auto output = g_HttpRequests.Request( "https://discord.com/api/v9/invites/iancu?with_counts=true&with_expiration=true" , "GET" , "" , g_ProxyHelper.RollProxyData( ) );
	/*g_HttpRequests.AppendHeader( "Authorization: token" );
	auto output = g_HttpRequests.Request( "https://discord.com/api/v9/users/@me" , "GET" , "" , g_ProxyHelper.RollProxyData( ) );
	std::cout << output.response;*/

	while ( !g_exit ) { }

	curl_global_cleanup( );

	switch ( runtime_mode )
	{
	case 1: {
		g_InviteChecker.Shutdown( );
	} break;
	case 2: {
		g_TokenChecker.Shutdown( );
	} break;
	case 3: {
		g_DuplicateGuildsChecker.Shutdown( );
	} break;
	case 4: {
		g_DuplicateGuildsLeaver.Shutdown( );
	} break;
	case 5: {
		g_InviteScraper.Shutdown( );
	} break;
	}

	g_ProxyHelper.Quit( );

	return 0;
}