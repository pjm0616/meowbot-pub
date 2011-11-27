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
#include "etc.h"
#include "module.h"
#include "tcpsock.h"
#include "ircbot.h"




static int irc_event_handler(ircbot_conn *isrv, ircevent *evdata, void *)
{
	int evtype = evdata->event_id;
	(void)isrv;
	
	return HANDLER_GO_ON;
}

static int bottimer_callback(ircbot *bot, void *userdata)
{
	if(!bot->check_ircconn(((ircbot_conn *)userdata)))
		return HANDLER_GO_ON;
	((ircbot_conn *)userdata)->privmsg("##", ".");
	
	
	return 0;
}

static int cmd_cb_test(ircbot_conn *isrv, irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	(void)carg;
	
	isrv->privmsg(mdest, "테스트");
	// isrv->privmsg(mdest, IRCTEXT_COLOR_FG(IRCTEXT_COLOR_RED) "ㅁㄴㅇㄹ");
	// isrv->privmsg(mdest, IRCTEXT_COLOR_BG(IRCTEXT_COLOR_GREEN) "ㅁㄴㅇㄹ");
	// isrv->privmsg(mdest, IRCTEXT_COLOR_FGBG(IRCTEXT_COLOR_RED, IRCTEXT_COLOR_BLUE) "ㅁㄴㅇㄹ");
	
	//isrv->bot->add_timer("", bottimer_callback, 2000, isrv);
	
	return HANDLER_FINISHED;
}


static int cmd_cb_test2(ircbot_conn *isrv, irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	(void)carg;
	
	isrv->bot->del_timer("test");
	
	return HANDLER_FINISHED;
}



int mod_skel_init(ircbot *bot)
{
	//bot->register_event_handler(irc_event_handler);
	//bot->register_cmd("테스트", cmd_cb_test);
	//bot->register_cmd("테스트2", cmd_cb_test2);
	
	return 0;
}

int mod_skel_cleanup(ircbot *bot)
{
	//bot->unregister_event_handler(irc_event_handler);
	//bot->unregister_cmd("테스트");
	//bot->unregister_cmd("테스트2");
	
	//bot->del_timer("test");
	//bot->del_timer_by_cbptr(bottimer_callback);
	
	return 0;
}



MODULE_INFO(mod_skel, mod_skel_init, mod_skel_cleanup)




