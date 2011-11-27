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
#include "hangul.h"
#include "module.h"
#include "sqlite3_db.h"
#include "ircbot.h"


// 검색금지어
// #define ENABLE_SPECIAL_EXCEPTIONS





#define REMEMBER_DB_FILE "./data/remember_db.sdb"
sqlite3_db g_rem_db;



static int create_remember_db()
{
	g_rem_db.query(NULL, "CREATE TABLE IF NOT EXISTS remember_data "
		"(id int not null primary key, nick text, word text, "
		"data text, time integer default 0);");
	g_rem_db.query(NULL, "CREATE TABLE IF NOT EXISTS remember_history "
		"(id int not null primary key, pid int not null, seq int not null, "
		"nick text, word text, data text, time integer default 0);");
	
	return 0;
}

static int remember(const std::string &nick, const std::string &word, const std::string &data)
{
	std::vector<std::vector<std::string> > ret;
	
	g_rem_db.query(&ret, "SELECT id FROM remember_data WHERE word = ?", word);
	if(ret.begin() == ret.end())
	{
		g_rem_db.query(&ret, "SELECT max(id) FROM remember_data;");
		int maxid = (ret.begin() == ret.end())?1:(atoi(ret[0][0].c_str()) + 1);
		g_rem_db.query(NULL, "INSERT INTO remember_data(id, nick, word, data, time) "
			"VALUES(?, ?, ?, ?, ?)", int2string(maxid), nick, word, data, 
			int2string(time(NULL)));
	}
	else
	{
		std::string id_str = ret[0][0];
		int id = atoi(id_str.c_str());
		g_rem_db.query(NULL, "UPDATE remember_data SET nick = ?, data = ?, time = ? WHERE id = ?", 
			nick, data, int2string(time(NULL)), id_str);
	}
	
	return 0;
}

/* static */
extern "C" int getremdb_primitive(const std::string &word, 
	std::string &nick, std::string &desc)
{
	std::vector<std::vector<std::string> > ret;
	
	g_rem_db.query(&ret, "SELECT nick, data FROM remember_data WHERE word = ? LIMIT 1", word);
	std::vector<std::vector<std::string> >::const_iterator it = ret.begin();
	if(it == ret.end())
	{
		nick.clear();
		desc.clear();
		return -1;
	}
	
	if(!pcrecpp::RE("([^!@]+).*").FullMatch((*it)[0], &nick)) // strip hostname
		nick = (*it)[0];
	desc = (*it)[1];
	
	return 0;
}

/* static */
extern "C" int getremdb_v2(const std::string &word, 
	std::string &nick, std::string &desc, std::string *realword)
{
	static pcrecpp::RE re_redir("^link:([^ ]+)$");
	const int max_redirects = 10;
	
	int ret, redirs, result = 0;
	std::vector<std::string> redir_history;
	std::vector<std::string>::const_iterator it;
	std::string rword(word), tmp;
	for(redirs = 0; redirs < max_redirects; redirs++)
	{
		ret = getremdb_primitive(rword, nick, desc);
		if(ret < 0)
		{
			result = -1;
			if(redirs != 0)
				desc = "존재하지 않는 값으로 redirect할 수 없습니다: " + redir_history.back() + " -> " + rword;
			else
				desc = "존재하지 않는 기억입니다";
			goto end_of_loop;
		}
		if(re_redir.FullMatch(desc, &tmp))
		{
			for(it = redir_history.begin(); it != redir_history.end(); it++)
			{
				if(*it == tmp)
				{
					result = -1;
					desc = "재귀적 연결은 금지되어있습니다";
					goto end_of_loop;
				}
			}
			redir_history.push_back(rword);
			rword = tmp;
		}
		else
			goto end_of_loop;
	}
	if(result == 0 && redirs == max_redirects)
	{
		result = -1;
		desc = "연결 횟수 제한을 초과했습니다";
	}
end_of_loop:
	if(result < 0)
		nick = "오류";
	if(realword)
		realword->assign(rword);
	
	return result;
}

/* static */
extern "C" std::pair<std::string, std::string> getremdb(const std::string &word)
{
	std::pair<std::string, std::string> tmp;
	int ret = getremdb_v2(word, tmp.first, tmp.second, NULL);
	if(ret < 0)
		tmp.first.clear();
	return tmp;
}


static int srchremdb(const std::string &word, int start, int count, std::vector<std::string> &dest)
{
	std::vector<std::vector<std::string> > ret;
	std::string data;
	
	g_rem_db.query(&ret, "SELECT word FROM remember_data WHERE word LIKE ? LIMIT ?, ?", 
		"%" + word + "%", int2string(start), int2string(count));
	
	std::vector<std::vector<std::string> >::const_iterator it;
	for(it = ret.begin(); it != ret.end(); it++)
	{
		dest.push_back((*it)[0]);
	}
	
	return 0;
}






