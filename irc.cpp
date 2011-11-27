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
#include <list>
#include <map>
#include <deque>
#include <algorithm>

#include <cstring>
#include <ctime>
#include <cstdarg>
#include <assert.h>

#include <pthread.h>
#include <pcrecpp.h>



#include "defs.h"
#include "tcpsock.h"
#include "ircutils.h"
#include "ircdefs.h"
#include "ircstring.h"
#include "irc.h"
#include "ircbot.h"



static ircevent_idle g_ircevent_idle;
static ircevent_connected g_ircevent_connected;
static ircevent_loggedin g_ircevent_loggedin;
static ircevent_disconnected g_ircevent_disconnected;



static int dummy_event_cb_fxn(irc *self, ircevent *evdata, void *data)
{
	(void)self;
	(void)data;
	delete_ircevent(evdata);
	
	return 0;
}

irc::irc()
	:
	status(IRCSTATUS_DISCONNECTED),
	port(0),
	server(""),
	nick(""),
	username("user"),
	realname("user"),
	server_pass(""),
	srv_encoding(""),
	conv_m2s(NULL),
	conv_s2m(NULL), 
	event_cb(dummy_event_cb_fxn),
	event_cb_userdata(NULL), 
	send_thread_id(0), 
	send_ready(false), 
	sendq_added(false), 
	preserve_eilseq_line(false), 
	enable_flood_protection(true)
{
	pthread_rwlock_init(&chanlist_lock, NULL);
	pthread_rwlock_init(&chanuserlist_lock, NULL);
	pthread_rwlock_init(&encconv_lock, NULL);
	
#ifdef IRC_IRCBOT_MOD
	this->pause_irc_main_thread_on = 0;
#endif
}

irc::~irc()
{
	if(this->status == IRCSTATUS_CONNECTED || 
		this->status == IRCSTATUS_CONNECTING)
	{
		this->quit();
		this->disconnect();
	}
	if(this->conv_m2s)
	{
		delete this->conv_m2s;
		this->conv_m2s = NULL;
	}
	if(this->conv_s2m)
	{
		delete this->conv_s2m;
		this->conv_s2m = NULL;
	}
}

int irc::connect(const std::string &server, int port)
{
	if(this->status != IRCSTATUS_DISCONNECTED)
	{
		return -1;
	}
	
	this->status = IRCSTATUS_CONNECTING;
	this->quit_on_timer = 0;
	int ret = this->sock.connect(server, port);
	if(ret < 0)
	{
		this->status = IRCSTATUS_DISCONNECTED;
		return -1;
	}
	
	this->wrlock_chanlist();
	this->chans.clear();
	this->unlock_chanlist();
	
	this->wrlock_chanuserlist();
	this->chan_users.clear();
	this->unlock_chanuserlist();
	
	this->server = server;
	this->port = port;
	
	this->srvinfo.uhnames_on = false;
	this->srvinfo.namesx_on = false;
	this->srvinfo.netname.clear();
	this->srvinfo.chantypes.clear();
	this->srvinfo.chanmodes.clear();
	this->srvinfo.chanusermode_prefixes.clear();
	this->srvinfo.chanusermodes.clear();
	this->srvinfo.max_channels = 20;
	this->srvinfo.nicklen = 31;
	this->srvinfo.casemapping.clear();
	this->srvinfo.charset.clear();
	
	this->create_send_thread();
	
	this->send_event(&g_ircevent_connected);
	
	return 0;
}

int irc::change_nick(const std::string &nick)
{
	if(this->status != IRCSTATUS_CONNECTED)
	{
		this->nick = nick;
		return 1;
	}
	else
	{
		this->quote("NICK " + nick);
		return 0;
	}
}

int irc::join_channel(const std::string &channel)
{
	if(this->status != IRCSTATUS_CONNECTED)
		return -1;
	this->quote("JOIN " + channel);
	
	return 0;
}

// thread safe
int irc::leave_channel(const std::string &channel, const std::string &reason)
{
	if(this->status != IRCSTATUS_CONNECTED)
		return -1;
	
	if(!this->is_joined(channel))
		return -2;

	if(reason.empty())
		this->quote("PART " + channel);
	else
		this->quote("PART " + channel + " :" + reason);
	
	return 0;
}

int irc::quit(const std::string &quitmsg)
{
	if(this->status == IRCSTATUS_DISCONNECTED)
	{
		return -1;
	}
	else
	{
		this->quit_on_timer = time(NULL);
		if(quitmsg.empty())
		{
			this->quote("QUIT", IRC_PRIORITY_MAX);
		}
		else
		{
			this->quote("QUIT :" + quitmsg, IRC_PRIORITY_MAX);
		}
		// this->disconnect();
	}
	
	return 0;
}

void irc::set_event_callback(irc_event_cb_t *fxn, void *data)
{
	this->event_cb = fxn;
	this->event_cb_userdata = data;
}

int irc::send_event(ircevent *evdata)
{
	return this->event_cb(this, evdata, this->event_cb_userdata);
}

