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
#include <deque>

#include <pthread.h>
#include <pcrecpp.h>

#include "defs.h"
#include "module.h"
#include "tcpsock.h"
#include "etc.h"
#include "ircbot.h"



enum
{
	EXTCMDTYPE_NONE, 
	EXTCMDTYPE_EXEC, 
	EXTCMDTYPE_HTTP, 
	EXTCMDTYPE_END
};

struct extcmd_list
{
	int type;
	const char *cmdname, *uri, *encoding;
};

extcmd_list extcmds[] = 
{
	{EXTCMDTYPE_EXEC, "뒤집기", "reverse.php", ""}, 
	{EXTCMDTYPE_EXEC, "촛엉", "ChoseongTrifler.php", ""}, 
	{EXTCMDTYPE_EXEC, "종올", "JongseongTrifler.php", ""}, 
	
	{EXTCMDTYPE_HTTP, "강화", "http://gksrnr.awardspace.com/inklbot/enchant.php?item=%s", "cp949"}, 
	{EXTCMDTYPE_NONE, NULL, NULL, NULL}
};

// 				명령어			쿠키
static std::map<std::string, std::string> cookie_list;
static pthread_rwlock_t g_cookie_lock;


static int cmd_cb_extcmd(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	const std::string &chan = pmsg.getchan();
	const std::string &nick = pmsg.getnick();
	std::string ecmd, ecarg, buf;
	tcpsock ts;
	int i, ret;
	
	if(cmd[0] == '!')
	{
		ecmd = cmd.substr(1);
		ecarg = carg;
	}
	else
	{
		ecmd = cmd;
		ecarg = carg;
	}
	
	for(i = 0; extcmds[i].type != EXTCMDTYPE_NONE; i++)
	{
		if(extcmds[i].cmdname == ecmd)
		{
			if(extcmds[i].type == EXTCMDTYPE_HTTP)
			{
				url_parser url(extcmds[i].uri);
				if(!url.is_valid() || url.get_protocol() != "http")
					break;
				
				if(extcmds[i].encoding)
					ts.set_encoding(extcmds[i].encoding);
				ret = ts.connect(url.get_host().c_str(), url.get_port(), 4000);
				if(ret)
				{
					isrv->privmsg(mdest, "접속 실패");
					
					return 0;
				}
				std::string location = url.get_location();
				if(extcmds[i].encoding)
					if(extcmds[i].encoding[0])
						ecarg = encconv::convert("utf-8", extcmds[i].encoding, ecarg);
				pcrecpp::RE("%s").GlobalReplace(urlencode(ecarg), &location);
				// HTTP/1.0 - disables chunked encoding
				ts.send("GET " + location + " HTTP/1.0\r\n");
				ts.send("Host: " + url.get_host() + "\r\n");
				ts.send("Accept-Encoding: *;q=0\r\n");
				pthread_rwlock_rdlock(&g_cookie_lock);
				ts.send("Cookie: nick=" + nick + "; " + cookie_list[ecmd] + "\r\n");
				pthread_rwlock_unlock(&g_cookie_lock);
				ts.send("Connection: close\r\n\r\n");
				
				// skip http header
				std::string cookie;
				while((ret = ts.readline(buf, 3500)) > 0) // break on timeout || socket error || empty line
				{
					static const pcrecpp::RE re_set_cookie("^Set-Cookie: (.*)$");
					// PHPSESSID=caa765e8fff04fd141e4730150dc9e0b; path=/
					static const pcrecpp::RE re_strip_cookie_meta(" path=[^ ;=]*");
					if(re_set_cookie.FullMatch(buf, &cookie))
					{
						re_strip_cookie_meta.GlobalReplace("", &cookie);
						pthread_rwlock_wrlock(&g_cookie_lock);
						cookie_list[ecmd] = cookie;
						pthread_rwlock_unlock(&g_cookie_lock);
					}
				}
			}
			else
			{
				if(extcmds[i].encoding)
					ts.set_encoding(extcmds[i].encoding);
				const char *argv[] = {NULL, chan.c_str(), pmsg.getident().c_str(), ecmd.c_str(), ecarg.c_str(), NULL};
				ret = ts.execute(std::string(std::string("./extcmds/") + extcmds[i].uri).c_str(), argv);
				if(ret)
				{
					isrv->privmsg(mdest, "실행 실패");
					
					return 0;
				}
			}
			
			int valid_cmd = 0;
			while((ret = ts.readline(buf, 3500)) >= 0) // break on timeout || socket error
			{
				if(buf == "inklbot" || buf == "meowbot")
				{
					valid_cmd = 1;
					break;
				}
			}
			if(ret == tcpsock::READLINE_RESULT_TIMEOUT)
			{
				isrv->privmsg(mdest, "timed out");
				break;
			}
			if(valid_cmd == 0)
			{
				isrv->privmsg(mdest, "내부 오류 (vc=0)");
				break;
			}
			i = 0;
			while((ret = ts.readline(buf, 3500)) >= 0) // break on timeout || socket error
			{
				if(pcrecpp::RE("^ *$").FullMatch(buf))
					continue;
				isrv->privmsg_nh(mdest, " " + buf.substr(0, 400));
				if(i >= 2)
				{
					break;
				}
				i++;
			}
			ts.close();
			break;
		}
	}
	
	return 0;
}





int mod_extcmd_init(ircbot *bot)
{
	for(int i = 0; extcmds[i].type != EXTCMDTYPE_NONE; i++)
	{
		if(extcmds[i].type == EXTCMDTYPE_EXEC)
			bot->register_cmd(extcmds[i].cmdname, cmd_cb_extcmd);
		else
			bot->register_cmd(std::string("!") + extcmds[i].cmdname, cmd_cb_extcmd);
	}
	pthread_rwlock_init(&g_cookie_lock, NULL);
	
	return 0;
}

int mod_extcmd_cleanup(ircbot *bot)
{
	for(int i = 0; extcmds[i].type != EXTCMDTYPE_NONE; i++)
	{
		if(extcmds[i].type == EXTCMDTYPE_EXEC)
			bot->unregister_cmd(extcmds[i].cmdname);
		else
			bot->unregister_cmd(std::string("!") + extcmds[i].cmdname);
	}
	
	return 0;
}



MODULE_INFO(mod_extcmd, mod_extcmd_init, mod_extcmd_cleanup)




