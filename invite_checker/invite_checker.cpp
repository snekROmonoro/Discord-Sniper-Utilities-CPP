#include "invite_checker.hpp"
#include "../http/http.hpp"
#include "../proxy/proxy.hpp"
#include "../utils.hpp"
#include "../nlohmann/json.hpp"
#include "../config/config.hpp"
#include <fstream>
#include <regex>

void UpdateConsoleTitle( ) {
	int good = g_InviteCheckerData.good_count;
	int bad = g_InviteCheckerData.bad_count;
	int proxies = g_ProxyHelper.GetProxiesNum( );

	// ! not thread safe
	int remaining = g_InviteCheckerData.remainings.size( );
	remaining += g_InviteCheckerData.left_to_check;

	SetConsoleTitleA( FormatString( "INVITES | GOOD: %d | BAD: %d | REMAINING: %d | PROXIES: %d" , good , bad , remaining , proxies ).c_str( ) );
}

void InviteCheckerThread( )
{
	while ( !g_exit && !g_InviteChecker.bFinished )
	{
		// otherwise ip rate limit..
		std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

		InviteCodeData_t invite = InviteCodeData_t( "" );
		bool already_checked = false;

		UpdateConsoleTitle( );

		g_InviteCheckerMutex.lock( );
		if ( g_InviteCheckerData.remainings.size( ) ) {
			g_InviteCheckerData.left_to_check++;
			invite = g_InviteCheckerData.remainings.front( );
			g_InviteCheckerData.remainings.pop_front( );

			if ( std::find( g_InviteCheckerData.checked.begin( ) , g_InviteCheckerData.checked.end( ) , invite ) != g_InviteCheckerData.checked.end( ) ) {
				already_checked = true;
			}
		}

		g_InviteCheckerMutex.unlock( );

		UpdateConsoleTitle( );

		if ( invite.code.empty( ) )
			continue;

		if ( already_checked ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " code \"%s\" already checked\n" , invite.code.data( ) );
			g_InviteCheckerData.left_to_check--;
			g_InviteCheckerData.bad_count++;
			if ( g_InviteCheckerData.remainings.empty( ) && g_InviteCheckerData.left_to_check <= 0 ) {
				g_InviteChecker.bFinished = true;
			}
			UpdateConsoleTitle( );
			continue;
		}

		auto output = g_HttpRequests.Request( FormatString( "https://discord.com/api/v9/invites/%s?with_counts=true&with_expiration=true" , invite.code.data( ) ) , "GET" , "" , g_ProxyHelper.RollProxyData( ) );
		if ( !output.success ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed http request code \"%s\", re-adding to queue, error: %s\n" , invite.code.data( ) , output.error.data( ) );

			g_InviteCheckerMutex.lock( );
			g_InviteCheckerData.left_to_check--;
			g_InviteCheckerData.remainings.push_front( invite );
			g_InviteCheckerMutex.unlock( );

			continue;
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
			if ( output.response.find( "error code: 1015" ) != std::string::npos ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find guild data, ip rate-limited, re-adding to queue\n" , invite.code.data( ) );

				g_InviteCheckerMutex.lock( );
				g_InviteCheckerData.left_to_check--;
				g_InviteCheckerData.remainings.push_front( invite );
				g_InviteCheckerMutex.unlock( );

				continue;
			}

			if ( output.response.find( "Bad Gateway" ) != std::string::npos ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find guild data, bad gateway, re-adding to queue\n" , invite.code.data( ) );

				g_InviteCheckerMutex.lock( );
				g_InviteCheckerData.left_to_check--;
				g_InviteCheckerData.remainings.push_front( invite );
				g_InviteCheckerMutex.unlock( );

				continue;
			}

			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to parse \"%s\" guild data, output: %s\n" , invite.code.data( ) , output.response.data( ) );

			g_InviteCheckerMutex.lock( );
			g_InviteCheckerData.checked.push_back( InviteCodeData_t( invite.code , false , invite.expires_at , invite.guild , invite.guild_name , invite.online_member_count , invite.member_count ) );
			g_InviteCheckerMutex.unlock( );

			g_InviteCheckerData.bad_count++;
			g_InviteCheckerData.left_to_check--;
			if ( g_InviteCheckerData.remainings.empty( ) && g_InviteCheckerData.left_to_check <= 0 ) {
				g_InviteChecker.bFinished = true;
			}
			UpdateConsoleTitle( );
			continue;
		}

		if ( !( j [ "guild" ][ "id" ].is_string( ) || j [ "guild" ][ "id" ].is_number( ) ) ) {
			if ( j [ "message" ].is_string( ) && j [ "code" ].is_number( ) ) {
				if ( j [ "code" ] == 1015 || j [ "message" ].get< std::string >( ).find( "rate limit" ) != std::string::npos ) {
					printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find guild data, ip rate-limited, re-adding to queue\n" , invite.code.data( ) );

					g_InviteCheckerMutex.lock( );
					g_InviteCheckerData.left_to_check--;
					g_InviteCheckerData.remainings.push_front( invite );
					g_InviteCheckerMutex.unlock( );

					continue;
				}
			}

			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find guild data, output: %s\n" , invite.code.data( ) , output.response.data( ) );

			g_InviteCheckerMutex.lock( );
			g_InviteCheckerData.checked.push_back( InviteCodeData_t( invite.code , false , invite.expires_at , invite.guild , invite.guild_name , invite.online_member_count , invite.member_count ) );
			g_InviteCheckerMutex.unlock( );

			g_InviteCheckerData.bad_count++;
			g_InviteCheckerData.left_to_check--;
			if ( g_InviteCheckerData.remainings.empty( ) && g_InviteCheckerData.left_to_check <= 0 ) {
				g_InviteChecker.bFinished = true;
			}
			UpdateConsoleTitle( );
			continue;
		}

		std::string guild_id = j [ "guild" ][ "id" ].get< std::string >( );
		std::string guild_name = j [ "guild" ][ "name" ].get< std::string >( );
		int online_member_count = j [ "approximate_presence_count" ].get< int >( );
		int member_count = j [ "approximate_member_count" ].get< int >( );

		std::string expires_at = "unknown";
		if ( j [ "expires_at" ].is_string( ) ) {
			time_t utctime = getEpochTime( j [ "expires_at" ].get< std::string >( ).c_str( ) );
			struct tm tm;
			localtime_s( &tm , &utctime );

			char localTime [ 16 ];
			strftime( localTime , 16 , "%d/%m/%Y" , &tm );
			expires_at = std::string( localTime );
		}

		g_InviteCheckerMutex.lock( );

		if ( std::find( g_InviteCheckerData.blacklist_guilds.begin( ) , g_InviteCheckerData.blacklist_guilds.end( ) , guild_id ) != g_InviteCheckerData.blacklist_guilds.end( ) ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " code \"%s\" goes to blacklisted guild: %s\n" , invite.code.data( ) , guild_id.data( ) );
			g_InviteCheckerData.left_to_check--;
			g_InviteCheckerData.bad_count++;
			if ( g_InviteCheckerData.remainings.empty( ) && g_InviteCheckerData.left_to_check <= 0 ) {
				g_InviteChecker.bFinished = true;
			}
			g_InviteCheckerMutex.unlock( );
			UpdateConsoleTitle( );
			continue;
		}

		// config check
		{
			int minimum_members = g_Config.invite_checker.min_members;
			int minimum_members_online = g_Config.invite_checker.min_members_online;
			int maximum_members = g_Config.invite_checker.max_members;

			if ( minimum_members > 0 && maximum_members > minimum_members )
			{
				if ( member_count < minimum_members || member_count > maximum_members || ( minimum_members_online > 0 && online_member_count < minimum_members_online ) )
				{
					printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " code \"%s\" goes to guild %s which doesn't meet member requirements\n" , invite.code.data( ) , guild_id.data( ) );
					g_InviteCheckerData.left_to_check--;
					if ( g_InviteCheckerData.remainings.empty( ) && g_InviteCheckerData.left_to_check <= 0 ) {
						g_InviteChecker.bFinished = true;
					}
					g_InviteCheckerData.bad_count++;
					g_InviteCheckerMutex.unlock( );
					UpdateConsoleTitle( );
					continue;
				}
			}
		}

		invite.guild = guild_id;
		invite.guild_name = guild_name;
		invite.expires_at = expires_at;
		invite.online_member_count = online_member_count;
		invite.member_count = member_count;
		auto it_check = std::find( g_InviteCheckerData.checked.begin( ) , g_InviteCheckerData.checked.end( ) , invite );
		already_checked = it_check != g_InviteCheckerData.checked.end( );
		invite.good = !already_checked;

		// prefer vanity codes or without expire date over the others or non expiring ones
		if ( already_checked && expires_at == "unknown" ) {
			if ( it_check->expires_at != expires_at ) {
				printf( ANSI_COLOR_YELLOW "[warn]" ANSI_COLOR_RESET " \"%s\" is a no-expire code, erasing \"%s\", guild: %s, guild name: %s\n" , invite.code.data( ) , it_check->code.data( ) , guild_id.data( ) , guild_name.data( ) );
				invite.good = true;
				g_InviteCheckerData.checked.erase( it_check );
			}
		}

		g_InviteCheckerData.checked.push_back( invite );

		if ( invite.good ) {
			g_InviteCheckerData.good_count++;
			printf( ANSI_COLOR_GREEN "[good]" ANSI_COLOR_RESET " code: \"%s\" | expires at: %s | guild id: %s | guild name: \"%s\" | members (on/all): %d/%d\n" , invite.code.data( ) , expires_at.data( ) , guild_id.data( ) , guild_name.data( ) , online_member_count , member_count );
		}
		else {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" is not meeting requirements/already checked\n" , invite.code.data( ) );
			g_InviteCheckerData.bad_count++;
		}

		g_InviteCheckerData.left_to_check--;
		if ( g_InviteCheckerData.remainings.empty( ) && g_InviteCheckerData.left_to_check <= 0 ) {
			g_InviteChecker.bFinished = true;
		}

		UpdateConsoleTitle( );
		g_InviteCheckerMutex.unlock( );
	}
}

