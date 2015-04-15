#ifndef GO_CURL_H
#define GO_CURL_H



#endif // GO_CURL_H

#include <curl.h>
#include <iostream>
#include <string>
#include <fstream>
#include <regex>
#include <time.h>

#pragma execution_character_set("utf-8")

using namespace std;

static	string _content;


size_t process_data (char *contents,size_t size,size_t nmemb,void *user_p);


int writelog(string &log_str,ofstream &file);

int go_curl();
