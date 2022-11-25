#include "invite_scraper.hpp"
#include "../http/http.hpp"
#include "../proxy/proxy.hpp"
#include "../utils.hpp"
#include "../nlohmann/json.hpp"
#include "../config/config.hpp"
#include <fstream>
#include <regex>
#include <array>

// use keywords when possible
std::array< std::string , 7 > vecKeywords = {
	"" , // none
	"nitro" ,
	"gw" ,
	"giveaway" ,
	"nitro-drop" ,
	"drop" ,
	"community"
};

void ScrapeGuildsFromReddit( )
{
	auto output = g_HttpRequests.RequestEx( "https://www.reddit.com/domain/discord.gg/new.json" , "GET" , "" , { "User-Agent: reddit discord scraper" } , g_ProxyHelper.RollProxyData( ) );
	if ( !output.success ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed http request to reddit, error: %s\n" , output.error.data( ) );
		return;
	}

	bool success_parse = true;
	nlohmann::json j;
	try {
		j = nlohmann::json::parse( output.response );
	}
	catch ( ... ) {
		success_parse = false;
	}

	if ( !success_parse ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to parse data from reddit\n" );
		return;
	}

	for ( auto& _url : j [ "data" ][ "children" ] ) {
		if ( !_url [ "data" ][ "url" ].is_string( ) ) {
			continue;
		}

		std::lock_guard< std::mutex > l( g_InviteScraperMutex );

		std::string url = _url [ "data" ][ "url" ].get< std::string >( );

		if ( std::find( g_InviteScraperData.invites.begin( ) , g_InviteScraperData.invites.end( ) , url ) != g_InviteScraperData.invites.end( ) )
			continue;

		if ( url.find( "discord.gg/" ) != std::string::npos ) {
			printf( ANSI_COLOR_GREEN "[reddit]" ANSI_COLOR_RESET " %s\n" , url.data( ) );

			g_InviteScraperData.invites.push_back( url );
		}
	}
}

// shitcode..
template< typename T , int N >
struct atomic_array {
	std::array< T , N > arr;
	std::mutex m;

	T increase( int pos , T val ) {
		std::lock_guard< std::mutex > l( m );
		arr [ pos ] += val;
		return arr [ pos ];
	}

	T decrease( int pos , T val ) {
		std::lock_guard< std::mutex > l( m );
		arr [ pos ] -= val;
		return arr [ pos ];
	}

	T get( int pos ) {
		std::lock_guard< std::mutex > l( m );
		return arr [ pos ];
	}
};

atomic_array< int , vecKeywords.size( ) > discordServersPagePerKeyword = { 0 , 0 , 0 , 0 , 0 };
/*std::atomic< int >*/ int discordServersGuildsPerPage = 50; // no need to be atomic, we only read this variable
std::atomic< int > discordServersCurrentKeyword = 0;

void ScrapeGuildsFromDiscordServers( ) {
	int current_keyword = ( discordServersCurrentKeyword++ ) % vecKeywords.size( );
	int page = discordServersPagePerKeyword.get( current_keyword );
	int guilds_per_page = discordServersGuildsPerPage;
	discordServersPagePerKeyword.increase( current_keyword , guilds_per_page );
	std::string keyword = vecKeywords [ current_keyword ];

	std::string url = FormatString( "https://search.discordservers.com/?term=&size=%d&from=%d&keyword=%s" , guilds_per_page , page , keyword.data( ) );

	auto output = g_HttpRequests.RequestEx( url , "GET" , "" , { "User-Agent: discord scraper" } , g_ProxyHelper.RollProxyData( ) );
	if ( !output.success ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed http request to discordservers, error: %s\n" , output.error.data( ) );
		return;
	}

	bool success_parse = true;
	nlohmann::json j;
	try {
		j = nlohmann::json::parse( output.response );
	}
	catch ( ... ) {
		success_parse = false;
	}

	if ( !success_parse ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to parse data from discordservers\n" );
		return;
	}

	if ( j [ "results" ].is_null( ) ) {
		return;
	}

	for ( auto& _invite : j [ "results" ] ) {
		if ( !_invite [ "customInvite" ].is_string( ) ) {
			continue;
		}

		std::string invite = _invite [ "customInvite" ].get< std::string >( );
		if ( invite.empty( ) ) {
			continue;
		}

		size_t space_in_code_pos = invite.find_first_of( ' ' );
		if ( space_in_code_pos != std::string::npos ) {
			invite = invite.substr( 0 , space_in_code_pos );
		}

		if ( invite.empty( ) )
			continue;

		std::string invite_url = invite;
		if ( !invite_url.starts_with( "https://discord.gg/" ) ) {
			if ( invite_url.find( "https://" ) != std::string::npos ) {
				continue;
			}

			size_t slash_in_code_pos = invite.find_first_of( '/' );
			if ( slash_in_code_pos != std::string::npos ) {
				invite = invite.substr( 0 , slash_in_code_pos );
			}

			size_t backslash_in_code_pos = invite.find_first_of( '\\' );
			if ( backslash_in_code_pos != std::string::npos ) {
				invite = invite.substr( 0 , backslash_in_code_pos );
			}

			invite_url = "https://discord.gg/" + invite;
		}

		std::lock_guard< std::mutex > l( g_InviteScraperMutex );
		if ( std::find( g_InviteScraperData.invites.begin( ) , g_InviteScraperData.invites.end( ) , invite_url ) != g_InviteScraperData.invites.end( ) )
			continue;

		printf( ANSI_COLOR_GREEN "[discordservers]" ANSI_COLOR_RESET " %s\n" , invite_url.data( ) );

		g_InviteScraperData.invites.push_back( invite_url );
	}
}

void InviteScraperThread( )
{
	while ( !g_exit && !g_InviteScraper.bFinished )
	{
		ScrapeGuildsFromReddit( );
		std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
		ScrapeGuildsFromDiscordServers( );
		std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
	}
}

bool CInviteScraper::Init( )
{
	return ( this->bInitialized = true );
}

void CInviteScraper::Run( )
{
	if ( !this->bInitialized ) {
		return;
	}

	int max_threads = std::thread::hardware_concurrency( );
	if ( g_Config.threads > 0 ) {
		max_threads = g_Config.threads;
	}

	for ( int i = 0; i < max_threads; i++ ) {
		if ( g_exit ) {
			break;
		}

		this->vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) InviteScraperThread , NULL , 0 , NULL ) );
	}

	while ( !this->bFinished ) { if ( g_exit ) break; }

	if ( this->bWroteScrapedInvites ) {
		return;
	}

	this->bWroteScrapedInvites = true;

	g_InviteScraperMutex.lock( );
	g_InviteScraperMutex.unlock( );

	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " starting to write invites to scraped_invites.txt\n" );

	std::ofstream file( "scraped_invites.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open scraped_invites.txt to write scraped invites\n" );
		return;
	}

	std::deque< std::string > already_written_invites = { };
	g_InviteScraperMutex.lock( );
	for ( auto& invite : g_InviteScraperData.invites )
	{
		if ( std::find( already_written_invites.begin( ) , already_written_invites.end( ) , invite ) != already_written_invites.end( ) )
			continue;

		file << FormatString( "%s\n" , invite.data( ) );

		already_written_invites.push_back( invite );
	}
	g_InviteScraperMutex.unlock( );

	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote %d invite codes to scraped_invites.txt\n" , already_written_invites.size( ) );
}

void CInviteScraper::Shutdown( )
{
	if ( !this->bInitialized ) {
		return;
	}

	g_InviteScraperMutex.lock( );
	g_InviteScraperMutex.unlock( );

	for ( auto& thread : this->vecThreads ) {
		CloseHandle( thread );
	}

	this->bInitialized = false;
}