int irc::privmsg(const irc_msginfo &dest, const std::string &msg)
{
	if(this->status != IRCSTATUS_CONNECTED)
		return -1;
	bool convert_enc = !dest.is_line_converted();
	
	if(!msg.empty() && dest.getchantype() != IRC_CHANTYPE_DCC)
	{
		int priority = dest.getpriority();
		switch(dest.getmsgtype())
		{
		case IRC_MSGTYPE_NORMAL:
			this->quotef(priority, convert_enc, "PRIVMSG %s :%s", 
				dest.getchan().c_str(), msg.c_str());
			break;
		case IRC_MSGTYPE_NOTICE:
			this->quotef(priority, convert_enc, "NOTICE %s :%s", 
				dest.getchan().c_str(), msg.c_str());
			break;
		case IRC_MSGTYPE_ACTION:
			this->quotef(priority, convert_enc, "PRIVMSG %s :\1ACTION %s\1", 
				dest.getchan().c_str(), msg.c_str());
			break;
		case IRC_MSGTYPE_CTCP:
			this->quotef(priority, convert_enc, "PRIVMSG %s :\1%s\1", 
				dest.getchan().c_str(), msg.c_str());
			break;
		case IRC_MSGTYPE_CTCPREPLY:
			this->quotef(priority, convert_enc, "NOTICE %s :\1%s\1", 
				dest.getchan().c_str(), msg.c_str());
			break;
		default:
			return -2;
			break;
		}
	}
	
	return 0;
}

int irc::login_to_server()
{
	assert(this->status == IRCSTATUS_CONNECTING);
	
	if(!this->server_pass.empty())
		this->quote("PASS :" + this->server_pass);
	this->quote("NICK " + this->nick);
	this->quote("USER " + this->username + " 8 * :" + this->realname);

	return 0;
}

// chanlist: thread unsafe, chanuserlist: thread safe
int irc::add_to_chanlist(const std::string &chan)
{
	std::vector<std::string>::iterator it;
	
	if(chan.empty())
		return -1;
	
	for(it = this->chans.begin(); it != this->chans.end(); it++)
	{
		if(!irc_strcasecmp(*it, chan))
		{
			// assert(!"irc::add_to_chanlist: channel already exists");
			return -1;
		}
	}
	
	this->chans.push_back(chan);
	
	/////////////////////
	this->wrlock_chanuserlist();
	this->clear_chanuserlist(chan);
	this->unlock_chanuserlist();
	
	return 0;
}

// chanlist: thread unsafe, chanuserlist: thread safe
int irc::del_from_chanlist(const std::string &chan)
{
	std::vector<std::string>::iterator it;
	
	if(chan.empty())
		return -1;
	
	for(it = this->chans.begin(); it != this->chans.end(); it++)
	{
		if(!irc_strcasecmp(*it, chan))
		{
			this->chans.erase(it);
			
			/////////////////////
			this->wrlock_chanuserlist();
			this->clear_chanuserlist(chan);
			this->unlock_chanuserlist();
			
			return 0;
		}
	}
	// assert(!"irc::del_from_chanlist: channel not exists");
	
	return -1;
}

// thread safe
bool irc::is_joined(const std::string &chan, bool autolock /*=true*/)
{
	std::vector<std::string>::const_iterator it;
	
	if(chan.empty())
		return -1;
	
	if(autolock)
		this->rdlock_chanlist();
	for(it = this->chans.begin(); it != this->chans.end(); it++)
	{
		if(!irc_strcasecmp(*it, chan))
		{
			if(autolock)
				this->unlock_chanlist();
			return 1;
		}
	}
	if(autolock)
		this->unlock_chanlist();
	
	return 0;
}

// thread unsafe
int irc::add_to_chanuserlist(const std::string &chan, const std::string &nick, 
	const std::string &ident /*=""*/)
{
	irc_strmap<std::string, irc_user> &users = this->chan_users[chan];
	std::string realnick;
	
	if(chan.empty() || nick.empty())
		return -1;
	
	split_nick_usermode(nick, NULL, &realnick);
	
	// 반드시 nick을 나중에. nick에는 채널의 유저 모드를 포함할 수 있음
	if(!ident.empty())
		users[realnick].set_ident(ident);
	users[realnick].set_nick(nick);
	
	return 0;
}

// thread unsafe
int irc::del_from_chanuserlist(const std::string &chan, const std::string &nick)
{
	irc_strmap<std::string, irc_user> &users = this->chan_users[chan];
	irc_strmap<std::string, irc_user>::iterator it;
	std::string realnick;
	
	if(chan.empty() || nick.empty())
		return -1;
	
	split_nick_usermode(nick, NULL, &realnick);
	
	it = users.find(realnick);
	if(it != users.end())
	{
		users.erase(it);
		return 0;
	}
	// assert(!"irc::del_from_chanuserlist: nick not found on channel list");
	this->refresh_chanuserlist(chan);
	
	return -1;
}

// thread unsafe
void irc::del_from_chanuserlist_all(const std::string &nick)
{
	std::vector<std::string>::const_iterator it;
	
	for(it = this->chans.begin(); it != this->chans.end(); it++)
	{
		if(is_user_joined(*it, nick, false)) // do not lock
			this->del_from_chanuserlist(*it, nick);
	}
}

/////
// (none)			WHO
// NAMESX			NAMES
// UHNAMES			WHO->NAMES
// NAMESX UHNAMES	NAMES
/////
int irc::refresh_chanuserlist(const std::string &chan)
{
	if(this->srvinfo.namesx_on)
		this->quote("NAMES " + chan);
	else
		this->quote("WHO " + chan);
	
	return 0;
}

// thread safe
bool irc::is_user_joined(const std::string &chan, const std::string &nick, 
	bool autolock /*=true*/)
{
	static pcrecpp::RE re_extract_nick("^[~&@%+]*(.*)");
	
	if(chan.empty() || nick.empty())
		return -1;
	
	if(autolock)
		this->rdlock_chanuserlist();
	irc_strmap<std::string, irc_user> &users = this->chan_users[chan];
	irc_strmap<std::string, irc_user>::const_iterator it;
	std::string realnick;
	
	if(!re_extract_nick.FullMatch(nick, &realnick))
		realnick = nick;
	
	it = users.find(realnick);
	if(it == users.end())
	{
		if(autolock)
			this->unlock_chanuserlist();
		return false;
	}
	if(autolock)
		this->unlock_chanuserlist();
	return true;
}

