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
#include <algorithm>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>

#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h> // inet_ntoa()

#include <pcrecpp.h>


#include "defs.h"
#include "module.h"
#include "etc.h"
#include "tcpsock.h"
#include "irc.h"
#include "luacpp.h"
#include "ircbot.h"

#include "base64.h"
#include "md5.h"
#include "wildcard.h"

// 봇이 메시지 보내기 전에 전처리를 할 수 있지만, 봇이 바쁠 경우 메시지가 역순으로 출력될 수도 있음
//#define ENABLE_PRIVMSG_SEND_EVENT


static int cmd_cb_module_admin(ircbot_conn *isrv, irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	int ret;
	
	if(!isrv->bot->is_botadmin(pmsg.getident()))
	{
		//isrv->privmsg(mdest, "당신은 관리자가 아닙니다.");
		goto end;
	}
	
	if(cmd == "load")
	{
		if(carg.empty())
		{
			isrv->privmsg(mdest, "모듈 이름을 입력하세요");
			goto end;
		}
		ret = isrv->bot->pause_event_threads(true);
		if(ret < 0)
			return isrv->privmsg(mdest, "failed to lock event threads"), 0;
		ret = isrv->bot->load_module(carg);
		isrv->bot->pause_event_threads(false);
		if(ret < 0)
		{
			const char *errmsg = "";
			if(ret == -100) errmsg = "invalid module name";
			else if(ret == -101) errmsg = "already loaded";
			isrv->privmsgf(mdest, "failed to load module %s: %d %s", carg.c_str(), ret, errmsg);
		}
		else
			isrv->privmsg(mdest, "module " + carg + " loaded");
	}
	else if(cmd == "unload")
	{
		if(carg.empty())
		{
			isrv->privmsg(mdest, "모듈 이름을 입력하세요");
			goto end;
		}
		ret = isrv->bot->pause_event_threads(true);
		if(ret < 0)
			return isrv->privmsg(mdest, "failed to lock event threads"), 0;
		ret = isrv->bot->unload_module(carg);
		isrv->bot->pause_event_threads(false);
		if(ret < 0)
		{
			const char *errmsg = "";
			if(ret == -100) errmsg = "invalid module name";
			else if(ret == -101) errmsg = "not loaded";
			isrv->privmsgf(mdest, "failed to unload module %s: %d %s", carg.c_str(), ret, errmsg);
		}
		else
			isrv->privmsg(mdest, "module " + carg + " unloaded");
	}
	else if(cmd == "reload")
	{
		if(carg.empty())
		{
			isrv->privmsg(mdest, "모듈 이름을 입력하세요");
			goto end;
		}
		ret = isrv->bot->pause_event_threads(true);
		if(ret < 0)
			return isrv->privmsg(mdest, "failed to lock event threads"), 0;
		bool is_unloaded = true;
		ret = isrv->bot->unload_module(carg);
		if(ret < 0)
			is_unloaded = false;
		ret = isrv->bot->load_module(carg);
		isrv->bot->pause_event_threads(false);
		if(ret < 0)
		{
			const char *errmsg = "";
			if(ret == -100) errmsg = "invalid module name";
			else if(ret == -101) errmsg = "already loaded";
			isrv->privmsgf(mdest, "failed to %sload module %s: %d %s", 
				is_unloaded?"re":"", carg.c_str(), ret, errmsg);
		}
		else
		{
			isrv->privmsgf(mdest, "module %s %sloaded", carg.c_str(), is_unloaded?"re":"");
		}
	}
	else if(cmd == "sendraw")
	{
		isrv->quote(carg);
	}
	
end:
	return HANDLER_FINISHED;
}


ircbot::ircbot()
	:event_thread_pause_level(0), 
	next_connid(0), 
	max_lines_per_cmd(3)
{
	time_t now = time(NULL);
	
	this->timer_thread_id = (pthread_t)-1;
	
	pthread_rwlock_init(&this->event_handler_list_lock, NULL);
	pthread_rwlock_init(&this->cmd_handler_list_lock, NULL);
	pthread_rwlock_init(&this->cfgdata_lock, NULL);
	pthread_rwlock_init(&this->timer_lock, NULL);
	this->init_event_threads();
	this->clear_event_queue();
	
	this->timer_nextid = 0;
	this->timer_thread_state = IRCEVTHREAD_STATE_IDLE;
	this->dummy_ircbot_conn.bot = this;
	this->dummy_ircbot_conn.conn_id = this->next_connid++;
	
	//pthread_mutexattr_t mutexattr_recursive;
	//pthread_mutexattr_init(&mutexattr_recursive);
	//pthread_mutexattr_settype(&mutexattr_recursive, PTHREAD_MUTEX_RECURSIVE);
	//pthread_mutex_init(&this->event_thread_pause_lock, &mutexattr_recursive);
	//pthread_mutexattr_destroy(&mutexattr_recursive);
	
	pthread_mutex_init(&this->pause_event_threads_fxn_lock, NULL);
	pthread_mutex_init(&this->set_event_thread_cnt_fxn_lock, NULL);
	
	this->disconnect_all_ts = 0;
	
	// 통계 리셋
	this->start_ts = now;
	this->bot_statistics_last_update_time = now;
	this->statistics.total_event_cnt = 0;
	this->statistics.events_per_second = 0.0f;
	this->statistics.evpersec_counter = 0;
	
	// module admin commands
	this->register_cmd("load", cmd_cb_module_admin);
	this->register_cmd("unload", cmd_cb_module_admin);
	this->register_cmd("reload", cmd_cb_module_admin);
	this->register_cmd("sendraw", cmd_cb_module_admin);
	
#ifdef IRC_IRCBOT_MOD
	this->register_event_handler(irc::ircbot_event_handler); // ㅁㄴㅇㄹ
#endif
}

ircbot::~ircbot()
{
	// 전부 disconnect된 상태에서 dtor가 호출돼야함
	this->cleanup();
}

int ircbot::initialize()
{
	int ret;
	
	ret = this->create_event_threads(this->cfg.getint("event_threads", 1));
	if(ret < 0)
		return -1;
	if(this->create_timer_thread() == false)
		return -1;
	
	ret = this->modules.unload_all();
	if(ret > 0) // 언로드 된 모듈의 수
	{
		fprintf(stderr, "Warning: ircbot::initialize: not clean: %d modules unloaded\n", ret);
	}
	
	// load modules
	std::vector<std::string> autoload_modules;
	this->cfg.getarray(autoload_modules, "autoload_modules");
	std::vector<std::string>::const_iterator it;
	for(it = autoload_modules.begin(); it != autoload_modules.end(); it++)
	{
		if(it->empty())
			continue;
		fprintf(stderr, "DEBUG:: module %s loaded\n", it->c_str());
		this->load_module(*it);
	}
	
	return 0;
}

int ircbot::cleanup()
{
	this->disconnect_all();
	this->kill_timer_thread();
	this->irc_conns.clear();
	this->kill_event_threads();
	this->modules.unload_all();
	
	return 0;
}

static bool ircbot_check_conn_name(const std::string &name)
{
	static pcrecpp::RE re_conn_name("^[0-9a-zA-Z_-]+$");
	return re_conn_name.FullMatch(name)?true:false;
}

