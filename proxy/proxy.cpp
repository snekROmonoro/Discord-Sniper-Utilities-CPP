#include "proxy.hpp"
#include <fstream>
#include <Windows.h>
#include "../utils.hpp"
#include "../config/config.hpp"
#include "../http/http.hpp"
#include <regex>

std::mutex ProxyMutex;

bool CProxyHelper::Init( )
{
	/* example:
		this->m_vecProxyData.push_back( CProxyData(
			"http" , // protocol
			"1.1.1.1" , // address
			"1111" , // port
			"wtohwawv" , // username - optional, required only for proxies that need it
			"1pdbyhdos2bf" // password - optional, required only for proxies that need it
		) );
	*/

	std::ifstream file( "proxies.txt" );
	if ( !file.good( ) ) {
		printf( ANSI_COLOR_RED "[fail]" ANSI_COLOR_RESET " failed to open proxies.txt to get proxies\n" );
		return false;
	}

	std::string line;
	while ( std::getline( file , line ) )
	{
		std::deque< std::string > matches;
		std::string a = line;

		// could've probably used regex?

		size_t find = a.find_first_of( ":" );
		while ( find != std::string::npos ) {

			matches.push_back( a.substr( 0 , find ) );
			a = a.substr( find + 1 );
			find = a.find_first_of( ":" );
		}

		matches.push_back( a );

		if ( matches.size( ) >= 3 ) {
			std::string protocol = matches [ 0 ];
			std::string address = matches [ 1 ];
			std::string port = matches [ 2 ];

			if ( matches.size( ) >= 5 ) {
				std::string username = matches [ 3 ];
				std::string password = matches [ 4 ];
				this->m_vecProxyData.push_back( CProxyData(
					protocol ,
					address ,
					port ,
					username ,
					password
				) );
			}
			else {
				this->m_vecProxyData.push_back( CProxyData(
					protocol ,
					address ,
					port
				) );
			}
		}
	}

	file.close( );

	if ( this->m_vecProxyData.size( ) > 0 ) {
		printf( ANSI_COLOR_BLUE "[proxies]" ANSI_COLOR_RESET " found %d proxies. make sure format is protocol:address:port:username:password\n" , this->m_vecProxyData.size( ) );
	}

	return !this->m_vecProxyData.empty( );
}

struct ProxyScraperLink_t {
	std::string protocol;
	std::string link;
};

