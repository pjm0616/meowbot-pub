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

#include <pthread.h>
#include <sys/sysinfo.h> // sysinfo()

#include <pcrecpp.h>

#include "defs.h"
#include "module.h"
#include "ircbot.h"





static int cmd_cb_basic(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	const std::string &chan = pmsg.getchan();
	const std::string &nick = pmsg.getnick();
	caseless_string cmd_nc(cmd.c_str());
	
	if(cmd_nc == "봇나가" || cmd_nc == "냐옹이나가")
	{
		if(chan == "#." || chan == "#/" || chan == "#\\" || 
			chan == "#SIGSEGV" || chan == "#Only")
		{
			isrv->privmsg(mdest, "시져시져 'ㅅ'");
			goto end;
		}
		isrv->rdlock_chanuserlist();
		const irc_strmap<std::string, irc_user> &names = isrv->get_chanuserlist(chan);
		irc_strmap<std::string, irc_user>::const_iterator it;
		int oper_exists = 0, allowed = 1;
		for(it = names.begin(); it != names.end(); it++)
		{
			if(oper_exists == 0 && strstr(it->second.getchanusermode().c_str(), "@"))
			{
				oper_exists = 1;
				allowed = 0;
			}
			if(oper_exists)
			{
				if(strstr(it->second.getchanusermode().c_str(), "@") && it->second.getnick() == nick)
				{
					allowed = 1;
					break;
				}
			}
		}
		isrv->unlock_chanuserlist();
		if(allowed == 0)
		{
			isrv->privmsg(mdest, "옵이 있는 채널에서는 옵을 가진 사람만 봇을 퇴장시킬 수 있습니다.");
			goto end;
		}
		
		if(carg.empty())
		{
			isrv->privmsg(mdest, " !"+cmd+" ㅇ  <- 을(를) 입력하면 나갑니다.");
		}
		else
		{
			isrv->leave_channel(chan, "Leaving [by "+nick+"]");
		}
	}
	else if(cmd_nc == "사용자목록")
	{
		isrv->rdlock_chanuserlist();
		const irc_strmap<std::string, irc_user> &names = isrv->get_chanuserlist(chan);
		irc_strmap<std::string, irc_user>::const_iterator it;
		std::string tmp;
		int i = 1;
		for(it = names.begin(); it != names.end(); it++)
		{
			tmp += it->second.getnick() + ", ";
			if((i % 10) == 0)
			{
				isrv->privmsg(mdest, tmp);
				tmp = "";
			}
			i++;
		}
		isrv->unlock_chanuserlist();
		isrv->privmsg_nh(mdest, tmp);
		isrv->privmsgf(mdest, "총 %d명", i - 1);
	}
	else if(cmd_nc == "인원수")
	{
		isrv->rdlock_chanuserlist();
		const irc_strmap<std::string, irc_user> &list = isrv->get_chanuserlist(chan);
		isrv->privmsgf(mdest, "%d명", list.size());
		isrv->unlock_chanuserlist();
	}
	else if(cmd_nc == "컴업타임")
	{
		struct sysinfo si;
		sysinfo(&si);
		int updays, uphours, upminutes, upseconds;
		updays = (int)(si.uptime / (60 * 60 * 24));
		uphours = (int)((si.uptime / (60 * 60)) % 24);
		upminutes = (int)((si.uptime / 60) % 60);
		upseconds = (int)(si.uptime % 60);
		
		isrv->privmsgf(mdest, "업타임: %d일 %d시간 %d분 %d초", 
			updays, uphours, upminutes, upseconds);
	}
	else if(cmd_nc == "ipsn")
	{
		if(carg.empty())
		{
			isrv->privmsg(mdest, "현재 채널에서 지정한 IP를 사용하고 있는 사람을 검색합니다. 사용법: !ipsn <IP>");
			return 0;
		}
		isrv->rdlock_chanuserlist();
		const irc_strmap<std::string, irc_user> &names = isrv->get_chanuserlist(chan);
		irc_strmap<std::string, irc_user>::const_iterator it;
		std::string tmp;
		for(it = names.begin(); it != names.end(); it++)
		{
			if(it->second.gethost() == carg)
			{
				tmp += it->second.getnick();
				tmp += " ";
			}
		}
		isrv->unlock_chanuserlist();
		if(tmp.empty())
			isrv->privmsg_nh(mdest, "찾지 못했습니다.");
		else
			isrv->privmsg_nh(mdest, tmp);
	}
	
end:
	return HANDLER_FINISHED;
}


