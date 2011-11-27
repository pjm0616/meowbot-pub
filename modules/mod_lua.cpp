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
#include <assert.h>

#include <pthread.h>

#include <pcrecpp.h>

#include "defs.h"
#include "etc.h"
#include "hangul.h"
#include "my_vsprintf.h"
#include "module.h"
#include "ircbot.h"
#include "luacpp.h"

#ifdef __cplusplus
extern "C" {
#endif
// for lua_State, global_State definitions. L->l_G->mainthread
#include <lstate.h>
#ifdef __cplusplus
}
#endif




class pmutex
{
public:
	pmutex()
	{
		pthread_mutex_init(&this->mutex, NULL);
	}
	~pmutex()
	{
	}
	void init()
	{
	}
	void lock()
	{
		pthread_mutex_lock(&this->mutex);
	}
	void unlock()
	{
		pthread_mutex_unlock(&this->mutex);
	}
	
private:
	pthread_mutex_t mutex;
};


////////////////////


static ircbot *g_bot;
static std::map<lua_State *, pmutex> g_lua_mutexes;
static pthread_rwlock_t g_lua_mutexlist_lock; // -_-
static caseless_strmap<std::string, luaref> g_cmdmap; // cmdname, callback luafxn reference
static std::map<int, std::vector<luaref> > g_eventhdlmap; // event id, callback luafxn reference(with lua_State) list
static pthread_rwlock_t g_cmdmap_lock, g_eventhdlmap_lock;

struct bottimer_lua
{
	lua_State *L;
	std::string name;
	int cbfxn_refid, userdata_refid;
	int attr;
};
static pobj_vector<bottimer_lua> g_bottmrlist;

//static ircbot *lua_tobot(lua_State *L, int index);
static ircbot *lua_checkbot(lua_State *L, int index);
static ircbot **lua_push_bot(lua_State *L, ircbot *bot);

static int bottimer_callback_lua(ircbot *bot, const std::string &name, void *userdata);


//////////////////////////////////

// WARNING: arg1(L) must be Lua MAIN thread (L->l_G->mainthread)
static bool lock_lua_instance(lua_State *L)
{
	assert(L == L->l_G->mainthread);
	
	bool result = false;
	pthread_rwlock_rdlock(&g_lua_mutexlist_lock);
	
	std::map<lua_State *, pmutex>::iterator it = g_lua_mutexes.find(L);
	if(it != g_lua_mutexes.end())
	{
		result = true;
		it->second.lock();
	}
	else
	{
		fprintf(stderr, "Warning: tried to lock unexistant lua_State %p\n", L);
		fflush(stderr);
	}
	
	pthread_rwlock_unlock(&g_lua_mutexlist_lock);
	return result;
}
static bool unlock_lua_instance(lua_State *L)
{
	assert(L == L->l_G->mainthread);
	
	bool result = false;
	pthread_rwlock_rdlock(&g_lua_mutexlist_lock);
	
	std::map<lua_State *, pmutex>::iterator it = g_lua_mutexes.find(L);
	if(it != g_lua_mutexes.end())
	{
		result = true;
		it->second.unlock();
	}
	else
	{
		fprintf(stderr, "Warning: tried to lock unexistant lua_State %p\n", L);
		fflush(stderr);
	}
	
	pthread_rwlock_unlock(&g_lua_mutexlist_lock);
	return result;
}

//////////////////////////////////

static void *luaL_check_udata_type(lua_State *L, int idx, const char *tname)
{
	void *p = lua_touserdata(L, idx);
	if(p != NULL) /* value is a userdata? */
	{
		if(lua_getmetatable(L, idx)) /* does it have a metatable? */
		{
			lua_getfield(L, LUA_REGISTRYINDEX, tname); /* get correct metatable */
			if(lua_rawequal(L, -1, -2)) /* does it have the correct mt? */
			{
				lua_pop(L, 2); /* remove both metatables */
				return p;
			}
		}
	}
	return NULL;
}

//////////////////////////////////

static int evname2id(const char *evname)
{
	if(!strcmp(evname, "privmsg"))
		return IRCEVENT_PRIVMSG;
	else if(!strcmp(evname, "join"))
		return IRCEVENT_JOIN;
	else if(!strcmp(evname, "part"))
		return IRCEVENT_PART;
	else if(!strcmp(evname, "kick"))
		return IRCEVENT_KICK;
	else if(!strcmp(evname, "nick"))
		return IRCEVENT_NICK;
	else if(!strcmp(evname, "quit"))
		return IRCEVENT_QUIT;
	else if(!strcmp(evname, "mode"))
		return IRCEVENT_MODE;
	else if(!strcmp(evname, "invite"))
		return IRCEVENT_INVITE;
	else if(!strcmp(evname, "srvincoming"))
		return IRCEVENT_SRVINCOMING;
	else if(!strcmp(evname, "idle"))
		return IRCEVENT_IDLE;
	else if(!strcmp(evname, "connect"))
		return IRCEVENT_CONNECTED;
	else if(!strcmp(evname, "login"))
		return IRCEVENT_LOGGEDIN;
	else if(!strcmp(evname, "disconnect"))
		return IRCEVENT_DISCONNECTED;
	else if(!strcmp(evname, "botcommand"))
		return BOTEVENT_COMMAND;
	else if(!strcmp(evname, "evhandler_done"))
		return BOTEVENT_EVHANDLER_DONE;
	else if(!strcmp(evname, "privmsg_send"))
		return BOTEVENT_PRIVMSG_SEND;
	else
		return -1;
}

static const char *msgtype2str(int msgtype)
{
	switch(msgtype)
	{
	case IRC_MSGTYPE_NORMAL:
		return "normal";
	case IRC_MSGTYPE_NOTICE:
		return "notice";
	case IRC_MSGTYPE_ACTION:
		return "action";
	case IRC_MSGTYPE_CTCP:
		return "ctcp";
	case IRC_MSGTYPE_CTCPREPLY:
		return "ctcpreply";
	default:
		return "unknown";
	}
}

static irc_msgtype translate_msgtypestr(const char *msgtype_str)
{
	irc_msgtype msgtype;
	if(!strcmp(msgtype_str, "normal"))
		msgtype = IRC_MSGTYPE_NORMAL;
	else if(!strcmp(msgtype_str, "notice"))
		msgtype = IRC_MSGTYPE_NOTICE;
	else if(!strcmp(msgtype_str, "action"))
		msgtype = IRC_MSGTYPE_ACTION;
	else if(!strcmp(msgtype_str, "ctcp"))
		msgtype = IRC_MSGTYPE_CTCP;
	else if(!strcmp(msgtype_str, "ctcpreply"))
		msgtype = IRC_MSGTYPE_CTCPREPLY;
	else
		msgtype = IRC_MSGTYPE_NONE;
	return msgtype;
}


static const char *chantype2str(int msgtype)
{
	switch(msgtype)
	{
	case IRC_CHANTYPE_CHAN:
		return "chan";
	case IRC_CHANTYPE_QUERY:
		return "query";
	case IRC_CHANTYPE_DCC:
		return "dcc";
	default:
		return "unknown";
	}
}

//////////////////////////////////

static irc_ident *lua_toident(lua_State *L, int index)
{
	irc_ident **data = (irc_ident **)lua_touserdata(L, index);
	if(!data)
	{
		luaL_typerror(L, index, "irc_ident");
	}
	return *data;
}

static irc_ident *lua_checkident(lua_State *L, int index)
{
	irc_ident **data, *ident;
	luaL_checktype(L, index, LUA_TUSERDATA);
	data = (irc_ident **)luaL_checkudata(L, index, "irc_ident");
	if(!data)
	{
		luaL_typerror(L, index, "irc_ident");
	}
	ident = *data;
	if(!ident)
	{
		luaL_error(L, "null irc_ident object");
	}
	return ident;
}

static irc_ident **lua_push_ident(lua_State *L, irc_ident *ident)
{
	irc_ident **data = (irc_ident **)lua_newuserdata(L, sizeof(irc_ident *));
	*data = ident;
	luaL_getmetatable(L, "irc_ident");
	lua_setmetatable(L, -2);
	return data;
}

static int lua_irc_ident_new(lua_State *L)
{
	const char *ident_str = luaL_checkstring(L, 1);
	lua_push_ident(L, new irc_ident(ident_str));
	return 1;
}

static int lua_irc_ident_getident(lua_State *L)
{
	irc_ident *ident = lua_checkident(L, 1);
	lua_pushstdstring(L, ident->getident());
	return 1;
}

static int lua_irc_ident_getnick(lua_State *L)
{
	irc_ident *ident = lua_checkident(L, 1);
	lua_pushstdstring(L, ident->getnick());
	return 1;
}

static int lua_irc_ident_getuser(lua_State *L)
{
	irc_ident *ident = lua_checkident(L, 1);
	lua_pushstdstring(L, ident->getuser());
	return 1;
}

static int lua_irc_ident_gethost(lua_State *L)
{
	irc_ident *ident = lua_checkident(L, 1);
	lua_pushstdstring(L, ident->gethost());
	return 1;
}

static const luaL_reg irc_ident_methods[] = 
{
	{"new", lua_irc_ident_new}, 
	{"getident", lua_irc_ident_getident}, 
	{"getnick", lua_irc_ident_getnick}, 
	{"getuser", lua_irc_ident_getuser}, 
	{"gethost", lua_irc_ident_gethost}, 
	{NULL, NULL}
};

static int lua_irc_ident_gc(lua_State *L)
{
	irc_ident *ident = lua_toident(L, 1);
	if(ident)
	{
		delete ident;
	}
	return 0;
}

static const luaL_reg irc_ident_meta_methods[] = 
{
	{"__gc", lua_irc_ident_gc}, 
	{"__tostring", lua_irc_ident_getident}, 
	{NULL, NULL}
};