// FIXME
int ircbot::run()
{
	if(this->irc_conns.getsize() > 0)
	{
		fprintf(stderr, "ERROR: this->irc_conns.size > 0\n");
		return -1;
	}
	this->cfg.lock_ls();
	lua_State *L = this->cfg.Lw.lua;
	
	lua_pushliteral(L, "irc_servers");
	lua_rawget(L, LUA_GLOBALSINDEX);
	if(lua_type(L, -1) != LUA_TTABLE)
	{
		fprintf(stderr, "ERROR: error found in config file: 'irc_servers' is not a table\n");
		lua_pop(L, 1);
		this->cfg.unlock_ls();
		return -1;
	}
	int irc_servers_tbl = lua_gettop(L);
	lua_pushnil(L);
	while(lua_next(L, irc_servers_tbl) != 0) // stack: -2(key), -1(value)
	{
		if(lua_type(L, -1) != LUA_TTABLE)
		{
			fprintf(stderr, "ERROR: error found in config file 'irc_servers' table\n");
			lua_pop(L, 1);
			continue;
		}
		
		// =_=
#define lua_rawget_kstdstring(_lua, _tblidx, _key) \
({ \
	lua_pushliteral(_lua, _key); lua_rawget(_lua, (_tblidx<0)?(_tblidx-1):(_tblidx)); \
	std::string res = lua_my_tostdstring(_lua, -1); lua_pop(_lua, 1); \
	res; \
})
#define lua_rawget_kint(_lua, _tblidx, _key) \
({ \
	lua_pushliteral(_lua, _key); lua_rawget(_lua, (_tblidx<0)?(_tblidx-1):(_tblidx)); \
	int res = lua_tointeger(_lua, -1); lua_pop(_lua, 1); \
	res; \
})
#define lua_rawget_kbool(_lua, _tblidx, _key, _default_val) \
({ \
	lua_pushliteral(_lua, _key); lua_rawget(_lua, (_tblidx<0)?(_tblidx-1):(_tblidx)); \
	int res; bool ret = lua_my_toint(_lua, -1, &res); lua_pop(_lua, 1); \
	if(!ret) res = _default_val; \
	(res?true:false); \
})
		
		std::string cname(lua_my_tostdstring(L, -2));
		if(!ircbot_check_conn_name(cname))
		{
			fprintf(stderr, "ERROR: invalid botname: %s\n", cname.c_str());
			lua_pop(L, 1);
			continue;
		}
		
		ircbot_conn *ic = this->create_irc_conn();
		ic->set_name(cname);
		ic->set_nick(lua_rawget_kstdstring(L, -1, "nickname"));
		ic->set_username(lua_rawget_kstdstring(L, -1, "username"));
		ic->set_realname(lua_rawget_kstdstring(L, -1, "realname"));
		ic->set_encoding(lua_rawget_kstdstring(L, -1, "encoding"));
		ic->set_server(lua_rawget_kstdstring(L, -1, "server"), lua_rawget_kint(L, -1, "port"));
		ic->enable_flood_protection = lua_rawget_kbool(L, -1, "enable_flood_protection", true);
		{
			lua_pushliteral(L, "irc_bind_address"); lua_rawget(L, -2);
			std::string bindaddr;
			bool ret = lua_my_tostdstring(L, -1, &bindaddr);
			lua_pop(L, 1);
			if(ret)
				ic->sock.set_bind_address(bindaddr);
		}
#undef lua_rawget_kstdstring
#undef lua_rawget_kint
#undef lua_rawget_kbool
		
		lua_pop(L, 1);
	}
	lua_pop(L, 2);
	this->cfg.unlock_ls();
	
	for(size_t i = 0; i < this->irc_conns.getsize(); i++)
	{
#if 0
		if(i != 0)
			sleep(10);
#endif
		this->irc_conns[i]->launch_thread();
	}
	
	return 0;
}



int ircbot::connect_1(const char *name)
{
	if(!ircbot_check_conn_name(name))
	{
		fprintf(stderr, "ERROR: invalid botname: %s\n", name);
		return -1;
	}
	
	this->irc_conns.rdlock();
	for(size_t i = 0; i < this->irc_conns.getsize(); i++)
	{
		if(this->irc_conns[i]->get_name() == name)
		{
			this->irc_conns.unlock();
			return -1;
		}
	}
	this->irc_conns.unlock();
	
	this->cfg.lock_ls();
	lua_State *L = this->cfg.Lw.lua;
	
	lua_pushliteral(L, "irc_servers");
	lua_rawget(L, LUA_GLOBALSINDEX);
	if(lua_type(L, -1) != LUA_TTABLE)
	{
		fprintf(stderr, "ERROR: error found in config file: 'irc_servers' is not a table\n");
		lua_pop(L, 1);
		this->cfg.unlock_ls();
		return -1;
	}
	int irc_servers_tbl = lua_gettop(L);
	lua_pushstring(L, name);
	lua_rawget(L, irc_servers_tbl);
	if(lua_type(L, -1) != LUA_TTABLE)
	{
		fprintf(stderr, "ERROR: error found in config file: 'irc_servers' is not a table\n");
		lua_pop(L, 2);
		this->cfg.unlock_ls();
		return -1;
	}
	
	// =_=
#define lua_rawget_kstdstring(_lua, _tblidx, _key) \
({ \
	lua_pushliteral(_lua, _key); lua_rawget(_lua, (_tblidx<0)?(_tblidx-1):(_tblidx)); \
	std::string res = lua_my_tostdstring(_lua, -1); lua_pop(_lua, 1); \
	res; \
})
#define lua_rawget_kint(_lua, _tblidx, _key) \
({ \
	lua_pushliteral(_lua, _key); lua_rawget(_lua, (_tblidx<0)?(_tblidx-1):(_tblidx)); \
	int res = lua_tointeger(_lua, -1); lua_pop(_lua, 1); \
	res; \
})
#define lua_rawget_kbool(_lua, _tblidx, _key, _default_val) \
({ \
	lua_pushliteral(_lua, _key); lua_rawget(_lua, (_tblidx<0)?(_tblidx-1):(_tblidx)); \
	int res; bool ret = lua_my_toint(_lua, -1, &res); lua_pop(_lua, 1); \
	if(!ret) res = _default_val; \
	(res?true:false); \
})
	
	ircbot_conn *ic = this->create_irc_conn();
	ic->set_name(name);
	ic->set_nick(lua_rawget_kstdstring(L, -1, "nickname"));
	ic->set_username(lua_rawget_kstdstring(L, -1, "username"));
	ic->set_realname(lua_rawget_kstdstring(L, -1, "realname"));
	ic->set_encoding(lua_rawget_kstdstring(L, -1, "encoding"));
	ic->set_server(lua_rawget_kstdstring(L, -1, "server"), lua_rawget_kint(L, -1, "port"));
	ic->enable_flood_protection = lua_rawget_kbool(L, -1, "enable_flood_protection", true);
	{
		lua_pushliteral(L, "irc_bind_address"); lua_rawget(L, -2);
		std::string bindaddr;
		bool ret = lua_my_tostdstring(L, -1, &bindaddr);
		lua_pop(L, 1);
		if(ret)
			ic->sock.set_bind_address(bindaddr);
	}
#undef lua_rawget_kstdstring
#undef lua_rawget_kint
#undef lua_rawget_kbool
	lua_pop(L, 2);
	this->cfg.unlock_ls();
	
	ic->launch_thread();
	
	return 0;
}

static std::string ircbot_normalize_modname(const std::string &name)
{
	static pcrecpp::RE re_mod_name("^mod_[a-zA-Z0-9_]+$"), 
						re_mod_name2("^[a-zA-Z0-9_]+$");
	
	if(!re_mod_name.FullMatch(name))
	{
		if(re_mod_name2.FullMatch(name))
			return "mod_" + name;
		else
			return "";
	}
	else
		return name;
}

int ircbot::load_module(const std::string &name)
{
	std::string modname(ircbot_normalize_modname(name));
	int ret;
	
	if(modname.empty())
		return -100;
	if(this->modules.find_by_name(modname))
		return -101;
	
	//ret = this->pause_event_threads(true);
	//if(ret < 0)
	//	return -102;
#ifdef STATIC_MODULES
	ret = this->modules.load_static_module(modname, this);
#else
	ret = this->modules.load_module(std::string("./modules/") + modname + ".so", this);
#endif
	//this->pause_event_threads(false);
	if(ret < 0)
		std::cerr << "failed to load module " 
			<< modname << ": result: " << ret << std::endl;
	
	return ret;
}

