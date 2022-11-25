#include "token_checker.hpp"
#include "../http/http.hpp"
#include "../proxy/proxy.hpp"
#include "../utils.hpp"
#include "../nlohmann/json.hpp"
#include "../config/config.hpp"
#include <fstream>
#include <regex>
#include <bitset>

// bool[0] = token works; bool[1] = token requires verify
std::pair< bool , bool > GetTokenGuilds( TokenData_t& token_data , CProxyData* proxy = nullptr ) {
	int retries = -1;
retry_request:
	retries++;

	auto output = g_HttpRequests.RequestEx( "https://discord.com/api/v9/users/@me/guilds" , "GET" , "" , { FormatString( "Authorization: %s" , token_data.token.data( ) ) } , proxy );
	if ( !output.success ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " (guild check) failed http request token \"%s\", re-trying with different proxy, error: %s\n" , token_data.token.data( ) , output.error.data( ) );
		if ( retries < 5 ) {
			proxy = g_ProxyHelper.RollProxyData( );
			goto retry_request;
		}

		return { false , false };
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
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find guild data, ip rate-limited, re-trying with different proxy\n" , token_data.token.data( ) );
			if ( retries < 5 ) {
				proxy = g_ProxyHelper.RollProxyData( );
				goto retry_request;
			}

			return { false , false };
		}

		if ( output.response.find( "Bad Gateway" ) != std::string::npos ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find guild data, bad gateway, re-trying with different proxy\n" , token_data.token.data( ) );
			if ( retries < 5 ) {
				proxy = g_ProxyHelper.RollProxyData( );
				goto retry_request;
			}

			return { false , false };
		}

		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to parse \"%s\" guild data, output: %s\n" , token_data.token.data( ) , output.response.data( ) );
		return { false , false };
	}

	if ( !j.is_array( ) ) {
		if ( j [ "message" ].is_string( ) && j [ "code" ].is_number( ) ) {
			if ( j [ "code" ] == 1015 || j [ "message" ].get< std::string >( ).find( "rate limit" ) != std::string::npos ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find user data, ip rate-limited, re-trying with different proxy\n" , token_data.token.data( ) );
				if ( retries < 5 ) {
					proxy = g_ProxyHelper.RollProxyData( );
					goto retry_request;
				}

				return { false , false };
			}

			if ( j [ "message" ].get< std::string >( ) == "401: Unauthorized" ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " invalid token \"%s\"\n" , token_data.token.data( ) );
				return { false , false };
			}

			if ( j [ "message" ].get< std::string >( ) == "You need to verify your account in order to perform this action." ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " requires verify, token: \"%s\"\n" , token_data.token.data( ) );
				return { false , true };
			}
		}

		return { false , false };
	}

	for ( auto& e : j ) {
		std::string guild_id = e [ "id" ];
		token_data.guilds.push_back( guild_id );
	}

	return { true , false };
}