static int lua_register_irc_identlib(lua_State *L)
{
	int methods, metatable;
	
	luaL_register(L, "irc_ident", irc_ident_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "irc_ident"); metatable = lua_gettop(L); // create metatable, add it to the Lua registry
	luaL_register(L, NULL, irc_ident_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__metatable = methods
	
	lua_pop(L, 2); // drop metatable, methods table
	
	return 0;
}

//////////////////////////////////

//////////////////////////////////

#if 0
static irc_msginfo *lua_tomsginfo(lua_State *L, int index)
{
	irc_msginfo **data = (irc_msginfo **)lua_touserdata(L, index);
	if(!data)
	{
		luaL_typerror(L, index, "irc_msginfo");
	}
	return *data;
}
#endif

static irc_msginfo *lua_checkmsginfo(lua_State *L, int index)
{
	irc_msginfo **data, *msginfo;
	luaL_checktype(L, index, LUA_TUSERDATA);
	data = (irc_msginfo **)luaL_checkudata(L, index, "irc_msginfo");
	if(!data)
	{
		luaL_typerror(L, index, "irc_msginfo");
	}
	msginfo = *data;
	if(!msginfo)
	{
		luaL_error(L, "null irc_msginfo object");
	}
	return msginfo;
}

static irc_msginfo *lua_check_msginfo_compatible(lua_State *L, int index, irc_msginfo *buffer)
{
	int type = lua_type(L, index);
	if(type == LUA_TUSERDATA)
	{
		void **p;
		if((p = (void **)luaL_check_udata_type(L, index, "irc_msginfo")))
		{
			if(!*p)
				luaL_error(L, "null irc_msginfo object");
			else
			{
				return (irc_msginfo *)*p;
			}
		}
		else if((p = (void **)luaL_check_udata_type(L, index, "irc_ident")))
		{
			if(!*p)
				luaL_error(L, "Null irc_ident object");
			else
			{
				buffer->setchan(((irc_ident *)*p)->getnick());
				return buffer;
			}
		}
	}
	else if(type == LUA_TSTRING)
	{
		const char *chan = luaL_checkstring(L, index);
		buffer->setchan(chan);
		return buffer;
	}
	
	luaL_error(L, "invalid arg (expected irc_msginfo or string or irc_ident)");
	return NULL; // avoid warning
}

static irc_msginfo **lua_push_msginfo(lua_State *L, irc_msginfo *msginfo)
{
	// irc_msginfo **data = (irc_msginfo **)lua_newuserdata(L, sizeof(irc_msginfo *));
	irc_msginfo **data = (irc_msginfo **)lua_newuserdata(L, sizeof(irc_msginfo *) + sizeof(int));
	*data = msginfo;
	*(int *)&data[1] = 0; // delete irc_msginfo obj in gc fxn
	luaL_getmetatable(L, "irc_msginfo");
	lua_setmetatable(L, -2);
	return data;
}

static irc_msginfo **lua_push_msginfo_nofree(lua_State *L, irc_msginfo *msginfo)
{
	// irc_msginfo **data = (irc_msginfo **)lua_newuserdata(L, sizeof(irc_msginfo *));
	irc_msginfo **data = (irc_msginfo **)lua_newuserdata(L, sizeof(irc_msginfo *) + sizeof(int));
	data[0] = msginfo;
	*(int *)&data[1] = 1; // do not delete irc_msginfo obj in gc fxn
	luaL_getmetatable(L, "irc_msginfo");
	lua_setmetatable(L, -2);
	return data;
}

static int lua_irc_msginfo_new(lua_State *L)
{
	// const char *chan = luaL_checkstring(L, 1);
	irc_msginfo msginfo_buffer, 
		*msginfo = lua_check_msginfo_compatible(L, 1, &msginfo_buffer);
	const char *chan = msginfo->getchan().c_str();
	const char *msgtype_str = luaL_optstring(L, 2, NULL);
	
	irc_msgtype msgtype = msginfo->getmsgtype();
	irc_chantype chantype = msginfo->getchantype();
	if(msgtype_str)
	{
		msgtype = translate_msgtypestr(msgtype_str);
		if(msgtype < -1)
			luaL_error(L, "invalid msgtype");
	}
	
	lua_push_msginfo(L, new irc_msginfo(chan, msgtype, chantype));
	return 1;
}

static int lua_irc_msginfo_getchan(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	lua_pushstdstring(L, msginfo->getchan());
	return 1;
}
static int lua_irc_msginfo_setchan(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	const char *chan = luaL_checkstring(L, 2);
	msginfo->setchan(chan);
	return 0;
}

static int lua_irc_msginfo_getmsgtype(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	// lua_pushinteger(L, msginfo->getmsgtype());
	lua_pushstring(L, msgtype2str(msginfo->getmsgtype()));
	return 1;
}
static int lua_irc_msginfo_setmsgtype(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	const char *msgtype_str = luaL_checkstring(L, 2);
	irc_msgtype msgtype = translate_msgtypestr(msgtype_str);
	if(msgtype == IRC_MSGTYPE_NONE)
		luaL_error(L, "invalid msgtype");
	
	msginfo->setmsgtype(msgtype);
	return 0;
}

static int lua_irc_msginfo_getchantype(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	lua_pushstring(L, chantype2str(msginfo->getchantype()));
	return 1;
}

static int lua_irc_msginfo_getpriority(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	lua_pushinteger(L, msginfo->getpriority());
	return 1;
}
static int lua_irc_msginfo_setpriority(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	int priority = luaL_checkinteger(L, 2);
	if(priority < IRC_PRIORITY_MIN || 
		priority > IRC_PRIORITY_MAX)
	{
		luaL_error(L, "priority value out of range");
		return 0;
	}
	msginfo->setpriority(priority);
	return 0;
}

static int lua_irc_msginfo_is_line_converted(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	lua_pushboolean(L, msginfo->is_line_converted());
	return 1;
}
static int lua_irc_msginfo_set_line_converted(lua_State *L)
{
	irc_msginfo *msginfo = lua_checkmsginfo(L, 1);
	luaL_checktype(L, 2, LUA_TBOOLEAN);
	bool v = lua_toboolean(L, 2);
	msginfo->set_line_converted(v);
	return 0;
}


static const luaL_reg irc_msginfo_methods[] = 
{
	{"new", lua_irc_msginfo_new}, 
	
	{"getchan", lua_irc_msginfo_getchan}, 
	{"setchan", lua_irc_msginfo_setchan}, 
	{"getmsgtype", lua_irc_msginfo_getmsgtype}, 
	{"setmsgtype", lua_irc_msginfo_setmsgtype}, 
	{"getchantype", lua_irc_msginfo_getchantype}, 
	// {"setchantype", lua_irc_msginfo_setchantype}, 
	{"getpriority", lua_irc_msginfo_getpriority}, 
	{"setpriority", lua_irc_msginfo_setpriority}, 
	{"is_line_converted", lua_irc_msginfo_is_line_converted}, 
	{"set_line_converted", lua_irc_msginfo_set_line_converted}, 
	
	{NULL, NULL}
};

static int lua_irc_msginfo_gc(lua_State *L)
{
	irc_msginfo **data = (irc_msginfo **)lua_touserdata(L, 1);
	if(!data)
	{
		luaL_typerror(L, 1, "irc_msginfo");
	}
	if(data[1] == (irc_msginfo *)0) // 0이 아닐 경우 delete하지 않음
	{
		irc_msginfo *msginfo = *data;
		if(msginfo)
		{
			delete msginfo;
		}
	}
	return 0;
}

static const luaL_reg irc_msginfo_meta_methods[] = 
{
	{"__gc", lua_irc_msginfo_gc}, 
	{"__tostring", lua_irc_msginfo_getchan}, 
	{NULL, NULL}
};


static int lua_register_irc_msginfolib(lua_State *L)
{
	int methods, metatable;
	
	luaL_register(L, "irc_msginfo", irc_msginfo_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "irc_msginfo"); metatable = lua_gettop(L); // create metatable, add it to the Lua registry
	luaL_register(L, NULL, irc_msginfo_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__metatable = methods
	
	lua_pop(L, 2); // drop metatable, methods table
	
	return 0;
}

//////////////////////////////////


static ircbot_conn *lua_tobot_ircconn(lua_State *L, int index)
{
	ircbot_conn **data = (ircbot_conn **)lua_touserdata(L, index);
	if(!data)
	{
		luaL_typerror(L, index, "meowbot_ircconn");
	}
	return *data;
}

static ircbot_conn *lua_checkbot_ircconn(lua_State *L, int index)
{
	ircbot_conn **data, *isrv;
	luaL_checktype(L, index, LUA_TUSERDATA);
	data = (ircbot_conn **)luaL_checkudata(L, index, "meowbot_ircconn");
	if(!data)
	{
		luaL_typerror(L, index, "meowbot_ircconn");
	}
	isrv = *data;
	if(!isrv)
	{
		luaL_error(L, "Null meowbot_ircconn object");
	}
	
	// FIXME: this is somewhat expensive
	if(!g_bot->check_ircconn(isrv))
	{
		luaL_error(L, "Attempted to use deleted irc_conn object");
	}
	
	return isrv;
}

static ircbot_conn **lua_push_bot_ircconn(lua_State *L, ircbot_conn *isrv)
{
	ircbot_conn **data = (ircbot_conn **)lua_newuserdata(L, sizeof(ircbot_conn *));
	*data = isrv;
	luaL_getmetatable(L, "meowbot_ircconn");
	lua_setmetatable(L, -2);
	return data;
}


static int lua_meowbot_ircconn_getbot(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	lua_push_bot(L, isrv->bot);
	return 1;
}

static int lua_meowbot_ircconn_privmsg(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	irc_msginfo msginfo_buf, 
		&msginfo = *lua_check_msginfo_compatible(L, 2, &msginfo_buf);
	const char *msg = luaL_checkstring(L, 3);
	
	isrv->privmsg(msginfo, msg);
	lua_pushboolean(L, true);
	return 1;
}

static int lua_meowbot_ircconn_privmsgf(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	irc_msginfo msginfo_buf, 
		&msginfo = *lua_check_msginfo_compatible(L, 2, &msginfo_buf);
	const char *fmt = luaL_checkstring(L, 3);
	char buf[512*3]; // rfc2812: max line length is 510, so this will be enough in most cases
	my_vsnprintf_lua(buf, sizeof(buf), fmt, L, 4);
	
	isrv->privmsg(msginfo, buf);
	lua_pushboolean(L, true);
	return 1;
}

static int lua_meowbot_ircconn_privmsg_nh(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	irc_msginfo msginfo_buf, 
		&msginfo = *lua_check_msginfo_compatible(L, 2, &msginfo_buf);
	const char *msg = luaL_checkstring(L, 3);
	
	isrv->privmsg_nh(msginfo, msg);
	lua_pushboolean(L, true);
	return 1;
}

static int lua_meowbot_ircconn_privmsgf_nh(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	irc_msginfo msginfo_buf, 
		&msginfo = *lua_check_msginfo_compatible(L, 2, &msginfo_buf);
	const char *fmt = luaL_checkstring(L, 3);
	char buf[512*3]; // rfc2812: max line length is 510, so this will be enough in most cases
	my_vsnprintf_lua(buf, sizeof(buf), fmt, L, 4);
	
	isrv->privmsg_nh(msginfo, buf);
	lua_pushboolean(L, true);
	return 1;
}

static int lua_meowbot_ircconn_prevent_irc_highlighting_all(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *chan = luaL_checkstring(L, 2);
	const char *msg = luaL_checkstring(L, 3);
	lua_pushstdstring(L, isrv->prevent_irc_highlighting_all(chan, msg));
	return 1;
}

static int lua_meowbot_ircconn_quote(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *msg = luaL_checkstring(L, 2);
	int priority = luaL_optinteger(L, 3, IRC_PRIORITY_NORMAL);
	bool convert_enc = luaL_optinteger(L, 4, 1) == 1;
	
	isrv->quote(msg, priority, convert_enc);
	lua_pushboolean(L, true);
	return 1;
}

static int lua_meowbot_ircconn_quotef(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *fmt = luaL_checkstring(L, 2);
	int priority = IRC_PRIORITY_NORMAL;
	char buf[1024];
	my_vsnprintf_lua(buf, sizeof(buf), fmt, L, 3);
	
	isrv->quote(buf, priority);
	lua_pushboolean(L, true);
	return 1;
}

static int lua_meowbot_ircconn_my_nick(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	lua_pushstdstring(L, isrv->my_nick());
	return 1;
}

static int lua_meowbot_ircconn_get_chanuserlist(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *chan = luaL_checkstring(L, 2);
	
	isrv->rdlock_chanuserlist();
	const irc_strmap<std::string, irc_user> &list = 
		isrv->get_chanuserlist(chan);
	irc_strmap<std::string, irc_user>::const_iterator it;
	lua_createtable(L, 0, list.size());
	int listtbl = lua_gettop(L);
	for(it = list.begin(); it != list.end(); it++)
	{
		//lua_pushstdstring(L, it->first); // hack
		lua_pushstring(L, it->first.c_str());
		
		lua_createtable(L, 0, 5);
#define SETUSERTBL(name) \
	lua_pushliteral(L, #name); \
	/*lua_pushstdstring(L, it->second.get##name()); HACK*/ \
	lua_pushstring(L, it->second.get##name().c_str()); \
	lua_rawset(L, -3);
		SETUSERTBL(ident)
		SETUSERTBL(nick)
		SETUSERTBL(user)
		SETUSERTBL(host)
		SETUSERTBL(chanusermode)
#undef SETUSERTBL
		lua_rawset(L, listtbl);
	}
	isrv->unlock_chanuserlist();
	
	return 1;
}

static int lua_meowbot_ircconn_get_chanuserinfo(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *chan = luaL_checkstring(L, 2);
	const char *nick = luaL_checkstring(L, 3);
	
	isrv->rdlock_chanuserlist();
	const irc_strmap<std::string, irc_user> &list = 
		isrv->get_chanuserlist(chan);
	irc_strmap<std::string, irc_user>::const_iterator it = list.find(nick);
	if(it == list.end())
	{
		lua_pushnil(L);
	}
	else
	{
		lua_createtable(L, 0, 5);
#define SETUSERTBL(name) \
	lua_pushliteral(L, #name); \
	/*lua_pushstdstring(L, it->second.get##name()); HACK*/ \
	lua_pushstring(L, it->second.get##name().c_str()); \
	lua_rawset(L, -3);
		SETUSERTBL(ident)
		SETUSERTBL(nick)
		SETUSERTBL(user)
		SETUSERTBL(host)
		SETUSERTBL(chanusermode)
#undef SETUSERTBL
	}
	isrv->unlock_chanuserlist();
	
	return 1;
}

static int lua_meowbot_ircconn_get_chanlist(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	
	isrv->rdlock_chanlist();
	const std::vector<std::string> &list = 
		isrv->get_chanlist();
	std::vector<std::string>::const_iterator it;
	lua_createtable(L, list.size(), 0);
	int listtbl = lua_gettop(L);
	int n = 1;
	for(it = list.begin(); it != list.end(); it++)
	{
		lua_pushstdstring(L, *it);
		lua_rawseti(L, listtbl, n);
		n++;
	}
	isrv->unlock_chanlist();
	
	return 1;
}

static int lua_meowbot_ircconn_is_joined(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *chan = luaL_checkstring(L, 2);
	lua_pushboolean(L, isrv->is_joined(chan));
	return 1;
}

static int lua_meowbot_ircconn_get_srvinfo(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *name = luaL_checkstring(L, 2);
	
	if(!strcmp(name, "uhnames_on"))
		lua_pushboolean(L, isrv->srvinfo.uhnames_on);
	else if(!strcmp(name, "namesx_on"))
		lua_pushboolean(L, isrv->srvinfo.namesx_on);
	else if(!strcmp(name, "netname"))
		lua_pushstdstring(L, isrv->srvinfo.netname);
	else if(!strcmp(name, "chantypes"))
		lua_pushstdstring(L, isrv->srvinfo.chantypes);
	else if(!strcmp(name, "chanmodes"))
		lua_pushstdstring(L, isrv->srvinfo.chanmodes);
	else if(!strcmp(name, "chanusermodes"))
		lua_pushstdstring(L, isrv->srvinfo.chanusermodes);
	else if(!strcmp(name, "chanusermode_prefixes"))
		lua_pushstdstring(L, isrv->srvinfo.chanusermode_prefixes);
	else if(!strcmp(name, "max_channels"))
		lua_pushinteger(L, isrv->srvinfo.max_channels);
	else if(!strcmp(name, "nicklen"))
		lua_pushinteger(L, isrv->srvinfo.nicklen);
	else if(!strcmp(name, "casemapping"))
		lua_pushstdstring(L, isrv->srvinfo.casemapping);
	else if(!strcmp(name, "charset"))
		lua_pushstdstring(L, isrv->srvinfo.charset);
	else if(!strcmp(name, "encoding"))
		lua_pushstdstring(L, isrv->get_encoding());
	else
		lua_pushnil(L);
	
	return 1;
}

static int lua_meowbot_ircconn_equals(lua_State *L)
{
	ircbot_conn *isrv = lua_tobot_ircconn(L, 1);
	ircbot_conn *isrv2 = lua_tobot_ircconn(L, 2);
	lua_pushboolean(L, isrv == isrv2);
	return 1;
}

static int lua_meowbot_ircconn_get_connid(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	lua_pushinteger(L, isrv->get_connid());
	return 1;
}

static int lua_meowbot_ircconn_get_name(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	lua_pushstdstring(L, isrv->get_name());
	return 1;
}

static int lua_meowbot_ircconn_get_status(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	int status = isrv->get_status();
	const char *statstr;
	switch(status)
	{
	case IRCSTATUS_DISCONNECTED:
		statstr = "disconnected";
		break;
	case IRCSTATUS_CONNECTING:
		statstr = "connecting";
		break;
	case IRCSTATUS_CONNECTED:
		statstr = "connected";
		break;
	default:
		statstr = "unknown";
	}
	lua_pushstring(L, statstr);
	return 1;
}

static int lua_meowbot_ircconn_get_netname(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	lua_pushstdstring(L, isrv->srvinfo.netname);
	return 1;
}

static int lua_meowbot_ircconn_quit(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *quitmsg = luaL_checkstring(L, 2);
	lua_pushboolean(L, isrv->quit(quitmsg));
	return 1;
}
static int lua_meowbot_ircconn_disconnect(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	lua_pushboolean(L, isrv->disconnect());
	return 1;
}
static int lua_meowbot_ircconn_reconnect(lua_State *L)
{
	ircbot_conn *isrv = lua_checkbot_ircconn(L, 1);
	const char *quitmsg = luaL_checkstring(L, 2);
	lua_pushboolean(L, isrv->reconnect(quitmsg));
	return 1;
}

static const luaL_reg meowbot_ircconn_methods[] = 
{
	{"getbot", lua_meowbot_ircconn_getbot}, 
	
	{"quote", lua_meowbot_ircconn_quote}, 
	{"quotef", lua_meowbot_ircconn_quotef}, 
	{"privmsg", lua_meowbot_ircconn_privmsg}, 
	{"privmsgf", lua_meowbot_ircconn_privmsgf}, 
	{"privmsg_nh", lua_meowbot_ircconn_privmsg_nh}, 
	{"privmsgf_nh", lua_meowbot_ircconn_privmsgf_nh}, 
	
	{"prevent_irc_highlighting_all", lua_meowbot_ircconn_prevent_irc_highlighting_all}, 
	{"my_nick", lua_meowbot_ircconn_my_nick}, 
	{"get_srvinfo", lua_meowbot_ircconn_get_srvinfo}, 
	{"get_chanlist", lua_meowbot_ircconn_get_chanlist}, 
	{"get_chanuserlist", lua_meowbot_ircconn_get_chanuserlist}, 
	{"get_chanuserinfo", lua_meowbot_ircconn_get_chanuserinfo}, 
	{"is_joined", lua_meowbot_ircconn_is_joined}, 
	
	{"quit", lua_meowbot_ircconn_quit}, 
	{"disconnect", lua_meowbot_ircconn_disconnect}, 
	{"reconnect", lua_meowbot_ircconn_reconnect}, 
	
	{"equals", lua_meowbot_ircconn_equals}, 
	//{"conn_id", lua_meowbot_ircconn_get_connid}, 
	{"get_connid", lua_meowbot_ircconn_get_connid}, 
	{"get_name", lua_meowbot_ircconn_get_name}, 
	{"get_status", lua_meowbot_ircconn_get_status}, 
	{"get_netname", lua_meowbot_ircconn_get_netname}, 
	
	{NULL, NULL}
};

static int lua_meowbot_ircconn_gc(lua_State *L)
{
	(void)L;
	
	return 0;
}

static const luaL_reg meowbot_ircconn_meta_methods[] = 
{
	{"__gc", lua_meowbot_ircconn_gc}, 
	{"__eq", lua_meowbot_ircconn_equals}, 
	{NULL, NULL}
};


static int lua_register_meowbot_ircconn_lib(lua_State *L)
{
	int methods, metatable;
	
	luaL_register(L, "meowbot_ircconn", meowbot_ircconn_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "meowbot_ircconn"); metatable = lua_gettop(L); // create metatable, add it to the Lua registry
	luaL_register(L, NULL, meowbot_ircconn_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__metatable = methods
	
	lua_pop(L, 2); // drop metatable, methods table
	
	return 0;
}

//////////////////////////////////

#if 0
static ircbot *lua_tobot(lua_State *L, int index)
{
	ircbot **data = (ircbot **)lua_touserdata(L, index);
	if(!data)
	{
		luaL_typerror(L, index, "meowbot");
	}
	return *data;
}
#endif

static ircbot *lua_checkbot(lua_State *L, int index)
{
	ircbot **data, *bot;
	luaL_checktype(L, index, LUA_TUSERDATA);
	data = (ircbot **)luaL_checkudata(L, index, "meowbot");
	if(!data)
	{
		luaL_typerror(L, index, "meowbot");
	}
	bot = *data;
	if(!bot)
	{
		luaL_error(L, "Null meowbot object");
	}
	return bot;
}

static ircbot **lua_push_bot(lua_State *L, ircbot *bot)
{
	ircbot **data = (ircbot **)lua_newuserdata(L, sizeof(ircbot *));
	*data = bot;
	luaL_getmetatable(L, "meowbot");
	lua_setmetatable(L, -2);
	return data;
}

static int cmd_cb_lua(ircbot_conn *isrv, irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *);
static int lua_meowbot_register_cmd(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	const char *cmd = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);
	
	if(bot->register_cmd(cmd, cmd_cb_lua))
	{
		pthread_rwlock_wrlock(&g_cmdmap_lock);
		
		g_cmdmap[cmd].ref(L, 3);
		g_cmdmap[cmd].L = L->l_G->mainthread;
		
		pthread_rwlock_unlock(&g_cmdmap_lock);
		lua_pushboolean(L, true);
	}
	else
		lua_pushboolean(L, false);
	
	return 1;
}

static int lua_meowbot_unregister_cmd(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	const char *cmd = luaL_checkstring(L, 2);
	
	if(bot->unregister_cmd(cmd))
	{
		pthread_rwlock_wrlock(&g_cmdmap_lock);
		caseless_strmap<std::string, luaref>::iterator it = g_cmdmap.find(cmd);
		
		lua_State *L2 = it->second.L;
		if(unlikely(!L2))
		{
			fprintf(stderr, "Warning: luacmd lua_State is NULL\n");
			fflush(stderr);
		}
		else
		{
			lock_lua_instance(L2);
			it->second.unref();
			unlock_lua_instance(L2);
		}
		
		g_cmdmap.erase(it);
		pthread_rwlock_unlock(&g_cmdmap_lock);
		lua_pushboolean(L, true);
	}
	else
		lua_pushboolean(L, false);
	
	return 1;
}

static int lua_meowbot_is_botadmin(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	
	const char *ident = luaL_checkstring(L, 2);
	
	lua_pushboolean(L, bot->is_botadmin(ident));
	return 1;
}

static int lua_meowbot_register_event_handler(lua_State *L)
{
	// ircbot *bot = lua_checkbot(L, 1);
	const char *evname = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);
	
	int evid = evname2id(evname);
	if(evid < 0)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	
	pthread_rwlock_wrlock(&g_eventhdlmap_lock);
	std::vector<luaref> &cblist = g_eventhdlmap[evid];
	std::vector<luaref>::const_iterator it = cblist.begin();
	for(; it != cblist.end(); it++)
	{
		if(it->L == L)
		{
			pthread_rwlock_unlock(&g_eventhdlmap_lock);
			lua_pushboolean(L, false);
			return 1;
		}
	}
	
	luaref fxn(L, 3);
	fxn.L = L->l_G->mainthread;
	
	cblist.push_back(fxn);
	pthread_rwlock_unlock(&g_eventhdlmap_lock);
	fxn.clearref(); // dtor에 의해 unref되는것을 막음
	
	lua_pushboolean(L, true);
	return 1;
}

static int lua_meowbot_unregister_event_handler(lua_State *L)
{
	// ircbot *bot = lua_checkbot(L, 1);
	const char *evname = luaL_checkstring(L, 2);
	
	int evid = evname2id(evname);
	if(evid < 0)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	
	pthread_rwlock_wrlock(&g_eventhdlmap_lock);
	std::vector<luaref> &cblist = g_eventhdlmap[evid];
	if(cblist.empty())
	{
		pthread_rwlock_unlock(&g_eventhdlmap_lock);
		lua_pushboolean(L, false);
		return 1;
	}
	
	lua_State *Lm = L->l_G->mainthread;
	std::vector<luaref>::iterator it = cblist.begin();
	for(; it != cblist.end(); it++)
	{
		if(it->L == Lm)
		{
			it->L = L;
			it->unref();
			
			cblist.erase(it);
			pthread_rwlock_unlock(&g_eventhdlmap_lock);
			lua_pushboolean(L, true);
			return 1;
		}
	}
	pthread_rwlock_unlock(&g_eventhdlmap_lock);
	
	lua_pushboolean(L, false);
	return 1;
}

static int lua_meowbot_parse_timerattr_str(lua_State *L, const char *attrstr)
{
	unsigned int attr = 0;
	
	int len;
	const char *p = attrstr, *next;
	do
	{
		next = strchr(p, ',');
		if(next)
			len = next - p;
		else
			len = strlen(p);
		
		if(len > 0)
		{
			if(!strncasecmp(p, "once", len))
				attr |= IRCBOT_TIMERATTR_ONCE;
			else if(!strncasecmp(p, "attime", len))
				attr |= IRCBOT_TIMERATTR_ATTIME;
			else
			{
				char buf[128] = "invalid timer attribute string: `";
				strncat(buf, p, len>32?32:len);
				strcat(buf, "'");
				luaL_error(L, buf);
			}
		}
		
		p = next + 1;
	} while(next);
	
	return attr;
}
static int lua_meowbot_add_timer(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	const char *tmrname = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);
	int interval = luaL_checkint(L, 4);
	// stack5: userdata
	int attr = lua_meowbot_parse_timerattr_str(L, luaL_optstring(L, 6, ""));
	
	// 새 timer object 생성
	bottimer_lua *tp = g_bottmrlist.newobj();
	tp->L = L->l_G->mainthread;
	
	// 함수 등록
	lua_pushvalue(L, 3); // timer function
	tp->cbfxn_refid = luaL_ref(L, LUA_REGISTRYINDEX);
	
	// userdata 등록
	if(lua_type(L, 5) == LUA_TNIL)
		tp->userdata_refid = LUA_NOREF;
	else
	{
		lua_pushvalue(L, 5); // timer userdata
		tp->userdata_refid = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	// 속성, 이름 설정
	tp->name = tmrname;
	tp->attr = attr;
	
	// 등록
	int ret = bot->add_timer(tmrname, bottimer_callback_lua, interval, tp, attr);
	lua_pushboolean(L, ret > 0);
	
	return 1;
}

static int lua_meowbot_del_timer(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	// stack2: timername or timerfxn
	int tmrtype = lua_type(L, 2);
	
	if(tmrtype == LUA_TFUNCTION)
	{
		// 조낸복잡해-_-
		int ndeleted = 0;
		bottimer_lua *tmr;
		g_bottmrlist.wrlock();
		for(size_t i = 0; i < g_bottmrlist.getsize(); )
		{
			bool erased = false;
			tmr = g_bottmrlist[i];
			if(tmr->L == L->l_G->mainthread)
			{
				lua_rawgeti(L, LUA_REGISTRYINDEX, tmr->cbfxn_refid);
				if(lua_rawequal(L, 2, -1) == 1)
				{
					// timer found. erase it
					bot->wrlock_timers();
					for(std::vector<ircbot_timer_info>::iterator it = bot->timers.begin(); 
						it != bot->timers.end(); )
					{
						if(it->fxn == bottimer_callback_lua && 
							it->userdata == tmr)
						{
							bot->timers.erase(it);
							ndeleted++;
							erased = true;
						}
						else
							it++;
					}
					bot->sort_timers();
					bot->unlock_timers();
					bot->signal_timer_thread();
					g_bottmrlist.del_by_id(i, false);
				}
				lua_pop(L, 1); // pop fxn index
			}
			if(!erased)
				i++;
		}
		g_bottmrlist.unlock();
		
		lua_pushboolean(L, ndeleted > 0);
	}
	else if(tmrtype == LUA_TSTRING)
	{
		const char *tmrname = luaL_checkstring(L, 2);
		int ret = bot->del_timer(tmrname);
		
		g_bottmrlist.wrlock();
		for(size_t i = 0; i < g_bottmrlist.getsize(); )
		{
			bottimer_lua *tmr = g_bottmrlist[i];
			if(tmr->name == tmrname)
				g_bottmrlist.del_by_id(i, false);
			else
				i++;
		}
		g_bottmrlist.unlock();
		
		lua_pushboolean(L, ret > 0);
	}
	else
	{
		luaL_error(L, "meowbot.del_timer: arg1: expected function or string");
		// never reaches
		return 0;
	}
	
	return 1;
}

static int lua_meowbot_version(lua_State *L)
{
	//ircbot *bot = lua_checkbot(L, 1);
	
	// lua_pushstring(L, bot->version());
	lua_pushliteral(L, "meowbot 0.23");
	
	return 1;
}

static int lua_meowbot_get_conns(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	
	bot->irc_conns.rdlock();
	lua_createtable(L, bot->irc_conns.getsize(), 0);
	for(size_t i = 0; i < bot->irc_conns.getsize(); i++)
	{
		ircbot_conn *isrv = bot->irc_conns[i];
		lua_push_bot_ircconn(L, isrv);
		lua_rawseti(L, -2, i+1);
	}
	bot->irc_conns.unlock();
	
	return 1;
}

static int lua_meowbot_get_conn_by_name(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	const char *name = luaL_checkstring(L, 2);
	
	ircbot_conn *ic = bot->find_conn_by_name(name);
	if(ic)
		lua_push_bot_ircconn(L, ic);
	else
		lua_pushnil(L);
	
	return 1;
}

static int lua_meowbot_quit_all(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	const char *quitmsg = luaL_optstring(L, 2, "");
	int ret = bot->quit_all(quitmsg);
	lua_pushboolean(L, ret == 0);
	return 1;
}

static int lua_meowbot_disconnect_all(lua_State *L)
{
	ircbot *bot = lua_checkbot(L, 1);
	int ret = bot->disconnect_all();
	lua_pushboolean(L, ret == 0);
	return 1;
}


static const luaL_reg meowbot_methods[] = 
{
	{"register_cmd", lua_meowbot_register_cmd}, 
	{"unregister_cmd", lua_meowbot_unregister_cmd}, 
	{"register_event_handler", lua_meowbot_register_event_handler}, 
	{"unregister_event_handler", lua_meowbot_unregister_event_handler}, 
	{"add_timer", lua_meowbot_add_timer}, 
	{"del_timer", lua_meowbot_del_timer}, 
	
	{"is_botadmin", lua_meowbot_is_botadmin}, 
	{"version", lua_meowbot_version}, 
	{"quit_all", lua_meowbot_quit_all}, 
	{"disconnect_all", lua_meowbot_disconnect_all}, 
	
	{"get_conns", lua_meowbot_get_conns}, 
	{"get_conn_by_name", lua_meowbot_get_conn_by_name}, 
	
	{NULL, NULL}
};

static int lua_meowbot_gc(lua_State *L)
{
	(void)L;
	
	return 0;
}

static const luaL_reg meowbot_meta_methods[] = 
{
	{"__gc", lua_meowbot_gc}, 
	{NULL, NULL}
};


static int lua_register_meowbot_lib(lua_State *L)
{
	int methods, metatable;
	
	luaL_register(L, "meowbot", meowbot_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "meowbot"); metatable = lua_gettop(L); // create metatable, add it to the Lua registry
	luaL_register(L, NULL, meowbot_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__metatable = methods
	
	lua_pop(L, 2); // drop metatable
	
	return 0;
}


//////////////////////////////////



// FIXME
time_t timestr_to_time(const std::string &timestr)
{
	static pcrecpp::RE re_timestr(
		"(\\d++)\\s*+([가-힣a-zA-Z]*+)"
		, pcrecpp::UTF8());
	time_t t = 0;
	int num;
	std::string unit;
	pcrecpp::StringPiece input(timestr);
	
	while(re_timestr.FindAndConsume(&input, &num, &unit))
	{
		if(unit.empty() || unit == "초" || unit.substr(0,3) == "sec")
			t += num;
		else if(unit == "분" || unit.substr(0, 3) == "min")
			t += num * 60;
		else if(unit == "시간" || unit.substr(0, 4) == "hour" || unit.substr(0, 2) == "hr")
			t += num * 3600;
		else if(unit == "일" || unit.substr(0, 3) == "day")
			t += num * 3600 * 24;
	}
	
	return t;
}
time_t datestr_to_time(const std::string &timestr, const char **errmsg_out = NULL)
{
	static pcrecpp::RE
		re_space("\\s++"), 
		re_timestr1("(\\d++)\\s*+([가-힣a-zA-Z]++)", pcrecpp::UTF8()), 
		// re_timestr2("(?:([0-9]{2,4})/)?([0-9]{1,2})/([0-9]{1,2})", pcrecpp::UTF8()), // 년/월/일
		re_timestr_ymd("([0-9]{2,4})[/-]([0-9]{1,2})[/-]([0-9]{1,2})", pcrecpp::UTF8()), // 년/월/일
		re_timestr_md("([0-9]{1,2})[/-]([0-9]{1,2})", pcrecpp::UTF8()), // 월/일
		//re_timestr_hms("([0-9]{1,2}):([0-9]{1,2})(?::([0-9]{1,2}))?", pcrecpp::UTF8()); // 시:분:초 // 이게 왜 안되는거임-_-
		re_timestr_hms("([0-9]{1,2}):([0-9]{1,2}):([0-9]{1,2})", pcrecpp::UTF8()), // 시:분:초
		re_timestr_hm("([0-9]{1,2}):([0-9]{1,2})", pcrecpp::UTF8()); // 시:분
	pcrecpp::StringPiece input(timestr);
	time_t crnttime;
	struct tm crnttm, date;
	const char *errmsg = NULL;
	
	crnttime = time(NULL);
	localtime_r(&crnttime, &crnttm);
	date.tm_sec = -1; // 0~59
	date.tm_min = -1; // 0~59
	date.tm_hour = -1; // 0~23
	date.tm_mday = -1; // 1~31
	date.tm_mon = -1; // 0~11
	date.tm_year = -1; // year-1900
	// date.tm_wday = -1; // 0~6
	// date.tm_yday = -1; // 0~365
	date.tm_isdst = 0;
	
	int num;
	int tyear, tmon, tday, thour, tmin, tsec;
	std::string unit;
	while(1)
	{
#define TMPARSEERR(msg) ({errmsg = (msg); break;})
		if(re_timestr1.Consume(&input, &num, &unit))
		{
			char unitchr = unit[0];
			if(unit.empty() || unit == "초" || unitchr == 's')
			{
				if(date.tm_sec >= 0)
					TMPARSEERR("`초'가 2번 이상 있으면 안됩니다");
				if(num < 0 || num > 59)
					TMPARSEERR("`초'는 0~59사이만 가능합니다");
				date.tm_sec = num;
			}
			else if(unit == "분" || unitchr == 'm')
			{
				if(date.tm_min >= 0)
					TMPARSEERR("`분'이 2번 이상 있으면 안됩니다");
				if(num < 0 || num > 59)
					TMPARSEERR("`분'은 0~59사이만 가능합니다");
				date.tm_min = num;
			}
			else if(unit == "시" || unitchr == 'h')
			{
				if(date.tm_hour >= 0)
					TMPARSEERR("`시'가 2번 이상 있으면 안됩니다");
				if(num < 0 || num > 23)
					TMPARSEERR("`시'는 0~23사이만 가능합니다");
				date.tm_hour = num;
			}
			else if(unit == "일" || unitchr == 'd')
			{
				if(date.tm_mday >= 0)
					TMPARSEERR("`일'이 2번 이상 있으면 안됩니다");
				if(num < 1 || num > 31)
					TMPARSEERR("`일'은 1~31사이만 가능합니다");
				date.tm_mday = num;
			}
			else if(unit == "월" || unitchr == 'd')
			{
				if(date.tm_mon >= 0)
					TMPARSEERR("`월'이 2번 이상 있으면 안됩니다");
				if(num < 1 || num > 12)
					TMPARSEERR("`월'은 1~12사이만 가능합니다");
				date.tm_mon = num;
			}
			else if(unit == "년" || unitchr == 'd')
			{
				if(date.tm_year >= 0)
					TMPARSEERR("`년'이 2번 이상 있으면 안됩니다");
				if(num < 1900)
					TMPARSEERR("`년'은 1900년 이상만 가능합니다");
				date.tm_year = num;
			}
		}
		else if(re_timestr_ymd.Consume(&input, &tyear, &tmon, &tday))
		{
			if(tyear < 100)
				tyear += 2000;
			if(date.tm_year >= 0)
				TMPARSEERR("`년'이 2번 이상 있으면 안됩니다");
			if(tyear < 1900)
				TMPARSEERR("`년'은 1900년 이상만 가능합니다");
			if(date.tm_mon >= 0)
				TMPARSEERR("`월'이 2번 이상 있으면 안됩니다");
			if(tmon < 1 || tmon > 12)
				TMPARSEERR("`월'은 1~12사이만 가능합니다");
			if(date.tm_mday >= 0)
				TMPARSEERR("`일'이 2번 이상 있으면 안됩니다");
			if(tday < 1 || tday > 31)
				TMPARSEERR("`일'은 1~31사이만 가능합니다");
			date.tm_year = tyear;
			date.tm_mon = tmon;
			date.tm_mday = tday;
		}
		else if(re_timestr_md.Consume(&input, &tmon, &tday))
		{
			if(date.tm_mon >= 0)
				TMPARSEERR("`월'이 2번 이상 있으면 안됩니다");
			if(tmon < 1 || tmon > 12)
				TMPARSEERR("`월'은 1~12사이만 가능합니다");
			if(date.tm_mday >= 0)
				TMPARSEERR("`일'이 2번 이상 있으면 안됩니다");
			if(tday < 1 || tday > 31)
				TMPARSEERR("`일'은 1~31사이만 가능합니다");
			date.tm_mon = tmon;
			date.tm_mday = tday;
		}
		else if(re_timestr_hms.Consume(&input, &thour, &tmin, &tsec))
		{
			if(date.tm_hour >= 0)
				TMPARSEERR("`시'가 2번 이상 있으면 안됩니다");
			if(thour < 0 || thour > 23)
				TMPARSEERR("`시'는 0~23사이만 가능합니다");
			if(date.tm_min >= 0)
				TMPARSEERR("`분'이 2번 이상 있으면 안됩니다");
			if(tmin < 0 || tmin > 59)
				TMPARSEERR("`분'은 0~59사이만 가능합니다");
			if(date.tm_sec >= 0)
				TMPARSEERR("`초'가 2번 이상 있으면 안됩니다");
			if(tsec < 0 || tsec > 59)
				TMPARSEERR("`초'는 0~59사이만 가능합니다");
			date.tm_hour = thour;
			date.tm_min = tmin;
			date.tm_sec = tsec;
		}
		else if(re_timestr_hm.Consume(&input, &thour, &tmin))
		{
			if(date.tm_hour >= 0)
				TMPARSEERR("`시'가 2번 이상 있으면 안됩니다");
			if(thour < 0 || thour > 23)
				TMPARSEERR("`시'는 0~23사이만 가능합니다");
			if(date.tm_min >= 0)
				TMPARSEERR("`분'이 2번 이상 있으면 안됩니다");
			if(tmin < 0 || tmin > 59)
				TMPARSEERR("`분'은 0~59사이만 가능합니다");
			date.tm_hour = thour;
			date.tm_min = tmin;
		}
		else if(re_space.Consume(&input))
		{
			// nothing to do
		}
		else
		{
			if(!input.as_string().empty())
			{
				errmsg = "잘못된 형식입니다";
			}
			break;
		}
#undef TMPARSEERR
	}
	
	date.tm_mon--; // 0~11
	date.tm_year -= 1900;
#define SETCRNTTIMEIFUNDEFP(name) \
	if(date.tm_##name < 0) \
	{ \
		date.tm_##name = crnttm.tm_##name;
#define CLOSEB }
	SETCRNTTIMEIFUNDEFP(year)
	SETCRNTTIMEIFUNDEFP(mon)
	SETCRNTTIMEIFUNDEFP(mday)
	SETCRNTTIMEIFUNDEFP(hour)
	SETCRNTTIMEIFUNDEFP(min)
	SETCRNTTIMEIFUNDEFP(sec)
	CLOSEB CLOSEB CLOSEB
	CLOSEB CLOSEB CLOSEB
#undef SETCRNTTIMEIFUNDEFP
#undef CLOSEB
#define SETZEROIFUNDEF(name, v) \
	if(date.tm_##name < 0) \
		date.tm_##name = (v);
	SETZEROIFUNDEF(year, 0)
	SETZEROIFUNDEF(mon, 0)
	SETZEROIFUNDEF(mday, 1)
	SETZEROIFUNDEF(hour, 0)
	SETZEROIFUNDEF(min, 0)
	SETZEROIFUNDEF(sec, 0)
#undef SETZEROIFUNDEF
	
	time_t t = mktime(&date);
	if(errmsg_out)
		*errmsg_out = errmsg;
	
	return t;
}
//////////////////////////////////


static int lua_str_irc_lower(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, irc_strlower(str));
	return 1;
}
static int lua_str_irc_upper(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, irc_strupper(str));
	return 1;
}
#if 1
//// 20090103
static int lua_get_random_word(lua_State *L)
{
	const char *(*get_random_word_fxn)(const char *startword) = 
		(const char *(*)(const char *))
			g_bot->modules.dlsym("mod_ewordquiz", "get_random_word");
	if(!get_random_word_fxn)
	{
		lua_pushliteral(L, "내부 오류: 이 기능에 필요한 모듈이 로드되지 않았습니다.");
		return 1;
	}
	
	const char *startword = luaL_optstring(L, 1, NULL);
	const char *word = get_random_word_fxn(startword);
	if(!word)
		lua_pushliteral(L, "");
	else
		lua_pushstring(L, word);
	
	return 1;
}
//// 20090120
static int lua_timestr_to_time(lua_State *L)
{
	const char *timestr = luaL_checkstring(L, 1);
	time_t t = timestr_to_time(timestr);
	lua_pushinteger(L, t);
	
	return 1;
}
static int lua_datestr_to_time(lua_State *L)
{
	const char *timestr = luaL_checkstring(L, 1);
	const char *errmsg;
	time_t t = datestr_to_time(timestr, &errmsg);
	lua_pushinteger(L, t);
	if(errmsg)
	{
		lua_pushstring(L, errmsg);
		return 2;
	}
	
	return 1;
}
#endif

static int lua_register_mod_etc_lib(lua_State *L)
{
	// string functions
	lua_getfield(L, LUA_GLOBALSINDEX, "string");
	int strtable = lua_gettop(L);
	
	lua_pushcfunction(L, lua_str_irc_lower);
	lua_setfield(L, strtable, "irc_lower");
	lua_pushcfunction(L, lua_str_irc_upper);
	lua_setfield(L, strtable, "irc_upper");
	lua_pop(L, 1);
	
#if 1
	lua_register(L, "get_random_word", lua_get_random_word);
	lua_register(L, "timestr_to_time", lua_timestr_to_time);
	lua_register(L, "datestr_to_time", lua_datestr_to_time);
#endif
	
	return 0;
}

//////////////////////////////////


class luamod: public luacpp
{
public:
	luamod()
	{
	}
	~luamod()
	{
		if(this->lua)
		{
			// cleanup handler 호출
			lock_lua_instance(this->lua);
			lua_State *Lt = lua_newthread(this->lua);
			luaref thread_ref(this->lua, -1);
			lua_pop(this->lua, 1);
			unlock_lua_instance(this->lua);
			
			lua_getfield(Lt, LUA_GLOBALSINDEX, "cleanup");
			lua_push_bot(Lt, g_bot);
			int ret = lua_pcall(Lt, 1, 0, 0);
			if(ret != 0)
			{
				const char *errmsg = lua_tostring(Lt, -1);
				fprintf(stderr, "lua script error in cleanup callback: %s\n", errmsg);
				fflush(stderr);
				lua_pop(Lt, 1);
			}
			
			lock_lua_instance(this->lua);
			thread_ref.unref();
			unlock_lua_instance(this->lua);
			
			// mutex list에서 삭제
			pthread_rwlock_wrlock(&g_lua_mutexlist_lock);
			std::map<lua_State *, pmutex>::iterator it = g_lua_mutexes.find(this->lua);
			if(it != g_lua_mutexes.end())
				g_lua_mutexes.erase(it);
			else
			{
				fprintf(stderr, "Warning: tried to delete unexistant lua_State %p\n", this->lua);
				fflush(stderr);
			}
			pthread_rwlock_unlock(&g_lua_mutexlist_lock);
			
			// 아래 목록들을 순회하면서 이 lua module이 등록한 것들을 삭제
			// 아아 적절하게 더럽다-_-
			// caseless_strmap<std::string, luaref> g_cmdmap; 순회하면서 이 모듈이 등록한 것들 삭제
			{
				pthread_rwlock_wrlock(&g_cmdmap_lock);
				for(caseless_strmap<std::string, luaref>::iterator it = g_cmdmap.begin(); 
					it != g_cmdmap.end(); )
				{
					if(it->second.L == this->lua)
					{
						g_bot->unregister_cmd(it->first);
						it->second.unref();
						g_cmdmap.erase(it++);
					}
					else
						it++;
				}
				pthread_rwlock_unlock(&g_cmdmap_lock);
			}
			// std::map<int, std::vector<luaref> > g_eventhdlmap; 순회하면서 이 모듈이 등록한 것들 삭제
			{
				pthread_rwlock_wrlock(&g_eventhdlmap_lock);
				for(std::map<int, std::vector<luaref> >::iterator it = g_eventhdlmap.begin(); 
					it != g_eventhdlmap.end(); it++)
				{
					for(std::vector<luaref>::iterator it2 = it->second.begin(); 
						it2 != it->second.end(); )
					{
						if(it2->L == this->lua)
						{
							it2->unref();
							it->second.erase(it2);
						}
						else
							it2++;
					}
				}
				pthread_rwlock_unlock(&g_eventhdlmap_lock);
			}
			// pobj_vector<bottimer_lua> g_bottmrlist; 순회하면서 이 모듈이 등록한 것들 삭제
			{
				g_bottmrlist.wrlock();
				g_bot->wrlock_timers();
				for(size_t i = 0; i < g_bottmrlist.getsize(); )
				{
					bool erased = false;
					bottimer_lua *tmr = g_bottmrlist[i];
					if(tmr->L == this->lua)
					{
						for(std::vector<ircbot_timer_info>::iterator it = g_bot->timers.begin(); 
							it != g_bot->timers.end(); )
						{
							if(it->fxn == bottimer_callback_lua && 
								it->userdata == tmr)
							{
								g_bot->timers.erase(it);
								erased = true;
							}
							else
								it++;
						}
						g_bottmrlist.del_by_id(i, false);
					}
					if(!erased)
						i++;
				}
				g_bot->sort_timers();
				g_bot->unlock_timers();
				g_bot->signal_timer_thread();
				g_bottmrlist.unlock();
			}
			// 이 Lua module이 등록한 값들 삭제 완료
			// lua module 언로드 끝
		}
	}
	int init()
	{
		int ret = static_cast<luacpp *>(this)->init();
		if(ret == 0)
		{
			// 라이브러리 로드
#if 0
			luaL_openlibs(this->lua);
			luaopen_libluapcre(this->lua);
			luaopen_libluautf8(this->lua);
			luaopen_libluahangul(this->lua);
#endif
#if 1
			luaopen_libluaetc(this->lua);
			luaopen_libluasock(this->lua);
			luaopen_libluathread(this->lua);
			luaopen_libluasqlite3(this->lua);
#endif
			lua_register_mod_etc_lib(this->lua);
			lua_register_irc_identlib(this->lua);
			lua_register_irc_msginfolib(this->lua);
			lua_register_meowbot_ircconn_lib(this->lua);
			lua_register_meowbot_lib(this->lua);
			
			// lua mutex list에 추가
			pthread_rwlock_wrlock(&g_lua_mutexlist_lock);
			g_lua_mutexes[this->lua].init();
			pthread_rwlock_unlock(&g_lua_mutexlist_lock);
		}
		return ret;
	}
	
private:
	
	
};







static std::string normalize_luamod_name(const std::string &name)
{
	static pcrecpp::RE re_validate_mod_name("^(?:[a-zA-Z0-9_]+/)?[a-zA-Z0-9_가-힣-]+(?:\\.lua)?$", pcrecpp::UTF8());
	if(!re_validate_mod_name.FullMatch(name))
	{
		return "";
	}
	
	std::string modname(name);
	if(modname.length() >= 4)
	{
		if(strncasecmp(modname.c_str() + modname.length() - 4, ".lua", 4))
			modname += ".lua";
	}
	else
		modname += ".lua";
	
	return modname;
}

static std::map<std::string, luamod> luamods;
static pthread_rwlock_t g_luamods_mutex;
static std::string g_lua_last_error; // FIXME HACK

// do pthread_rwlock_wrlock(&g_luamods_mutex); and call this
static int load_lua_module(const std::string &modname)
{
	int result = 0;
	
	//static std::string modname(normalize_luamod_name(name));
	if(modname.empty())
	{
		g_lua_last_error = "invalid module name";
		result = -2;
	}
	else
	{
		//g_bot->pause_event_threads(true);
		//pthread_rwlock_wrlock(&g_luamods_mutex);
		
		if(luamods.find(modname) != luamods.end())
		{
			g_lua_last_error = "already loaded";
			result = -2;
			return result; // XXX
		}
		
		luamod &lm = luamods[modname];
		lm.init();
		int ret = lm.dofile("./modules/lua/" + modname);
		if(ret != 0)
		{
			g_lua_last_error = lm.get_lasterr_str();
			fprintf(stderr, "%s: error: %s\n", modname.c_str(), g_lua_last_error.c_str());
			fflush(stderr);
			luamods.erase(luamods.find(modname));
			result = -1;
		}
		else
		{
			// init handler 호출
			lock_lua_instance(lm.lua);
			lua_State *Lt = lua_newthread(lm.lua);
			luaref thread_ref(lm.lua, -1);
			lua_pop(lm.lua, 1);
			unlock_lua_instance(lm.lua);
			
			lua_getfield(Lt, LUA_GLOBALSINDEX, "init");
			lua_push_bot(Lt, g_bot);
			int ret = lua_pcall(Lt, 1, 0, 0);
			if(ret != 0)
			{
				const char *errmsg = lua_tostring(Lt, -1);
				fprintf(stderr, "%s: init failed: %s\n", modname.c_str(), errmsg);
				fflush(stderr);
				lua_pop(Lt, 1);
				
				lock_lua_instance(lm.lua);
				thread_ref.unref();
				unlock_lua_instance(lm.lua);
				
				luamods.erase(luamods.find(modname));
				result = -1;
			}
			else
			{
				lock_lua_instance(lm.lua);
				thread_ref.unref();
				unlock_lua_instance(lm.lua);
			}
		}
		//pthread_rwlock_unlock(&g_luamods_mutex);
		//g_bot->pause_event_threads(false);
	}
	
	return result;
}





static int irc_event_handler_lua(ircbot_conn *isrv, ircevent *evdata, void *)
{
	int etype = evdata->event_id;
	lua_State *L, *Lt;
	luaref thread_ref;
	int ret, result = HANDLER_GO_ON;
	bool lret;
	
	if(unlikely(evdata == NULL))
		return HANDLER_GO_ON;
	
#define LUA_PCALL_HANDLER_FXN(nargs) \
	ret = lua_pcall(Lt, (nargs), 1, 0); \
	if(ret != 0) \
	{ \
		const char *errmsg = lua_tostring(Lt, -1); \
		fprintf(stderr, "script error in irc_event_handler: %s\n", errmsg); \
		fflush(stderr); \
		lua_pop(Lt, 1); \
	} \
	else if(lua_isboolean(Lt, -1)) \
	{ \
		lret = lua_toboolean(Lt, -1); \
		if(lret == false) \
		{ \
			lock_lua_instance(L); \
			thread_ref.unref(); \
			unlock_lua_instance(L); \
			result = HANDLER_FINISHED; \
			goto end; \
		} \
	}
#define ITERATE_LOOP_START \
	for(; it != cblist.end(); it++) \
	{ \
		L = it->L; \
		lock_lua_instance(L); \
		Lt = lua_newthread(L); \
		thread_ref.ref(L, -1); \
		lua_pop(L, 1); \
		unlock_lua_instance(L); \
		it->pushref(Lt);
#define ITERATE_LOOP_END \
		lock_lua_instance(L); \
		thread_ref.unref(); \
		unlock_lua_instance(L); \
	}
	
	pthread_rwlock_rdlock(&g_eventhdlmap_lock);
	const std::vector<luaref> &cblist = g_eventhdlmap[etype]; // FIXME: 가끔씩 여기서 segfault 발생-_-
	std::vector<luaref>::const_iterator it = cblist.begin();
	if(cblist.empty())
		goto end;
	
	if(etype == IRCEVENT_IDLE || etype == IRCEVENT_CONNECTED || etype == IRCEVENT_LOGGEDIN || etype == IRCEVENT_DISCONNECTED)
	{
		// no args
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			LUA_PCALL_HANDLER_FXN(1);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_PRIVMSG)
	{
		ircevent_privmsg *event = static_cast<ircevent_privmsg *>(evdata);
		// ident, msginfo, msg
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(*static_cast<irc_ident *>(event)));
			lua_push_msginfo_nofree(Lt, static_cast<irc_msginfo *>(event));
			lua_pushstdstring(Lt, event->getmsg());
			LUA_PCALL_HANDLER_FXN(4);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_JOIN)
	{
		ircevent_join *event = static_cast<ircevent_join *>(evdata);
		// ident, chan
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->who));
			lua_pushstdstring(Lt, event->chan);
			LUA_PCALL_HANDLER_FXN(3);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_PART)
	{
		ircevent_part *event = static_cast<ircevent_part *>(evdata);
		// ident, chan, partmsg
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->who));
			lua_pushstdstring(Lt, event->chan);
			lua_pushstdstring(Lt, event->msg);
			LUA_PCALL_HANDLER_FXN(4);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_KICK)
	{
		ircevent_kick *event = static_cast<ircevent_kick *>(evdata);
		// ident, chan, kicked_user, kickmsg
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->who));
			lua_pushstdstring(Lt, event->chan);
			lua_push_ident(Lt, new irc_ident(event->kicked_user));
			lua_pushstdstring(Lt, event->msg);
			LUA_PCALL_HANDLER_FXN(5);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_QUIT)
	{
		ircevent_quit *event = static_cast<ircevent_quit *>(evdata);
		// ident, quitmsg
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->who));
			lua_pushstdstring(Lt, event->msg);
			LUA_PCALL_HANDLER_FXN(3);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_NICK)
	{
		ircevent_nick *event = static_cast<ircevent_nick *>(evdata);
		// ident, newnick
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->who));
			lua_pushstdstring(Lt, event->newnick);
			LUA_PCALL_HANDLER_FXN(3);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_MODE)
	{
		ircevent_mode *event = static_cast<ircevent_mode *>(evdata);
		// ident, target, modechar, modeparam
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->who));
			lua_pushstdstring(Lt, event->target);
			lua_pushstdstring(Lt, event->mode);
			lua_pushstdstring(Lt, event->modeparam);
			LUA_PCALL_HANDLER_FXN(5);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_INVITE)
	{
		ircevent_invite *event = static_cast<ircevent_invite *>(evdata);
		// who, chan
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->who));
			lua_pushstdstring(Lt, event->chan);
			LUA_PCALL_HANDLER_FXN(3);
		ITERATE_LOOP_END
	}
	else if(etype == IRCEVENT_SRVINCOMING)
	{
		ircevent_srvincoming *event = static_cast<ircevent_srvincoming *>(evdata);
		// line
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_pushstdstring(Lt, event->line);
			lua_pushboolean(Lt, event->is_line_converted);
			LUA_PCALL_HANDLER_FXN(3);
		ITERATE_LOOP_END
	}
	else if(etype == BOTEVENT_COMMAND)
	{
		botevent_command *event = static_cast<botevent_command *>(evdata);
		// who, src, dest, cmd_trigger, cmd, carg
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_ident(Lt, new irc_ident(event->pmsg));
			lua_push_msginfo(Lt, new irc_msginfo(event->pmsg));
			lua_push_msginfo_nofree(Lt, &event->mdest);
			lua_pushstdstring(Lt, event->cmd_trigger);
			lua_pushstdstring(Lt, event->cmd);
			lua_pushstdstring(Lt, event->carg);
			LUA_PCALL_HANDLER_FXN(7);
		ITERATE_LOOP_END
	}
	else if(etype == BOTEVENT_EVHANDLER_DONE)
	{
		// not supported in lua
		fprintf(stderr, "WARNING: do not use evhandler_done event in lua\n");
		//botevent_evhandler_done *event = static_cast<botevent_evhandler_done *>(evdata);
		// ev
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			// ircevent *event->ev; <- not supported
			LUA_PCALL_HANDLER_FXN(1); //
		ITERATE_LOOP_END
	}
	else if(etype == BOTEVENT_PRIVMSG_SEND)
	{
		botevent_privmsg_send *event = static_cast<botevent_privmsg_send *>(evdata);
		// mdest, msg
		ITERATE_LOOP_START
			lua_push_bot_ircconn(Lt, isrv);
			lua_push_msginfo(Lt, new irc_msginfo(event->mdest));
			lua_pushstdstring(Lt, event->msg);
			LUA_PCALL_HANDLER_FXN(3);
		ITERATE_LOOP_END
	}
