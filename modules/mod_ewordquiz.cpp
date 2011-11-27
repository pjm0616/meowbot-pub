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
#include <string.h>
#include <assert.h>

#include <pthread.h>
#include <pcrecpp.h>
#include <boost/algorithm/string/replace.hpp>

#include "defs.h"
#include "utf8.h"
#include "hangul.h"
#include "etc.h"
#include "module.h"
#include "ircbot.h"

// THIS MUST BE UPDATED IF ./data/static/kwordlist.txt HAS CHANGED!!!
#define KWORDLIST_FILE "./data/static/kwordlist.txt"
#define KWORDLIST_CNT 261561
// THIS MUST BE UPDATED IF ./data/static/kwordlist_ewq.txt HAS CHANGED!!!
#define KWORDLIST_EWQ_FILE "./data/static/kwordlist_ewq.txt"
#define KWORDLIST_EWQ_CNT 190935

static char **kwordlist;
static char **kwordlist_ewq;

static char **load_wordlist(const char *file, int wordcnt)
{
	int i = 0, len;
	char *buf, *save_ptr, *p;
	char **result;
	
	result = new char *[wordcnt];
	buf = readfile(file, &len);
	if(!buf)
		return NULL;
	buf[len] = 0;
	
	p = strtok_r(buf, "\n", &save_ptr);
	do
	{
		result[i] = p;
		i++;
		if(i > wordcnt)
		{
			puts("wordlist count exceeded");
			
			break;
		}
	} while((p = strtok_r(NULL, "\n", &save_ptr)) != NULL);
	
	return result;
}

static int free_wordlist(char **wordlist)
{
	delete[] wordlist[0];
	delete[] wordlist;
	
	return 0;
}






static int word_compare_function(const void *key, const void *item)
{
	// key < item: -1
	// key == item: 0
	// ket > item: 1
	
	return strcmp((const char *)key, *(const char **)item);
}

static int is_word_exists(char **wordlist, int wordcnt, const char *word)
{
	const char **ret;
	
	if(!word || word[0] == 0)
		return -1; 
	
	ret = (const char **)bsearch(word, reinterpret_cast<const char *>(wordlist), wordcnt, sizeof(char *), word_compare_function);
	
	return ret?1:0;
}

static const char *get_random_word(char **wordlist, int wordcnt)
{
	return wordlist[rand() % wordcnt];
}


static int word_compare_function_find(const void *key, const void *item)
{
	return memcmp(key, *(const void **)item, strlen((const char *)key));
}

#if 0
static const char *find_word_by_startword(const char **wordlist, int cnt, const char *startword)
{
	const char **ret;
	
	if(!startword || startword[0] == 0)
		return NULL;
	
	ret = (const char **)bsearch(startword, reinterpret_cast<const char *>(wordlist), cnt, sizeof(char *), word_compare_function_find);
	if(!ret)
		return NULL;
	
	return *ret;
}
#endif

static const char *find_word_by_startword_random(char **wordlist, int wordcnt, const char *startword)
{
	const char **start, *ret;
	
	if(!startword || startword[0] == 0)
		return NULL;
	
	start = (const char **)bsearch(startword, reinterpret_cast<const char *>(wordlist), wordcnt, sizeof(char *), word_compare_function_find);
	if(!start)
		return NULL;
	
	int pos = ((const char *)start - (const char *)wordlist) / sizeof(const char *);
	int cnt = (wordcnt - pos);
	int swlen = strlen(startword);
	int i;
	for(i = 0; start[i] && i < cnt; i++)
	{
		if(strncmp(startword, (const char *)start[i], swlen))
		{
			i = rand() % i;
			break;
		}
		if(rand() % 100 == 50)
			break;
	}
	ret = start[i];
	
	return ret;
}

#define is_word_exists_ewq(_word) is_word_exists(kwordlist_ewq, KWORDLIST_EWQ_CNT, _word)
#define get_random_word_ewq() get_random_word(kwordlist_ewq, KWORDLIST_EWQ_CNT)
#define find_word_by_startword_random_ewq(_startword) find_word_by_startword_random(kwordlist_ewq, KWORDLIST_EWQ_CNT, _startword)