std::deque< ProxyScraperLink_t > vecProxyScraperLinks = {
	{ "http" , "http://ab57.ru/downloads/proxyold.txt" },
	{ "http" , "http://alexa.lr2b.com/proxylist.txt" },
	{ "http" , "http://free-fresh-proxy-daily.blogspot.com/feeds/posts/default" },
	{ "http" , "http://proxysearcher.sourceforge.net/Proxy%20List.php?type=http" },
	{ "http" , "http://rootjazz.com/proxies/proxies.txt" },
	{ "http" , "http://spys.me/proxy.txt" },
	{ "http" , "http://worm.rip/http.txt" },
	{ "http" , "http://www.httptunnel.ge/ProxyListForFree.aspx" },
	{ "http" , "http://www.proxyserverlist24.top/feeds/posts/default" },
	{ "http" , "https://api.openproxylist.xyz/http.txt" },
	{ "http" , "https://api.proxyscrape.com/?request=displayproxies&proxytype=http" },
	{ "http" , "https://api.proxyscrape.com/?request=getproxies&proxytype=http&timeout=6000&country=all&ssl=yes&anonymity=all" },
	{ "http" , "https://api.proxyscrape.com/v2/?request=getproxies&protocol=http" },
	{ "http" , "https://api.proxyscrape.com/v2/?request=getproxies&protocol=https" },
	{ "http" , "https://dstat.one/proxies.php?id=2" },
	{ "http" , "https://dstat.one/proxies.php?id=3" },
	{ "http" , "https://free-proxy-list.net/" },
	{ "http" , "https://free-proxy-list.net/anonymous-proxy.html" },
	{ "http" , "https://free-proxy-list.net/uk-proxy.html" },
	{ "http" , "https://hidemy.name/en/proxy-list/" },
	{ "http" , "https://multiproxy.org/txt_all/proxy.txt" },
	{ "http" , "https://openproxy.space/list/http" },
	{ "http" , "https://openproxylist.xyz/http.txt" },
	{ "http" , "https://proxy-spider.com/api/proxies.example.txt" },
	{ "http" , "https://proxy11.com/api/proxy.txt?key=NDAzNg.YYHPVA.QB8moHDjsHJ_R_q8lkgkUV3wt2c" },
	{ "http" , "https://proxy50-50.blogspot.com" },
	{ "http" , "https://proxy50-50.blogspot.com/" },
	{ "http" , "https://proxyspace.pro/http.txt" },
	{ "http" , "https://proxyspace.pro/https.txt" },
	{ "http" , "https://raw.githubusercontent.com/almroot/proxylist/master/list.txt" },
	{ "http" , "https://raw.githubusercontent.com/aslisk/proxyhttps/main/https.txt" },
	{ "http" , "https://raw.githubusercontent.com/B4RC0DE-TM/proxy-list/main/HTTP.txt" },
	{ "http" , "https://raw.githubusercontent.com/clarketm/proxy-list/master/proxy-list-raw.txt" },
	{ "http" , "https://raw.githubusercontent.com/clarketm/proxy-list/master/proxy-list.txt" },
	{ "http" , "https://raw.githubusercontent.com/fate0/proxylist/master/proxy.list" },
	{ "http" , "https://raw.githubusercontent.com/hanwayTech/free-proxy-list/main/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/hanwayTech/free-proxy-list/main/https.txt" },
	{ "http" , "https://raw.githubusercontent.com/hendrikbgr/Free-Proxy-Repo/master/proxy_list.txt" },
	{ "http" , "https://raw.githubusercontent.com/HyperBeats/proxy-list/main/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-http.txt" },
	{ "http" , "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-https.txt" },
	{ "http" , "https://raw.githubusercontent.com/mertguvencli/http-proxy-list/main/proxy-list/data.txt" },
	{ "http" , "https://raw.githubusercontent.com/mmpx12/proxy-list/master/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/mmpx12/proxy-list/master/https.txt" },
	{ "http" , "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies_anonymous/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/opsxcq/proxy-list/master/list.txt" },
	{ "http" , "https://raw.githubusercontent.com/proxy4parsing/proxy-list/main/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/roosterkid/openproxylist/main/HTTPS_RAW.txt" },
	{ "http" , "https://raw.githubusercontent.com/RX4096/proxy-list/main/online/all.txt" },
	{ "http" , "https://raw.githubusercontent.com/RX4096/proxy-list/main/online/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/RX4096/proxy-list/main/online/https.txt" },
	{ "http" , "https://raw.githubusercontent.com/saisuiu/uiu/main/free.txt" },
	{ "http" , "https://raw.githubusercontent.com/saschazesiger/Free-Proxies/master/proxies/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/scidam/proxy-list/master/proxy.json" },
	{ "http" , "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/https.txt" },
	{ "http" , "https://raw.githubusercontent.com/shiftytr/proxy-list/master/proxy.txt" },
	{ "http" , "https://raw.githubusercontent.com/sunny9577/proxy-scraper/master/proxies.json" },
	{ "http" , "https://raw.githubusercontent.com/sunny9577/proxy-scraper/master/proxies.txt" },
	{ "http" , "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/TheSpeedX/SOCKS-List/master/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/TheSpeedX/SOCKS-List/master/socks5.txt" },
	{ "http" , "https://raw.githubusercontent.com/UserR3X/proxy-list/main/online/http.txt" },
	{ "http" , "https://raw.githubusercontent.com/UserR3X/proxy-list/main/online/https.txt" },
	{ "http" , "https://raw.githubusercontent.com/yemixzy/proxy-list/main/proxy-list/data.txt" },
	{ "http" , "https://rootjazz.com/proxies/proxies.txt" },
	{ "http" , "https://sheesh.rip/http.txt" },
	{ "http" , "https://spys.me/proxy.txt" },
	{ "http" , "https://www.freeproxychecker.com/result/http_proxies.txt" },
	{ "http" , "https://www.hide-my-ip.com/proxylist.shtml" },
	{ "http" , "https://www.my-proxy.com/free-anonymous-proxy.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-10.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-2.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-3.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-4.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-5.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-6.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-7.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-8.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list-9.html" },
	{ "http" , "https://www.my-proxy.com/free-proxy-list.html" },
	{ "http" , "https://www.my-proxy.com/free-socks-4-proxy.html" },
	{ "http" , "https://www.my-proxy.com/free-socks-5-proxy.html" },
	{ "http" , "https://www.my-proxy.com/free-transparent-proxy.html" },
	{ "http" , "https://www.proxy-list.download/api/v1/get?type=http" },
	{ "http" , "https://www.proxy-list.download/api/v1/get?type=https" },
	{ "http" , "https://www.proxy-list.download/HTTP" },
	{ "http" , "https://www.proxy-list.download/HTTPS" },
	{ "http" , "https://www.proxyscan.io/download?type=http" },
	{ "http" , "https://www.proxyscan.io/download?type=https" },
	{ "http" , "https://www.socks-proxy.net/" },
	{ "http" , "https://www.sslproxies.org/" },
	{ "http" , "https://www.us-proxy.org/" },
	{ "socks4" , "http://proxysearcher.sourceforge.net/Proxy%20List.php?type=socks" },
	{ "socks4" , "http://worm.rip/socks4.txt" },
	{ "socks4" , "http://www.socks24.org/feeds/posts/default" },
	{ "socks4" , "https://api.openproxylist.xyz/socks4.txt" },
	{ "socks4" , "https://api.proxyscrape.com/?request=displayproxies&proxytype=socks4" },
	{ "socks4" , "https://api.proxyscrape.com/?request=displayproxies&proxytype=socks4&country=all" },
	{ "socks4" , "https://api.proxyscrape.com/v2/?request=displayproxies&protocol=socks4" } ,
	{ "socks4" , "https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks4" } ,
	{ "socks4" , "https://dstat.one/proxies.php?id=4" } ,
	{ "socks4" , "https://proxyspace.pro/socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/B4RC0DE-TM/proxy-list/main/SOCKS4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/HyperBeats/proxy-list/main/socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/mmpx12/proxy-list/master/socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies_anonymous/socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/roosterkid/openproxylist/main/SOCKS4_RAW.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/saschazesiger/Free-Proxies/master/proxies/socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/socks4.txt" } ,
	{ "socks4" , "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks4.txt" },
	{ "socks4" , "https://raw.githubusercontent.com/TheSpeedX/SOCKS-List/master/socks4.txt" } ,
	{ "socks4" , "https://www.freeproxychecker.com/result/socks4_proxies.txt" } ,
	{ "socks4" , "https://www.my-proxy.com/free-socks-4-proxy.html" } ,
	{ "socks4" , "https://www.proxy-list.download/api/v1/get?type=socks4" },
	{ "socks4" , "https://www.proxyscan.io/download?type=socks4" } ,
	{ "socks5" , "http://worm.rip/socks5.txt" } ,
	{ "socks5" , "http://www.live-socks.net/feeds/posts/default" } ,
	{ "socks5" , "http://www.socks24.org/feeds/posts/default" } ,
	{ "socks5" , "https://api.openproxylist.xyz/socks5.txt" } ,
	{ "socks5" , "https://api.proxyscrape.com/?request=displayproxies&proxytype=socks5" } ,
	{ "socks5" , "https://api.proxyscrape.com/v2/?request=displayproxies&protocol=socks5" } ,
	{ "socks5" , "https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks5" } ,
	{ "socks5" , "https://api.proxyscrape.com/v2/?request=getproxies&protocol=socks5&timeout=10000&country=all&simplified=true" } ,
	{ "socks5" , "https://dstat.one/proxies.php?id=5" } ,
	{ "socks5" , "https://proxyspace.pro/socks5.txt" },
	{ "socks5" , "https://raw.githubusercontent.com/B4RC0DE-TM/proxy-list/main/SOCKS5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/hookzof/socks5_list/master/proxy.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/HyperBeats/proxy-list/main/socks5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/jetkai/proxy-list/main/online-proxies/txt/proxies-socks5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/manuGMG/proxy-365/main/SOCKS5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/mmpx12/proxy-list/master/socks5.txt" },
	{ "socks5" , "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies/socks5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/monosans/proxy-list/main/proxies_anonymous/socks5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/roosterkid/openproxylist/main/SOCKS5_RAW.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/saschazesiger/Free-Proxies/master/proxies/socks5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/socks5.txt" } ,
	{ "socks5" , "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/socks5.txt" },
	{ "socks5" , "https://www.freeproxychecker.com/result/socks5_proxies.txt" } ,
	{ "socks5" , "https://www.my-proxy.com/free-socks-5-proxy.html" } ,
	{ "socks5" , "https://www.proxy-list.download/api/v1/get?type=socks5" } ,
	{ "socks5" , "https://www.proxyscan.io/download?type=socks5" }

};