void TokenCheckerThread( )
{
	CProxyData* proxy = nullptr;
	while ( !g_exit && !g_TokenChecker.bFinished )
	{
		// don't be too fast buddy
		std::this_thread::sleep_for( std::chrono::seconds( 5 ) );

		std::string token = "";
		bool got_token = false;

		/* get a token */ {
			std::lock_guard< std::mutex > l( g_TokenCheckerMutex );
			if ( g_TokenCheckerData.remainings.empty( ) ) {
				if ( g_TokenCheckerData.left_to_check <= 0 ) {
					g_TokenChecker.bFinished = true;
				}

				continue;
			}
			else {
				token = g_TokenCheckerData.remainings.front( );
				got_token = true;
				g_TokenCheckerData.remainings.pop_front( );
				g_TokenCheckerData.left_to_check++;
				proxy = g_ProxyHelper.RollProxyData( );
			}
		}

		if ( !got_token || token.empty( ) ) {
			continue;
		}

		auto output = g_HttpRequests.RequestEx( "https://discord.com/api/v9/users/@me" , "GET" , "" , { FormatString( "Authorization: %s" , token.data( ) ) } , proxy );
		if ( !output.success ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed http request token \"%s\", re-adding to queue, error: %s\n" , token.data( ) , output.error.data( ) );

			g_TokenCheckerMutex.lock( );
			g_TokenCheckerData.left_to_check--;
			g_TokenCheckerData.remainings.push_front( token );
			g_TokenCheckerMutex.unlock( );

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
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find user data, ip rate-limited, re-adding to queue\n" , token.data( ) );

				g_TokenCheckerMutex.lock( );
				g_TokenCheckerData.left_to_check--;
				g_TokenCheckerData.remainings.push_front( token );
				g_TokenCheckerMutex.unlock( );

				continue;
			}

			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to parse \"%s\" user data, output: %s\n" , token.data( ) , output.response.data( ) );

			g_TokenCheckerMutex.lock( );
			g_TokenCheckerData.left_to_check--;
			g_TokenCheckerData.invalid.push_back( token );
			g_TokenCheckerMutex.unlock( );

			continue;
		}

		if ( j [ "message" ].is_string( ) && j [ "code" ].is_number( ) ) {
			if ( j [ "code" ] == 1015 || j [ "message" ].get< std::string >( ).find( "rate limited" ) != std::string::npos ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find user data, ip rate-limited, re-adding to queue\n" , token.data( ) );

				g_TokenCheckerMutex.lock( );
				g_TokenCheckerData.left_to_check--;
				g_TokenCheckerData.remainings.push_front( token );
				g_TokenCheckerMutex.unlock( );

				continue;
			}

			if ( j [ "message" ].get< std::string >( ) == "401: Unauthorized" ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " invalid token \"%s\"\n" , token.data( ) );

				g_TokenCheckerMutex.lock( );
				g_TokenCheckerData.left_to_check--;
				g_TokenCheckerData.invalid.push_back( token );
				g_TokenCheckerMutex.unlock( );

				continue;
			}

			if ( j [ "message" ].get< std::string >( ) == "You need to verify your account in order to perform this action.You need to verify your account in order to perform this action." ) {
				printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " requires verify, token: \"%s\"\n" , token.data( ) );

				g_TokenCheckerMutex.lock( );
				g_TokenCheckerData.left_to_check--;
				g_TokenCheckerData.requires_verify.push_back( token );
				g_TokenCheckerMutex.unlock( );

				continue;
			}
		}

		if ( !( j [ "id" ].is_string( ) || j [ "username" ].is_string( ) ) ) {
			if ( j [ "message" ].is_string( ) && j [ "code" ].is_number( ) ) {
				if ( j [ "code" ] == 1015 || j [ "message" ].get< std::string >( ).find( "rate limited" ) != std::string::npos ) {
					printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find user data, ip rate-limited, re-adding to queue\n" , token.data( ) );

					g_TokenCheckerMutex.lock( );
					g_TokenCheckerData.left_to_check--;
					g_TokenCheckerData.remainings.push_front( token );
					g_TokenCheckerMutex.unlock( );

					continue;
				}
			}

			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " \"%s\" couldn't find user data, output: %s\n" , token.data( ) , output.response.data( ) );

			g_TokenCheckerMutex.lock( );
			g_TokenCheckerData.left_to_check--;
			g_TokenCheckerData.invalid.push_back( token );
			g_TokenCheckerMutex.unlock( );

			continue;
		}

		TokenData_t token_data { };
		token_data.token = token;
		token_data.userId = j [ "id" ];
		token_data.userName = j [ "username" ];
		token_data.emailVerified = j [ "verified" ].is_boolean( ) ? j [ "verified" ].get< bool >( ) : false;
		token_data.email = j [ "email" ].is_string( ) ? j [ "email" ].get< std::string >( ) : "~";
		token_data.phone = j [ "phone" ].is_string( ) ? j [ "phone" ].get< std::string >( ) : "~";
		token_data.premium_type = j [ "premium_type" ].is_number( ) ? j [ "premium_type" ].get< int >( ) : 0;
		token_data.bot = j [ "bot" ].is_boolean( ) ? j [ "bot" ].get< bool >( ) : false;
		token_data.twoFactorAuthEnabled = j [ "mfa_enabled" ].is_boolean( ) ? j [ "mfa_enabled" ].get< bool >( ) : false;
		token_data.language = j [ "locale" ].is_string( ) ? j [ "locale" ].get< std::string >( ) : "~";
		token_data.guilds = { };

		std::pair< bool , bool > got_guilds = GetTokenGuilds( token_data , proxy );
		if ( got_guilds.second ) {
			printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " requires verify \"%s\"\n" , token.data( ) );

			g_TokenCheckerMutex.lock( );
			g_TokenCheckerData.left_to_check--;
			g_TokenCheckerData.requires_verify.push_back( token );
			g_TokenCheckerMutex.unlock( );

			continue;
		}

		token_data.successParsingGuilds = got_guilds.first;

		printf( ANSI_COLOR_GREEN "[good]" ANSI_COLOR_RESET " good token \"%s\"\n" , token.data( ) );

		g_TokenCheckerMutex.lock( );
		g_TokenCheckerData.left_to_check--;
		g_TokenCheckerData.valid.push_back( token_data );
		g_TokenCheckerMutex.unlock( );
	}
}