int ircbot::unload_module(const std::string &name)
{
	std::string modname(ircbot_normalize_modname(name));
	
	if(modname.empty())
		return -100;
	if(!this->modules.find_by_name(modname))
		return -101;
	
	//ret = this->pause_event_threads(true);
	//if(ret < 0)
	//	return -102;
	int ret = this->modules.unload_module(modname);
	//this->pause_event_threads(false);
	
	return ret;
}

bool ircbot::register_cmd(const std::string &name, ircbot_cmd_handler_t *handler, 
	void *userdata /*=NULL*/)
{
	this->wrlock_cmd_handler_list();
	
	caseless_strmap<std::string, cmd_handler>::const_iterator it = 
		this->cmd_handlers.find(name);
	if(it == this->cmd_handlers.end())
	{
		this->cmd_handlers[name] = cmd_handler(handler, userdata);
		
		this->unlock_cmd_handler_list();
		return true;
	}
	
	this->unlock_cmd_handler_list();
	return false;
}

bool ircbot::unregister_cmd(const std::string &name)
{
	this->wrlock_cmd_handler_list();
	
	caseless_strmap<std::string, cmd_handler>::iterator it = 
		this->cmd_handlers.find(name);
	if(it == this->cmd_handlers.end())
	{
		this->unlock_cmd_handler_list();
		return false;
	}
	this->cmd_handlers.erase(it);
	
	this->unlock_cmd_handler_list();
	return true;
}

const cmd_handler *ircbot::get_cmd_handler(const std::string &name)
{
	this->rdlock_cmd_handler_list();
	caseless_strmap<std::string, cmd_handler>::const_iterator it = 
		this->cmd_handlers.find(name);
	const cmd_handler *fxn = NULL;
	if(it != this->cmd_handlers.end())
	{
		fxn = &it->second;
	}
	
	this->unlock_cmd_handler_list();
	return fxn;
}

static bool event_handler_priority_compator(const event_handler &eh1, 
	const event_handler &eh2)
{
	// ..., 1, 0, -1, .. 순으로 정렬
	return eh1.priority > eh2.priority;
}
bool ircbot::register_event_handler(ircbot_event_handler_t *handler, 
	int priority, void *userdata)
{
	std::vector<event_handler>::const_iterator it;
	int handler_found = 0;
	
	this->wrlock_event_handler_list();
	for(it = this->event_handlers.begin(); 
		it != this->event_handlers.end(); it++)
	{
		if(it->handler == handler)
		{
			handler_found = 1;
			break;
		}
	}
	if(handler_found == 0)
	{
		this->event_handlers.push_back(event_handler(handler, priority, userdata));
		std::sort(this->event_handlers.begin(), this->event_handlers.end(), 
			event_handler_priority_compator);
		
		this->unlock_event_handler_list();
		return true;
	}
	
	this->unlock_event_handler_list();
	return false;
}

bool ircbot::unregister_event_handler(ircbot_event_handler_t *handler)
{
	std::vector<event_handler>::iterator it;
	
	this->wrlock_event_handler_list();
	for(it = this->event_handlers.begin(); 
		it != this->event_handlers.end(); it++)
	{
		if(it->handler == handler)
		{
			this->event_handlers.erase(it);
			std::sort(this->event_handlers.begin(), this->event_handlers.end(), 
				event_handler_priority_compator);
			
			this->unlock_event_handler_list();
			return true;
		}
	}
	this->unlock_event_handler_list();
	return false;
}

static bool ircbot_timer_sort_comparator(const ircbot_timer_info &o1, 
	const ircbot_timer_info &o2)
{
	return o1.next < o2.next;
}
int ircbot::sort_timers()
{
	std::sort(this->timers.begin(), this->timers.end(), ircbot_timer_sort_comparator);
	return 0;
}
int ircbot::signal_timer_thread()
{
	// TODO
	return 0;
}

int ircbot::add_timer(const std::string &name, ircbot_timer_cb_t *fxn, time_t interval, 
	void *userdata, unsigned int attr)
{
	if(unlikely(fxn == NULL))
		return -1;
	
	this->wrlock_timers();
#if 0
	if(!(attr & IRCBOT_TIMERATTR_ONCE) && 
		(this->find_timer(name) != this->timers.end())
		)
	{
		this->unlock_timers();
		return -1;
	}
#endif
	
	int tmrid = this->timer_nextid++;
	if(this->timer_nextid < 0)
		this->timer_nextid = 0;
	
	this->timers.push_back(ircbot_timer_info(tmrid, name, fxn, interval, attr, userdata));
	this->sort_timers();
	this->unlock_timers();
	this->signal_timer_thread();
	
	return tmrid;
}

int ircbot::del_timer(const std::string &name, bool do_lock)
{
	if(do_lock) this->wrlock_timers();
	
	std::vector<ircbot_timer_info>::iterator it = this->timers.begin();
	for(; it != this->timers.end(); )
	{
		if(it->name == name)
		{
			this->timers.erase(it);
		}
		else
			it++;
	}
	
	this->sort_timers();
	if(do_lock) this->unlock_timers();
	if(do_lock) this->signal_timer_thread();
	return 0;
}
int ircbot::del_timer_by_cbptr(ircbot_timer_cb_t *fxn, bool do_lock)
{
	if(do_lock) this->wrlock_timers();
	
	std::vector<ircbot_timer_info>::iterator it = this->timers.begin();
	for(; it != this->timers.end(); )
	{
		if(it->fxn == fxn)
		{
			this->timers.erase(it);
		}
		else
			it++;
	}
	
	this->sort_timers();
	if(do_lock) this->unlock_timers();
	if(do_lock) this->signal_timer_thread();
	return 0;
}

std::vector<ircbot_timer_info>::iterator ircbot::find_timer(const std::string &name, bool do_lock)
{
	if(do_lock) this->rdlock_timers();
	
	std::vector<ircbot_timer_info>::iterator it = this->timers.begin();
	for(; it != this->timers.end(); it++)
	{
		if(it->name == name)
		{
			if(do_lock) this->unlock_timers();
			return it;
		}
	}
	
	if(do_lock) this->unlock_timers();
	return this->timers.end();
}
std::vector<ircbot_timer_info>::iterator ircbot::find_cbtimer(ircbot_timer_cb_t *fxn, bool do_lock)
{
	if(do_lock) this->rdlock_timers();
	
	std::vector<ircbot_timer_info>::iterator it = this->timers.begin();
	for(; it != this->timers.end(); it++)
	{
		if(it->fxn == fxn)
		{
			if(do_lock) this->unlock_timers();
			return it;
		}
	}
	
	if(do_lock) this->unlock_timers();
	return this->timers.end();
}

bool ircbot::is_botcommand(const std::string &msg, std::string *cmd_trigger, std::string *cmd, std::string *carg)
{
	this->rdlock_cfgdata(); // this->re_command
	bool ret = this->re_command.FullMatch(msg, cmd_trigger, cmd, carg);
	this->unlock_cfgdata();
	return ret;
}

int ircbot::disconnect_all()
{
	this->irc_conns.wrlock();
	for(size_t i = 0; i < this->irc_conns.getsize(); i++)
	{
		this->irc_conns[i]->disconnect();
	}
	this->irc_conns.unlock();
	this->disconnect_all_ts = time(NULL);
	return 0;
}

int ircbot::quit_all(const std::string &quitmsg)
{
	this->irc_conns.wrlock();
	for(size_t i = 0; i < this->irc_conns.getsize(); i++)
	{
		this->irc_conns[i]->quit(quitmsg);
	}
	this->irc_conns.unlock();
	this->disconnect_all_ts = time(NULL);
	return 0;
}

int ircbot::force_kill_all()
{
	this->irc_conns.wrlock();
	for(size_t i = 0; i < this->irc_conns.getsize(); i++)
	{
		this->irc_conns[i]->force_kill();
	}
	this->irc_conns.unlock();
	if(this->disconnect_all_ts == 0)
		this->disconnect_all_ts = time(NULL);
	return 0;
}