bool CInviteChecker::ParseInviteCodes( )
{
	std::ifstream file( "invites.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open invites.txt to read invite codes\n" );
		return false;
	}

	std::string line;
	while ( std::getline( file , line ) )
	{
		if ( g_exit )
			break;

		/*
		var regex = {
	invite: /discord(?:\.com|app\.com|\.gg)(\/invite\/|\/)(?:[a-zA-Z0-9\-]+)/gim,
	url: /(discord.gg\/|discord.com\/invite\/|discordapp.com\/invite\/)/gim
};
		*/

		std::smatch m;

		std::regex regex_a( "/discord(?:\.com|app\.com|\.gg)(\/invite\/|\/)(?:[a-zA-Z0-9\-]+)" );
		std::string a = line;
		if ( std::regex_search( a , m , regex_a ) ) {
			a = m.suffix( ).str( );
		}

		std::regex regex_b( "/(discord.gg\/|discord.com\/invite\/|discordapp.com\/invite\/)" );
		std::string b = line;
		if ( std::regex_search( b , m , regex_b ) ) {
			b = m.suffix( ).str( );
		}

		size_t space_in_code_pos = b.find_first_of( ' ' );
		if ( space_in_code_pos != std::string::npos ) {
			b = b.substr( 0 , space_in_code_pos );
		}

		if ( b.empty( ) )
			continue;

		if ( std::find_if( g_InviteCheckerData.remainings.begin( ) , g_InviteCheckerData.remainings.end( ) , [ & ] ( const InviteCodeData_t& data ) {
			return data.code == b;
			} ) != g_InviteCheckerData.remainings.end( ) ) {
			continue;
		}

		g_InviteCheckerData.remainings.push_back( InviteCodeData_t( b ) );
	}

	file.close( );

	return g_InviteCheckerData.remainings.size( ) > 0;
}