#if 0
class flood_timer
{
public:
	flood_timer(int n = 5, int penalty = 2)
	{
		pthread_mutexattr_t mattr;
		pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&this->lock, &mattr);
		
		this->init(n, penalty);
	}
	~flood_timer()
	{
	}
	
	// 1초 내에 n번 이상 사용하며면 p초만큼 중지됨
	int init(int n, int penalty)
	{
		pthread_mutex_lock(&this->lock);
		this->prev_now = 0;
		this->next_tm = 0;
		this->cnt = 0;
		this->penalty = penalty;
		this->max = penalty * n;
		pthread_mutex_unlock(&this->lock);
		
		return 0;
	}
	
	// >0: 허용
	// <0: 중지
	// 1, -1: 전과 같은 상태 지속
	// 2, -2: 전과 상태가 바뀜
	int check()
	{
		int result = 0;
		time_t now;
		
		pthread_mutex_lock(&this->lock);
		now = time(0);
		
		if(now < this->prev_now)
		{
			// clock skew
			this->next_tm = now;
			// goto `else'
		}
		if(this->next_tm - now >= this->max)
		{
			if(this->cnt == 0)
			{
				this->cnt = 1; // 
				result = -2;
			}
			else
				result = -1;
			//this->cnt++; // tick()으로 옮김
		}
		else
		{
			if(this->next_tm < now)
				this->next_tm = now;
			
			if(this->cnt != 0)
			{
				this->next_tm = now; // 정지->허용으로 바뀌었을 때 1초내 n번 이하 호출에도 정지되는것을 막기 위해 리셋하고 다시 시작함.
				result = 2;
			}
			else
				result = 1;
			this->cnt = 0;
		}
		this->prev_now = now;
		// this->next_tm += this->penalty; // tick()으로 옮김
		pthread_mutex_unlock(&this->lock);
		
		return result;
	}
	
	// cnt가 mcnt이하일 경우에만 next_tm을 증가시킴. mcnt가 -1이면 항상 증가
	int tick(int mcnt = -1)
	{
		pthread_mutex_lock(&this->lock);
		int ret = this->check();
		if(this->cnt <= (unsigned int)mcnt) // 이미 중지됐을 경우 더이상 지연하지 않음
		{
			this->next_tm += this->penalty;
			if(ret == -1)
				this->cnt++;
		}
		pthread_mutex_unlock(&this->lock);
		
		return ret;
	}
	
	// 자동 초기화
	int tick(int n, int penalty, int mcnt = -1)
	{
		if(this->penalty != penalty || this->max != penalty * n)
			this->init(n, penalty);
		return this->tick(mcnt);
	}
	
private:
	time_t next_tm, prev_now;
	int max, penalty;
	unsigned int cnt;
	pthread_mutex_t lock;
	
};


static irc_strmap<std::string, flood_timer> g_chan_cmd_flood_timers;
static irc_strmap<std::string, flood_timer> g_nick_cmd_flood_timers;
pthread_mutex_t g_floodtmr_mutex;

