#include "duplicate_guilds_checker.hpp"
#include "../http/http.hpp"
#include "../proxy/proxy.hpp"
#include "../utils.hpp"
#include "../nlohmann/json.hpp"
#include "../config/config.hpp"
#include <fstream>
#include <regex>

void DuplicateGuildsCheckerThread( )
{
	while ( !g_exit && !g_DuplicateGuildsChecker.bFinished )
	{
		// otherwise ip rate limit..
		std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

		std::string token = "";
		/* get token */ {
			std::lock_guard< std::mutex > l( g_DuplicateGuildsCheckerMutex );
			if ( g_DuplicateGuildsCheckerData.tokens.empty( ) ) {
				if ( g_DuplicateGuildsChecker.left_to_check <= 0 ) {
					g_DuplicateGuildsChecker.bFinished = true;
				}
			}
			else {
				token = g_DuplicateGuildsCheckerData.tokens.front( );
				g_DuplicateGuildsCheckerData.tokens.pop_front( );
				g_DuplicateGuildsChecker.left_to_check++;
			}
		}

		if ( token.empty( ) ) {
			continue;
		}

		auto output = g_HttpRequests.RequestEx( "https://discord.com/api/v9/users/@me/guilds" , "GET" , "" , { FormatString( "Authorization: %s", token.data( ) ) } , g_ProxyHelper.RollProxyData( ) );

		if ( !output.success ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed http request for token \"%s\", re-adding to queue, error: %s\n" , token.data( ) , output.error.data( ) );

			g_DuplicateGuildsChecker.left_to_check--;
			g_DuplicateGuildsCheckerMutex.lock( );
			g_DuplicateGuildsCheckerData.tokens.push_front( token );
			g_DuplicateGuildsCheckerMutex.unlock( );

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
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" error (ip rate limited), re-adding to queue\n" , token.data( ) );
				g_DuplicateGuildsChecker.left_to_check--;
				g_DuplicateGuildsCheckerMutex.lock( );
				g_DuplicateGuildsCheckerData.tokens.push_front( token );
				g_DuplicateGuildsCheckerMutex.unlock( );

				continue;
			}

			if ( output.response.find( "Bad Gateway" ) != std::string::npos ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" error (bad gateway), re-adding to queue\n" , token.data( ) );
				g_DuplicateGuildsChecker.left_to_check--;
				g_DuplicateGuildsCheckerMutex.lock( );
				g_DuplicateGuildsCheckerData.tokens.push_front( token );
				g_DuplicateGuildsCheckerMutex.unlock( );

				continue;
			}

			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to parse \"%s\" guild data: %s\n" , token.data( ) , output.response.data( ) );

			g_DuplicateGuildsChecker.left_to_check--;

			g_DuplicateGuildsCheckerMutex.lock( );
			if ( g_DuplicateGuildsCheckerData.tokens.empty( ) && g_DuplicateGuildsChecker.left_to_check <= 0 ) {
				g_DuplicateGuildsChecker.bFinished = true;
			}
			g_DuplicateGuildsCheckerMutex.unlock( );

			continue;
		}

		if ( j.contains( "message" ) && j.contains( "code" ) ) {
			if ( j [ "code" ] == 1015 ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" error: %s, re-adding to queue\n" , token.data( ) , j [ "message" ].get< std::string >( ).data( ) );
				g_DuplicateGuildsChecker.left_to_check--;
				g_DuplicateGuildsCheckerMutex.lock( );
				g_DuplicateGuildsCheckerData.tokens.push_front( token );
				g_DuplicateGuildsCheckerMutex.unlock( );

				continue;
			}

			if ( j [ "message" ].get< std::string >( ).find( "rate limit" ) != std::string::npos ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" error: %s, re-adding to queue\n" , token.data( ) , j [ "message" ].get< std::string >( ).data( ) );
				g_DuplicateGuildsChecker.left_to_check--;
				g_DuplicateGuildsCheckerMutex.lock( );
				g_DuplicateGuildsCheckerData.tokens.push_front( token );
				g_DuplicateGuildsCheckerMutex.unlock( );
				continue;
			}

			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" error: %s\n" , token.data( ) , j [ "message" ].get< std::string >( ).data( ) );

			g_DuplicateGuildsChecker.left_to_check--;

			g_DuplicateGuildsCheckerMutex.lock( );
			if ( g_DuplicateGuildsCheckerData.tokens.empty( ) && g_DuplicateGuildsChecker.left_to_check <= 0 ) {
				g_DuplicateGuildsChecker.bFinished = true;
			}
			g_DuplicateGuildsCheckerMutex.unlock( );

			continue;
		}

		g_DuplicateGuildsCheckerMutex.lock( );
		for ( auto& e : j ) {
			std::string guild_id = e [ "id" ];
			std::string guild_name = e [ "name" ];

			auto it = std::find_if( g_DuplicateGuildsCheckerData.guilds.begin( ) , g_DuplicateGuildsCheckerData.guilds.end( ) , [ & ] ( const DuplicateGuildsGuildData_t& guild ) {
				return guild.guild_id == guild_id;
				} );

			if ( it != g_DuplicateGuildsCheckerData.guilds.end( ) ) {
				it->tokens.push_back( token );
				continue;
			}

			g_DuplicateGuildsCheckerData.guilds.push_back( DuplicateGuildsGuildData_t( guild_id , guild_name , { token } ) );
		}

		printf( ANSI_COLOR_GREEN "[success]" ANSI_COLOR_RESET " %d guilds from \"%s\"\n" , j.size( ) , token.data( ) );

		g_DuplicateGuildsChecker.left_to_check--;
		if ( g_DuplicateGuildsCheckerData.tokens.empty( ) && g_DuplicateGuildsChecker.left_to_check <= 0 ) {
			g_DuplicateGuildsChecker.bFinished = true;
		}
		g_DuplicateGuildsCheckerMutex.unlock( );
	}
}

