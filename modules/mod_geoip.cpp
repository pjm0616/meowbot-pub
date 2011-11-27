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
#include <algorithm>
#include <map>
#include <deque>
#include <sstream>

#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netdb.h> /* getprotobyname */
#include <netinet/in.h> //
#include <arpa/inet.h> //
#include <dlfcn.h>

#include <pcrecpp.h>
#include <boost/algorithm/string/replace.hpp>

#include "defs.h"
#include "module.h"
#include "tcpsock.h"
#include "etc.h"
#include "utf8.h"
#include "ircbot.h"


#define ENABLE_GEOIP



static std::string get_ipaddr_by_nick(ircbot_conn *isrv, const std::string &chan, const std::string &name)
{
	static pcrecpp::RE re_hidden_addr("[^.]+\\.users\\.HanIRC\\.org");
	
	if(unlikely(name.empty()))
		return "E;empty";
	
	isrv->rdlock_chanuserlist();
	const irc_strmap<std::string, irc_user> &users = isrv->get_chanuserlist(chan);
	irc_strmap<std::string, irc_user>::const_iterator it = users.find(name);
	
	std::string host(name);
	if(it != users.end())
	{
		host = it->second.gethost();
		if(re_hidden_addr.FullMatch(host))
		{
			isrv->unlock_chanuserlist();
			return "E;hidden";
		}
	}
	isrv->unlock_chanuserlist();
	
	return host;
	// return my_gethostbyname_s(host.c_str());
}


#ifdef ENABLE_GEOIP
//#include <GeoIP.h>
struct GeoIP;
typedef enum {
	GEOIP_STANDARD = 0,
	GEOIP_MEMORY_CACHE = 1,
	GEOIP_CHECK_CACHE = 2,
	GEOIP_INDEX_CACHE = 4,
	GEOIP_MMAP_CACHE = 8,
} GeoIPOptions;

static void *g_libgeoip_handle = NULL;
static GeoIP *g_geoip = NULL;

static GeoIP *(*GeoIP_new)(int flags) = NULL;
static void (*GeoIP_delete)(GeoIP *gi) = NULL;
static int (*GeoIP_id_by_name)(GeoIP *gi, const char *name) = NULL;
static const char (*GeoIP_country_code)[3] = NULL;
static const char **GeoIP_country_name = NULL;


static bool init_geoip()
{
	g_libgeoip_handle = dlopen("/usr/lib/libGeoIP.so.1", RTLD_NOW | RTLD_LOCAL);
	if(!g_libgeoip_handle)
		return false;
	GeoIP_new = (GeoIP *(*)(int)) dlsym(g_libgeoip_handle, "GeoIP_new");
	GeoIP_delete = (void (*)(GeoIP *)) dlsym(g_libgeoip_handle, "GeoIP_delete");
	GeoIP_id_by_name = (int (*)(GeoIP *, const char *)) dlsym(g_libgeoip_handle, "GeoIP_id_by_name");
	GeoIP_country_code = (const char (*)[3]) dlsym(g_libgeoip_handle, "GeoIP_country_code");
	GeoIP_country_name = (const char **) dlsym(g_libgeoip_handle, "GeoIP_country_name");
	if(!GeoIP_new || !GeoIP_delete || !GeoIP_id_by_name || !GeoIP_country_code || !GeoIP_country_name)
	{
		dlclose(g_libgeoip_handle);
		return false;
	}
	g_geoip = GeoIP_new(GEOIP_STANDARD);
	if(!g_geoip)
	{
		dlclose(g_libgeoip_handle);
		return false;
	}
	
	return true;
}
static bool delete_geoip()
{
	if(!g_libgeoip_handle)
		return false;
	
	GeoIP_delete(g_geoip);
	g_geoip = NULL;
	dlclose(g_libgeoip_handle);
	g_libgeoip_handle = NULL;
	
	return true;
}

static int cmd_cb_geoip(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	if(carg.empty())
	{
		isrv->privmsg(mdest, "어느 나라의 IP인지 알려줍니다. | 사용법: !" + cmd + " <IP주소|도메인|닉네임>");
		return 0;
	}
	if(unlikely(g_geoip == NULL))
	{
		isrv->privmsg(mdest, "내부 오류(초기화 실패)");
		return 0;
	}
	
	std::string nick(carg);
	boost::algorithm::replace_all(nick, " ", "");
	const std::string &host = get_ipaddr_by_nick(isrv, pmsg.getchan(), nick);
	if(host.empty())
	{
		isrv->privmsg(mdest, "찾을 수 없습니다.");
	}
	else if(host == "E;hidden")
	{
		isrv->privmsg(mdest, "해당 사용자는 IP를 숨기고 있습니다.");
	}
	else
	{
		int country_id = GeoIP_id_by_name(g_geoip, host.c_str());
		if(country_id < 0 || country_id > 252)
		{
			isrv->privmsg(mdest, "내부 오류");
		}
		isrv->privmsgf(mdest, "%s: %s [%s]", host.c_str(), 
			GeoIP_country_code[country_id], GeoIP_country_name[country_id]);
	}
	
	return 0;
}
#endif



int mod_geoip_init(ircbot *bot)
{
#ifdef ENABLE_GEOIP
	init_geoip();
	bot->register_cmd("geoip", cmd_cb_geoip);
	bot->register_cmd("IP위치", cmd_cb_geoip);
#endif
	
	return 0;
}

int mod_geoip_cleanup(ircbot *bot)
{
#ifdef ENABLE_GEOIP
	delete_geoip();
	bot->unregister_cmd("geoip");
	bot->unregister_cmd("IP위치");
#endif
	
	return 0;
}



MODULE_INFO(mod_geoip, mod_geoip_init, mod_geoip_cleanup)