#undef LUA_PCALL_HANDLER_FXN
#undef ITERATE_LOOP_START
#undef ITERATE_LOOP_END
	
end:
	pthread_rwlock_unlock(&g_eventhdlmap_lock);
	
	return result;
}

static int cmd_cb_lua(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	pthread_rwlock_rdlock(&g_cmdmap_lock);
	luaref &cb_fxn = g_cmdmap[cmd];
	pthread_rwlock_unlock(&g_cmdmap_lock);
	lua_State *L = cb_fxn.L;
	
	lock_lua_instance(L);
	lua_State *Lt = lua_newthread(L); // create new lua thread
#if 0
	lua_xmove(L, Lt, 1); // save thread reference
#else
	luaref thread_ref(L, -1); // gc를 막기위해 ref생성
	lua_pop(L, 1); // 스택에서 pop
#endif
	unlock_lua_instance(L);
	
	cb_fxn.pushref(Lt);
	lua_push_bot_ircconn(Lt, isrv);
	lua_push_ident(Lt, new irc_ident(pmsg));
	lua_push_msginfo(Lt, new irc_msginfo(pmsg));
	lua_push_msginfo(Lt, new irc_msginfo(mdest));
	lua_pushstdstring(Lt, cmd);
	lua_pushstdstring(Lt, carg);
	int ret = lua_pcall(Lt, 6, 0, 0);
	if(ret != 0)
	{
		const char *errmsg = lua_tostring(Lt, -1);
		isrv->privmsgf(pmsg.getchan(), "스크립트 오류: %s", errmsg);
		lua_pop(Lt, 1);
	}
#if 0
	lua_settop(Lt, 0); // lua_pop(Lt, 1); // removes Lt
#else
	lock_lua_instance(L);
	thread_ref.unref(); // gc에 의해 스레드 해제됨
	unlock_lua_instance(L);
#endif
	
	return 0;
}

static int bottimer_callback_lua(ircbot *bot, const std::string &name, void *userdata)
{
	bottimer_lua *tmr = (bottimer_lua *)userdata;
	lua_State *L = tmr->L;
	
	lock_lua_instance(L);
	lua_State *Lt = lua_newthread(L);
#if 0
	lua_xmove(L, Lt, 1); // push this thread reference to new lua_State
#else
	luaref thread_ref(L, -1); // gc를 막기위해 ref생성
	lua_pop(L, 1); // 스택에서 pop
#endif
	unlock_lua_instance(L);
	
	lua_rawgeti(Lt, LUA_REGISTRYINDEX, tmr->cbfxn_refid);
	lua_push_bot(Lt, bot);
	lua_pushstdstring(Lt, name);
	lua_rawgeti(Lt, LUA_REGISTRYINDEX, tmr->userdata_refid);
	int ret = lua_pcall(Lt, 3, 0, 0);
	if(ret != 0)
	{
		const char *errmsg = lua_tostring(Lt, -1);
		fprintf(stderr, "lua script error in timer callback: %s\n", errmsg);
		fflush(stderr);
		lua_pop(Lt, 1);
	}
#if 0
	lua_settop(Lt, 0); // lua_pop(Lt, 1); // remove this lua thread
#else
	lock_lua_instance(L);
	thread_ref.unref(); // gc에 의해 스레드 해제됨
	unlock_lua_instance(L);
#endif
	
	return 0;
}

