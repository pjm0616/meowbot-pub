/*
	meowbot
	Copyright (C) 2008-2009 Park Jeong Min <pjm0616_at_gmail_d0t_com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>

#include <pcrecpp.h>
#if 0
#include <curl/curl.h>
#endif


#include "defs.h"
#include "utf8.h"
#include "tcpsock.h"
#include "my_vsprintf.h"
#include "etc.h"



unsigned int get_ticks(void)
{
	struct timeval now;
	static struct timeval start = {0, 0};
	unsigned int ticks;

	gettimeofday(&now, NULL);
	if(start.tv_sec == 0 && start.tv_usec == 0)
	{
		start.tv_sec = now.tv_sec;
		start.tv_usec = now.tv_sec;
	}
	ticks = (now.tv_sec - start.tv_sec) * 1000 + 
		(now.tv_usec - start.tv_usec) / 1000;

	return ticks;
}

/////////////////////////////////////////// RANDOM NUMBER GENERATOR STARTS

static pthread_mutex_t g_rand_mutex = PTHREAD_MUTEX_INITIALIZER;
#if defined(HAVE_RAND48) && defined(HAVE_TLS___THREAD)
static __thread struct drand48_data g_drand46_data;
#endif

long int ts_rand()
{
	pthread_mutex_lock(&g_rand_mutex);
	long int result = (long int)rand()%RAND_MAX;
	pthread_mutex_unlock(&g_rand_mutex);
	return result;
}


void my_srand(long int seed)
{
#if defined(HAVE_RAND48) && defined(HAVE_TLS___THREAD)
	srand48_r(seed, &g_drand46_data);
#else
	pthread_mutex_lock(&g_rand_mutex);
	srand(seed);
	pthread_mutex_unlock(&g_rand_mutex);
#endif
}

long int my_rand()
{
#if defined(HAVE_RAND48) && defined(HAVE_TLS___THREAD)
	if(unlikely(*(unsigned int *)&g_drand46_data == 0)) // check if initialized
		my_srand(ts_rand());
	
	long int result;
	lrand48_r(&g_drand46_data, &result);
	return result;
#else
	return ts_rand();
#endif
}

/////////////////////////////////////////// RANDOM NUMBER GENERATOR ENDS

std::string int2string(int n)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", n);
	
	return buf;
}

std::string gettimestr(void)
{
	char buf[64];
	time_t ct;
	struct tm ctc;
	
	ct = time(NULL);
	localtime_r(&ct, &ctc);
	if(strftime(buf, sizeof(buf), "%Y년 %m월 %d일 %H시 %M분 %S초", &ctc) > 0)
	{
		return buf;
	}
	else
	{
		return "";
	}
}

std::string gettimestr_simple(void)
{
	char buf[64];
	time_t ct;
	struct tm ctc;
	
	ct = time(NULL);
	localtime_r(&ct, &ctc);
	if(strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &ctc) > 0)
	{
		return buf;
	}
	else
	{
		return "";
	}
}

// 0x, 0b 만 지원, 0nnnn은  10진수로 처리
int my_atoi(const char *str)
{
	int base;
	int val;
	char nextchar;
	bool isnegative;
	
	base = 10;
	val = 0;
	isnegative = false;
	
	if(str[0] == '-') /* negative */
	{
		isnegative = true;
		str++; /* skip '-' */
	}
	else if(str[0] == '+') /* positive */
	{
		str++; /* skip '+' */
	}
	if((str[0] == '0') && (str[1] == 'x')) /* hex */
	{
		base = 16;
		str += 2; /* skip '0x' */
	}
	else if(((str[0] >= 'a') && (str[0] <= 'f')) || 
		((str[0] >= 'A') && (str[0] <= 'F'))) /* hex */
	{
		base = 16;
	}
	else if((str[0] == '0') && (str[1] == 'b')) /* binary */
	{
		base = 1;
		str += 2; /* skip '0b' */
	}
	while(*str)
	{
		nextchar = *str;
		if(base <= 10)
		{
			if((nextchar >= '0') && (nextchar < (base + '0')))
			{
				val *= base;
				val += nextchar - '0';
			}
			else
			{
				break;
			}
		}
		else if (base <= 16)
		{
			if ((nextchar >= '0') && (nextchar <= '9'))
			{
				val *= base;
				val += nextchar - '0';
			}
			else if ((nextchar >= 'A') && (nextchar <= 'Z'))
			{
				val *= base;
				val += nextchar-'A' + 10;
			}
			else if ((nextchar >= 'a') && (nextchar <= 'z'))
			{
				val *= base;
				val += nextchar - 'a' + 10;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
		str++;
	}
	if(isnegative)
	{
		val = -val;
	}
	
	return val;
}

int my_atoi_base(const char *str, int base)
{
	int val;
	char nextchar;
	bool isnegative;
	
	val = 0;
	isnegative = 0;
	
	if(str[0] == '-') /* negative */
	{
		isnegative = 1;
		str++; /* skip '-' */
	}
	else if(str[0] == '+') /* positive */
	{
		str++; /* skip '+' */
	}
	while(*str)
	{
		nextchar = *str;
		if(base <= 10)
		{
			if((nextchar >= '0') && (nextchar < (base + '0')))
			{
				val *= base;
				val += nextchar - '0';
			}
			else
			{
				break;
			}
		}
		else if (base <= 16)
		{
			if ((nextchar >= '0') && (nextchar <= '9'))
			{
				val *= base;
				val += nextchar - '0';
			}
			else if ((nextchar >= 'A') && (nextchar <= 'Z'))
			{
				val *= base;
				val += nextchar-'A' + 10;
			}
			else if ((nextchar >= 'a') && (nextchar <= 'z'))
			{
				val *= base;
				val += nextchar - 'a' + 10;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
		str++;
	}
	if(isnegative)
	{
		val = -val;
	}
	
	return val;
}

int urlencode(const char *str, char *dst, int size)
{
	int dlen;
	const unsigned char *sp;
	unsigned char *dp;
	
#define CHECK_OVERFLOW \
	if(dlen >= size) \
	{ \
		dlen = -1; \
		break; \
	}
#define RANGE(min, max, val) ((val>=min) && (val<=max))
	
	dp = (unsigned char *)dst;
	dlen = 0;
	for(sp=(const unsigned char *)str; *sp != 0; sp++)
	{
		if(RANGE('0', '9', *sp) || RANGE('A', 'Z', *sp) || \
			RANGE('a', 'z', *sp) || (*sp=='-') || \
			(*sp=='_') || (*sp=='.') || (*sp=='*'))
		{
			dlen++;
			CHECK_OVERFLOW
			*dp++ = *sp;
		}
		else
		{
			dlen += 3;
			CHECK_OVERFLOW
			snprintf((char *)dp, 4, "%%%02x", *sp);
			dp += 3;
		}
	}
	*dp = 0;
	
#undef RANGE
#undef CHECK_OVERFLOW
	
	return dlen;
}

std::string urlencode(const std::string &str)
{
	unsigned int bufsize = str.length() * 4 + 1;
	char *buf = new char[bufsize];
	std::string result;
	
	if(urlencode(str.c_str(), buf, bufsize) > 0)
	{
		result = buf;
	}
	else
	{
		result = "";
	}
	delete[] buf;
	
	return result;
}

int urldecode(const char *str, char *dst, int size)
{
	int dlen;
	const unsigned char *sp;
	unsigned char buf[16], *dp;
	
#define CHECK_OVERFLOW \
	if(dlen >= size) \
	{ \
		dlen = -1; \
		break; \
	}
	
	dp = (unsigned char *)dst;
	dlen = 0;
	for(sp=(const unsigned char *)str; *sp != 0; sp++)
	{
		if(*sp=='%')
		{
			dlen++;
			CHECK_OVERFLOW
			buf[0]='0';
			buf[1]='x';
			buf[2]=*++sp;
			buf[3]=*++sp;
			buf[4]=0;
			*dp++ = (char)my_atoi((char *)buf);
		}
		else
		{
			dlen++;
			CHECK_OVERFLOW
			*dp++ = *sp;
		}
	}
	*dp = 0;
	
	return dlen;
}

std::string urldecode(const std::string &str)
{
	unsigned int bufsize = str.length() + 1;
	char *buf = new char[bufsize];
	std::string result;
	
	if(urldecode(str.c_str(), buf, bufsize) > 0)
	{
		result = buf;
	}
	else
	{
		result = "";
	}
	delete[] buf;
	
	return result;
}

std::string decode_html_entities(const std::string &str)
{
	static const char *entitylist[][2] = 
	{
		{"quot", "\""}, {"apos", "'"}, {"amp", "&"}, {"lt", "<"}, 
		{"gt", ">"}, {"nbsp", " "}, {"iexcl", "¡"}, {"cent", "¢"}, 
		{"pound", "£"}, {"curren", "¤"}, {"yen", "¥"}, {"brvbar", "¦"}, 
		{"sect", "§"}, {"uml", "¨"}, {"copy", "©"}, {"ordf", "ª"}, 
		{"laquo", "«"}, {"not", "¬"}, {"shy", "­ "}, {"reg", "®"}, 
		{"macr", "¯"}, {"deg", "°"}, {"plusmn", "±"}, {"sup2", "²"}, 
		{"sup3", "³"}, {"acute", "´"}, {"micro", "µ"}, {"para", "¶"}, 
		{"middot", "·"}, {"cedil", "¸"}, {"sup1", "¹"}, {"ordm", "º"}, 
		{"raquo", "»"}, {"frac14", "¼"}, {"frac12", "½"}, {"frac34", "¾"}, 
		{"iquest", "¿"}, {"times", "×"}, {"divide", "÷"}, {NULL, NULL}
	};
	
	const char *p = str.c_str();
	std::string result;
	std::string ename;
	int mode = 0;
	
	for(; *p; p++)
	{
		if(mode == 0) // find '&'
		{
			if(*p == '&')
			{
				ename.clear();
				mode = 1;
			}
			else
				result.append(p, 1);
		}
		else if(mode == 1) // collect entity name
		{
			if(*p == ';') // 끝
			{
				if(ename.empty()) // 비어있을 경우
					result.append("&" + ename + ";");
				else
				{
					if(ename[0] == '#') // &#dd; &#xxxx;
					{
						// '#'를 0으로 치환 -> 0nnnn
						// 10진수, 0xnnnn - my_atoi에서 16진수로 인식됨
						ename[0] = '0';
						int code = my_atoi(ename.c_str());
						char buf[8];
						int cl = utf8_encode(code, buf, sizeof(buf));
						if(cl <= 0) // 인코딩 실패
						{
							ename[0] = '#'; // 다시 복구
							result.append("&" + ename + ";");
						}
						else
						{
							buf[cl] = 0;
							result.append(buf);
						}
					} /* if(ename[0] == '#') */
					else
					{
						int found = 0;
						for(int j = 0; entitylist[j][0]; j++)
						{
							if(ename == entitylist[j][0])
							{
								result += entitylist[j][1];
								found = 1;
								break;
							}
						}
						if(found == 0) // 목록에 없을 경우
							result.append("&" + ename + ";");
					} /* if(ename[0] == '#') else */
				} /* if(ename.empty()) else */
				mode = 0;
			} /* if(*p == ';') */
			else
				ename.append(p, 1);
		}
	}
	if(mode != 0) // 중간에 짤렸을 경우
		result.append(ename);
	
	return result;
}

std::string prevent_irc_highlighting_1(const std::string &nick)
{
	std::string b1, b2;
	
	b1 = utf8_strpos_s(nick, 0);
	b2 = utf8_substr_s(nick, 1);
	if(b2.empty())
		return nick;
	else
		return b1 + "·" + b2;
}

static bool prevent_irc_highlighting_replace_cb(std::string &dest, 
	const std::vector<std::string> &groups, void *)
{
	dest += prevent_irc_highlighting_1(groups[0]);
	return true;
}
std::string prevent_irc_highlighting(const std::string &word, std::string msg)
{
	static pcrecpp::RE_Options re_options(/*PCRE_UTF8 | */PCRE_CASELESS);
	pcrecpp::RE("(" + pcrecpp::RE::QuoteMeta(word) + ")", re_options)
		.GlobalReplaceCallback(&msg, prevent_irc_highlighting_replace_cb);
	
	return msg;
}

std::string prevent_irc_highlighting_all(const std::vector<std::string> &list, 
	std::string msg)
{
	std::vector<std::string>::const_iterator it;
	
	for(it = list.begin(); it != list.end(); it++)
	{
		msg = prevent_irc_highlighting(*it, msg);
	}
	
	return 0;
}

int pcre_csplit(const pcrecpp::RE &re_delimeter, const std::string &data, 
	std::vector<std::string> &dest)
{
	pcrecpp::StringPiece input(data);
	std::string buf;
	
	dest.clear();
	while(re_delimeter.Consume(&input, &buf))
	{
		dest.push_back(buf);
	}
	buf = input.as_string();
	if(!buf.empty())
		dest.push_back(buf);
	if(dest.empty())
		dest.push_back("");
	
	return 0;
}

int pcre_split(const std::string &delimeter, const std::string &data, 
	std::vector<std::string> &dest)
{
	static const pcrecpp::RE_Options re_opts(PCRE_UTF8);
	
	return pcre_csplit(pcrecpp::RE("(.*?)" + delimeter, re_opts), data, dest);
}

char *readfile(const char *path, int *readsize /*=NULL*/)
{
	int ret;
	struct stat sb;
	
	ret = stat(path, &sb);
	if(ret < 0)
		return NULL;
	if(sb.st_size >= INT_MAX)
		return NULL;
	int size = sb.st_size;
	
	int fd = open(path, O_RDONLY);
	if(fd < 0)
		return NULL;
	const int chunk_size = 1024;
	char *buf = new char[size + chunk_size];
	int nread, pos = 0;
	while((nread = read(fd, &buf[pos], chunk_size)) > 0)
	{
		if(pos >= size)
			break;
		pos += nread;
	}
	if(readsize)
		*readsize = pos;
	close(fd);
	
	return buf;
}

int readfile(const char *path, std::string &dest)
{
	int ret;
	struct stat sb;
	
	ret = stat(path, &sb);
	if(ret < 0)
		return -1;
	if(sb.st_size >= INT_MAX)
		return -1;
	int size = sb.st_size;
	
	int fd = open(path, O_RDONLY);
	if(fd < 0)
		return -1;
	
	dest.clear();
	int nread, pos = 0;
	char buf[1024];
	while((nread = read(fd, buf, sizeof(buf))) > 0)
	{
		dest.append(buf, nread);
		if(pos >= size)
			break;
		pos += nread;
	}
	close(fd);
	
	return pos;
}

int s_vsprintf(std::string *dest, const char *format, va_list &ap)
{
	char buf[1024];
	int len;
	
	assert(dest != NULL);
	len = my_vsnprintf(buf, sizeof(buf), format, ap);
	assert(len >= 0);
	if((size_t)len >= sizeof(buf))
	{
		char *buf2 = new char[len + 1];
		len = my_vsnprintf(buf2, len + 1, format, ap);
		dest->assign(buf2, len);
		delete[] buf2;
	}
	else
		dest->assign(buf, len);
	
	return len;
}

std::string s_vsprintf(const char *format, va_list &ap)
{
	std::string result;
	s_vsprintf(&result, format, ap);
	return result;
}

std::string strip_irc_colors(const std::string &msg)
{
	static pcrecpp::RE re_strip_irc_colors
		(
			"\x03[0-9]{0,2}+,?+[0-9]{0,2}+|" // 색글
			"[\x02\x1f\x0f]" // 볼드, 이탤릭, 노멀
		);
	std::string stripped_msg(msg);
	re_strip_irc_colors.GlobalReplace("", &stripped_msg);
	
	return stripped_msg;
}

std::string strip_spaces(const std::string &str)
{
	std::string result;
	const char *p;
	
	for(p = str.c_str(); *p; p++)
	{
		if(*p == ' ' || *p == '\t')
			continue;
		result.append(p, 1);
	}
	
	return result;
}

bool is_bot_command(const std::string &str)
{
	static pcrecpp::RE re_is_bot_cmd(
		"^(?:"
			"\x03[0-9]{0,2}+,?+[0-9]{0,2}+|" // 색글
			"[ \t\x02\x1f\x0f]" // 볼드, 이탤릭, 노멀, 공백
		")*?"
		"[!@][^ ]+"
		);
	return re_is_bot_cmd.PartialMatch(str);
}



url_parser::url_parser(const std::string &url)
{
	static pcrecpp::RE re_url("^([^:]+)://([^:/]+):?([0-9]+)?(/.*)");
	std::string port;
	
	if(!re_url.FullMatch(url, &this->protocol, &this->host, &port, &this->location))
	{
		this->is_valid_url = false;
		return;
	}
	
	if(port.empty())
	{
		if(this->protocol == "http")
			this->port = 80;
		else if(this->protocol == "https")
			this->port = 443;
		else if(this->protocol == "ftp")
			this->port = 21;
		else if(this->protocol == "telnet")
			this->port = 23;
		else if(this->protocol == "ssh")
			this->port = 22;
		else if(this->protocol == "irc")
			this->port = 6667;
		else
			this->port = 0;
	}
	else
		this->port = atoi(port.c_str());
	this->is_valid_url = true;
	
	return;
}







#if 0
static size_t curl_write_function(void *buffer, size_t size, size_t nmemb, 
	void *userp)
{
	size_t realsize = size * nmemb;
	std::string *dest = static_cast<std::string *>(userp);
	
	dest->append(static_cast<const char *>(buffer), realsize);
	
	return realsize;
}

int curl_get_url(const std::string &url, std::string &dest)
{
	int ret;
	CURL *curl;
	
	curl = curl_easy_init();
	if(!curl)
	{
		return -1;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dest);
	curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10);
	curl_easy_setopt(curl, CURLOPT_REFERER, "");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; U; "
		"Linux i686; ko-KR; rv:1.9.0.1) Gecko/2008072820 Firefox/3.0.1");
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	
	dest = "";
	ret = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if(ret)
	{
		dest = std::string("Error: ") + curl_easy_strerror((CURLcode)ret);
		return -1;
	}
	
	return 0;
}

std::string curl_get_url(const std::string &url)
{
	std::string dest;
	curl_get_url(url, dest);
	return dest;
}
#endif




// vim: set tabstop=4 textwidth=80:

