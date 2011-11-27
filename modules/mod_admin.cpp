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
#include <malloc.h>

#include <pcrecpp.h>

#include "defs.h"
#include "module.h"
#include "tcpsock.h"
#include "etc.h"
#include "ircbot.h"





static int cmd_cb_admin(ircbot_conn *isrv, irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	ircbot *bot = isrv->bot;
	std::string tmp, tmp2;
	// int ret;
	
	if(cmd == "mallinfo")
	{
		struct mallinfo mi = mallinfo();
		isrv->privmsgf(mdest, "malloc: %fKiB(used: %fKiB + unused: %fKiB), "
			"mmapcnt: %d, mmap: %fKiB, unusedcnt: %d, releaseable: %fKiB", 
			(float)mi.arena/1024, (float)mi.uordblks/1024, (float)mi.fordblks/1024, 
			mi.hblks, (float)mi.hblkhd/1024, mi.ordblks, (float)mi.keepcost/1024);
		return 0;
	}
	
	if(!bot->is_botadmin(pmsg.getident()))
	{
		//isrv->privmsg(mdest, "당신은 관리자가 아닙니다.");
		goto end;
	}
	
	if(cmd == "join")
	{
		tmp = carg;
		pcrecpp::RE(" ").GlobalReplace("", &tmp);
		isrv->join_channel(tmp);
		isrv->privmsg(mdest, "joined " + carg);
	}
	else if(cmd == "part")
	{
		isrv->privmsg(mdest, "left " + carg);
		isrv->leave_channel(carg);
	}
	else if(cmd == "chans")
	{
		const std::vector<std::string> &chans = isrv->get_chanlist();
		std::vector<std::string>::const_iterator it;
		tmp = "";
		for(it = chans.begin(); it != chans.end(); it++)
		{
			tmp += *it;
			if(it != chans.end() - 1)
			{
				tmp += ", ";
			}
		}
		isrv->privmsgf(mdest, "%d: %s", chans.size(), tmp.c_str());
	}
	else if(cmd == "names")
	{
		const irc_strmap<std::string, irc_user> &names = isrv->get_chanuserlist(carg);
		irc_strmap<std::string, irc_user>::const_iterator it;
		tmp = "";
		int i = 1;
		for(it = names.begin(); it != names.end(); it++)
		{
			tmp += it->second.getchanusermode() + it->second.getnick() + ", ";
			if((i % 20) == 0)
			{
				isrv->privmsgf_nh(mdest, "%d: %s", names.size(), tmp.c_str());
				tmp = "";
			}
			i++;
		}
		isrv->privmsgf_nh(mdest, "%d: %s", names.size(), tmp.c_str());
	}
	else if(cmd == "maxlinespercmd")
	{
		int mpc = atoi(carg.c_str());
		if(mpc < 2) mpc = 2;
		if(mpc > 5) mpc = 5;
		bot->max_lines_per_cmd = mpc;
		
	}
	else if(cmd == "rehash")
	{
		// FIXME!!!
		//isrv->pause_event_threads(1);
		bot->load_cfg_file(bot->cfg.get_filename());
		//isrv->pause_event_threads(0);
		isrv->privmsg(mdest, "rehashed");
	}
	else if(cmd == "quit")
	{
		isrv->quit(carg);
	}
	else if(cmd == "disconnect")
	{
		isrv->disconnect();
	}
	else if(cmd == "quitall")
	{
		bot->quit_all(carg);
	}
	else if(cmd == "disconnectall")
	{
		bot->disconnect_all();
	}
	else if(cmd == "connect")
	{
		if(carg.empty())
		{
			isrv->privmsg(mdest, "!connect <name>");
		}
		else
		{
			int ret = bot->connect_1(carg.c_str());
			isrv->privmsgf(mdest, "connect result: %d", ret);
		}
	}
	else if(cmd == "force_kill")
	{
		if(carg.empty())
		{
			isrv->privmsg(mdest, "!force_kill <name>");
		}
		else
		{
			const std::string &cname = carg;
			ircbot_conn *isrv_t = bot->find_conn_by_name(cname);
			if(isrv_t == NULL)
			{
				isrv->privmsg(mdest, "not found");
			}
			else
			{
				isrv_t->force_kill();
				isrv->privmsg(mdest, "killed "+cname);
			}
		}
	}
	else if(cmd == "evthrdcnt")
	{
		isrv->privmsgf(mdest, "현재 이벤트 스레드 개수: %d", bot->number_of_event_threads());
	}
	else if(cmd == "setevthrdcnt")
	{
		int cnt = atoi(carg.c_str());
		if(cnt <= 0 || cnt > 50)
		{
			isrv->privmsg(mdest, "스레드 개수는 1~50개 사이에서만 설정 가능합니다.");
			goto end;
		}
		int prevcnt = bot->number_of_event_threads();
		int newcnt = bot->set_event_thread_cnt(cnt);
		if(newcnt < 0)
			isrv->privmsg(mdest, "이미 스레드 수를 조정중입니다.");
		else
			isrv->privmsgf(mdest, "이벤트 스레드 개수: %d -> %d", prevcnt, newcnt);
	}
	else if(cmd == "modlist")
	{
		std::string buf;
		bot->modules.rdlock();
		std::map<std::string, module>::const_iterator it = bot->modules.modules.begin();
		for(int i = 0; it != bot->modules.modules.end(); it++, i++)
		{
			buf += it->first + ", ";
			if((i + 1) % 20 == 0)
			{
				isrv->privmsg_nh(mdest, buf);
				buf.clear();
			}
		}
		bot->modules.unlock();
		if(!buf.empty())
			isrv->privmsg_nh(mdest, buf);
	}
	else if(cmd == "charset")
	{
		if(carg.empty())
		{
			isrv->privmsgf(mdest, "current charset: %s", isrv->srv_encoding.c_str());
		}
		else
		{
			int ret = isrv->set_encoding(carg);
			isrv->privmsgf(mdest, "set_encoding() result: %d", ret);
		}
	}
	else if(cmd == "preserve_eilseq_line")
	{
		isrv->preserve_eilseq_line = (carg=="1")?true:false;
		isrv->privmsgf(mdest, "preserve_eilseq_line = %d", isrv->preserve_eilseq_line);
	}
	else if(cmd == "flood_protection")
	{
		if(carg.empty())
		{
			isrv->privmsgf(mdest, "enable_flood_protection == %d", isrv->enable_flood_protection);
		}
		else
		{
			isrv->enable_flood_protection = (carg=="0")?false:true;
			isrv->privmsgf(mdest, "enable_flood_protection = %d", isrv->enable_flood_protection);
		}
	}
	else if(cmd == "timercnt")
	{
		isrv->privmsgf(mdest, "현재 타이머 개수: %d", isrv->bot->timers.size());
	}
	
end:
	return HANDLER_FINISHED;
}





static const char *const cmdlist_admin[] = 
{
	"join", "part", 
	"names", "maxlinespercmd", "rehash", 
	"quit", "disconnect", "quitall", "disconnectall", "connect", "force_kill", 
	"evthrdcnt", "setevthrdcnt", "modlist", "charset", "preserve_eilseq_line", 
	"flood_protection", "timercnt", "mallinfo", 
	NULL
};

int mod_admin_init(ircbot *bot)
{
	int i;
	
	for(i = 0; cmdlist_admin[i]; i++)
	{
		bot->register_cmd(cmdlist_admin[i], cmd_cb_admin);
	}
	
	return 0;
}

int mod_admin_cleanup(ircbot *bot)
{
	int i;
	
	for(i = 0; cmdlist_admin[i]; i++)
	{
		bot->unregister_cmd(cmdlist_admin[i]);
	}
	
	return 0;
}




MODULE_INFO(mod_admin, mod_admin_init, mod_admin_cleanup)




