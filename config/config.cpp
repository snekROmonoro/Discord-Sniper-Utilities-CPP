#include "config.hpp"
#include <fstream>
#include "../toml.hpp"
#include "../utils.hpp"

bool CConfig::Init( )
{
	toml::table tbl;
	try {
		tbl = toml::parse_file( "config.toml" );
	}
	catch ( ... ) {
		std::ofstream file( "config.toml" );
		if ( !file.good( ) )
			return false;

		file << R"(
threads = 500

[invite_checker]
min_members = 200
min_members_online = 100
max_members = 75000

[proxies]
scraper = true
from_file = true
		)";

		file.close( );

		printf( ANSI_COLOR_GREEN "[good]" ANSI_COLOR_RESET " wrote pseudo-config to config.toml\n" );

		return false;
	}

	this->threads = tbl [ "threads" ].value_or( 0 );

	this->invite_checker.min_members = tbl [ "invite_checker" ][ "min_members" ].value_or( 1 );
	this->invite_checker.min_members_online = tbl [ "invite_checker" ][ "min_members_online" ].value_or( 1 );
	this->invite_checker.max_members = tbl [ "invite_checker" ][ "max_members" ].value_or( INT_MAX );

	this->invite_checker.whitelist_tokens = { };
	if ( toml::array* arr = tbl [ "invite_checker" ][ "whitelist_tokens" ].as_array( ) ) {
		arr->for_each( [ & ] ( auto&& el )
			{
				if constexpr ( toml::is_string<decltype( el )> ) {
					this->invite_checker.whitelist_tokens.push_back( *el );
				}
			} );

	}

	this->proxies.scraper = tbl [ "proxies" ][ "scraper" ].value_or( true );
	this->proxies.from_file = tbl [ "proxies" ][ "from_file" ].value_or( true );
	return true;
}