void ircbot::update_statistics()
{
	time_t now = time(NULL);
	time_t t = now - this->bot_statistics_last_update_time;
	if(t == 0)
		t = 1;
	
	this->statistics.events_per_second = 
		(float)this->statistics.evpersec_counter / t;
	this->statistics.evpersec_counter = 0;
	
	this->bot_statistics_last_update_time = now;
}

int ircbot::wait()
{
	// FIXME: use pthread_cond
	while(this->irc_conns.getsize() > 0)
	{
		sleep(2);
		if(this->disconnect_all_ts != 0)
		{
			time_t now = time(NULL);
			if((now - this->disconnect_all_ts) > 8)
			{
				// 8초 초과
				printf("ircbot::wait: time(%ld) - disconnect_all_ts(%ld) == %ld; killing all bots\n", 
					this->disconnect_all_ts, now, now - this->disconnect_all_ts);
				this->force_kill_all();
			}
			else if((now - this->disconnect_all_ts) > 32)
			{
				// 40초 초과
				printf("ircbot::wait: time(%ld) - disconnect_all_ts(%ld) == %ld; forcing quit\n", 
					this->disconnect_all_ts, now, now - this->disconnect_all_ts);
				return -1;
			}
		}
		this->update_statistics(); // 통계 업데이트
	}
	
	return 0;
}

int ircbot::event_cb_real(ircbot_conn *isrv, ircevent *evdata)
{
	assert(evdata != NULL);
	int etype = evdata->event_id;
	
	// call event handlers
	{
		int ret = this->call_event_handlers(isrv, evdata);
		if(ret == HANDLER_FINISHED)
		{
			goto out;
		}
	}
	
	if(etype == IRCEVENT_PRIVMSG)
	{
		ircevent_privmsg *event = static_cast<ircevent_privmsg *>(evdata);
		const std::string &msg = event->getmsg();
		std::string cmd_trigger, cmd, carg;
		
		if(event->getmsgtype() != IRC_MSGTYPE_CTCP && 
			event->getmsgtype() != IRC_MSGTYPE_CTCPREPLY)
		{
			this->rdlock_cfgdata();
			bool is_command = this->re_command.FullMatch(msg, &cmd_trigger, &cmd, &carg);
			this->unlock_cfgdata();
			if(is_command)
			{
				this->rdlock_cmd_handler_list();
				caseless_strmap<std::string, cmd_handler>::const_iterator it = 
					this->cmd_handlers.find(cmd);
				if(it == this->cmd_handlers.end())
				{
					// command not found
					this->unlock_cmd_handler_list();
				}
				else
				{
					ircbot_cmd_handler_t *cmd_handler_fxn = it->second.handler;
					void *userdata = it->second.userdata;
					this->unlock_cmd_handler_list();
					
					irc_msginfo mdest(*event); // 명령어에서 결과가 출력될 곳. 기본적으로 src
					mdest.set_line_converted(false);
					// call botevent_command event handlers
					{
						botevent_command evdata_cmd(*static_cast<irc_privmsg *>(event), 
							mdest, cmd_trigger, cmd, carg);
						int ret = this->call_event_handlers(isrv, &evdata_cmd);
						if(ret == HANDLER_FINISHED)
						{
							goto out;
						}
					}
					
					if(unlikely(!cmd_handler_fxn))
					{
						std::cerr << "handler found buf cmd_handler_fxn is NULL; "
							"cmd=" + cmd + ", arg=" + carg << std::endl;
					}
					else
					{
						cmd_handler_fxn(isrv, *static_cast<irc_privmsg *>(event), mdest, 
							cmd, carg, userdata);
					}
				}
			}
		}
	}
#ifdef ENABLE_PRIVMSG_SEND_EVENT
	else if(etype == BOTEVENT_PRIVMSG_SEND)
	{
		botevent_privmsg_send *event = static_cast<botevent_privmsg_send *>(evdata);
		isrv->privmsg_real(event->mdest, event->msg);
	}
#endif
	else if(etype == IRCEVENT_LOGGEDIN)
	{
		std::cout << isrv->get_name() << ": Logged in" << std::endl;
		
		const std::string &ac1 = this->cfg["autojoin_channels"];
		if(!ac1.empty())
			isrv->join_channel(ac1);
		
		const std::string &ac2 = this->cfg["irc_servers['"+isrv->get_name()+"'].autojoin_channels"];
		if(!ac2.empty())
			isrv->join_channel(ac2);
	}
	
out:
	botevent_evhandler_done evhandler_done_event(evdata);
	this->call_event_handlers(isrv, &evhandler_done_event);
	
	delete_ircevent(evdata);
	return 0;
}

/* [static] */
int ircbot::event_cb(irc *irc_client, ircevent *evdata, void *)
{
	ircbot *self = static_cast<ircbot *>(((ircbot_conn *)irc_client)->bot);
	self->add_to_event_queue(irc_client, evdata);
	
	return 0;
}


bool ircbot::is_botadmin(const std::string &ident)
{
	std::vector<std::string> admin_masks;
	this->cfg.getarray(admin_masks, "botadmin_masks");
	
	std::vector<std::string>::const_iterator it;
	for(it = admin_masks.begin(); it != admin_masks.end(); it++)
	{
		if(wc_match_irccase(it->c_str(), ident.c_str()))
		{
			return true;
		}
	}
	
	return false;
}


// WARNING: 
// THIS FUNCTION MUST BE CALLED BEFORE ENTERING MAIN LOOP
// re_command IS INITIALIZED HERE
int ircbot::load_cfg_file(const std::string &path)
{
	int ret;
	bool retb;
	int result = 0;
	
	this->wrlock_cfgdata();
	ret = this->cfg.open(path);
	if(ret < 0)
	{
		result = ret;
		goto out;
	}
	
	// bind address 설정
	{
		const std::string &default_bind_address = 
			this->cfg.getstr("default_bind_address", "");
		set_default_sock_bind_address(default_bind_address);
	}
	
	retb = this->re_command.set_pattern(
		"^(?:"
			"\x03[0-9]{0,2}+,?+[0-9]{0,2}+|" // 색글
			"[ \t\x02\x1f\x0f]" // 공백, 볼드, 이탤릭, 노멀
		")*?"
		"(" + this->cfg.getstr("cmd_prefix", "[!.]") + ")" // 명령 트리거
		"([^ ]+) ?(.*)" // 명령어 이름, 인자
		);
	if(retb == false)
	{
		fprintf(stderr, "ircbot::load_cfg_file: error found in cmd_prefix regexp: %s\n", 
			this->re_command.error().c_str());
		result = -1;
		goto out;
	}
	this->max_lines_per_cmd = this->cfg.getint("max_lines_per_cmd", 3);
	
out:
	this->unlock_cfgdata();
	return result;
}




int ircbot::add_to_event_queue(irc *src, ircevent *evdata)
{
	pthread_mutex_lock(&this->event_queue_mutex);
	this->events.push_back(std::pair<ircbot_conn *, ircevent *>(
		/*dynamic*/static_cast<ircbot_conn *>(src), evdata));
	pthread_cond_signal(&this->event_queue_cond);
	
	// 통계 업데이트
	this->statistics.total_event_cnt++;
	this->statistics.evpersec_counter++;
	
	// unlock and return
	pthread_mutex_unlock(&this->event_queue_mutex);
	
	return 0;
}

int ircbot::clear_event_queue()
{
	pthread_mutex_lock(&this->event_queue_mutex);
	// 모든 이벤트를 지운 후 queue를 clear
	std::deque<std::pair<ircbot_conn *, ircevent *> >::const_iterator it = 
		this->events.begin();
	for(; it != this->events.end(); it++)
	{
		delete_ircevent(it->second);
	}
	this->events.clear();
	pthread_mutex_unlock(&this->event_queue_mutex);
	
	return 0;
}

