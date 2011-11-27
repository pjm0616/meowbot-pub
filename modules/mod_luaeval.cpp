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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>

#include <pcrecpp.h>

#include "defs.h"
#include "ircbot.h"
#include "luacpp.h"



/*
	Lua는 다른 언어와 달리 기본적으로 순수한 언어로서의 기능만 제공하며, 
	기타 시스템 접근은 별도 라이브러리를 로드해서 사용.
	따라서 시스템 접근이 가능한 라이브러리만 로드하지 않으면 안전.
	예외인 루아 파일을 로드하는 dofile, loadfile 2가지 함수는
	코드 실행 전에 삭제.
*/

///FIXME: 대충 만듬=_= 수정 필요



static int lua_safeeval_child(const char *code)
{
	lua_State *L = luaL_newstate();
	
	// 기본 라이브러리 로드
#define LOADLIB(name, func) \
		lua_pushcfunction(L, func); \
		lua_pushstring(L, name); \
		lua_call(L, 1, 0);
	LOADLIB("", luaopen_base)
	LOADLIB("table", luaopen_table)
	LOADLIB("string", luaopen_string)
	LOADLIB("math", luaopen_math)
#undef LOADLIB
	luaopen_libluapcre(L);
	luaopen_libluautf8(L);
	luaopen_libluahangul(L);
	
	// 파일에 접근할 수 있는 함수 삭제
#define SETNIL(name) \
		lua_pushliteral(L, name); \
		lua_pushnil(L); \
		lua_rawset(L, LUA_GLOBALSINDEX);
	SETNIL("dofile");
	SETNIL("loadfile");
#undef SETNIL
	
	// 코드 실행
	std::string codebuf(code);
	codebuf += '\n';
	int ret = luaL_loadstring(L, codebuf.c_str());
	if(ret)
	{
		const char *errmsg = lua_tostring(L, -1);
		printf("Error: %s\n", errmsg);
		lua_pop(L, 1);
	}
	else
	{
		ret = lua_pcall(L, 0, 0, 0);
		if(ret != 0)
		{
			const char *errmsg = lua_tostring(L, -1);
			printf("Error: %s\n", errmsg);
			lua_pop(L, 1);
		}
	}
	lua_close(L);
	
	return 0;
}

static void luaeval_child_signal_handler(int sig)
{
	if(sig == SIGALRM)
		puts("Timeout");
	else
		printf("Recieved signal %d\n", sig);
	exit(1);
}

static int lua_safeeval(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, const char *code, const char *cparam)
{
	int rdpipe[2];
	pipe(rdpipe);
	int pid = fork();
	if(pid < 0)
	{
		close(rdpipe[0]);
		close(rdpipe[1]);
		return -1;
	}
	else if(pid == 0)
	{
		close(0);
		dup2(rdpipe[1], 1);
		dup2(rdpipe[1], 2);
		close(rdpipe[0]);
		
		signal(SIGALRM, luaeval_child_signal_handler);
		alarm(2);
		lua_safeeval_child(code);
		
		exit(0);
	}
	else
	{
		close(rdpipe[1]);
		int status, ret;
		ret = waitpid(pid, &status, 0);
		char buf[512*3+1];
		ret = read(rdpipe[0], buf, sizeof(buf)-1);
		close(rdpipe[0]);
		if(ret < 0)
			return -1;
		buf[ret] = 0;
		
		char *pp, *p = strtok_r(buf, "\n", &pp);
		if(!p)
		{
			isrv->privmsg(mdest, "(No output)");
		}
		else
		{
			int lines = 3;
			do
			{
				isrv->privmsg_nh(mdest, p);
			} while((p = strtok_r(NULL, "\n", &pp)) && --lines);
		}
	}
	
	return 0;
}


int cmd_cb_luaeval(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	if(carg.empty())
	{
		isrv->privmsg(mdest, "사용법: !" + cmd + " <코드>");
	}
	else
	{
		lua_safeeval(isrv, pmsg, mdest, carg.c_str(), NULL);
	}
	
	return HANDLER_FINISHED;
}

#if 0
int cmd_cb_memeval(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	
	std::string mname, marg;
	if(!pcrecpp::RE("([^ ]+) ?(.*)").FullMatch(carg, &mname, &marg))
	{
		isrv->privmsg(mdest, 
			"사용법: !기억해 <기억 이름> !perl <코드> 등으로 기억시킨 후, "
			"!기억실행 <기억 이름> 또는 !기억실행 <기억 이름> <파라미터> 로 실행 (단축: !.)");
		isrv->privmsg(mdest, 
			"    %nick%은 닉네임으로, %param%는 파라미터로 치환됩니다. "
			"C,C++에서는 int nparam, const char *params[] 전역변수가 제공됩니다.");
	}
	else
	{
		const std::string &nick = pmsg.getnick();
		
		// FIXME: use getremdb_v2
		std::pair<std::string, std::string> (*getremdb_fxn)(const std::string &word);
		getremdb_fxn = (std::pair<std::string, std::string> (*)(const std::string &))
			isrv->bot->modules.dlsym("mod_remember", "getremdb");
		if(!getremdb_fxn)
		{
			isrv->privmsg(mdest, "내부 오류: 이 명령에 필요한 모듈이 로드되지 않았습니다.");
			return 0;
		}
		std::pair<std::string, std::string> data = getremdb_fxn(mname);
		std::string cmd2, carg2;
		
		if(data.first.empty())
		{
			isrv->privmsg(mdest, "오류: " + data.second);
		}
		else if(pcrecpp::RE("^!([^ ]+) (.+)").FullMatch(data.second, &cmd2, &carg2))
		{
			irc_cmd_eval_code(isrv, mdest, nick, cmd2, carg2, marg);
		}
		else
		{
			isrv->privmsgf_nh(mdest, "올바른 형식이 아닙니다: %s", data.second.c_str());
		}
	}
	
	return HANDLER_FINISHED;
}
#endif

#include "main.h"
int mod_luaeval_init(ircbot *bot)
{
	//bot->register_cmd("기억실행", cmd_cb_memeval);
	//bot->register_cmd(".", cmd_cb_memeval);
	bot->register_cmd("luaeval", cmd_cb_luaeval);
	
	return 0;
}

int mod_luaeval_cleanup(ircbot *bot)
{
	//bot->unregister_cmd("기억실행");
	//bot->unregister_cmd(".");
	bot->unregister_cmd("luaeval");
	
	return 0;
}



MODULE_INFO(mod_luaeval, mod_luaeval_init, mod_luaeval_cleanup)





