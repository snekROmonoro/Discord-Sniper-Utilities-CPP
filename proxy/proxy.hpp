#pragma once
#include <Windows.h>
#include <atomic>
#include <deque>
#include <string>

class CProxyData {
public:
	CProxyData( std::string _protocol ,
		std::string _address ,
		std::string _port ) {
		this->protocol = _protocol;
		this->address = _address;
		this->port = _port;
		this->authentication_required = false;
		this->username = "";
		this->password = "";
	}

	CProxyData( std::string _protocol ,
		std::string _address ,
		std::string _port ,
		std::string _username ,
		std::string _password ) {
		this->protocol = _protocol;
		this->address = _address;
		this->port = _port;
		this->authentication_required = true;
		this->username = _username;
		this->password = _password;
	}

	std::string protocol = "";
	std::string address = "";
	std::string port = "";
	bool authentication_required = true;
	std::string username = "";
	std::string password = "";

	// protocol://address:port
	std::string GetFullInfo( ) {
		return this->protocol + "://" + this->address + ":" + this->port;
	}
};

class CProxyHelper {
private:
	std::deque< HANDLE > m_vecThreads = { };
	std::deque< CProxyData > m_vecProxyData = { };
	std::atomic_int m_iRollProxyIndex = -1;

public:
	bool Init( );
	bool RunScraperAsync( );
	void Quit( );

	std::deque< CProxyData > GetProxiesData( );
	CProxyData* GetProxyDataAtIndex( int nIndex );
	CProxyData* GetLastRolledProxyData( );
	CProxyData* RollProxyData( );
	void AddProxyData( CProxyData proxy );
	int GetProxiesNum( );
};

inline CProxyHelper g_ProxyHelper { };