#include "http.hpp"
#include <curl/curl.h>

std::deque< std::string > CHttpRequests::GetHeaders( )
{
	this->m_Mutex.lock( );
	std::deque< std::string > vecCopy = this->m_vecHeaders;
	this->m_Mutex.unlock( );

	return vecCopy;
}

void CHttpRequests::SetHeaders( std::deque< std::string > vecHeaders )
{
	this->m_Mutex.lock( );
	this->m_vecHeaders = vecHeaders;
	this->m_Mutex.unlock( );
}

void CHttpRequests::AppendHeader( std::string szHeader )
{
	this->m_Mutex.lock( );
	this->m_vecHeaders.push_back( szHeader );
	this->m_Mutex.unlock( );
}

void CHttpRequests::ResetHeaders( )
{
	this->SetHeaders( { } );
}

static size_t WriteStringCallback( void* contents , size_t size , size_t nmemb , void* userp ) {
	( ( std::string* ) userp )->append( ( char* ) contents , size * nmemb );
	return size * nmemb;
}

HttpRequestOutput_t CHttpRequests::Request( std::string url , std::string request_type , std::string post_fields , CProxyData* proxy )
{
	HttpRequestOutput_t output { };

	std::deque< std::string > vecHeaders = this->GetHeaders( );

	this->ResetHeaders( );

	CURL* curl = curl_easy_init( );
	if ( !curl ) {
		output.success = false;
		output.response = output.error = "curl_easy_init failed";
		return output;
	}

	struct curl_slist* headers = NULL;
	if ( vecHeaders.size( ) ) {
		for ( auto& it : vecHeaders ) {
			headers = curl_slist_append( headers , it.data( ) );
		}

		curl_easy_setopt( curl , CURLOPT_HTTPHEADER , headers );
	}
	
	curl_easy_setopt( curl , CURLOPT_HTTP_VERSION , CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE );

	curl_easy_setopt( curl , CURLOPT_CUSTOMREQUEST , request_type.data( ) );
	curl_easy_setopt( curl , CURLOPT_FOLLOWLOCATION , 1L );
	curl_easy_setopt( curl , CURLOPT_URL , url.data( ) );

	//if ( request_type == "POST" ) {
	curl_easy_setopt( curl , CURLOPT_POSTFIELDS , post_fields.data( ) );
	//}

	if ( proxy != nullptr ) {
		curl_easy_setopt( curl , CURLOPT_PROXY , proxy->GetFullInfo( ).data( ) );
		if ( proxy->authentication_required ) {
			curl_easy_setopt( curl , CURLOPT_PROXYUSERNAME , proxy->username.data( ) );
			curl_easy_setopt( curl , CURLOPT_PROXYPASSWORD , proxy->password.data( ) );
		}
	}

	curl_easy_setopt( curl , CURLOPT_WRITEFUNCTION , WriteStringCallback );
	curl_easy_setopt( curl , CURLOPT_WRITEDATA , &output.response );

	CURLcode res = curl_easy_perform( curl );
	output.success = res == CURLE_OK;
	if ( !output.success ) {
		output.error = curl_easy_strerror( res );
	}

	if ( vecHeaders.size( ) ) {
		curl_slist_free_all( headers );
	}

	curl_easy_cleanup( curl );

	return output;
}

HttpRequestOutput_t CHttpRequests::RequestEx( std::string url , std::string request_type , std::string post_fields , std::deque< std::string > vecHeaders , CProxyData* proxy )
{
	HttpRequestOutput_t output { };

	CURL* curl = curl_easy_init( );
	if ( !curl ) {
		output.success = false;
		output.response = output.error = "curl_easy_init failed";
		return output;
	}

	struct curl_slist* headers = NULL;
	if ( vecHeaders.size( ) ) {
		for ( auto& it : vecHeaders ) {
			headers = curl_slist_append( headers , it.data( ) );
		}

		curl_easy_setopt( curl , CURLOPT_HTTPHEADER , headers );
	}

	curl_easy_setopt( curl , CURLOPT_HTTP_VERSION , CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE );

	curl_easy_setopt( curl , CURLOPT_CUSTOMREQUEST , request_type.data( ) );
	curl_easy_setopt( curl , CURLOPT_FOLLOWLOCATION , 1L );
	curl_easy_setopt( curl , CURLOPT_URL , url.data( ) );

	//if ( request_type == "POST" ) {
	curl_easy_setopt( curl , CURLOPT_POSTFIELDS , post_fields.data( ) );
	//}

	if ( proxy != nullptr ) {
		curl_easy_setopt( curl , CURLOPT_PROXY , proxy->GetFullInfo( ).data( ) );
		if ( proxy->authentication_required ) {
			curl_easy_setopt( curl , CURLOPT_PROXYUSERNAME , proxy->username.data( ) );
			curl_easy_setopt( curl , CURLOPT_PROXYPASSWORD , proxy->password.data( ) );
		}
	}

	curl_easy_setopt( curl , CURLOPT_WRITEFUNCTION , WriteStringCallback );
	curl_easy_setopt( curl , CURLOPT_WRITEDATA , &output.response );

	CURLcode res = curl_easy_perform( curl );
	output.success = res == CURLE_OK;
	if ( !output.success ) {
		output.error = curl_easy_strerror( res );
	}

	if ( vecHeaders.size( ) ) {
		curl_slist_free_all( headers );
	}

	curl_easy_cleanup( curl );

	return output;
}