static int cmd_remember(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	const std::string &ident = pmsg.getident();
	std::string word, data;
	//static pcrecpp::RE re_strip_dot("·", pcrecpp::UTF8());
	
	if(!pcrecpp::RE("([^ ]+) (.*)", pcrecpp::UTF8()).FullMatch(carg, &word, &data))
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <단어> <내용>");
		return 0;
	}
	
	std::string nick, desc, realword;
	int prevdata_ret = getremdb_v2(word, nick, desc, &realword);
	
	//re_strip_dot.GlobalReplace("", &data);
	remember(ident, word, data);
	if(prevdata_ret < 0)
		isrv->privmsg_nh(mdest, word + select_josa(word, "을를") + " 기억했습니다.");
	else
	{
		if(word == realword)
		{
			isrv->privmsgf_nh(mdest, "%s%s 기억했습니다. (기존 데이터 덮어씀)", 
				word.c_str(), select_josa(word, "을를").c_str());
		}
		else
		{
			isrv->privmsgf_nh(mdest, "%s%s 기억했습니다. (주의: %s로의 연결을 덮어썼습니다. "
				"복구하려면 " IRCTEXT_BOLD "!연결 %s %s" IRCTEXT_NORMAL "를 입력한 후 " 
				IRCTEXT_BOLD "!기억 %s <내용>" IRCTEXT_NORMAL "을 해주세요)", 
				word.c_str(), select_josa(word, "을를").c_str(), 
				realword.c_str(), 
				word.c_str(), realword.c_str(), realword.c_str()
				);
		}
	}
	
	return 0;
}

static int cmd_whatis(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	static pcrecpp::RE re_strip_space("[ ]", pcrecpp::UTF8());
	
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <단어>");
		return 0;
	}
	
	std::string word(carg), nick, desc, realword;
	re_strip_space.GlobalReplace("", &word);
	int ret = getremdb_v2(word, nick, desc, &realword);
	if(ret < 0)
	{
		if(desc == "존재하지 않는 기억입니다")
			desc += ". " IRCTEXT_BOLD "!i알려 " + word + IRCTEXT_NORMAL " " + select_josa(word, "을를") + " 쓰면 인클봇의 기억 데이터를 볼 수 있습니다.";
		isrv->privmsg_nh(mdest, "오류: " + desc);
	}
	else
	{
		if(word != realword)
			realword = word + "(" + realword + ")";
		isrv->privmsgf_nh(mdest, "%.50s: %.500s %s라고 %s%s 알려주었어요.", 
			realword.c_str(), desc.c_str(), select_josa(desc, "이").c_str(), 
			nick.c_str(), select_josa(nick, "이가").c_str()
			);
	}
	
	return 0;
}

static int cmd_append(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	const std::string &ident = pmsg.getident();
	std::string word, data;
	static pcrecpp::RE	//re_strip_dot("·", pcrecpp::UTF8()), 
						re_strip_space("[ ]", pcrecpp::UTF8());
	
	if(!pcrecpp::RE("([^ ]+) (.*)", pcrecpp::UTF8()).FullMatch(carg, &word, &data))
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <단어> <덧붙일 내용>");
		return 0;
	}
	
	//re_strip_dot.GlobalReplace("", &data);
	re_strip_space.GlobalReplace("", &word);
	
	std::string nick, desc, realword;
	int ret = getremdb_v2(word, nick, desc, &realword);
	if(ret < 0)
	{
		isrv->privmsg_nh(mdest, "오류: " + desc);
	}
	else
	{
		remember(ident, realword, desc + " | " + data);
		if(word == realword)
			isrv->privmsg_nh(mdest, realword + select_josa(realword, "을를") + " 기억했습니다.");
		else
			isrv->privmsg_nh(mdest, realword + select_josa(realword, "을를") + " 기억했습니다. (" + word + " -> " + realword + ")");
	}
	
	return 0;
}

static int cmd_find(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	std::string tmp, tmp2;
	const int maxitemsperline = 20;
	
	if(!pcrecpp::RE("([^ ]+)(?: ([0-9]+))?", pcrecpp::UTF8()).Match(carg, &tmp, &tmp2))
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <단어>");
		isrv->privmsg(mdest, "사용법: !" + cmd + " <단어> [페이지]");
		return 0;
	}
	pcrecpp::RE("%").GlobalReplace("", &tmp);
	if(tmp.empty())
	{
		isrv->privmsg(mdest, "검색어를 입력하세요");
		return 0;
	}
#ifdef ENABLE_SPECIAL_EXCEPTIONS
	if(!isrv->is_botadmin(pmsg.getident()) && tmp == "검고")
	{
		isrv->privmsg(mdest, "本人의 要請에 依해 檢索 禁止된 單語이빈다 'ㅅ'");
		return 0;
	}
