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
#include "utf8.h"
#include "module.h"
#include "ircbot.h"

#define MAX_MEANING_CNT 4
#define MAX_SUBMEANING_CNT 3
struct hanja_data_s
{
	const char *hanja;
	struct
	{
		const char *submeanings[MAX_SUBMEANING_CNT + 1];
		const char *sound;
	} meanings[MAX_MEANING_CNT + 1];
	const char *busu;
	int stroke;
};
static const hanja_data_s hanja_data[] = 
#include "mod_hanja_data.h"

//{"刺", {{{"찌를", "가시", NULL}, "자"}, {"칼로찌를", NULL}, "척"}, {"수라", NULL}, "라"}, {"수라", NULL}, "나"}, {{NULL}, NULL}}, "刂", 8}, 

static const hanja_data_s *find_by_hanja(const std::string &hanja)
{
	for(int i = 0; i < HANJA_CNT; i++)
		if(hanja_data[i].hanja == hanja)
			return &hanja_data[i];
	
	return NULL;
}

static const hanja_data_s *find_by_sound(std::vector<const hanja_data_s *> &dest, const std::string &sound)
{
	dest.clear();
	
	for(int i = 0; i < HANJA_CNT; i++)
		for(int j = 0; hanja_data[i].meanings[j].sound; j++)
			if(hanja_data[i].meanings[j].sound == sound)
				dest.push_back(&hanja_data[i]);
	
	dest.push_back(NULL);
	return dest.empty()?NULL:dest[0];
}

static const hanja_data_s *find_by_meaning(std::vector<const hanja_data_s *> &dest, const std::string &meaning)
{
	dest.clear();
	
	for(int i = 0; i < HANJA_CNT; i++)
		for(int j = 0; hanja_data[i].meanings[j].sound; j++)
			for(int k = 0; hanja_data[i].meanings[j].submeanings[k]; k++)
				if(hanja_data[i].meanings[j].submeanings[k] == meaning)
					dest.push_back(&hanja_data[i]);
	
	dest.push_back(NULL);
	return dest.empty()?NULL:dest[0];
}

static std::string render_hanja_data(const hanja_data_s *hd)
{
	std::string result;
	if(!hd)
		return "";
	
	result = std::string(hd->hanja) + " [";
	for(int i = 0; hd->meanings[i].sound; i++)
	{
		for(int j = 0; hd->meanings[i].submeanings[j]; j++)
		{
			result += hd->meanings[i].submeanings[j];
			if(hd->meanings[i].submeanings[j + 1])
				result += ", ";
		}
		result += std::string(" ") + hd->meanings[i].sound;
		if(hd->meanings[i + 1].sound)
			result += " / ";
	}
	result += std::string("] ") + int2string(hd->stroke) + "획";
	
	return result;
}





static int cmd_cb_hanja(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	const hanja_data_s *hd = NULL;
	std::vector<const hanja_data_s *> hlist;
	std::string buf;
	
	(void)cmd;
	(void)carg;
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !한자 [한자|음|뜻]");
		return 0;
	}
	
	// U+4E00 - U+9FFF : CJK unified ideographs
	//if(pcrecpp::RE("^[一-鿿]+$", pcrecpp::UTF8()).FullMatch(carg))
	if(pcrecpp::RE("[一-鿿]", pcrecpp::UTF8()).PartialMatch(carg)) // 한자가 하나라도 있다면
	{
		std::vector<std::string> chars = utf8_split_chars(carg);
		int lines = 0;
		for(std::vector<std::string>::const_iterator it = chars.begin(); 
			it != chars.end(); it++)
		{
			unsigned int code = utf8_decode_s(*it);
			if(code >= 0x4E00 && code <= 0x9FFF)
			{
				// 한자인 경우
				hd = find_by_hanja(*it);
				if(hd)
				{
					buf += render_hanja_data(hd);
					if(it + 1 != chars.end())
						buf += IRCTEXT_BOLD" | "IRCTEXT_NORMAL;
				}
				else
				{
					buf += *it; // 찾지 못한 경우
				}
			}
			else
			{
				buf += *it; // 한자가 아닌 경우
			}
			if(buf.length() > 350)
			{
				isrv->privmsg(mdest, buf);
				buf.clear();
				lines++;
			}
			if(lines >= isrv->bot->max_lines_per_cmd)
			{
				buf.clear();
				break;
			}
		}
		if(!buf.empty())
			isrv->privmsg(mdest, buf);
		
		return 0;
	}
	if(pcrecpp::RE("^[가-힣]$", pcrecpp::UTF8()).FullMatch(carg))
	{
		hd = find_by_sound(hlist, carg);
	}
	if(!hd && pcrecpp::RE("^[가-힣]+$", pcrecpp::UTF8()).FullMatch(carg))
	{
		hd = find_by_meaning(hlist, carg);
	}
	else if(!hd)
	{
		isrv->privmsg(mdest, "형식이 잘못되었습니다. 한자 한 글자 또는 음/뜻을 한글로 입력하세요");
		return 0;
	}
	
	if(!hd)
		isrv->privmsg(mdest, "검색 결과가 없습니다.");
	else
	{
		int i, ln;
		for(i = 0, ln = 0; hlist[i] && ln < isrv->bot->max_lines_per_cmd; i++)
		{
			buf += render_hanja_data(hlist[i]);
			if((i + 1) % 5 == 0)
			{
				isrv->privmsg(mdest, buf);
				ln++;
				buf.clear();
			}
			else if(hlist[i + 1] && ln < isrv->bot->max_lines_per_cmd)
				buf += IRCTEXT_BOLD" | "IRCTEXT_NORMAL;
		}
		if(!buf.empty())
			isrv->privmsg(mdest, buf);
	}
	
	return 0;
}

int mod_hanja_init(ircbot *bot)
{
	bot->register_cmd("한자", cmd_cb_hanja);
	
	return 0;
}

int mod_hanja_cleanup(ircbot *bot)
{
	bot->unregister_cmd("한자");
	
	return 0;
}



MODULE_INFO(mod_hanja, mod_hanja_init, mod_hanja_cleanup)