#if 1
// 20080103 lua에서 쓰기 위해
extern "C" const char *get_random_word(const char *startword)
{
	if(startword)
		return find_word_by_startword_random_ewq(startword);
	else
		return get_random_word_ewq();
}
#endif


struct ewordq_stat
{
	ewordq_stat()
		:status(0)
	{}
	~ewordq_stat(){}
	int status, score;
	int total_n, crnt_n;
	std::string lastword;
	time_t timeout, time;
	std::map<std::string, int> userstat;
};

static irc_strmap<std::string, ewordq_stat> g_ewqstat;
static pthread_rwlock_t g_ewq_lock;


static int irc_event_handler(ircbot_conn *isrv, ircevent *evdata, void *)
{
	int etype = evdata->event_id;
	
	if(etype == IRCEVENT_IDLE)
	{
		time_t ctime = time(NULL);
		pthread_rwlock_wrlock(&g_ewq_lock); //
		for(std::map<std::string, ewordq_stat>::iterator it = 
				g_ewqstat.begin(); 
			it != g_ewqstat.end(); it++)
		{
			if(it->second.status == 1)
			{
				if(ctime - it->second.time > it->second.timeout)
				{
					std::string chan(it->first);
					if(it->second.crnt_n >= it->second.total_n)
					{
						it->second.status = 0;
						isrv->privmsg_nh(chan, "15회 초과");
					}
					else
					{
						const char *nword = find_word_by_startword_random_ewq(
							utf8_strpos_s(it->second.lastword, -1).c_str());
						if(!nword)
						{
							it->second.status = 0;
							isrv->privmsg_nh(chan, "내가 졌다능 orz");
						}
						else
						{
							it->second.crnt_n++;
							it->second.lastword = nword;
							isrv->privmsgf_nh(chan, "%d초가 지났습니다.", g_ewqstat[chan].timeout);
							isrv->privmsgf_nh(chan, IRCTEXT_BOLD"%s", g_ewqstat[chan].lastword.c_str());
							it->second.time = time(NULL);
						}
					}
				}
			}
		}
		pthread_rwlock_unlock(&g_ewq_lock);
	}
	else if(etype == IRCEVENT_PRIVMSG)
	{
		ircevent_privmsg *pmsg = static_cast<ircevent_privmsg *>(evdata);
		if(pmsg == NULL)
			return HANDLER_GO_ON;
		irc_msgtype msgtype = pmsg->getmsgtype();
		const std::string &chan = pmsg->getchan();
		const std::string &nick = pmsg->getnick();
		std::string msg = strip_irc_colors(pmsg->getmsg());
		
		if(msgtype != IRC_MSGTYPE_CTCP && chan[0] == '#')
		{
			pthread_rwlock_rdlock(&g_ewq_lock);
			ewordq_stat &ewq = g_ewqstat[chan];
			pthread_rwlock_unlock(&g_ewq_lock);
			if(ewq.status == 1)
			{
				pthread_rwlock_wrlock(&g_ewq_lock);
				int print_word = 0;
				boost::algorithm::replace_all(msg, " ", "");
				
				if(utf8_strlen_s(msg) > 2 && is_word_exists_ewq(msg.c_str()))
				{
					std::string firstchar = apply_head_pron_rule(utf8_strpos_s(msg, 0));
					std::string lastchar = apply_head_pron_rule(utf8_strpos_s(ewq.lastword, -1));
					
					if(firstchar == lastchar)
					{
						ewq.userstat[nick] += 1;
						isrv->privmsg_nh(chan, "정답: " + msg);
						ewq.crnt_n++;
						print_word = 1;
					}
				}
				
				std::map<std::string, int>::const_iterator it;
				for(it = ewq.userstat.begin(); it != ewq.userstat.end(); it++)
				{
					if(it->second >= ewq.score)
					{
						ewq.status = 0;
						isrv->privmsg_nh(chan, it->first + 
							select_josa(it->first, "이가") + " 이겼습니다.");
						return 0;
					}
				}
				
				if(ewq.crnt_n >= ewq.total_n)
				{
					ewq.status = 0;
					isrv->privmsg_nh(chan, "15회 초과");
					return 0;
				}
				if(print_word == 1 && ewq.status == 1)
				{
					const char *nword = find_word_by_startword_random_ewq(utf8_strpos_s(msg, -1).c_str());
					if(!nword)
					{
						isrv->privmsg_nh(chan, "내가 졌다능 orz");
						ewq.status = 0;
						pthread_rwlock_unlock(&g_ewq_lock);
						return 0;
					}
					ewq.lastword = nword;
					isrv->privmsgf_nh(chan, IRCTEXT_BOLD"%s", ewq.lastword.c_str());
					g_ewqstat[chan].time = time(NULL);
				}
				pthread_rwlock_unlock(&g_ewq_lock);
			}
		}
	}
	
	return HANDLER_GO_ON;
}

