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

#include <pcrecpp.h>

#include "defs.h"
#include "module.h"
#include "tcpsock.h"
#include "etc.h"
#include "ircbot.h"


const char *g_useragent = "Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.9) Gecko/2008061015 Firefox/3.0";

#define TARGET_IP "speller.cs.pusan.ac.kr"

static int check_kor_orth(const std::string &text, std::vector<std::string> &dest)
{
	tcpsock ts;
	int ret;
	std::string buf;
	
	dest.clear();
	ts.set_encoding("cp949");
	ret = ts.connect(TARGET_IP, 80, 1000);
	if(ret < 0)
		return -1;
	std::string postdata = "text1=" + urlencode(encconv::convert("utf-8", "cp949", text));
	ts << "POST /WebSpell_ISAPI.dll?Check HTTP/1.1\r\n"
		"Host: " TARGET_IP "\r\n"
		"User-Agent: " << g_useragent << "\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: " + int2string(postdata.length()) + "\r\n"
		"Connection: close\r\n\r\n";
	ts << postdata;
	
	while(ts.readline(buf, 500) > 0){} ts.readline(buf, 500); // skip http header
	int mode = 0;
	std::string orig, fixed, errtype, desc;
	while(ts.readline(buf, 500) >= 0)
	{
		//<tr><td align=center style='color:#FD7A1A;' width=50>15일자</td>
		//<td align=center style='color:#FD7A1A;' width=50>15일 자<br></td>
		//<td width=220>조사·어미 오류<br>어미의 사용이 잘못되었습니다. 문서 작성시 필요에 의해 잘못된 어미를 제시하는 상황이 아니라면 검사기의 대치어로 바꾸도록 합니다.
		
		if(mode == 0)
		{
			orig.clear();
			fixed.clear();
			errtype.clear();
			desc.clear();
			if(pcrecpp::RE("^<tr><td align=center style='color:#FD7A1A;' width=50>(.+)</td>$")
				.FullMatch(buf, &orig))
			{
				mode = 1;
			}
		}
		else if(mode == 1)
		{
			if(pcrecpp::RE("^<td align=center style='color:#FD7A1A;' width=50>(.+)<br></td>$")
				.FullMatch(buf, &fixed))
			{
				pcrecpp::RE("<br>").GlobalReplace(", ", &fixed);
				mode = 2;
			}
		}
		else if(mode == 2)
		{
			if(pcrecpp::RE("^<td width=220>([^<]++)<br>(.++)$").FullMatch(buf, &errtype, &desc))
			{
				pcrecpp::RE("<[^>]+>").GlobalReplace("", &errtype);
				pcrecpp::RE("<[^>]+>").GlobalReplace("", &desc);
				pcrecpp::RE("\\[NARAINFOTECH 맞춤법 검사기 [^]]+\\]").GlobalReplace("", &desc);
				dest.push_back(orig + " -> " + fixed + " (" + errtype + "; " + desc + ")");
				mode = 0;
			}
		}
	}
	ts.close();
	
	return 0;
}











///////////////////////////////////////////////////////////////






static int cmd_cb_kor_orth(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <글>");
		return HANDLER_FINISHED;
	}
	
	std::vector<std::string> srchres;
	if(!check_kor_orth(carg, srchres))
	{
		if(srchres.begin() == srchres.end())
		{
			isrv->privmsg(mdest, "틀린 부분이 발견되지 않았습니다.");
		}
		else
		{
			std::vector<std::string>::const_iterator it;
			int i;
			for(i = 0, it = srchres.begin(); it != srchres.end() && i < isrv->bot->max_lines_per_cmd; i++, it++)
			{
				isrv->privmsg_nh(mdest, it->substr(0, 300));
			}
		}
	}
	else
	{
		isrv->privmsg(mdest, "내부 오류");
	}
	
	return HANDLER_FINISHED;
}




int mod_kor_orthography_init(ircbot *bot)
{
	bot->register_cmd("맞춤법", cmd_cb_kor_orth);
	
	return 0;
}

int mod_kor_orthography_cleanup(ircbot *bot)
{
	bot->unregister_cmd("맞춤법");
	
	return 0;
}



MODULE_INFO(mod_kor_orthography, mod_kor_orthography_init, mod_kor_orthography_cleanup)