static int irc_event_handler_botflood(ircbot_conn *isrv, ircevent *evdata, void *)
{
	(void)self;
	int etype = evdata->event_id;
	
	if(etype == BOTEVENT_COMMAND)
	{
		botevent_command *ev = static_cast<botevent_command *>(evdata);
		
		if(
			//&& isrv->is_botadmin(ev->getident())
			//&& 0
			)
		{
			//닉네임별 제한
			{
				//pthread_mutex_lock(&g_floodtmr_mutex);
				flood_timer &ft = g_nick_cmd_flood_timers[ev->getchan()];
				int r = ft.tick(6, 10, 0);
				//pthread_mutex_unlock(&g_floodtmr_mutex);
				if(r == -2)
					isrv->privmsgf_nh(*ev, "도배로 인해 %s의 명령어 사용이 10초간 중지됩니다.", ev->getnick().c_str());
				if(r < 0)
					return HANDLER_FINISHED;
			}
			// 채널별 제한
			{
				//pthread_mutex_lock(&g_floodtmr_mutex);
				flood_timer &ft = g_chan_cmd_flood_timers[ev->getchan()];
				int r = ft.tick(12, 10, 0);
				//pthread_mutex_unlock(&g_floodtmr_mutex);
				if(r == -2)
					isrv->privmsg_nh(*ev, "도배로 인해 이 채널에서 명령어 사용이 10초간 중지됩니다.");
				if(r < 0)
					return HANDLER_FINISHED;
			}
		}
	}
	else if(etype == IRCEVENT_PART)
	{
		ircevent_part *ev = static_cast<ircevent_part *>(evdata);
		const std::string &nick = ev->who.getnick();
		irc_strmap<std::string, flood_timer>::iterator it;
		
		pthread_mutex_lock(&g_floodtmr_mutex);
		if(nick == isrv->my_nick())
		{
			it = g_chan_cmd_flood_timers.find(ev->chan);
			g_chan_cmd_flood_timers.erase(it);
		}
		else
		{
			it = g_nick_cmd_flood_timers.find(nick);
			g_nick_cmd_flood_timers.erase(it);
		}
		pthread_mutex_unlock(&g_floodtmr_mutex);
	}
	else if(etype == IRCEVENT_KICK)
	{
		ircevent_kick *ev = static_cast<ircevent_kick *>(evdata);
		const std::string &nick = ev->who.getnick();
		irc_strmap<std::string, flood_timer>::iterator it;
		
		pthread_mutex_lock(&g_floodtmr_mutex);
		if(nick == isrv->my_nick())
		{
			it = g_chan_cmd_flood_timers.find(ev->chan);
			g_chan_cmd_flood_timers.erase(it);
		}
		else
		{
			it = g_nick_cmd_flood_timers.find(nick);
			g_nick_cmd_flood_timers.erase(it);
		}
		pthread_mutex_unlock(&g_floodtmr_mutex);
	}
	else if(etype == IRCEVENT_QUIT)
	{
		ircevent_quit *ev = static_cast<ircevent_quit *>(evdata);
		const std::string &nick = ev->who.getnick();
		
		pthread_mutex_lock(&g_floodtmr_mutex);
		irc_strmap<std::string, flood_timer>::iterator it;
		it = g_nick_cmd_flood_timers.find(nick);
		g_nick_cmd_flood_timers.erase(it);
		pthread_mutex_unlock(&g_floodtmr_mutex);
	}
	else if(etype == IRCEVENT_NICK)
	{
		ircevent_nick *ev = static_cast<ircevent_nick *>(evdata);
		const std::string &nick = ev->who.getnick();
		
		pthread_mutex_lock(&g_floodtmr_mutex);
		irc_strmap<std::string, flood_timer>::iterator it;
		g_nick_cmd_flood_timers[ev->newnick] = g_nick_cmd_flood_timers[nick];
		it = g_nick_cmd_flood_timers.find(nick);
		g_nick_cmd_flood_timers.erase(it);
		pthread_mutex_unlock(&g_floodtmr_mutex);
		
	}
	
	return HANDLER_GO_ON;
}
#endif


static const char *const cmdlist_basic[] = 
{
	"봇나가", "냐옹이나가", "사용자목록", "인원수", "컴업타임", "ipsn", 
	NULL
};

int mod_basic_init(ircbot *bot)
{
	int i;
	
	//bot->register_event_handler(irc_event_handler_botflood, IRC_PRIORITY_MIN);
	for(i = 0; cmdlist_basic[i]; i++)
	{
		bot->register_cmd(cmdlist_basic[i], cmd_cb_basic);
	}
	//pthread_mutex_init(&g_floodtmr_mutex, NULL);
	
	return 0;
}

int mod_basic_cleanup(ircbot *bot)
{
	int i;
	
	//bot->unregister_event_handler(irc_event_handler_botflood);
	for(i = 0; cmdlist_basic[i]; i++)
	{
		bot->unregister_cmd(cmdlist_basic[i]);
	}
	
	return 0;
}



MODULE_INFO(mod_basic, mod_basic_init, mod_basic_cleanup)