// thread unsafe
const irc_user *irc::get_user_data(const std::string &chan, const std::string &nick)
{
	irc_strmap<std::string, irc_user> &users = this->chan_users[chan];
	irc_strmap<std::string, irc_user>::const_iterator it;
	std::string realnick;
	
	if(chan.empty() || nick.empty())
		return NULL;
	
	it = users.find(nick);
	if(it == users.end())
		return NULL;
		
	return &it->second;
}

// thread unsafe
int irc::clear_chanuserlist(const std::string &chan)
{
	this->chan_users[chan].clear();
	
	return 0;
}

// TODO: 510바이트 제한으로 인해 잘린 부분 처리
//// PRIVMSG에서 미리 인코딩 변환 후 줄이 길경우 여러줄로 보내기
int irc::quote(const char *line, int priority, bool convert_enc)
{
	char buf[IRC_MAX_LINE_SIZE + 3]; // +"\r\n\0"
	char c, *dst = buf;
	const char *src;
	unsigned int len;
	
	if(this->status == IRCSTATUS_DISCONNECTED)
		return -1;
	
	const char *orig_line = line;
	assert(orig_line != NULL);
	if(this->conv_m2s && convert_enc)
	{
		pthread_rwlock_rdlock(&this->encconv_lock);
		line = this->conv_m2s->convert(line, strlen(line));
		pthread_rwlock_unlock(&this->encconv_lock);
		if(!line)
			line = orig_line; // FIXME
	}
	assert(line != NULL);
	
	src = line;
	for(len = 0; len < IRC_MAX_LINE_SIZE; )
	{
		c = *src++;
		if(c == 0)
			break;
		else if(c == '\n' || c == '\r')
			continue;
		else
		{
			*dst++ = c;
			len++;
		}
	}
	*dst++ = '\r';
	*dst++ = '\n';
	*dst = 0;
	if(line != orig_line)
	{
		delete[] line;
	}
	
	this->add_to_send_queue(buf, priority);
	if(c != 0) // MAX_IRC_LINE_SIZE 초과
	{
		return -1;
	}
	
	return 0;
}

int irc::quote(const std::string &line, int priority, bool convert_enc)
{
	return this->quote(line.c_str(), priority, convert_enc);
}

int irc::quotef(const char *fmt, ...)
{
	if(!fmt)
		return -1;
	
	va_list ap;
	va_start(ap, fmt);
	std::string result;
	s_vsprintf(&result, fmt, ap);
	va_end(ap);
	
	return this->quote(result, IRC_PRIORITY_NORMAL);
}

int irc::quotef(int priority, const char *fmt, ...)
{
	if(!fmt)
		return -1;
	
	va_list ap;
	va_start(ap, fmt);
	std::string result = s_vsprintf(fmt, ap);
	va_end(ap);
	
	return this->quote(result, priority);
}

int irc::quotef(int priority, bool convert_enc, const char *fmt, ...)
{
	if(!fmt)
		return -1;
	
	va_list ap;
	va_start(ap, fmt);
	std::string result = s_vsprintf(fmt, ap);
	va_end(ap);
	
	return this->quote(result, priority, convert_enc);
}

/* [static] */ int irc::parse_irc_line(const std::string &line, 
	std::string &prefix, std::string &cmd, std::vector<std::string> &params)
{
	enum
	{
		STATE_PREFIX, STATE_CMD, STATE_PARAM
	} state;
	unsigned int pos = 0;
	const char *p;
	char buf[IRC_MAX_LINE_SIZE], c, isspace = 0;
	
	prefix.clear();
	params.clear();
	cmd.clear();
	
	buf[0] = 0;
	p = line.c_str();
	if(*p == ':')
	{
		state = STATE_PREFIX;
		p++;
	}
	else
	{
		state = STATE_CMD;
	}
	#define COMMIT_BUFFER \
		if(pos != 0) \
		{ \
			buf[pos] = 0; \
			if(state == STATE_PARAM) \
			{ \
				params.push_back(buf); \
			} \
			else if(state == STATE_CMD) \
			{ \
				cmd = irc_strupper(buf); \
				state = STATE_PARAM; \
			} \
			else if(state == STATE_PREFIX) \
			{ \
				prefix = buf; \
				state = STATE_CMD; \
			} \
			buf[0] = 0; \
			pos = 0; \
		}
	
	for(; ; p++)
	{
		c = *p;
		if(c == 0)
		{
			COMMIT_BUFFER;
			break;
		}
		else if(c == ' ')
		{
			isspace = 1;
			COMMIT_BUFFER;
		}
		else if(c == ':' && isspace != 0 && state == STATE_PARAM)
		{
			params.push_back(p + 1);
			break;
		}
		else
		{
			if(pos >= sizeof(buf) - 1) /* pos >= 511 - overflow */
			{
				COMMIT_BUFFER;
			}
			else
			{
				buf[pos++] = c;
				isspace = 0;
			}
		}
	}
	#undef COMMIT_BUFFER
	
	return (state == STATE_PARAM)?0:-1;
}

static std::string concat_param(const std::vector<std::string> &params, int start)
{
	std::string buf;
	for(std::vector<std::string>::const_iterator it = params.begin() + start; 
		it != params.end(); it++)
	{
		if(it->empty())
			continue;
		if(it != params.begin())
			buf += " ";
		buf += *it;
	}
	
	return buf;
}