static int cmd_cb_ewq(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	(void)carg;
	
	if(cmd == "단어확인")
	{
		if(carg.empty())
		{
			isrv->privmsg_nh(mdest, "단어가 사전에 있는지 검색합니다. | 사용법: !단어확인 <단어>");
			return 0;
		}
		if(is_word_exists(kwordlist, KWORDLIST_CNT, carg.c_str()))
			isrv->privmsgf_nh(mdest, "%s%s 존재하는 단어입니다.", 
				carg.c_str(), select_josa(carg, "은는").c_str());
		else
			isrv->privmsgf_nh(mdest, "%s%s 찾을 수 없습니다.", 
				carg.c_str(), select_josa(carg, "을를").c_str());
		
		return 0;
	}
	if(cmd == "끝말치트")
	{
		const char *ret = find_word_by_startword_random_ewq(carg.c_str());
		if(ret)
			isrv->privmsgf_nh(mdest, ": %s", ret);
		else
			isrv->privmsg_nh(mdest, "찾을 수 없습니다.");
		
		return 0;
	}
	
	const std::string &chan = pmsg.getchan();
	if(chan[0] != '#')
	{
		return 0;
	}
	pthread_rwlock_wrlock(&g_ewq_lock);
	if(g_ewqstat[chan].status == 0)
	{
		g_ewqstat[chan].status = 1;
		g_ewqstat[chan].total_n = 15;
		g_ewqstat[chan].crnt_n = 0;
		g_ewqstat[chan].score = 10;
		g_ewqstat[chan].timeout = 20; //30;
		g_ewqstat[chan].time = time(NULL);
		g_ewqstat[chan].lastword = get_random_word_ewq();
		g_ewqstat[chan].userstat.clear();
		isrv->privmsg_nh(chan, "시작. 중지하려면 `!끝말2 중지'를 입력하세요.");
		isrv->privmsgf_nh(chan, IRCTEXT_BOLD"%s", g_ewqstat[chan].lastword.c_str());
	}
	else if(carg == "중지" || carg == "stop")
	{
		g_ewqstat[chan].status = 0;
		isrv->privmsg_nh(chan, "끝");
	}
	pthread_rwlock_unlock(&g_ewq_lock);
	
	return HANDLER_FINISHED;
}



int mod_ewordquiz_init(ircbot *bot)
{
	kwordlist = load_wordlist(KWORDLIST_FILE, KWORDLIST_CNT);
	if(kwordlist == NULL)
		return -1;
	kwordlist_ewq = load_wordlist(KWORDLIST_EWQ_FILE, KWORDLIST_EWQ_CNT);
	if(kwordlist_ewq == NULL)
		return -1;
	bot->register_event_handler(irc_event_handler);
	bot->register_cmd("끝말2", cmd_cb_ewq);
	bot->register_cmd("끝말치트", cmd_cb_ewq);
	bot->register_cmd("단어확인", cmd_cb_ewq);
	
	pthread_rwlock_init(&g_ewq_lock, NULL);
	
	return 0;
}

int mod_ewordquiz_cleanup(ircbot *bot)
{
	free_wordlist(kwordlist);
	free_wordlist(kwordlist_ewq);
	bot->unregister_event_handler(irc_event_handler);
	bot->unregister_cmd("끝말2");
	bot->unregister_cmd("끝말치트");
	bot->unregister_cmd("단어확인");
	
	return 0;
}



MODULE_INFO(mod_ewordquiz, mod_ewordquiz_init, mod_ewordquiz_cleanup)




