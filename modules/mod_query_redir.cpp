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
#include <cstring>

#include <pthread.h>
#include <pcrecpp.h>

#include "defs.h"
#include "module.h"
#include "ircbot.h"





static std::map<std::string, bool> g_query_redir_chans;
static pthread_rwlock_t g_qrchanlist_lock;



static int irc_event_handler_qry_redir(ircbot_conn *isrv, ircevent *evdata, void *)
{
	int etype = evdata->event_id;
	
	if(etype == BOTEVENT_COMMAND)
	{
		botevent_command *ev = static_cast<botevent_command *>(evdata);
		
		if(ev->cmd_trigger == "`" || ev->cmd_trigger == "'")
		{
			ev->mdest.setchan(ev->pmsg.getnick());
			ev->mdest.setmsgtype(IRC_MSGTYPE_NOTICE);
			return HANDLER_GO_ON;
		}
		
		const std::string &chan = ev->pmsg.getchan();
		// 쿼리 전용 채널에서 명령어 사용시 자동으로 쿼리로 돌림
		std::map<std::string, bool>::const_iterator it;
		pthread_rwlock_rdlock(&g_qrchanlist_lock);
		it = g_query_redir_chans.find(chan);
		if(it != g_query_redir_chans.end() && 
			it->second == true && 
			ev->pmsg.getchantype() == IRC_CHANTYPE_CHAN)
		{
			if(ev->cmd != "쿼리출력")
			{
				ev->mdest.setchan(ev->pmsg.getnick());
				ev->mdest.setmsgtype(IRC_MSGTYPE_NOTICE);
			}
		}
		pthread_rwlock_unlock(&g_qrchanlist_lock);
	}
	
	return HANDLER_GO_ON;
}

static int cmd_query_redir_chan(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	(void)carg;
	const std::string &chan = pmsg.getchan();
	const std::string &nick = pmsg.getnick();
	
	if(pmsg.getchantype() != IRC_CHANTYPE_CHAN)
	{
		isrv->privmsg_nh(mdest, "채널이 아닙니다.");
		return 0;
	}
	
	isrv->rdlock_chanuserlist();
	const irc_strmap<std::string, irc_user> &names = 
		isrv->get_chanuserlist(chan);
	irc_strmap<std::string, irc_user>::const_iterator it;
	it = names.find(nick);
	if(it == names.end())
	{
		isrv->privmsg_nh(mdest, "해당 채널에 존재하지 않는 사용자입니다.");
		isrv->unlock_chanuserlist();
		return 0;
	}
	else if(it->second.getchanusermode() == "" || it->second.getchanusermode() == "+")
	{
		isrv->privmsg_nh(mdest, "권한이 없습니다.");
		isrv->unlock_chanuserlist();
		return 0;
	}
	isrv->unlock_chanuserlist();
	
	pthread_rwlock_wrlock(&g_qrchanlist_lock);
	if(g_query_redir_chans[chan] == true)
	{
		isrv->privmsg_nh(mdest, "명령어를 채널로 출력합니다.");
		g_query_redir_chans[chan] = false;
	}
	else
	{
		isrv->privmsg_nh(mdest, "명령어를 쿼리로 출력합니다.");
		g_query_redir_chans[chan] = true;
	}
	pthread_rwlock_unlock(&g_qrchanlist_lock);
	
	return 0;
}


int mod_query_redir_init(ircbot *bot)
{
	bot->register_event_handler(irc_event_handler_qry_redir, IRC_PRIORITY_MIN);
	bot->register_cmd("쿼리출력", cmd_query_redir_chan);
	pthread_rwlock_init(&g_qrchanlist_lock, NULL);
	
	return 0;
}

int mod_query_redir_cleanup(ircbot *bot)
{
	bot->unregister_event_handler(irc_event_handler_qry_redir);
	bot->unregister_cmd("쿼리출력");
	
	return 0;
}



MODULE_INFO(mod_query_redir, mod_query_redir_init, mod_query_redir_cleanup)