std::mutex ProxyScraperMutex;

struct ProxyScraperUnchecked_t {
	std::string protocol;
	std::string ip;
	std::string port;
};

std::deque< ProxyScraperUnchecked_t > vecProxyScrapedUnchecked = { };

void ProxyScraperThread( )
{
	int currScraperLinkIdx = 0;
	while ( !g_exit ) {
		std::this_thread::sleep_for( std::chrono::seconds( 5 ) );

		if ( currScraperLinkIdx > vecProxyScraperLinks.size( ) ) {
			currScraperLinkIdx = 0;
		}

		int retries = 0;

	retry_request:
		retries++;
		if ( retries > 5 ) {
			currScraperLinkIdx++;
			continue;
		}

		auto output = g_HttpRequests.Request( vecProxyScraperLinks [ currScraperLinkIdx ].link , "GET" , "" );
		if ( !output.success ) {
			std::this_thread::sleep_for( std::chrono::seconds( 3 ) );
			goto retry_request;
		}

		std::stringstream ss;
		ss << output.response;

		std::string line;
		while ( std::getline( ss , line ) ) {
			std::smatch m;

			std::regex regex_a( "([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}):([0-9]{1,5})" );
			std::string a = line;
			if ( std::regex_match( a , m , regex_a ) ) {
				std::string ip_port = a;
				if ( auto port_separator = ip_port.find_last_of( ":" ); port_separator != std::string::npos ) {
					std::string ip = ip_port.substr( 0 , port_separator );
					std::string port = ip_port.substr( port_separator + 1 );

					std::lock_guard< std::mutex > l( ProxyScraperMutex );
					ProxyScraperUnchecked_t proxy;
					proxy.protocol = vecProxyScraperLinks [ currScraperLinkIdx ].protocol;
					proxy.ip = ip;
					proxy.port = port;
					vecProxyScrapedUnchecked.push_back( proxy );
				}

			}
		}

		currScraperLinkIdx++;
	}
}

void ProxyScraperCheckerThread( )
{
	while ( !g_exit ) {
		std::this_thread::sleep_for( std::chrono::seconds( 3 ) );

		ProxyScraperMutex.lock( );
		if ( vecProxyScrapedUnchecked.empty( ) ) {
			ProxyScraperMutex.unlock( );
			continue;
		}

		ProxyScraperUnchecked_t proxyData = vecProxyScrapedUnchecked.front( );
		vecProxyScrapedUnchecked.pop_front( );
		ProxyScraperMutex.unlock( );

		CProxyData transformedProxyData = CProxyData( proxyData.protocol , proxyData.ip , proxyData.port );

		int retries = 0;

	retry_request:
		retries++;
		if ( retries > 5 ) {
			continue;
		}

		auto output = g_HttpRequests.Request( "https://api.ipify.org" , "GET" , "" , &transformedProxyData );

		// this is mostly because of our internet connection
		if ( output.error == "Timeout was reached" ) {
			std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
			goto retry_request;
		}

		if ( !output.success ) {
			std::this_thread::sleep_for( std::chrono::seconds( 3 ) );
			continue;
		}

		// todo check if ip (output.response) matches our own ip..

		g_ProxyHelper.AddProxyData( transformedProxyData );
	}
}

bool CProxyHelper::RunScraperAsync( )
{
	if ( !g_Config.proxies.scraper ) {
		return false;
	}

	if ( vecProxyScraperLinks.empty( ) ) {
		return false;
	}

	this->m_vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) ProxyScraperThread , NULL , 0 , NULL ) );
	this->m_vecThreads.push_back( CreateThread( NULL , NULL , ( LPTHREAD_START_ROUTINE ) ProxyScraperCheckerThread , NULL , 0 , NULL ) );

	return false;
}

void CProxyHelper::Quit( )
{
	for ( int i = 0; i < this->m_vecThreads.size( ); i++ ) {
		CloseHandle( this->m_vecThreads [ i ] );
	}

	this->m_vecThreads.clear( );
}

std::deque< CProxyData > CProxyHelper::GetProxiesData( )
{
	return this->m_vecProxyData;
}

CProxyData* CProxyHelper::GetProxyDataAtIndex( int nIndex )
{
	std::lock_guard< std::mutex > l( ProxyMutex );
	if ( nIndex < 0 || nIndex >= this->m_vecProxyData.size( ) )
		return nullptr;

	return &this->m_vecProxyData [ nIndex ];
}

CProxyData* CProxyHelper::GetLastRolledProxyData( )
{
	std::lock_guard< std::mutex > l( ProxyMutex );
	if ( this->m_iRollProxyIndex < 0 || this->m_iRollProxyIndex >= this->m_vecProxyData.size( ) )
		return nullptr;

	return &this->m_vecProxyData [ this->m_iRollProxyIndex ];
}

CProxyData* CProxyHelper::RollProxyData( )
{
	ProxyMutex.lock( );
	if ( this->m_vecProxyData.empty( ) ) {
		this->m_iRollProxyIndex = -1;
		ProxyMutex.unlock( );
		return nullptr;
	}

	this->m_iRollProxyIndex += 1;
	if ( this->m_iRollProxyIndex >= this->m_vecProxyData.size( ) ) {
		this->m_iRollProxyIndex = 0;
	}
	else if ( this->m_iRollProxyIndex < 0 ) {
		this->m_iRollProxyIndex = 0;
	}

	ProxyMutex.unlock( );
	return this->GetLastRolledProxyData( );
}

void CProxyHelper::AddProxyData( CProxyData proxy )
{
	std::lock_guard< std::mutex > l( ProxyMutex );
	if ( std::find_if( this->m_vecProxyData.begin( ) , this->m_vecProxyData.end( ) , [ & ] ( CProxyData it ) {
		return it.GetFullInfo( ) == proxy.GetFullInfo( );
		} ) != this->m_vecProxyData.end( ) ) {

		return;
	}

	this->m_vecProxyData.push_back( proxy );
}

int CProxyHelper::GetProxiesNum( )
{
	std::lock_guard< std::mutex > l( ProxyMutex );
	return this->m_vecProxyData.size( );
}