static int cmd_cb_lua_admin1(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	int ret;
	
	if(!isrv->bot->is_botadmin(pmsg.getident()))
	{
		isrv->privmsg(mdest, "당신은 관리자가 아닙니다.");
		return 0;
	}
	
	if(cmd == "luaload")
	{
		ret = isrv->bot->pause_event_threads(true);
		if(ret < 0)
			return isrv->privmsg(mdest, "failed to lock event threads"), 0;
		
		std::string modname(normalize_luamod_name(carg));
		std::map<std::string, luamod>::iterator it;
		pthread_rwlock_wrlock(&g_luamods_mutex);
		it = luamods.find(modname);
		if(it != luamods.end())
			isrv->privmsgf(mdest, "cannot load %s: already loaded", carg.c_str());
		else
		{
			ret = load_lua_module(modname);
			if(ret < 0)
				isrv->privmsgf(mdest, "failed to load lua module %s: %s", carg.c_str(), g_lua_last_error.c_str());
			else
				isrv->privmsgf(mdest, "lua module %s loaded", carg.c_str());
		}
		pthread_rwlock_unlock(&g_luamods_mutex);
		isrv->bot->pause_event_threads(false);
	}
	else if(cmd == "luaunload")
	{
		ret = isrv->bot->pause_event_threads(true);
		if(ret < 0)
			return isrv->privmsg(mdest, "failed to lock event threads"), 0;
		
		std::string modname(normalize_luamod_name(carg));
		std::map<std::string, luamod>::iterator it;
		pthread_rwlock_wrlock(&g_luamods_mutex);
		it = luamods.find(modname);
		if(it == luamods.end())
			isrv->privmsgf(mdest, "cannot unload %s: not loaded", carg.c_str());
		else
		{
			luamods.erase(it);
			isrv->privmsgf(mdest, "lua module %s unloaded", carg.c_str());
		}
		pthread_rwlock_unlock(&g_luamods_mutex);
		isrv->bot->pause_event_threads(false);
	}
	else if(cmd == "luareload")
	{
		ret = isrv->bot->pause_event_threads(true);
		if(ret < 0)
			return isrv->privmsg(mdest, "failed to lock event threads"), 0;
		
		std::string modname(normalize_luamod_name(carg));
		std::map<std::string, luamod>::iterator it;
		pthread_rwlock_wrlock(&g_luamods_mutex);
		it = luamods.find(modname);
		if(it == luamods.end())
			isrv->privmsgf(mdest, "cannot unload %s: not loaded", carg.c_str());
		else
		{
			luamods.erase(it);
			ret = load_lua_module(modname);
			if(ret < 0)
				isrv->privmsgf(mdest, "failed to load lua module %s: ", carg.c_str(), g_lua_last_error.c_str());
			else
				isrv->privmsgf(mdest, "lua module %s reloaded", carg.c_str());
		}
		pthread_rwlock_unlock(&g_luamods_mutex);
		isrv->bot->pause_event_threads(false);
	}
	else if(cmd == "luamodlist")
	{
		std::map<std::string, luamod>::iterator it = luamods.begin();
		std::string buf;
		pthread_rwlock_rdlock(&g_luamods_mutex);
		for(int i = 0; it != luamods.end(); it++, i++)
		{
			buf += it->first + ", ";
			if((i + 1) % 10 == 0)
			{
				isrv->privmsg_nh(mdest, buf);
				buf.clear();
			}
		}
		pthread_rwlock_unlock(&g_luamods_mutex);
		if(!buf.empty())
			isrv->privmsg_nh(mdest, buf);
	}
	
	return 0;
}