bool CTokenChecker::ParseTokens( )
{
	std::ifstream file( "tokens.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open tokens.txt to read tokens\n" );
		return false;
	}

	std::string line;
	while ( std::getline( file , line ) )
	{
		if ( g_exit )
			break;

		if ( std::find( g_TokenCheckerData.remainings.begin( ) , g_TokenCheckerData.remainings.end( ) , line ) == g_TokenCheckerData.remainings.end( ) ) {
			g_TokenCheckerData.remainings.push_back( line );
		}
	}

	file.close( );

	return g_TokenCheckerData.remainings.size( ) > 0;
}

void CTokenChecker::WriteValid( )
{
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " starting to write valid tokens data to tokens_valid.json\n" );

	std::ofstream file( "tokens_valid.json" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open tokens_valid.txt to write valid tokens\n" );
		return;
	}

	nlohmann::json j;
	for ( auto& token : g_TokenCheckerData.valid ) {
		j [ token.token ] = { };

		if ( token.bot ) {
			j [ token.token ][ "bot" ] = true;
		}

		j [ token.token ][ "id" ] = token.userId;
		j [ token.token ][ "username" ] = token.userName;

		// todo: get creation date

		j [ token.token ][ "email" ] = { };
		j [ token.token ][ "email" ][ "verified" ] = token.emailVerified;
		j [ token.token ][ "email" ][ "email" ] = token.email;

		j [ token.token ][ "phone" ] = token.phone;

		// https://discord.com/developers/docs/resources/user#user-object-premium-types
		std::string premium_type_str = "None";
		switch ( token.premium_type ) {
		case 1: {
			premium_type_str = "Nitro Classic";
		} break;
		case 2: {
			premium_type_str = "Nitro";
		} break;
		case 3: {
			premium_type_str = "Nitro Basic";
		} break;
		}

		j [ token.token ][ "premium_type" ] = premium_type_str;

		j [ token.token ][ "2fa" ] = token.twoFactorAuthEnabled;
		if ( token.language.size( ) ) {
			j [ token.token ][ "language" ] = token.language;
		}

		j [ token.token ][ "guilds" ] = { };
		j [ token.token ][ "guilds" ][ "count" ] = token.guilds.size( );
		j [ token.token ][ "guilds" ][ "success" ] = token.successParsingGuilds;
	}

	file << j.dump( 4 );
	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote valid tokens data to tokens_valid.json\n" );
}

void CTokenChecker::WriteValidSimple( )
{
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " starting to write valid tokens to tokens_valid_simple.txt\n" );

	std::ofstream file( "tokens_valid_simple.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open tokens_valid_simple.txt to write invalid tokens\n" );
		return;
	}

	for ( auto& token : g_TokenCheckerData.valid ) {
		file << token.token << "\n";
	}

	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote valid tokens to tokens_valid_simple.txt\n" );
}

void CTokenChecker::WriteInvalid( )
{
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " starting to write invalid tokens to tokens_invalid.txt\n" );

	std::ofstream file( "tokens_invalid.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open tokens_invalid.txt to write invalid tokens\n" );
		return;
	}

	for ( auto& token : g_TokenCheckerData.invalid ) {
		file << token << "\n";
	}

	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote invalid tokens to tokens_invalid.txt\n" );
}

void CTokenChecker::WriteRequiresVerify( )
{
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " starting to write unverified tokens to tokens_unverified.txt\n" );

	std::ofstream file( "tokens_unverified.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open tokens_unverified.txt to write invalid tokens\n" );
		return;
	}

	for ( auto& token : g_TokenCheckerData.requires_verify ) {
		file << token << "\n";
	}

	file.close( );
	printf( ANSI_COLOR_GREEN "[complete]" ANSI_COLOR_RESET " wrote unverified tokens to tokens_unverified.txt\n" );
}

bool CTokenChecker::Init( )
{
	this->bInitialized = false;

	g_TokenCheckerMutex.lock( );
	bool parsed_tokens = this->ParseTokens( );
	g_TokenCheckerMutex.unlock( );

	this->bInitialized = parsed_tokens;

	return this->bInitialized;
}

void CTokenChecker::Run( )
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

		this->vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) TokenCheckerThread , NULL , 0 , NULL ) );
	}

	while ( !this->bFinished ) { if ( g_exit ) break; }

	/*if ( !this->bFinished ) {
		return;
	}*/

	if ( this->bWroteData ) {
		return;
	}

	this->bWroteData = true;

	g_TokenCheckerMutex.lock( );
	this->WriteValid( );
	this->WriteValidSimple( );
	this->WriteInvalid( );
	this->WriteRequiresVerify( );
	g_TokenCheckerMutex.unlock( );
}

void CTokenChecker::Shutdown( )
{
	if ( !this->bInitialized ) {
		return;
	}

	g_TokenCheckerMutex.lock( );
	g_TokenCheckerMutex.unlock( );

	for ( auto& thread : this->vecThreads ) {
		CloseHandle( thread );
	}

	this->bInitialized = false;
}