/* [static] */
void ircbot::event_thread_cleanup(void *arg)
{
	ircevent_thread_state *ets = static_cast<ircevent_thread_state *>(arg);
	ircbot *self = ets->bot;
	
	if(ets->state == IRCEVTHREAD_STATE_RUNNING)
	{
		// should not happen
		printf("WARNING: event thread cancelled %lu was running, freeing evdata %p\n", 
			ets->tid, ets->evdata);
		if(ets->evdata)
			delete_ircevent(ets->evdata);
	}
	ets->state = IRCEVTHREAD_STATE_IDLE;
	
	pthread_cond_signal(&self->event_queue_cond); // signal another thread
	// PTHREAD_MUTEX_ERRORCHECK 설정됨: 
	// 이 스레드에서 lock한게 아니면 EPERM 리턴
	pthread_mutex_unlock(&self->event_queue_mutex);
	
	return;
}


/* [static] */
void *ircbot::event_thread(void *arg)
{
	assert(arg != NULL);
	ircevent_thread_state *ets = static_cast<ircevent_thread_state *>(arg);
	ircbot *self = ets->bot;
	assert(self != NULL);
	
	pthread_cleanup_push(&ircbot::event_thread_cleanup, arg);
	{
		pthread_mutex_lock(&self->event_queue_mutex);
		ets->state = IRCEVTHREAD_STATE_WAITING;
		while(1)
		{
			// unlock, wait, lock
			pthread_cond_wait(&self->event_queue_cond, &self->event_queue_mutex);
			if(ets->kill_on)
				goto out;
			
			while(!self->events.empty())
			{
				// 이벤트를 기다리는 중, 일시 중지 되었을 때는
				// 마음대로 스레드를 cancel할 수 있음
				if(self->event_thread_pause_level > 0)
				{
					// 이벤트 처리가 일시 중지됨
					ets->state = IRCEVTHREAD_STATE_PAUSED;
					pthread_mutex_unlock(&self->event_queue_mutex);
					while(self->event_thread_pause_level > 0)
					{
						if(ets->kill_on)
							goto out;
						usleep(100);
					}
					pthread_mutex_lock(&self->event_queue_mutex);
					if(self->events.empty())
					{
						ets->state = IRCEVTHREAD_STATE_WAITING;
						break; // 일시 정지한 사이에 다른 스레드에서 이벤트를 처리한 경우
					}
				}
				
				ets->state = IRCEVTHREAD_STATE_RUNNING;
				ircbot_conn *isrv = self->events.front().first;
				ircevent *evdata = self->events.front().second;
				self->events.pop_front();
				ets->evdata = evdata;
#if 1
				if(unlikely(self->irc_conns.find_by_ptr(isrv) < 0))
				{
					// event from a disconnected client
					fprintf(stderr, "DEBUG:: warning: found events from disconnected connection %p, evtype %d\n", isrv, evdata->event_id);
					delete_ircevent(evdata);
					goto skip_event_handler;
				}
#endif
				
				pthread_mutex_unlock(&self->event_queue_mutex);
				// 이벤트 처리
				self->event_cb_real(isrv, evdata); // evdata is freed here
				pthread_mutex_lock(&self->event_queue_mutex);
				
skip_event_handler:
				ets->state = IRCEVTHREAD_STATE_WAITING;
				ets->evdata = NULL;
				if(ets->state == ets->kill_on)
					goto out;
			}
		}
out:
		// signal another thread, and exit
		pthread_cond_signal(&self->event_queue_cond);
		pthread_mutex_unlock(&self->event_queue_mutex);
	} pthread_cleanup_pop(1);
	
	return (void *)0;
}

void ircbot::init_event_threads()
{
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&this->event_queue_mutex, &mattr);
	
	pthread_cond_init(&this->event_queue_cond, NULL);
	this->event_threads.clear();
	this->event_thread_pause_level = 0;
	this->event_thread_lock_owner = 0;
}

// 생성된 후 전체 스레드 수를 반환
int ircbot::create_event_threads(int cnt)
{
	this->event_threads.wrlock();
	for(int i = 0; i < cnt; i++)
	{
		ircevent_thread_state *ets = this->event_threads.newobj(false);
		ets->bot = this;
		int ret = pthread_create(&ets->tid, NULL, ircbot::event_thread, ets);
		if(ret < 0)
		{
			this->event_threads.delobj(ets, false);
			break;
		}
	}
	this->event_threads.unlock();
	
	return this->event_threads.getsize();
}

int ircbot::kill_event_threads()
{
	fprintf(stdout, "ircbot::kill_event_threads: pausing event threads\n");
	while(this->pause_event_threads(true) < 0) usleep(50);
	fprintf(stdout, "ircbot::kill_event_threads: event threads paused, killing all event threads\n");
	this->event_threads.wrlock();
	while(!this->event_threads.empty())
	{
		ircevent_thread_state *ets = this->event_threads[0];
		if(ets)
		{
			ets->kill(false);
		}
	}
	this->event_threads.unlock();
	this->pause_event_threads(false);
	this->init_event_threads();
	
	return 0;
}

// Return value:
//	>=0		success
//	<0		fail
/////
// this->event_thread_pause_level이 1 이상일 경우 일지정지
int ircbot::pause_event_threads(bool pause, unsigned int options)
{
	// function constants
	static const int max_exec_time = 15; // seconds
	static const char *const exec_time_excd_msg = "ERROR: ircbot::pause_event_threads(%d): max execution time excedded (my_tid: %lu)\n";
	// local varibles
	pthread_t my_tid = pthread_self();
	time_t fxn_start_time = time(NULL);
	
	if(pause == false)
	{
		pthread_mutex_lock(&this->pause_event_threads_fxn_lock);
	}
	else
	{
		if(pthread_mutex_trylock(&this->pause_event_threads_fxn_lock) != 0)
			return -1; // XXX FIXME
	}
	
	if(pause == false)
	{
		if(this->event_thread_pause_level > 0 && 
			this->event_thread_lock_owner != 0 && 
			this->event_thread_lock_owner != my_tid)
		{
			pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
			return -1; // already locked by another thread
		}
		if(this->event_thread_pause_level <= 0)
		{
			pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
			return 1;
		}
		this->event_thread_pause_level--;
		//pthread_mutex_unlock(&this->event_thread_pause_lock);
#if 0 // 아래로 옮김
		if(this->event_thread_pause_level == 0)
			this->event_thread_lock_owner = 0;
#endif
		
		// wait for other threads to be resumed
		if(my_tid == this->timer_thread_id)
		{
			if(this->timer_thread_state == IRCEVTHREAD_STATE_PAUSED)
			{
				fprintf(stderr, "ERROR: assertion failed: !(my_tid == this->timer_thread_id && this->timer_thread_state == IRCEVTHREAD_STATE_PAUSED)\n");
			}
		}
		else
		{
			while(this->timer_thread_state == IRCEVTHREAD_STATE_PAUSED)
			{
				usleep(100);
				if((time(NULL) - fxn_start_time) > max_exec_time)
				{
					//this->event_thread_pause_level++;
					//pthread_mutex_lock(&this->event_thread_pause_lock);
					pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
					fprintf(stderr, exec_time_excd_msg, pause, my_tid);
					return -1;
				}
			}
		}
		for(;;)
		{
			bool done = true;
			this->event_threads.rdlock();
			for(size_t i = 0; i < this->event_threads.getsize(); i++)
			{
				ircevent_thread_state *ets = this->event_threads[i];
				if(ets->tid == my_tid)
				{
					if(ets->state == IRCEVTHREAD_STATE_PAUSED)
					{
						fprintf(stderr, "ERROR: assertion failed: !(my_tid == this->timer_thread_id && ets->state == IRCEVTHREAD_STATE_PAUSED)\n");
					}
				}
				else
				{
					if(ets->state == IRCEVTHREAD_STATE_PAUSED)
					{
						done = false;
						break;
					}
				}
			}
			this->event_threads.unlock();
			if(done)
				break;
			usleep(100);
			if((time(NULL) - fxn_start_time) > max_exec_time)
			{
				//this->event_thread_pause_level++;
				//pthread_mutex_lock(&this->event_thread_pause_lock);
				pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
				fprintf(stderr, exec_time_excd_msg, pause, my_tid);
				return -1;
			}
		}
		
		if(this->event_thread_pause_level == 0)
			this->event_thread_lock_owner = 0;
	}
	else
	{
		if(this->event_thread_pause_level > 0 && 
			this->event_thread_lock_owner != 0 && 
			this->event_thread_lock_owner != my_tid)
		{
			pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
			return -1; // already locked by another thread
		}
		this->event_thread_pause_level++;
		//pthread_mutex_lock(&this->event_thread_pause_lock);
		if(this->event_thread_pause_level >= 2)
		{
			pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
			return 1; // already paused
		}
		this->event_thread_lock_owner = my_tid;
		
		// wait for timer thread
		if(my_tid != this->timer_thread_id)
		{
			while(this->timer_thread_state == IRCEVTHREAD_STATE_RUNNING)
			{
				usleep(100);
				if((time(NULL) - fxn_start_time) > max_exec_time)
				{
					this->event_thread_pause_level--;
					if(this->event_thread_pause_level == 0)
						this->event_thread_lock_owner = 0;
					//pthread_mutex_unlock(&this->event_thread_pause_lock);
					pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
					fprintf(stderr, exec_time_excd_msg, pause, my_tid);
					return -1;
				}
			}
		}
		
		// wait for event threads
		for(;;)
		{
			bool done = true;
			this->event_threads.rdlock();
			for(size_t i = 0; i < this->event_threads.getsize(); i++)
			{
				// 내가 실행중인 스레드와 이미 정지되어있는 스레드는 일지정지 할 필요가 없음
				ircevent_thread_state *ets = this->event_threads[i];
				if(ets->tid != my_tid && ets->state == IRCEVTHREAD_STATE_RUNNING)
				{
					done = false;
					break;
				}
			}
			this->event_threads.unlock();
			if(done)
				break;
			usleep(100);
			if((time(NULL) - fxn_start_time) > max_exec_time)
			{
				this->event_thread_pause_level--;
				if(this->event_thread_pause_level == 0)
					this->event_thread_lock_owner = 0;
				//pthread_mutex_unlock(&this->event_thread_pause_lock);
				pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
				fprintf(stderr, exec_time_excd_msg, pause, my_tid);
				return -1;
			}
		}
	}
	pthread_mutex_unlock(&this->pause_event_threads_fxn_lock);
	
	return 0;
}

