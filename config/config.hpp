#pragma once
#include <deque>
#include <string>

class CConfig {
public:
	bool Init( );

public:
	int threads;
	struct {
		int min_members;
		int min_members_online;
		int max_members;
		std::deque< std::string > whitelist_tokens;
	} invite_checker;

	struct {
		bool scraper;
		bool from_file;
	} proxies;
};

inline CConfig g_Config { };