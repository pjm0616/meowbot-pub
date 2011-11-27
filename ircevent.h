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


#ifndef IRCEVENT_H_INCLUDED
#define IRCEVENT_H_INCLUDED


class irc;

enum IRCEVENT
{
	IRCEVENT_NONE = -1, 
	IRCEVENT_IDLE = 0, 
	IRCEVENT_CONNECTED, 
	IRCEVENT_LOGGEDIN, 
	IRCEVENT_DISCONNECTED, 
	IRCEVENT_JOIN, 
	IRCEVENT_PART, 
	IRCEVENT_KICK, 
	IRCEVENT_QUIT, 
	IRCEVENT_NICK, 
	IRCEVENT_PRIVMSG, 
	IRCEVENT_MODE, 
	IRCEVENT_INVITE, 
	IRCEVENT_SRVINCOMING, 
	IRCEVENT_END
};
enum BOTEVENT
{
	BOTEVENT_START = IRCEVENT_END + 1, 
	BOTEVENT_COMMAND, 
	BOTEVENT_EVHANDLER_DONE, // special botevent: called when event handleing is finished
	BOTEVENT_PRIVMSG_SEND, 
	BOTEVENT_END
};

static const char *const gsc_botevent_2str_tab[] = 
{
	"IRCEVENT_IDLE", "IRCEVENT_CONNECTED", "IRCEVENT_LOGGEDIN", "IRCEVENT_DISCONNECTED", 
	"IRCEVENT_JOIN", "IRCEVENT_PART", "IRCEVENT_KICK", "IRCEVENT_QUIT", 
	"IRCEVENT_NICK", "IRCEVENT_PRIVMSG", "IRCEVENT_MODE", "IRCEVENT_INVITE", 
	"IRCEVENT_SRVINCOMING", "IRCEVENT_END", 
	"BOTEVENT_START", "BOTEVENT_COMMAND", "BOTEVENT_EVHANDLER_DONE", "BOTEVENT_END", 
	NULL
};


struct ircevent
{
	ircevent(): event_id(IRCEVENT_NONE)
	{
	}
	ircevent(int event_id): event_id(event_id)
	{
	}
	virtual ~ircevent()
	{
	}
	virtual ircevent *duplicate(){return new ircevent(this->event_id);}
	int event_id;
};

struct ircevent_idle: public ircevent
{
	ircevent_idle()
		:ircevent(IRCEVENT_IDLE){}
	virtual ircevent_idle *duplicate(){return this;}
};
struct ircevent_connected: public ircevent
{
	ircevent_connected()
		:ircevent(IRCEVENT_CONNECTED){}
	virtual ircevent_connected *duplicate(){return this;}
};
struct ircevent_loggedin: public ircevent
{
	ircevent_loggedin()
		:ircevent(IRCEVENT_LOGGEDIN){}
	virtual ircevent_loggedin *duplicate(){return this;}
};
struct ircevent_disconnected: public ircevent
{
	ircevent_disconnected()
		:ircevent(IRCEVENT_DISCONNECTED){}
	virtual ircevent_disconnected *duplicate(){return this;}
};
struct ircevent_join: public ircevent
{
	ircevent_join(const irc_ident &who, const std::string &chan)
		:ircevent(IRCEVENT_JOIN), who(who), chan(chan){}
	virtual ircevent_join *duplicate(){return new ircevent_join(who, chan);}
	irc_ident who; std::string chan;
};
struct ircevent_part: public ircevent
{
	ircevent_part(const irc_ident &who, const std::string &chan, const std::string &msg)
		:ircevent(IRCEVENT_PART), who(who), chan(chan), msg(msg){}
	virtual ircevent_part *duplicate(){return new ircevent_part(who, chan, msg);}
	irc_ident who; std::string chan, msg;
};
struct ircevent_kick: public ircevent
{
	ircevent_kick(const irc_ident &who, const std::string &chan, const irc_user &kicked_user, const std::string &msg)
		:ircevent(IRCEVENT_KICK), who(who), chan(chan), kicked_user(kicked_user), msg(msg){}
	virtual ircevent_kick *duplicate(){return new ircevent_kick(who, chan, kicked_user, msg);}
	irc_ident who;
	std::string chan;
	irc_user kicked_user;
	std::string msg;
};
struct ircevent_quit: public ircevent
{
	ircevent_quit(const irc_ident &who, const std::string &msg)
		:ircevent(IRCEVENT_QUIT), who(who), msg(msg){}
	virtual ircevent_quit *duplicate(){return new ircevent_quit(who, msg);}
	irc_ident who; std::string msg;
};
struct ircevent_nick: public ircevent
{
	ircevent_nick(const irc_ident &who, const std::string &newnick)
		:ircevent(IRCEVENT_NICK), who(who), newnick(newnick){}
	virtual ircevent_nick *duplicate(){return new ircevent_nick(who, newnick);}
	irc_ident who; std::string newnick;
};
struct ircevent_privmsg: public ircevent, public irc_privmsg
{
	ircevent_privmsg(const irc_ident &ident, const irc_msginfo &msginfo, const std::string &msg)
		:ircevent(IRCEVENT_PRIVMSG), irc_privmsg(ident, msginfo, msg){}
	ircevent_privmsg(const std::string &ident, const irc_msginfo &msginfo, const std::string &msg)
		:ircevent(IRCEVENT_PRIVMSG), irc_privmsg(irc_ident(ident), msginfo, msg){}
	virtual ircevent_privmsg *duplicate()
	{
		return new ircevent_privmsg(this->getident(), this->getmsginfo(), this->getmsg());
	}
};
struct ircevent_mode: public ircevent
{
	// a!b@c, #chan, +l, 255
	ircevent_mode(const irc_ident &who, const std::string &target, const std::string &mode, const std::string &modeparam)
		:ircevent(IRCEVENT_MODE), who(who), target(target), mode(mode), modeparam(modeparam){}
	virtual ircevent_mode *duplicate(){return new ircevent_mode(who, target, mode, modeparam);}
	irc_ident who; std::string target, mode, modeparam;
};
struct ircevent_invite: public ircevent
{
	ircevent_invite(const irc_ident &who, const std::string &chan)
		:ircevent(IRCEVENT_INVITE), who(who), chan(chan){}
	virtual ircevent_invite *duplicate(){return new ircevent_invite(who, chan);}
	irc_ident who; std::string chan;
};
struct ircevent_srvincoming: public ircevent
{
	ircevent_srvincoming(const std::string &line, bool is_line_converted)
		:ircevent(IRCEVENT_SRVINCOMING), line(line), is_line_converted(is_line_converted){}
	virtual ircevent_srvincoming *duplicate(){return new ircevent_srvincoming(line, is_line_converted);}
	std::string line;
	bool is_line_converted;
};


struct botevent_command: public ircevent
{
	botevent_command(irc_privmsg &pmsg, irc_msginfo &mdest, std::string &cmd_trigger, std::string &cmd, std::string &carg)
		:ircevent(BOTEVENT_COMMAND), pmsg(pmsg), mdest(mdest), cmd_trigger(cmd_trigger), cmd(cmd), carg(carg){}
	virtual botevent_command *duplicate(){return new botevent_command(pmsg, mdest, cmd_trigger, cmd, carg);} // unsafe
	
	irc_privmsg &pmsg;
	irc_msginfo &mdest;
	std::string &cmd_trigger, &cmd, &carg;
};

struct botevent_evhandler_done: public ircevent
{
	botevent_evhandler_done(ircevent *ev)
		:ircevent(BOTEVENT_EVHANDLER_DONE), ev(ev){}
	virtual botevent_evhandler_done *duplicate(){return new botevent_evhandler_done(ev);} // unsafe
	
	ircevent *ev;
};

struct botevent_privmsg_send: public ircevent
{
	botevent_privmsg_send(const irc_msginfo &mdest, const std::string &msg)
		:ircevent(BOTEVENT_PRIVMSG_SEND), mdest(mdest), msg(msg){}
	virtual botevent_privmsg_send *duplicate(){return new botevent_privmsg_send(mdest, msg);} // unsafe
	
	irc_msginfo mdest;
	std::string msg;
};





typedef int (irc_event_cb_t)(irc *self, ircevent *evdata, void *userdata);


// XX FIXME
static inline void delete_ircevent(ircevent *evdata)
{
	if(evdata)
	{
		int evid = evdata->event_id;
		if(!(
			evid == IRCEVENT_IDLE || 
			evid == IRCEVENT_CONNECTED || 
			evid == IRCEVENT_LOGGEDIN || 
			evid == IRCEVENT_DISCONNECTED || 
			evid == BOTEVENT_EVHANDLER_DONE
			))
		{
			delete evdata;
		}
	}
}







#endif // IRCEVENT_H_INCLUDED