int ircbot::set_event_thread_cnt(int newcnt)
{
	int crntcnt = this->event_threads.getsize();
	if(newcnt == crntcnt)
		return crntcnt;
	
	int ret = this->pause_event_threads(true);
	if(ret < 0)
	{
		return -1;
	}
	if(pthread_mutex_trylock(&this->set_event_thread_cnt_fxn_lock) != 0)
	{
		this->pause_event_threads(false);
		return -1;
	}
	
	if(newcnt > crntcnt)
	{
		crntcnt = this->create_event_threads(newcnt - crntcnt);
	}
	else /* if(newcnt < crntcnt) */
	{
		pthread_t my_tid = pthread_self();
		
		this->event_threads.wrlock();
		int nkilled = 0;
		for(size_t i = 0; i < this->event_threads.getsize(); )
		{
			ircevent_thread_state *ets = this->event_threads[i];
			assert(ets != NULL);
			if(ets->tid == my_tid)
			{
				i++;
				continue;
			}
			ets->kill(false);
			nkilled++;
			if(nkilled >= (crntcnt - newcnt))
				break;
		}
		crntcnt = this->event_threads.getsize();
		this->event_threads.unlock();
	}
	pthread_mutex_unlock(&this->set_event_thread_cnt_fxn_lock);
	this->pause_event_threads(false);
	
	return crntcnt;
}

void ircevent_thread_state::kill(bool do_lock)
{
	this->kill_on = true;
	//while(this->state != IRCEVTHREAD_RUNNING)
	//	usleep(100);
	pthread_cond_broadcast(&this->bot->event_queue_cond);
	pthread_join(this->tid, NULL);
	this->bot->event_threads.delobj(this, do_lock);
}

int ircbot::call_event_handlers(ircbot_conn *isrv, ircevent *ev)
{
	this->rdlock_event_handler_list();
	for(std::vector<event_handler>::const_iterator it = 
			this->event_handlers.begin(); 
		it != this->event_handlers.end(); it++)
	{
		ircbot_event_handler_t *event_handler_fxn = it->handler;
		if(unlikely(!event_handler_fxn))
		{
			std::cerr << "event handler callback is NULL" << std::endl;
		}
		else
		{
			int ret = event_handler_fxn(isrv, ev, it->userdata);
			if(ret == HANDLER_FINISHED)
			{
				this->unlock_event_handler_list();
				return HANDLER_FINISHED;
			}
		}
	}
	this->unlock_event_handler_list();
	return HANDLER_GO_ON;
}

/*[static]*/
void *ircbot::timer_thread(void *data)
{
	ircbot *self = (ircbot *)data;
	
	while(self->kill_timer_thread_on == false)
	{
		if(self->event_thread_pause_level != 0)
		{
			self->timer_thread_state = IRCEVTHREAD_STATE_PAUSED;
			while(self->event_thread_pause_level != 0)
				usleep(100);
		}
		self->timer_thread_state = IRCEVTHREAD_STATE_RUNNING;
		self->wrlock_timers();
		time_t crnttime = time(NULL);
		if(self->timers.size() > 0)
		{
			for(std::vector<ircbot_timer_info>::iterator it = self->timers.begin(); 
				it != self->timers.end(); )
			{
				if(it->next > crnttime)
					break;
				
				if(unlikely(it->fxn == NULL))
				{
					fprintf(stderr, "WARNING: timer callback is null\n");
				}
				else
				{
					// FIXME: should do this in event thread
					it->fxn(self, it->name, it->userdata);
				}
				
				if(it->attr & IRCBOT_TIMERATTR_ONCE)
				{
					self->timers.erase(it);
				}
				else
				{
					it->next = crnttime + it->interval/1000;
					it++;
				}
			}
		}
		int wait_time;
		std::vector<ircbot_timer_info>::iterator first_tmr = self->timers.begin();
		if(first_tmr == self->timers.end())
			wait_time = 1000;
		else
			wait_time = (first_tmr->next - crnttime) * 1000;
		self->unlock_timers();
		self->timer_thread_state = IRCEVTHREAD_STATE_WAITING;
		usleep(wait_time);
	}
	
	return (void *)0;
}
bool ircbot::create_timer_thread()
{
	this->kill_timer_thread_on = false;
	int ret = pthread_create(&this->timer_thread_id, NULL, &ircbot::timer_thread, this);
	if(ret < 0)
	{
		this->timer_thread_id = (pthread_t)-1;
		return false;
	}
	return true;
}
bool ircbot::kill_timer_thread()
{
	if((int)this->timer_thread_id == -1)
		return false;
	this->kill_timer_thread_on = true;
	pthread_join(this->timer_thread_id, NULL);
	this->timer_thread_id = (pthread_t)-1;
	return true;
}