bool CInviteChecker::ParseBlacklistGuilds( )
{
	std::ifstream file( "blacklist_guilds.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open blacklist_guilds.txt to read blacklisted guilds\n" );
		return false;
	}

	std::string line;
	while ( std::getline( file , line ) )
	{
		if ( g_exit )
			break;

		g_InviteCheckerData.blacklist_guilds.push_back( line );
	}

	file.close( );

	return g_InviteCheckerData.blacklist_guilds.size( ) > 0;
}

bool CInviteChecker::Init( )
{
	this->bInitialized = false;

	SetConsoleTitleA( "INVITES | Parsing.." );

	g_InviteCheckerMutex.lock( );
	bool parsed_invites = this->ParseInviteCodes( );
	bool parsed_blacklist = this->ParseBlacklistGuilds( );
	g_InviteCheckerMutex.unlock( );

	SetConsoleTitleA( "INVITES | Parsed files" );

	this->bInitialized = parsed_invites;

	return this->bInitialized;
}

void CInviteChecker::Run( )
{
	if ( !this->bInitialized ) {
		return;
	}

	int max_threads = std::thread::hardware_concurrency( );
	if ( g_Config.threads > 0 ) {
		max_threads = g_Config.threads;
	}

	SetConsoleTitleA( "INVITES | Creating Threads" );

	for ( int i = 0; i < max_threads; i++ ) {
		if ( this->bFinished || g_exit ) {
			break;
		}

		this->vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) InviteCheckerThread , NULL , 0 , NULL ) );
	}

	while ( !this->bFinished ) { if ( g_exit ) break; }

	/*if ( !this->bFinished ) {
		return;
	}*/

	SetConsoleTitleA( "INVITES | Writing codes" );

	if ( this->bWroteFinishedCodes ) {
		return;
	}

	this->bWroteFinishedCodes = true;

	g_InviteCheckerMutex.lock( );
	g_InviteCheckerMutex.unlock( );

	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " starting to write invites to valid_invites.txt\n" );

	std::ofstream file( "valid_invites.txt" );
	if ( !file.good( ) ) {
		SetConsoleTitleA( "INVITES | Failed to open valid codes file" );
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open valid_invites.txt to write valid invite codes\n" );
		return;
	}

	std::deque< std::string > already_written_codes = { };
	std::deque< std::string > already_written_guilds = { };
	g_InviteCheckerMutex.lock( );
	for ( auto& invite : g_InviteCheckerData.checked )
	{
		if ( !invite.good )
			continue;

		if ( std::find( already_written_codes.begin( ) , already_written_codes.end( ) , invite.code ) != already_written_codes.end( ) )
			continue;

		if ( std::find( already_written_guilds.begin( ) , already_written_guilds.end( ) , invite.guild ) != already_written_guilds.end( ) )
			continue;

		file << FormatString( "https://discord.gg/%s\n" , invite.code.data( ) );

		already_written_codes.push_back( invite.code );
		already_written_guilds.push_back( invite.guild );
	}
	g_InviteCheckerMutex.unlock( );

	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote %d invite codes to valid_invites.txt\n" , already_written_codes.size( ) );
	SetConsoleTitleA( FormatString( "INVITES | Wrote %d valid codes" , ( int ) already_written_codes.size( ) ).c_str( ) );
}

void CInviteChecker::Shutdown( )
{
	if ( !this->bInitialized ) {
		return;
	}

	g_InviteCheckerMutex.lock( );
	g_InviteCheckerMutex.unlock( );

	for ( auto& thread : this->vecThreads ) {
		CloseHandle( thread );
	}

	this->bInitialized = false;
}
