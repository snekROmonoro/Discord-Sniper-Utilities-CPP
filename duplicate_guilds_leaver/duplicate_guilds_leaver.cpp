#include "duplicate_guilds_leaver.hpp"
#include "../duplicate_guilds_checker/duplicate_guilds_checker.hpp"
#include "../http/http.hpp"
#include "../proxy/proxy.hpp"
#include "../utils.hpp"
#include "../config/config.hpp"
#include <fstream>
#include <regex>

void DuplicateGuildsLeaverThread( )
{
	DuplicateGuildsLeaverData_t data = DuplicateGuildsLeaverData_t( "" , { } );
	CProxyData* proxy = nullptr;
	while ( !g_exit && !g_DuplicateGuildsLeaver.bFinished )
	{
		// don't be sus
		std::this_thread::sleep_for( std::chrono::seconds( 5 ) );

		if ( data.guilds.empty( ) ) {
			std::lock_guard< std::mutex > l( g_DuplicateGuildsLeaverMutex );
			if ( g_DuplicateGuildsLeaverDatas.empty( ) ) {
				if ( g_DuplicateGuildsLeaver.left_to_leave <= 0 ) {
					g_DuplicateGuildsLeaver.bFinished = true;
				}

				continue;
			}
			else {
				data = g_DuplicateGuildsLeaverDatas.front( );
				g_DuplicateGuildsLeaverDatas.pop_front( );
				g_DuplicateGuildsLeaver.left_to_leave++;
				proxy = g_ProxyHelper.RollProxyData( );
			}
		}

		if ( data.token.empty( ) || data.guilds.empty( ) ) {
			continue;
		}

		std::string guild = data.guilds.front( );
		data.guilds.pop_front( );

		auto output = g_HttpRequests.RequestEx( FormatString( "https://discord.com/api/v9/users/@me/guilds/%s" , guild.data( ) ) , "DELETE" , "" , { FormatString( "Authorization: %s", data.token.data( ) ) } , proxy );
		if ( !output.success ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed http request to leave a server, re-adding to queue and re-rolling proxy..\n" );
			data.guilds.push_front( guild );
			proxy = g_ProxyHelper.RollProxyData( );
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

		if ( success_parse ) {
			if ( j.contains( "message" ) && j.contains( "code" ) ) {
				if ( j [ "code" ] == 1015 ) {
					printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " ip rate-limited, re-adding to queue and re-rolling proxy.. \n" );
					data.guilds.push_front( guild );
					proxy = g_ProxyHelper.RollProxyData( );
					continue;
				}

				if ( j [ "message" ].get< std::string >( ).find( "rate limit" ) != std::string::npos ) {
					printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " ip rate-limited, re-adding to queue and re-rolling proxy.. \n" );
					data.guilds.push_front( guild );
					proxy = g_ProxyHelper.RollProxyData( );
					continue;
				}
			}
		}

		if ( output.response.find( "error code: 1015" ) != std::string::npos ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " ip rate-limited, re-adding to queue and re-rolling proxy.. \n" );
			data.guilds.push_front( guild );
			proxy = g_ProxyHelper.RollProxyData( );
			continue;
		}

		if ( output.response.find( "Bad Gateway" ) != std::string::npos ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " bad gateway, re-adding to queue and re-rolling proxy.. \n" );
			data.guilds.push_front( guild );
			proxy = g_ProxyHelper.RollProxyData( );
			continue;
		}

		if ( output.response.empty( ) ) {
			printf( ANSI_COLOR_GREEN "[good]" ANSI_COLOR_RESET " left guild %s\n" , guild.data( ) );
		}
		else {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " %s\n" , output.response.data( ) );
		}

		if ( data.guilds.empty( ) ) {
			g_DuplicateGuildsLeaver.left_to_leave--;
		}
	}
}

bool CDuplicateGuildsLeaver::Init( )
{
	// todo: parse from file
	//this->vecBlacklistGuilds.push_back( "1024404410736054362" ); // conturi d-alea jmek
	this->vecBlacklistGuilds.push_back( "1017774367612076112" ); // Iancu nitro service
	this->vecBlacklistGuilds.push_back( "1018974933981868193" ); // Liberty.lua
	this->vecBlacklistGuilds.push_back( "990591809212264489" ); // Iancu Klan
	this->vecBlacklistGuilds.push_back( "876706632506150913" ); // Leito
	this->vecBlacklistGuilds.push_back( "837863680158466058" ); // random server some random tokens are in
	//this->vecBlacklistGuilds.push_back( "993950074482733196" ); // quenty's sniperboiz

	return ( this->bInitialized = g_DuplicateGuildsChecker.Init( ) );
}

void CDuplicateGuildsLeaver::Run( )
{
	if ( !this->bInitialized ) {
		return;
	}

	//g_DuplicateGuildsChecker.RunThreadsOnly( );
	g_DuplicateGuildsChecker.Run( );
	g_DuplicateGuildsChecker.Shutdown( );

	if ( g_exit ) {
		return;
	}

	std::deque< std::string > whitelist_tokens = g_Config.invite_checker.whitelist_tokens;
	for ( auto& guild : g_DuplicateGuildsCheckerData.guilds )
	{
		// blacklisted guild from leaving
		if ( std::find( this->vecBlacklistGuilds.begin( ) , this->vecBlacklistGuilds.end( ) , guild.guild_id ) != this->vecBlacklistGuilds.end( ) ) {
			continue;
		}

		bool tokens_include_us = false;
		for ( auto& our_token : whitelist_tokens ) {
			if ( std::find( guild.tokens.begin( ) , guild.tokens.end( ) , our_token ) != guild.tokens.end( ) ) {
				tokens_include_us = true;
				break;
			}
		}

		for ( int i = 0; i < guild.tokens.size( ); i++ ) {
			// let atleast one token in that server
			if ( tokens_include_us ) {
				if ( std::find( whitelist_tokens.begin( ) , whitelist_tokens.end( ) , guild.tokens [ i ] ) != whitelist_tokens.end( ) ) {
					continue;
				}
			}
			else if ( i == 0 ) {
				continue;
			}

			auto it = std::find_if( g_DuplicateGuildsLeaverDatas.begin( ) , g_DuplicateGuildsLeaverDatas.end( ) , [ & ] ( const DuplicateGuildsLeaverData_t& b ) {
				return guild.tokens [ i ] == b.token && !b.token.empty( );
				} );

			if ( it == g_DuplicateGuildsLeaverDatas.end( ) ) {
				g_DuplicateGuildsLeaverDatas.push_back( DuplicateGuildsLeaverData_t( guild.tokens [ i ] , { guild.guild_id } ) );
				continue;
			}

			it->guilds.push_back( guild.guild_id );
		}
	}

	if ( g_DuplicateGuildsLeaverDatas.empty( ) ) {
		printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " lucky you, there are no duplicate servers in your tokens\n" );
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

		this->vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) DuplicateGuildsLeaverThread , NULL , 0 , NULL ) );
	}

	while ( !this->bFinished ) { if ( g_exit ) break; }

	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " stopped leave process. most probably duplicates are removed\n" );
}

void CDuplicateGuildsLeaver::Shutdown( )
{
	if ( !this->bInitialized ) {
		return;
	}

	g_DuplicateGuildsLeaverMutex.lock( );
	g_DuplicateGuildsLeaverMutex.unlock( );

	for ( auto& thread : this->vecThreads ) {
		CloseHandle( thread );
	}

	this->bInitialized = false;
}