/*[static]*/const char *ircbot::evthrd_state_to_string(IRCEVTHREAD_STATE n)
{
	const char *state_str_list[] = {"IDLE", "WAITING", "RUNNING", "PAUSED", "unknown"};
	if(n < 0 || n > IRCEVTHREAD_STATE_END)
		n = IRCEVTHREAD_STATE_END;
	return state_str_list[n];
}
int ircbot::print_bot_status(int do_lock)
{
	bool locked;
	
	printf("########## bot status dump\n");
	// ===== bot thread stats
	printf("##### bot thread status dump\n");
	if(do_lock == 2)
		locked = (this->event_threads.rdlock(),true);
	else if(do_lock == 1)
		locked = this->event_threads.tryrdlock()==0?true:(printf("##### WARN: failed to lock ircbot::event_threads\n"),false);
	else
		locked = false;
	printf("Bot thread status:: Timer thread 0: tid %lu, state: %s\n", 
		this->timer_thread_id, ircbot::evthrd_state_to_string(this->timer_thread_state));
	for(size_t i = 0; i < this->event_threads.getsize(); i++)
	{
		const ircevent_thread_state *ets = this->event_threads[i];
		if(ets != NULL)
		{
			char evdump_buf[1024];
			if(!ets->evdata)
				strcpy(evdump_buf, "NULL");
			else
			{
				snprintf(evdump_buf, sizeof(evdump_buf), "evtype: %d(%s)", 
					ets->evdata->event_id, gsc_botevent_2str_tab[ets->evdata->event_id]);
			}
			printf("Bot thread status:: Event thread %d: tid %lu, state: %s, evdata: {%s}\n", i, 
				ets->tid, ircbot::evthrd_state_to_string(ets->state), evdump_buf);
		}
		else
		{
			printf("Bot thread status:: Event thread %d: ptr is NULL\n", i);
		}
	}
	printf("Bot thread status:: Total %d Event threads\n", this->event_threads.getsize());
	printf("Bot thread status:: Total %d threads\n", this->event_threads.getsize() + 1);
	printf("##### END OF bot thread status dump\n");
	if(locked) this->event_threads.unlock();
	// ===== irc connections
	printf("##### bot irc_conns\n");
	if(do_lock == 2)
		locked = (this->irc_conns.rdlock(),true);
	else if(do_lock == 1)
		locked = (this->irc_conns.tryrdlock()==0)?true:(printf("##### WARN: failed to lock ircbot::irc_conns\n"),false);
	else
		locked = false;
	
	for(size_t i = 0; i < this->irc_conns.getsize(); i++)
	{
		const ircbot_conn *isrv = this->irc_conns[i];
		if(isrv != NULL)
		{
			printf("Bot irc_conns status:: irc_conn %d: name: %s, main_thrd_tid: %lu\n", i, 
				isrv->get_name().c_str(), isrv->main_thread_tid);
		}
		else
		{
			printf("irc connection status:: irc_conn %d: ptr is NULL\n", i);
		}
	}
	printf("irc connection status:: Total %d connections\n", this->irc_conns.getsize());
	
	printf("##### END OF bot thread status dump\n");
	if(locked) this->irc_conns.unlock();
	// ===== registered cmds/event handlers, event queue size
	printf("Currently %d events has been queued\n", this->events.size());
	printf("There are %d event handlers and %d commands registered\n", 
		this->event_handlers.size(), this->cmd_handlers.size());
	printf("Current event_thread_pause_level == %d, evthrd lock owner tid: %lu\n", 
		this->event_thread_pause_level, this->event_thread_lock_owner);
	// ===== statistics
	printf("Bot uptime: %lu seconds\n", time(NULL) - this->start_ts);
	printf("Total %llu events has been processed, %.02f events per second\n", 
		this->statistics.total_event_cnt, this->statistics.events_per_second);
	// ========== end
	printf("########## END OF bot status dump\n");
	fflush(stdout);
	return 0;
}

ircbot_conn *ircbot::find_conn_by_name(const std::string &name, bool do_lock)
{
	ircbot_conn *res = NULL;
	if(do_lock) this->irc_conns.rdlock();
	for(size_t i = 0; i < this->irc_conns.getsize(); i++)
	{
		if(this->irc_conns[i]->get_name() == name)
		{
			res = this->irc_conns[i];
			break;
		}
	}
	if(do_lock) this->irc_conns.unlock();
	return res;
}

ircbot_conn *ircbot::create_irc_conn()
{
	ircbot_conn *isrv = this->irc_conns.newobj();
	isrv->bot = this;
	isrv->conn_id = this->next_connid++;
	
	return isrv;
}


////////////////////


ircbot_conn::ircbot_conn()
	:bot(NULL), 
	main_thread_tid((pthread_t)-1), 
	is_destructing(false), 
	conn_id(-1), 
	disconnect_on(false), 
	reconnect_on(false)
{
	this->set_event_callback(this->bot->event_cb);
	
	pthread_mutex_init(&this->connfxn_mutex, NULL);
}
ircbot_conn::~ircbot_conn()
{
	this->is_destructing = true;
	// MUST be already disconnected
	this->quit("Destructing"); // result MUST be <0
	this->disconnect();
	if((int)this->main_thread_tid == -1)
		return;
	
	// MUST NOT HAPPEN
	fprintf(stderr, "%s: @@@@@@@@@@@@@@@@@ WARNING in ircbot_conn::~ircbot:conn()\n", this->name.c_str());
	pthread_cancel(this->main_thread_tid);
	pthread_join(this->main_thread_tid, NULL);
	this->main_thread_tid = (pthread_t)-1;
}


int ircbot_conn::main_loop()
{
	int ret;
	time_t last_conn_time;
	
	/* 2 */ pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL); // 2
	this->preserve_eilseq_line = (this->bot->cfg.getbool("preserve_eilseq_line", false)); // FIXME
	//pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); //
	while(this->disconnect_on == false)
	{
		last_conn_time = time(NULL);
		std::cout << this->get_name() << ": [" + gettimestr_simple() + "] Connecting..." << std::endl;
		this->reconnect_on = false;
		/* 2 */ pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); // 2
		this->change_nick(this->nickbuf);
		this->set_server_pass(this->bot->cfg.getstr("irc_servers['" + this->get_name() + "'].server_pass", ""));
		ret = this->connect(this->server, this->port);
		/* 2 */ pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL); // 2
		if(this->disconnect_on)
		{
			if(ret == 0)
				this->disconnect();
			break;
		}
		if(ret < 0)
		{
			std::cout << this->get_name() << ": connect failed: " << 
				sock.get_last_error_str() << std::endl;
			std::cout << this->get_name() << ": I will retry after 5 seconds" << std::endl;
			sleep(5);
			continue;
		}
		
		this->login_to_server();
		
		std::cout << this->get_name() << ": Entering main loop..." << std::endl;
		//pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
		ret = static_cast<irc *>(this)->main_loop();
		//pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
		std::cout << this->get_name() << ": [" + gettimestr_simple() + "] Disconnected" << std::endl;
		
		//std::cout << "Last socket error message: " << 
		//	this->sock.get_last_error_str() << std::endl;
		
		time_t passed_time = time(NULL) - last_conn_time;
		std::cout << this->get_name() << ": connected " << passed_time << " seconds" << std::endl;
		
		if(reconnect_on)
		{
			std::cout << this->get_name() << ": Reconnecting..." << std::endl;
			continue;
		}
		if(ret == 0)
		{
			std::cout << this->get_name() << ": Exiting..." << std::endl;
			break;
		}
		
#define SHORT_SLEEP(_secs) \
	({ \
		for(int i = 0; (i < (_secs)) && (this->disconnect_on == false); i++) \
			sleep(1); \
	})
		if(passed_time < 60 * 5) // 5 minutes
		{
			std::cout << this->get_name() << ": I will restart after 60 seconds" << std::endl;
			SHORT_SLEEP(60);
		}
		else if(passed_time < 60 * 10) // 10 minutes
		{
			std::cout << this->get_name() << ": I will restart after 30 seconds" << std::endl;
			SHORT_SLEEP(30);
		}
		else
		{
			std::cout << this->get_name() << ": I will restart after 5 seconds" << std::endl;
			SHORT_SLEEP(5);
		}
		/* 2 */ pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); // 2
		/* 2 */ pthread_testcancel();
		/* 2 */ pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL); // 2
	}
	this->disconnect_on = false;
	this->reconnect_on = false;
	
	return 0;
}