#endif
	
	if(tmp2.empty())tmp2 = "1";
	int srchstart = atoi(tmp2.c_str()) - 1;
	if(srchstart < 0 || srchstart > 9)
	{
		isrv->privmsg(mdest, "페이지가 너무 작거나 큽니다. (1 ~ 10)");
		return 0;
	}
	
	std::vector<std::string> remdbsrch;
	srchremdb(tmp, srchstart * isrv->bot->max_lines_per_cmd * maxitemsperline, 
		isrv->bot->max_lines_per_cmd * maxitemsperline + 1, remdbsrch); // 다음 item 하나를 더 가져옴(다음 페이지를 표시하기 위해서)
	if(remdbsrch.begin() == remdbsrch.end())
	{
		isrv->privmsg_nh(mdest, " " + tmp + "에 대한 검색결과가 없습니다.");
	}
	std::vector<std::string>::const_iterator it = remdbsrch.begin();
	std::string remdbsrchret = "";
	for(int i = 0; it != remdbsrch.end() && i < (isrv->bot->max_lines_per_cmd-1) * maxitemsperline; it++, i++)
	{
		remdbsrchret += *it + ", ";
		if(((i+1) % maxitemsperline) == 0)
		{
			isrv->privmsg_nh(mdest, " " + remdbsrchret);
			remdbsrchret = "";
		}
	}
	if(!remdbsrchret.empty())
		isrv->privmsg_nh(mdest, " " + remdbsrchret);
	if(it != remdbsrch.end())
	{
		isrv->privmsg_nh(mdest, "다음 페이지: !" + cmd + " " + tmp + " " + int2string(srchstart + 2));
	}
	
	return 0;
}

static int cmd_link(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	const std::string &ident = pmsg.getident();
	std::string word, data;
	static pcrecpp::RE re_validate_link("^[^ ]+$", pcrecpp::UTF8());
	
	if(!pcrecpp::RE("([^ ]+) (.*)", pcrecpp::UTF8()).FullMatch(carg, &word, &data))
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <단어> <연결할 기억 이름>");
		return 0;
	}
	
	if(!re_validate_link.FullMatch(data))
	{
		isrv->privmsg_nh(mdest, "형식이 잘못되었습니다. 기억 이름에 ' '가 올 수 없습니다.");
		return 0;
	}
	remember(ident, word, "link:" + data);
	isrv->privmsg_nh(mdest, word + select_josa(word, "을를") + " " + data + select_josa(word, "으") + "로 연결했습니다.");
	
	return 0;
}

static int cmd_delete(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	(void)carg;
	isrv->privmsg(mdest, "`!기억해' 로 덮어쓰면 지워집니다.");
	return 0;
}

static int cmd_nofdata(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	(void)carg;
	
	std::vector<std::vector<std::string> > ret;
	g_rem_db.query(&ret, "SELECT count(*) FROM remember_data");
	int n = 0;
	if(ret.begin() != ret.end())
	{
		n = atoi(ret[0][0].c_str());
	}
	isrv->privmsgf(mdest, "%d 개의 기억 데이터가 있습니다.", n);
	
	return 0;
}





int mod_remember_init(ircbot *bot)
{
	g_rem_db.open(REMEMBER_DB_FILE);
	create_remember_db();
	
	bot->register_cmd("기억해", cmd_remember);
	bot->register_cmd("기억", cmd_remember);
	bot->register_cmd("learn", cmd_remember);
	bot->register_cmd("remember", cmd_remember);
	bot->register_cmd("알려줘", cmd_whatis);
	bot->register_cmd("알려", cmd_whatis);
	bot->register_cmd("whatis", cmd_whatis);
	bot->register_cmd("찾아줘", cmd_find);
	bot->register_cmd("찾아", cmd_find);
	bot->register_cmd("find", cmd_find);
	bot->register_cmd("덧붙여", cmd_append);
	bot->register_cmd("덧붙", cmd_append);
	bot->register_cmd("append", cmd_append);
	bot->register_cmd("연결", cmd_link);
	bot->register_cmd("잊어줘", cmd_delete);
	bot->register_cmd("잊어", cmd_delete);
	bot->register_cmd("지워줘", cmd_delete);
	bot->register_cmd("지워", cmd_delete);
	
	bot->register_cmd("기억개수", cmd_nofdata);
	
	return 0;
}

int mod_remember_cleanup(ircbot *bot)
{
	g_rem_db.close();
	
	bot->unregister_cmd("기억해");
	bot->unregister_cmd("기억");
	bot->unregister_cmd("learn");
	bot->unregister_cmd("remember");
	bot->unregister_cmd("알려줘");
	bot->unregister_cmd("알려");
	bot->unregister_cmd("whatis");
	bot->unregister_cmd("찾아줘");
	bot->unregister_cmd("찾아");
	bot->unregister_cmd("find");
	bot->unregister_cmd("덧붙여");
	bot->unregister_cmd("덧붙");
	bot->unregister_cmd("append");
	bot->unregister_cmd("연결");
	bot->unregister_cmd("잊어줘");
	bot->unregister_cmd("잊어");
	bot->unregister_cmd("지워줘");
	bot->unregister_cmd("지워");
	
	bot->unregister_cmd("기억개수");
	
	return 0;
}



MODULE_INFO(mod_remember, mod_remember_init, mod_remember_cleanup)




