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

#include <time.h>
#include <assert.h>

#include <pcrecpp.h>



#include "defs.h"
#include "irc.h"
#include "ircutils.h"



bool split_nick_usermode(const std::string &str, std::string *mode, std::string *nick)
{
	static pcrecpp::RE re_split_nick_usermode("^([~&@%+]*+)(.*)");
	
	if(likely(mode && nick))
		return re_split_nick_usermode.FullMatch(str, mode, nick);
	else if(unlikely(!mode && !nick))
		return false;
	else if(!mode) // && nick
		return re_split_nick_usermode.FullMatch(str, (void *)0, nick);
	else // if(!nick) // && mode
		return re_split_nick_usermode.FullMatch(str, mode, (void *)0);
}


bool irc_msginfo::operator==(const irc_msginfo &obj) const
{
	if(obj.chantype == this->chantype && 
		obj.msgtype == this->msgtype && 
		!irc_strcasecmp(obj.chan, this->chan))
	{
		return true;
	}
	else
	{
		return false;
	}
}

irc_privmsg::irc_privmsg(const irc_ident &ident, const irc_msginfo &msginfo, const std::string &msg)
	:irc_ident(ident), irc_msginfo(msginfo)
{
	static pcrecpp::RE	re_action_message("^\1ACTION (.*)\1$"), 
						re_ctcp_message("^\1(.*)\1$");
	std::string tmp;
	std::string nick = this->nick;
	
	if(msginfo.getmsgtype() == IRC_MSGTYPE_NOTICE)
	{
		if(re_ctcp_message.FullMatch(msg, &tmp))
		{
			this->setmsgtype(IRC_MSGTYPE_CTCPREPLY);
			this->msg = tmp;
		}
		else
		{
			this->msg = msg;
		}
	}
	else /* if(msginfo.msgtype == MSGTYPE_NORMAL) */
	{
		if(re_action_message.FullMatch(msg, &tmp))
		{
			this->setmsgtype(IRC_MSGTYPE_ACTION);
			this->msg = tmp;
		}
		else if(re_ctcp_message.FullMatch(msg, &tmp))
		{
			this->setmsgtype(IRC_MSGTYPE_CTCP);
			this->msg = tmp;
		}
		else
		{
			this->msg = msg;
		}
	}
	
	if(this->getchantype() != IRC_CHANTYPE_DCC)
	{
		if(msginfo.getchan()[0] == '#' || msginfo.getchan()[0] == '&')
			this->setchantype(IRC_CHANTYPE_CHAN);
		else
		{
			this->setchantype(IRC_CHANTYPE_QUERY);
			this->setchan(ident.getnick());
		}
	}
}