/* [static] */
void ircbot_conn::main_thread_cleanup(void *arg)
{
	ircbot_conn *isrv = (ircbot_conn *)arg;
	ircbot *bot = isrv->bot;
	
	isrv->is_destructing = true;
	while(bot->pause_event_threads(true)< 0) usleep(50); ////////// 20090822 아래에서 여기로 옮김
	pthread_mutex_lock(&isrv->connfxn_mutex);
#if 1
	isrv->main_thread_tid = (pthread_t)-1;
	
	//// evthrd pause, event queue lock& 순회하며 이 class 삭제
	//while(bot->pause_event_threads(true)< 0) usleep(50); // FIXME // 20090822 위로 옮김
	pthread_mutex_lock(&bot->event_queue_mutex);
	for(std::deque<std::pair<ircbot_conn *, ircevent *> >::iterator it = bot->events.begin(); 
		it != bot->events.end(); )
	{
		if((it->first == isrv) && (it->second->event_id != IRCEVENT_DISCONNECTED))
		{
			fprintf(stderr, "DEBUG:: deleting events from disconnected connection (%p -> %p), evtype %d\n", isrv, it->first, it->second->event_id);
			delete_ircevent(it->second);
			bot->events.erase(it);
		}
		else
			it++;
	}
	pthread_mutex_unlock(&bot->event_queue_mutex);
	bot->pause_event_threads(false);
	
	fprintf(stderr, "ircbot::main_thread_cleanup: Destructing object(%p)\n", isrv);
	isrv->bot->irc_conns.delobj(isrv); // XXX WARNING FIXME ircbot::wait 문제 수정후 이 줄 삭제.
#endif
	
	return;
}

/* [static] */
void *ircbot_conn::main_thread(void *data)
{
	ircbot_conn *isrv = (ircbot_conn *)data;
	pthread_detach(pthread_self());
	
	pthread_cleanup_push(&ircbot_conn::main_thread_cleanup, data);
	{
		isrv->main_loop();
		isrv->main_thread_tid = (pthread_t)-1;
	} pthread_cleanup_pop(1);
	
	return (void *)0;
}

// launch irc thread and connect
bool ircbot_conn::launch_thread()
{
	if((int)this->main_thread_tid != -1)
	{
		return false;
	}
	this->reconnect_on = false;
	int ret = pthread_create(&this->main_thread_tid, NULL, &ircbot_conn::main_thread, this);
	if(ret < 0)
	{
		this->main_thread_tid = (pthread_t)-1;
		return false;
	}
	return true;
}

// inserts dots on nicknames
std::string ircbot_conn::prevent_irc_highlighting_all(std::string chan, std::string msg)
{
	this->rdlock_chanuserlist();
	const irc_strmap<std::string, irc_user> &list = 
		this->get_chanuserlist(chan);
	irc_strmap<std::string, irc_user>::const_iterator it;
	const std::string &my_nick = this->my_nick();
	
	for(it = list.begin(); it != list.end(); it++)
	{
		const std::string &nick = it->second.getnick();
		if(nick != my_nick)
			msg = prevent_irc_highlighting(nick, msg);
	}
	this->unlock_chanuserlist();
	
	return msg;
}

int ircbot_conn::privmsg_real(const irc_msginfo &chan, const std::string &msg)
{
	return static_cast<irc *>(this)->privmsg(chan, msg);
}
int ircbot_conn::privmsg(const irc_msginfo &chan, const std::string &msg)
{
#ifdef ENABLE_PRIVMSG_SEND_EVENT
	// 이벤트 처리가 끝난 후에, ircbot::event_cb_real 에서 
	//  ircbot_conn:privmsg_real 을 사용하여 메시지를 보냄
	return this->bot->add_to_event_queue(this, new botevent_privmsg_send(chan, msg));
#else
	this->bot->add_to_event_queue(this, new botevent_privmsg_send(chan, msg));
	return this->privmsg_real(chan, msg);
#endif
}
int ircbot_conn::privmsgf(const irc_msginfo &dest, const char *fmt, ...)
{
	if(!fmt)
		return -1;
	
	va_list ap;
	va_start(ap, fmt);
	std::string result = s_vsprintf(fmt, ap);
	va_end(ap);
	
	return this->privmsg(dest, result);
}

// removes dots(added in prevent_irc_highlighting) in URLs
static bool remove_dots_on_urls_replace_cb(std::string &dest, 
	const std::vector<std::string> &groups, void *)
{
	for(const char *p = groups[0].c_str(); *p; p++)
		if(!(*p & 0x80))
			dest += *p;
	return true;
}
int ircbot_conn::privmsg_nh(const irc_msginfo &dest, const std::string &msg)
{
	static pcrecpp::RE re_remove_dots_on_urls("([a-zA-Z·]{3,4}://[0-9a-zA-Z./·-]+)", pcrecpp::UTF8());
	
	if(msg.empty())
		return -1;
	
	std::string buf(this->prevent_irc_highlighting_all(dest.getchan(), msg));
	re_remove_dots_on_urls.GlobalReplaceCallback(&buf, remove_dots_on_urls_replace_cb); // URL 사이의 점을 지움
	
	return this->privmsg(dest, buf);
}

int ircbot_conn::privmsgf_nh(const irc_msginfo &dest, const char *fmt, ...)
{
	if(!fmt)
		return -1;
	
	va_list ap;
	va_start(ap, fmt);
	std::string result = s_vsprintf(fmt, ap);
	va_end(ap);
	
	return this->privmsg_nh(dest, result);
}

bool ircbot_conn::quit(const std::string &quitmsg)
{
	if(this->is_destructing)
		return false;
	pthread_mutex_lock(&this->connfxn_mutex);
	this->reconnect_on = false;
	this->disconnect_on = true;
	bool ret = static_cast<irc *>(this)->quit(quitmsg);
	pthread_mutex_unlock(&this->connfxn_mutex);
	return ret;
}
bool ircbot_conn::disconnect()
{
	if(this->is_destructing)
		return false;
	pthread_mutex_lock(&this->connfxn_mutex);
	this->reconnect_on = false;
	this->disconnect_on = true;
	bool ret = static_cast<irc *>(this)->disconnect();
	pthread_mutex_unlock(&this->connfxn_mutex);
	return ret;
}
bool ircbot_conn::reconnect(const std::string &quitmsg)
{
	if(this->is_destructing || this->disconnect_on)
		return false;
	pthread_mutex_lock(&this->connfxn_mutex);
	this->reconnect_on = true;
	if(quitmsg.empty())
		return static_cast<irc *>(this)->quit("Reconnecting");
	else
		return static_cast<irc *>(this)->quit(quitmsg);
	
	pthread_mutex_unlock(&this->connfxn_mutex);
	return true;
}

// this object will be destructed
bool ircbot_conn::force_kill()
{
	if(this->is_destructing)
		return false;
	pthread_mutex_lock(&this->connfxn_mutex);
	if(this->status == IRCSTATUS_CONNECTED && 
		this->reconnect_on == false && this->disconnect_on == false)
	{
		static_cast<irc *>(this)->quit("Forced quit");
		// TODO: wait until thread ends
	}
	this->reconnect_on = false;
	this->disconnect_on = true;
	
	pthread_t tid = this->main_thread_tid;
	this->main_thread_tid = (pthread_t)-1;
	if((int)tid != -1)
	{
#if 0 // 20091030 pjm0616 modified: @@@@@@@ IMPORTANT BUG
		pthread_cancel(tid);
		pthread_join(tid, NULL);
		while(bot->pause_event_threads(true)< 0) usleep(50);
		pthread_mutex_unlock(&this->connfxn_mutex);
#else
		pthread_detach(tid);
		pthread_mutex_unlock(&this->connfxn_mutex);
		pthread_cancel(tid);
#endif
		return true;
	}
	
	pthread_mutex_unlock(&this->connfxn_mutex);
	return false;
}











// vim: set tabstop=4 textwidth=80:

