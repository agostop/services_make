#include <curl.h>
#include <iostream>
#include <string>
#include <fstream>
#include <regex>
#include <time.h>

#pragma execution_character_set("utf-8")

using namespace std;

static	string _content;


/*size_t process_data (char *contents,size_t size,size_t nmemb,void *user_p)
{
    string tmp;
    size_t sum=size*nmemb;
    tmp.assign(contents,sum);
    _content.append(tmp);
    user_p=NULL;
    return sum;
}
*/
size_t process_data (char *contents,size_t size,size_t nmemb,char *user_p)
{
    strcat(user_p, contents);

    return size * nmemb;
}

int writelog(string &log_str,ofstream &file)
{

    if (file.fail())
        cerr << "file handle error" << endl;
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];

    time (&rawtime);
    timeinfo = localtime (&rawtime);

    strftime (buffer,80,"%Y-%m-%d %H:%M:%S",timeinfo);

    string cur_time;

    cur_time.assign(buffer,strlen(buffer));

    file <<  log_str << " at " << cur_time << endl;
    file.flush();

    return 0;
}


int go_curl()
{
    string post_value("username=%E8%83%8C%E5%90%8E%E6%B2%A1%E7%95%AA%E5%8F%B7&password=20461240&invatecode=");
    ofstream file;
    file.open("d:/app.log",ios::trunc);
    if (!file)
    {
        cerr << "can't open the test.html" << endl;
        return -1;
    }

    CURLcode return_code;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers,"Cache-Control: max-age=0");
    headers = curl_slist_append(headers,"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    headers = curl_slist_append(headers,"Accept-Encoding: gzip,deflate,sdch");
    headers = curl_slist_append(headers,"Accept-Language: en-US,en;q=0.8,zh-TW;q=0.6,zh;q=0.4,zh-CN;q=0.2");
    headers = curl_slist_append(headers,"User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/28.0.1500.72 Safari/537.36");
    headers = curl_slist_append(headers,"Connection: keep-alive");

    return_code=curl_global_init(CURL_GLOBAL_WIN32);

    if (CURLE_OK != return_code)
    {
        writelog(string("init libcurl failed."), file);
        return -1;
    }


    while (return_code==CURLE_OK)
    {
        CURL *easy_handle = curl_easy_init();
        if (NULL == easy_handle)
        {
            writelog(string("get a easy handle failed."), file);
            curl_global_cleanup();
            return -1;
        }

        char *pc_ret;
        pc_ret=new char[30000];

        string _content;

       // curl_easy_setopt(easy_handle, CURLOPT_COOKIE, "LeemZfsid=3DPM1ZdGuk; LeemZfchatroom=");
        curl_easy_setopt(easy_handle, CURLOPT_COOKIEJAR , "d:/file.cookie");
        curl_easy_setopt(easy_handle, CURLOPT_COOKIEFILE, "d:/file.cookie");
        curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION,&process_data);
        curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, pc_ret);


        _content.clear();
        memset(pc_ret,0,strlen(pc_ret));

        curl_easy_setopt(easy_handle,CURLOPT_URL,"http://www.fuyuncun.com/login.asp?action=login");
        curl_easy_setopt(easy_handle, CURLOPT_POST, 1);
        curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDS, post_value.c_str());
        return_code = curl_easy_perform(easy_handle);

        curl_easy_setopt(easy_handle,CURLOPT_POST,0);
        curl_easy_setopt(easy_handle,CURLOPT_URL,"http://www.fuyuncun.com/index.asp");
        return_code = curl_easy_perform(easy_handle);

        curl_easy_setopt(easy_handle,CURLOPT_URL,"http://www.fuyuncun.com/forumdisplay.asp?fid=1");
        return_code = curl_easy_perform(easy_handle);

        if (pc_ret!=NULL)
            _content.assign(pc_ret);

        regex re("\\s+(MP:\\d+).*");
        smatch mat;
        regex_search(_content,mat,re);

        writelog(mat[1].str(),file);
       // cout << mat[1].str() << endl;

        delete []pc_ret;

        curl_easy_cleanup(easy_handle);

        Sleep(1820*1000);
    }

    file.clear();
    file.close();
    curl_slist_free_all(headers);
    curl_global_cleanup();

    return 0;
}