bool CDuplicateGuildsChecker::ParseTokens( )
{
	std::lock_guard< std::mutex > l( g_DuplicateGuildsCheckerMutex );

	std::ifstream file( "tokens.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open tokens.txt to read tokens\n" );
		return false;
	}

	std::string line;
	while ( std::getline( file , line ) )
	{
		if ( std::find( g_DuplicateGuildsCheckerData.tokens.begin( ) , g_DuplicateGuildsCheckerData.tokens.end( ) , line ) == g_DuplicateGuildsCheckerData.tokens.end( ) ) {
			g_DuplicateGuildsCheckerData.tokens.push_back( line );
		}
	}

	file.close( );

	return g_DuplicateGuildsCheckerData.tokens.size( ) > 0;
}

void CDuplicateGuildsChecker::WriteAllGuildsToAFile( )
{
	printf( ANSI_COLOR_GREEN "[write]" ANSI_COLOR_RESET " starting to write all guilds tokens are in to all_guilds.txt\n" );

	std::ofstream file( "all_guilds.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open all_guilds.txt to write all guilds tokens\n" );
		return;
	}

	g_DuplicateGuildsCheckerMutex.lock( );
	for ( auto& guild : g_DuplicateGuildsCheckerData.guilds )
	{
		file << guild.guild_id << "\n";
	}
	g_DuplicateGuildsCheckerMutex.unlock( );

	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote all guilds tokens to all_guilds.txt\n" );
}

bool CDuplicateGuildsChecker::Init( )
{
	return ( this->bInitialized = this->ParseTokens( ) );
}

void CDuplicateGuildsChecker::Run( )
{
	if ( !this->bInitialized ) {
		return;
	}

	int max_threads = std::thread::hardware_concurrency( );
	if ( g_Config.threads > 0 ) {
		max_threads = g_Config.threads;
	}

	for ( int i = 0; i < max_threads; i++ ) {
		if ( this->bFinished || g_exit ) {
			break;
		}

		this->vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) DuplicateGuildsCheckerThread , NULL , 0 , NULL ) );
	}

	while ( !this->bFinished ) { if ( g_exit ) break; }

	/*if ( !this->bFinished ) {
		return;
	}*/

	if ( this->bWroteDuplicatedGuilds ) {
		return;
	}

	this->bWroteDuplicatedGuilds = true;

	this->WriteAllGuildsToAFile( );

	g_DuplicateGuildsCheckerMutex.lock( );
	int guilds_count = g_DuplicateGuildsCheckerData.guilds.size( );
	g_DuplicateGuildsCheckerMutex.unlock( );

	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " starting to write duplicated guilds (out of %d) to duplicated_guilds.txt\n" , guilds_count );

	std::ofstream file( "duplicated_guilds.json" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open duplicated_guilds.json to write duplicated guilds\n" );
		return;
	}

	nlohmann::json j;

	g_DuplicateGuildsCheckerMutex.lock( );
	for ( auto& guild : g_DuplicateGuildsCheckerData.guilds )
	{
		// shouldn't happen
		if ( guild.tokens.empty( ) ) {
			continue;
		}

		// we don't care man
		if ( guild.tokens.size( ) < 2 ) {
			continue;
		}

		j [ guild.guild_id ] = { };
		j [ guild.guild_id ][ "name" ] = guild.guild_name;

		auto tokens_array = nlohmann::json::array( );
		for ( auto& token : guild.tokens ) {
			tokens_array.push_back( token );
		}

		j [ guild.guild_id ][ "tokens" ] = tokens_array;
	}
	g_DuplicateGuildsCheckerMutex.unlock( );

	file << j.dump( 4 );
	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote duplicated guilds to duplicated_guilds.json\n" );
}

void CDuplicateGuildsChecker::Shutdown( )
{
	if ( !this->bInitialized ) {
		return;
	}

	g_DuplicateGuildsCheckerMutex.lock( );
	g_DuplicateGuildsCheckerMutex.unlock( );

	for ( auto& thread : this->vecThreads ) {
		CloseHandle( thread );
	}

	this->bInitialized = false;
}

void CDuplicateGuildsChecker::RunThreadsOnly( )
{
	if ( !this->bInitialized ) {
		return;
	}

	int max_threads = std::thread::hardware_concurrency( );
	if ( g_Config.threads > 0 ) {
		max_threads = g_Config.threads;
	}

	for ( int i = 0; i < max_threads; i++ ) {
		this->vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) DuplicateGuildsCheckerThread , NULL , 0 , NULL ) );
	}

	while ( !this->bFinished ) { if ( g_exit ) break; }
}