#include "main.h"
int mod_lua_init(ircbot *bot)
{
	int ret;
	g_bot = bot;
	
	pthread_rwlock_init(&g_luamods_mutex, NULL);
	pthread_rwlock_init(&g_cmdmap_lock, NULL);
	pthread_rwlock_init(&g_eventhdlmap_lock, NULL);
	pthread_rwlock_init(&g_lua_mutexlist_lock, NULL);
	
	bot->register_cmd("luaload", cmd_cb_lua_admin1);
	bot->register_cmd("luaunload", cmd_cb_lua_admin1);
	bot->register_cmd("luareload", cmd_cb_lua_admin1);
	bot->register_cmd("luamodlist", cmd_cb_lua_admin1);
	bot->register_event_handler(irc_event_handler_lua);
	
	std::vector<std::string> autoload_list;
	std::vector<std::string>::const_iterator it;
	bot->cfg.getarray(autoload_list, "mod_lua.autoload_modules");
	
	pthread_rwlock_wrlock(&g_luamods_mutex);
	for(it = autoload_list.begin(); it != autoload_list.end(); it++)
	{
		if(it->empty())
			continue;
		ret = load_lua_module(*it);
		if(ret < 0)
		{
			fprintf(stderr, "failed to load lua module %s\n", it->c_str());
			fflush(stderr);
		}
	}
	pthread_rwlock_unlock(&g_luamods_mutex);
	
	return 0;
}

int mod_lua_cleanup(ircbot *bot)
{
	pthread_rwlock_wrlock(&g_luamods_mutex);
	luamods.clear();
	// g_cmdmap.clear(); g_eventhdlmap.clear(); // cleared in luamods dtor
	bot->unregister_event_handler(irc_event_handler_lua);
	bot->del_timer_by_cbptr(bottimer_callback_lua);
	bot->unregister_cmd("luaload");
	bot->unregister_cmd("luaunload");
	bot->unregister_cmd("luareload");
	bot->unregister_cmd("luamodlist");
	
	return 0;
}



MODULE_INFO(mod_lua, mod_lua_init, mod_lua_cleanup)