int irc::main_loop()
{
	int ret, len, result;
	std::string line, ident, cmd;
	std::vector<std::string> params;
	bool is_line_converted;
	unsigned int nparam;
	irc_ident origin;
	
	static pcrecpp::RE re_extract_mode_chars("([~&@%+]+)");
	
	std::string chan, nick, username, host;
	std::map<std::string, bool> names_receiving, who_receiving;
	
	result = 0;
	this->last_ping = this->last_pong = time(NULL);
	while(1)
	{
		len = this->sock.readline(line, 1000); // timeout = 1 second
		if(this->quit_on_timer != 0)
		{
			if(this->quit_on_timer + 5 > time(NULL))
			{
				// QUIT를 보냈으나 5초동안 응답이 없을경우
				goto end;
			}
		}
		if(len <= -2) // socket error
		{
			if(this->quit_on_timer == 0)
				result = -1; // 비정상 종료
			goto end;
		}
		else if(len == 0) // empty line
		{
			continue;
		}
		else if(len == -1) // timed out
		{
			time_t crnttime = time(NULL);
			if(crnttime - this->last_pong > 60*6) // 6 minutes
			{
				printf("Disconnecting: Ping timeout [bot -> server]: %ld seconds\n", crnttime - this->last_pong);
				result = -1;
				goto end;
			}
			if(crnttime - this->last_ping > 60*5) // 5 minutes
			{
				this->quotef("PING :PING%u", crnttime);
				this->last_ping = crnttime;
			}
			if(this->status == IRCSTATUS_CONNECTED)
			{
				this->send_event(&g_ircevent_idle);
			}
			continue;
		}
		
		if(this->conv_s2m)
		{
			pthread_rwlock_rdlock(&this->encconv_lock);
			//// this->conv_s2m->convert(line, line, true); // keep original string if invalid sequence is found
			//// this->conv_s2m->convert(line, line, false); // skips invalid sequence
			ret = this->conv_s2m->convert(line, line, this->preserve_eilseq_line);
			pthread_rwlock_unlock(&this->encconv_lock);
			is_line_converted = (ret == 0);
		}
		this->send_event(new ircevent_srvincoming(line, is_line_converted));
		ret = parse_irc_line(line, ident, cmd, params);
		if(ret != 0)
		{
			// invalid line
			continue;
		}
		origin.set_ident(ident);
		nparam = params.size();
		if(nparam < 32) // XXX FIXME
		{
			// FIXME: 일단 넉넉하게 -_-
			params.resize(32);
		}
		
#define CHECK_PARAM_MIN_CNT_GT(n) ({ if(nparam < n) continue; })
		if(cmd == "PING")
		{
			this->quote("PONG " + concat_param(params, 0), IRC_PRIORITY_MAX);
		}
		else if(cmd == "PRIVMSG")
		{
			CHECK_PARAM_MIN_CNT_GT(2);
			//// params[0] = chan
			//// params[1] = msg
			if(ident.empty())
				continue; // server message
			
			this->send_event(new ircevent_privmsg(origin, 
				irc_msginfo(params[0], IRC_MSGTYPE_NORMAL, IRC_CHANTYPE_CHAN, 
					is_line_converted), 
				params[1]));
		}
		else if(cmd == "NOTICE")
		{
			CHECK_PARAM_MIN_CNT_GT(2);
			//// params[0] = chan
			//// params[1] = msg
			if(ident.empty())
				continue; // server notice
			
			this->send_event(new ircevent_privmsg(origin, 
				irc_msginfo(params[0], IRC_MSGTYPE_NOTICE, IRC_CHANTYPE_CHAN, 
					is_line_converted), 
				params[1]));
		}
		else if(cmd == "MODE")
		{
			CHECK_PARAM_MIN_CNT_GT(2);
			//// params[0] = chan
			//// params[1] = mode
			//// params[2~] = mode params
			chan = params[0];
			const char *modes = params[1].c_str();
			char sign, mode, newmode[2] = {0, };
			sign = modes[0];
			if(sign == 0)
				continue;
			bool do_mode_refresh = false;
			
			// FIXME: 005의 mode목록을 파싱해서 처리
			int mi = 1, pi = 2;
			for(; ; mi++)
			{
				mode = modes[mi];
				if(mode == 0)
					break;
				if(mode == '+' || mode == '-')
				{
					sign = mode;
				}
				else if(mode == 'q' || mode == 'a' || 
					mode == 'o' || mode == 'h' || mode == 'v')
				{
					nick = params[pi++];
					if(nick.empty())
						break;
					
					if(mode == 'q')
						newmode[0] = '~';
					else if(mode == 'a')
						newmode[0] = '&';
					else if(mode == 'o')
						newmode[0] = '@';
					else if(mode == 'h')
						newmode[0] = '%';
					else if(mode == 'v')
						newmode[0] = '+';
					
					this->wrlock_chanuserlist();
					irc_user &user = this->chan_users[chan][nick];
					std::string crntmode = user.getchanusermode();
					if(sign == '+')
					{
						user.add_to_chanusermode(newmode);
					}
					else
					{
						user.del_from_chanusermode(newmode);
						do_mode_refresh = true; // 현재 채널의 사용자 모드를 다시 받음
					}
					this->unlock_chanuserlist();
					
					// send event
					{
						char buf[3] = {sign, mode, 0};
						this->send_event(new ircevent_mode(origin, chan, buf, nick));
					}
				}
				else if(
					// 
					mode == 'b' || // ban
					mode == 'd' || // realnameban
					mode == 'e' || // banexemption
					mode == 'I' || // inviteexemption
					mode == 'q' || // quiteuser
					mode == 'g' || // censoredwords
					// 
					(mode == 'l' && sign == '+') || // chanuserlimit
					mode == 'f' || // flood
					mode == 'J' || // jointhrottle/rejoindelay
					mode == 'k' ||  // channelkey
					mode == 'F' || // nickflood
					//mode == 'j' || // jointhrottle/jupechannel(freenode, noarg)
					//mode == 'L' || // forward/largelists(freenode, noarg)
					0)
				{
					if(params[pi].empty())
					{
						pi++;
						break;
					}
					
					// send event
					{
						char buf[3] = {sign, mode, 0};
						this->send_event(
							new ircevent_mode(origin, chan, buf, params[pi]));
					}
					
					pi++;
				}
				else
				{
					// +cgimnpPQrRstzDd, etc...
					// +jL (on freenode)
					// send event
					{
						char buf[3] = {sign, mode, 0};
						this->send_event(new ircevent_mode(origin, chan, buf, ""));
					}
				}
			} /* for(; ; mi++) */
			if(do_mode_refresh)
			{
#if 0
				// 심한 랙으로 인해 비활성화
				// qaohv 모드 삭제로 인해 지워진 모드를 복구
				if(this->srvinfo.namesx_on == false)
					this->quote("NAMES " + chan); // XXX 대량 모드 변경시 문제발생.
#endif
			}
		}
		else if(cmd == "JOIN")
		{
			CHECK_PARAM_MIN_CNT_GT(1);
			chan = params[0];
			nick = origin.getnick();
			if(nick == this->nick)
			{
				this->wrlock_chanlist();
				this->add_to_chanlist(chan);
				this->unlock_chanlist();
				if(!this->srvinfo.namesx_on)
					this->refresh_chanuserlist(chan); // "WHO $chan"
				this->quote("MODE " + chan);
			}
			else
			{
				this->wrlock_chanuserlist();
				this->add_to_chanuserlist(chan, nick, ident);
				this->unlock_chanuserlist();
			}
			this->send_event(new ircevent_join(origin, chan));
		}
		else if(cmd == "PART")
		{
			CHECK_PARAM_MIN_CNT_GT(1);
			//// params[0] = chan
			//// params[1] = part message
			chan = params[0];
			nick = origin.getnick();
			if(nick == this->nick)
			{
				this->wrlock_chanlist();
				this->del_from_chanlist(chan);
				this->unlock_chanlist();
			}
			else
			{
				this->wrlock_chanuserlist();
				this->del_from_chanuserlist(chan, nick);
				this->unlock_chanuserlist();
			}
			this->send_event(new ircevent_part(origin, chan, params[1]));
		}
		else if(cmd == "KICK")
		{
			CHECK_PARAM_MIN_CNT_GT(2);
			//// params[0] = channel
			//// params[1] = nick
			//// params[2] = reason
			chan = params[0];
			nick = params[1];
			
			this->rdlock_chanuserlist();
			const irc_strmap<std::string, irc_user> &list = this->get_chanuserlist(chan);
			irc_strmap<std::string, irc_user>::const_iterator it = list.find(nick);
			if(it == list.end())
			{
				this->unlock_chanuserlist();
				this->refresh_chanuserlist(chan);
				fprintf(stderr, "kicked user is not in channel nicklist");
				continue;
			}
			irc_user kicked_user(it->second);
			this->unlock_chanuserlist();
			
			if(nick == this->nick)
			{
				this->wrlock_chanlist();
				this->del_from_chanlist(chan);
				this->unlock_chanlist();
			}
			else
			{
				this->wrlock_chanuserlist();
				this->del_from_chanuserlist(chan, nick);
				this->unlock_chanuserlist();
			}
			//this->send_event(new ircevent_kick(origin, chan, nick, params[2]));
			this->send_event(new ircevent_kick(origin, chan, kicked_user, params[2]));
		}
		else if(cmd == "NICK")
		{
			CHECK_PARAM_MIN_CNT_GT(1);
			//// params[0] = newnick
			const std::string &newnick = params[0];
			nick = origin.getnick();
			if(nick == this->nick)
			{
				this->nick = newnick;
			}
			
			std::vector<std::string>::const_iterator it;
			std::string newnick2;
			this->rdlock_chanlist();
			for(it = this->chans.begin(); it != this->chans.end(); it++)
			{
				if(is_user_joined(*it, nick))
				{
					this->wrlock_chanuserlist();
					const irc_user *iu_tmp = this->get_user_data(*it, nick);
					if(iu_tmp)
						newnick2 = iu_tmp->getchanusermode() + newnick;
					else
						newnick2 = newnick;
					
					this->del_from_chanuserlist(*it, nick);
					// nick overrides ident
					this->add_to_chanuserlist(*it, newnick2, ident);
					this->unlock_chanuserlist();
				}
			}
			this->unlock_chanlist();
			
			this->send_event(new ircevent_nick(origin, newnick));
		}
		else if(cmd == "QUIT")
		{
			//// params[0] = quit message
			nick = origin.getnick();
			if(nick == this->nick)
			{
				std::cout << "QUIT: " << params[0] << std::endl;
				if(this->quit_on_timer == 0)
					result = -1; // 비정상 종료
				goto end;
				// never reached
			}
			else
			{
#ifdef IRC_IRCBOT_MOD
				// ircbot 클래스에서 이벤트 처리가 끝난 후에 irc::ircbot_event_handler에서 처리함
				// (quit_handler에서 종료한 사람의 채널 목록을 얻기 위해 필요함)
				// ircbot 클래스에서 이벤트 처리가 끝날때까지 기다림.
				// PING/PONG이 늦으면 곤란하기 때문에 quit_handler는 재빨리 처리를 해줘야함 - _-
				this->pause_irc_main_thread_on = 1;
				this->send_event(new ircevent_quit(origin, params[0]));
#if 0
				while(this->pause_irc_main_thread_on != 0)
					usleep(100);
#else
				// 최대 5초까지 대기하고 10초내에 안끝날경우 무시하고 진행
				for(int cnt = 0; this->pause_irc_main_thread_on != 0 && cnt < 100; cnt++)
					usleep(50);
				if(this->pause_irc_main_thread_on)
					this->pause_irc_main_thread_on = 0;
#endif
				this->pause_irc_main_thread_on = 0;
#else
				this->wrlock_chanuserlist();
				this->del_from_chanuserlist_all(nick);
				this->unlock_chanuserlist();
				
				this->send_event(new ircevent_quit(origin, params[0]));
#endif
			}
		}
		else if(cmd == "352") // WHO list
		{
			if(!this->srvinfo.namesx_on) // ignore WHO if server supports NAMESX
			{
				CHECK_PARAM_MIN_CNT_GT(7);
				//// params[1] = chan
				//// params[2] = username
				//// params[3] = host
				//// params[4] = server
				//// params[5] = nick
				//// params[6] = mode
				//// params[7] = realname
				chan = params[1];
				nick = params[5];
				username = params[2];
				host = params[3];
				if(!(chan[0] == '#' || chan[0] == '&'))
				{
					continue;
				}
				
				if(who_receiving[chan] == false)
				{
					who_receiving[chan] = true;
					this->wrlock_chanuserlist();
					this->clear_chanuserlist(chan);
					this->unlock_chanuserlist();
				}
				std::string mode;
				if(!re_extract_mode_chars.PartialMatch(params[6], &mode))
					mode.clear();
			
				this->wrlock_chanuserlist();
				this->add_to_chanuserlist(chan, mode + nick, 
					nick + "!" + username + "@" + host);
				this->unlock_chanuserlist();
			}
		}
		else if(cmd == "315") // End of WHO list.
		{
			if(!this->srvinfo.namesx_on)
			{
				CHECK_PARAM_MIN_CNT_GT(2);
				//// params[1] = chan
				chan = params[1];
				if(!(chan[0] == '#' || chan[0] == '&'))
				{
					continue;
				}
				
				who_receiving[chan] = false;
				if(this->srvinfo.uhnames_on)
					this->quote("NAMES " + chan); // 채널 사용자들의 모드를 얻음. (names 는 모든 모드가 표시됨)
			}
		}
		else if(cmd == "353") // NAMES reply
		{
			// namesx_on == false 일 경우 여기서 names 는 who로 얻은 사용자 목록의 오류(유저의 모드)를 고치는 역할만 함
			CHECK_PARAM_MIN_CNT_GT(4);
			//// params[2] = chan
			//// params[3] = space-seperated list
			chan = params[2];
			if(!(chan[0] == '#' || chan[0] == '&'))
			{
				continue;
			}
			if(who_receiving[chan])
			{
				std::cerr << "received names reply while receiving who list" << std::endl;
				continue;
			}
			if(names_receiving[chan] == false)
			{
				names_receiving[chan] = true;
				
				if(this->srvinfo.namesx_on)
				{
					this->wrlock_chanuserlist();
					this->clear_chanuserlist(chan);
					this->unlock_chanuserlist();
				}
			}
			
			// parse space-seperated list
			const char *p = params[3].c_str(), *startp = p;
			std::string arg;
			for(; ; p++)
			{
				if(*p == 0 || *p == ' ')
				{
					arg.assign(startp, p - startp);
					this->wrlock_chanuserlist();
					if(this->srvinfo.namesx_on)
					{
						irc_ident tmp(arg);
						static pcrecpp::RE re_mode_and_nick("^([~&@%+]*[^!]+)");
						std::string modenick;
						re_mode_and_nick.Match(arg, &modenick);
						this->add_to_chanuserlist(chan, modenick, tmp.getident());
					}
					else
					{
						this->add_to_chanuserlist(chan, arg);
					}
					this->unlock_chanuserlist();
					startp = p + 1;
					if(*p == 0 || *startp == 0)
						break;
				}
			}
		}
		else if(cmd == "366") // End of NAMES list.
		{
			CHECK_PARAM_MIN_CNT_GT(1);
			//// params[1] = chan
			chan = params[1];
			if(!(chan[0] == '#' || chan[0] == '&'))
			{
				continue;
			}
			names_receiving[chan] = false;
		}
		else if(cmd == "ERROR")
		{
			std::cerr << "Server error: " << params[0] << std::endl;
			if(this->quit_on_timer == 0)
				result = -1; // 비정상 종료
			else
				result = 0;
			goto end;
		}
		else if(cmd == "INVITE")
		{
			CHECK_PARAM_MIN_CNT_GT(2);
			//// params[0] = my nick
			//// params[1] = chan
			if(params[0] == this->nick)
			{
				this->send_event(new ircevent_invite(origin, params[1]));
			}
		}
		else if(cmd == "PONG")
		{
			//time_t lag = time(NULL) - this->last_pong;
			this->last_pong = time(NULL);
		}
		else if(cmd == "433") // Nick already in use
		{
			if(this->status == IRCSTATUS_CONNECTING)
			{
				this->nick += "_";
				this->quote("NICK " + this->nick);
			}
		}
		else if(cmd == "474")
		{
			CHECK_PARAM_MIN_CNT_GT(2);
			//// params[1] = chan
			//// params[2] = reason
			std::cout << "Cannot join " << params[1] << ": " 
				<< params[2] << std::endl;
		}
		else if(cmd == "403") // No such channel
		{
			//// params[1] = chan
		}
		else if(cmd == "TOPIC")
		{
			//// params[0] = chan
			//// params[1] = topic
		}
		else if(cmd == "005")
		{
			//// params[0] = my nick
			//// params[1~nparam-2] = isupport
			//// params[nparam-1] = string ":are supported by this server"
			
			/*
:irc.pjm0616.wo.tc 005 asdf WALLCHOPS WALLVOICES MODES=19 CHANTYPES=# PREFIX=(qaohv)~&@%+ MAP MAXCHANNELS=20 MAXBANS=60 VBANLIST NICKLEN=31 CASEMAPPING=rfc1459 STATUSMSG=@%+ CHARSET=ascii :are supported by this server
:irc.pjm0616.wo.tc 005 asdf TOPICLEN=307 KICKLEN=255 MAXTARGETS=20 AWAYLEN=200 CHANMODES=Ibe,k,Ll,CGKMNOQRSTVcimnpst FNC NETWORK=Localnet MAXPARA=32 ELIST=MU EXCEPTS=e INVEX=I UHNAMES NAMESX :are supported by this server
			*/
			
			static pcrecpp::RE re_005_param("^([^=]+)=(.*)$");
			std::string pname, parg;
			for(size_t i = 1; i < nparam-1; i++)
			{
				if(params[i] == "UHNAMES")
				{
					// ~&@nick
					this->srvinfo.uhnames_on = true;
					this->quote("PROTOCTL UHNAMES", IRC_PRIORITY_ANORMAL);
				}
				else if(params[i] == "NAMESX")
				{
					// @nick!user@host
					this->srvinfo.namesx_on = true;
					this->quote("PROTOCTL NAMESX", IRC_PRIORITY_ANORMAL);
				}
				else if(re_005_param.FullMatch(params[i], &pname, &parg))
				{
					if(pname == "NETWORK")
						this->srvinfo.netname = parg;
					else if(pname == "CHANTYPES")
						this->srvinfo.chantypes = parg;
					else if(pname == "CHANMODES")
						this->srvinfo.chanmodes = parg;
					else if(pname == "PREFIX")
					{
						static pcrecpp::RE re_split_005_prefix("^\\(([a-z]+)\\)(.+)$");
						re_split_005_prefix.FullMatch(parg, 
							&this->srvinfo.chanusermode_prefixes, 
							&this->srvinfo.chanusermodes);
					}
					else if(pname == "MAXCHANNELS")
						this->srvinfo.max_channels = atoi(parg.c_str());
					else if(pname == "NICKLEN")
						this->srvinfo.nicklen = atoi(parg.c_str());
					else if(pname == "CASEMAPPING")
						this->srvinfo.casemapping = parg;
					else if(pname == "CHARSET")
						this->srvinfo.charset = parg;
				}
			}
		}
		else if(cmd == "324") // :irc.server 324 nick #. +stn
		{
			//// params[0] = my nick
			//// params[1] = chan
			//// params[2] = channel mode
			
		}
		else if(cmd == "482") // You're not channel operator
		{
			
		}
#if 0
		// send event after "005"
		else if(cmd == "004" && this->status == IRCSTATUS_CONNECTING)
		{
			this->status = IRCSTATUS_CONNECTED;
			this->send_event(&g_ircevent_loggedin);
		}
#else
		else if((cmd == "376" || cmd == "422") && this->status == IRCSTATUS_CONNECTING)
		{
			this->status = IRCSTATUS_CONNECTED;
			this->send_event(&g_ircevent_loggedin);
		}
#endif
		else
		{
			static const char *ignored_cmds[] = {
				"001", "002", "003", "004", "251", "254", "255", "265", 
				"266", "375", "372", "465", "020", "042", "376", "422", 
				"252", "253", "332", "333", "301", "329", 
				NULL};
			bool ignored_cmd = false;
			for(int i = 0; ignored_cmds[i]; i++)
			{
				if(cmd == ignored_cmds[i])
				{
					ignored_cmd = true;
					break;
				}
			}
			if(!ignored_cmd)
				std::cerr << "$$ " << line << "" << std::endl;
		}
	} /* while(1) */
#undef CHECK_PARAM_MIN_CNT_GT
	
end:
	this->disconnect();
	
	return result;
}

int irc::disconnect()
{
	this->kill_send_thread();
	this->sock.close();
	
	// 20080113 disconnect되었을때 채널 목록이 필요하기 때문에 접속 시 목록을 지우도록 수정
	// this->chan_users.clear();
	// this->chans.clear();
	
	if(this->status == IRCSTATUS_CONNECTED || 
		this->status == IRCSTATUS_CONNECTING)
	{
		this->send_event(&g_ircevent_disconnected);
	}
	this->status = IRCSTATUS_DISCONNECTED;
	
	return 0;
}

int irc::set_encoding(const std::string &enc)
{
	int result = 0;
	
	pthread_rwlock_wrlock(&this->encconv_lock);
	if(this->conv_m2s)
	{
		delete this->conv_m2s;
		this->conv_m2s = NULL;
	}
	if(this->conv_s2m)
	{
		delete this->conv_s2m;
		this->conv_s2m = NULL;
	}
	this->srv_encoding = enc;
	if(!enc.empty())
	{
#if 0
		this->conv_m2s = new encconv("utf-8", enc, ICONV_TYPE_TRANSLIT);
		this->conv_s2m = new encconv(enc, "utf-8", ICONV_TYPE_TRANSLIT);
#else
		static pcrecpp::RE re_enc_opts("^(.*)//(IGNORE|TRANSLIT)$", PCRE_CASELESS);
		std::string real_enc, enc_opt;
		if(re_enc_opts.FullMatch(enc, &real_enc, &enc_opt))
		{
			// 20090823 always use //TRANSLIT flag in me->server converter
			//this->conv_m2s = new encconv("utf-8//"+enc_opt, real_enc, encconv::ICONV_TYPE_NONE);
			this->conv_m2s = new encconv("utf-8//TRANSLIT", real_enc, encconv::ICONV_TYPE_NONE);
			
			this->conv_s2m = new encconv(enc, "utf-8", encconv::ICONV_TYPE_NONE);
		}
		else
		{
			this->conv_m2s = new encconv("utf-8", enc, encconv::ICONV_TYPE_NONE);
			this->conv_s2m = new encconv(enc, "utf-8", encconv::ICONV_TYPE_NONE);
		}
#endif
		if(!this->conv_m2s || !this->conv_s2m)
		{
			this->srv_encoding.clear();
			if(this->conv_m2s)
			{
				delete this->conv_m2s;
				this->conv_m2s = NULL;
			}
			if(this->conv_s2m)
			{
				delete this->conv_s2m;
				this->conv_s2m = NULL;
			}
		}
	}
	pthread_rwlock_unlock(&this->encconv_lock);
	
	return result;
}





int irc::add_to_send_queue(const std::string &line, int priority)
{
	if(line.empty())
		return -1;
	
#if 0
	while(this->send_ready == false)
		usleep(50);
#endif
	
	if(this->enable_flood_protection)
	{
		pthread_mutex_lock(&this->send_mutex);
		this->send_queue.push_back(irc_sendq_data(line, priority));
		this->sendq_added = true;
		pthread_cond_signal(&this->send_cond);
		pthread_mutex_unlock(&this->send_mutex);
	}
	else
	{
		this->sock.send(line.c_str());
	}
	
	return 0;
}



static bool sendq_priority_comparator(const irc_sendq_data &d1, 
	const irc_sendq_data &d2)
{
	// ..., 1, 0, -1, ... 순으로 정렬
	return d1.priority > d2.priority;
}

/* [static] */ void *irc::send_thread(void *arg)
{
	irc *self = static_cast<irc *>(arg);
	time_t next_send, prev_now;
	time_t now;
	
	pthread_mutex_lock(&self->send_mutex);
	self->send_ready = true;
	next_send = prev_now = 0;
	while(1)
	{
		// unlock, wait, lock
		pthread_cond_wait(&self->send_cond, &self->send_mutex);
		if(self->send_ready == false)
			goto out;
		
		while(!self->send_queue.empty())
		{
			now = time(NULL);
			
			if(!self->enable_flood_protection)
			{
				next_send = now;
			}
			
			//// check to see if client's `message timer' is less than
			////// current time (set to be equal if it is);
			if(next_send < now)
				next_send = now;
			
			//// while the timer is less than ten (10) seconds ahead of the
			////// current time, parse any present messages and
			if(next_send - now >= 10)
			{
				if(now < prev_now)
				{
					// clock skew
					next_send = now;
				}
				else
				{
					// (next_send - now) 가 10 미만이 될때까지 기다림
					pthread_mutex_unlock(&self->send_mutex);
					//// FIXME: (next_send - now)가 10인 경우 
					////// sleep(0)이 되어버림.
					sleep(next_send - now - 10);
					pthread_mutex_lock(&self->send_mutex);
					now = time(NULL);
				}
			}
			if(self->sendq_added == true)
			{
				std::stable_sort(self->send_queue.begin(), 
					self->send_queue.end(), sendq_priority_comparator);
				self->sendq_added = false;
			}
			const std::string &line = self->send_queue.front().line;
			self->sock.send(line.c_str());
			
			//// penalize the client by two (2)
			////// seconds for each message;
			next_send += 2;
			prev_now = now;
			if(1)
			{
				// 줄이 너무 길 경우 추가 패널티
				const char *p = line.c_str();
				size_t len = line.length();
				for(; len && *p != ' '; p++, len--){}
				next_send += len / 120;
			}
			self->send_queue.pop_front();
			if(self->send_ready == false)
				goto out;
		}
	}
	
out:
	self->send_ready = false;
	pthread_mutex_unlock(&self->send_mutex);
	return (void *)0;
}

int irc::create_send_thread()
{
	pthread_t tid;
	int ret;
	
	pthread_mutex_init(&this->send_mutex, NULL);
	pthread_cond_init(&this->send_cond, NULL);
	this->send_queue.clear();
	this->sendq_added = false;
	
	ret = pthread_create(&tid, NULL, irc::send_thread, (void *)this);
	if(ret < 0)
	{
		this->send_thread_id = 0;
		return -1;
	}
	this->send_thread_id = tid;
	while(this->send_ready == false)
		usleep(50);
	
	return 0;
}

int irc::kill_send_thread()
{
	if(this->send_thread_id == 0)
		return -1;
	
	this->send_ready = false;
	pthread_cond_signal(&this->send_cond);
	pthread_join(this->send_thread_id, NULL);
	this->send_thread_id = 0;
	this->send_queue.clear();
	this->sendq_added = false;
	
	return 0;
}



#ifdef IRC_IRCBOT_MOD
/* [static] */
int irc::ircbot_event_handler(ircbot_conn *isrv, ircevent *evdata, void *)
{
	if(evdata->event_id == BOTEVENT_EVHANDLER_DONE)
	{
		ircevent *evdata2 = static_cast<botevent_evhandler_done *>(evdata)->ev;
		int evtype = evdata2->event_id;
		
		if(evtype == IRCEVENT_QUIT)
		{
			ircevent_quit *ev = static_cast<ircevent_quit *>(evdata2);
			isrv->wrlock_chanuserlist();
			isrv->del_from_chanuserlist_all(ev->who.getnick());
			isrv->unlock_chanuserlist();
			isrv->pause_irc_main_thread_on = 0; // irc::main_thread를 다시 돌림
		}
	}
	
	
	return HANDLER_GO_ON;
}
#endif






// vim: set tabstop=4 textwidth=80:

