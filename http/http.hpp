#pragma once
#include <deque>
#include <string>
#include <mutex>
#include "../proxy/proxy.hpp"

struct HttpRequestOutput_t {
	bool success = false;
	std::string error = "";
	std::string response = "";
};

class CHttpRequests {
private:
	std::deque< std::string > m_vecHeaders = { };
	mutable std::mutex m_Mutex;

public:

	std::deque< std::string > GetHeaders( );
	void SetHeaders( std::deque< std::string > vecHeaders );
	void AppendHeader( std::string szHeader );
	void ResetHeaders( );

	HttpRequestOutput_t Request( std::string url , std::string request_type = "GET" , std::string post_fields = "" , CProxyData* proxy = nullptr );
	HttpRequestOutput_t RequestEx( std::string url , std::string request_type = "GET" , std::string post_fields = "" , std::deque< std::string > vecHeaders = { } , CProxyData* proxy = nullptr );
};

inline CHttpRequests g_HttpRequests { };