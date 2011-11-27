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


#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <assert.h>

#if 0 // included in luacpp.h
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#endif

#include <pcrecpp.h>

#include "defs.h"
#include "etc.h"
#include "luacpp.h"


extern int do_backtrace();
#define CHECK_LUA_STATE_PTR \
	do \
	{ \
		if(unlikely(this->lua == NULL)) \
		{ \
			if(0)this->init(); \
			puts("this->lua is NULL. call luacpp::init() first."); \
			/*do_backtrace();*/ \
			assert(this->lua != NULL); \
		} \
	} while(0)

int luacpp::init()
{
	if(this->lua)
		this->close();
	
	this->lua = luaL_newstate();
	if(!this->lua)
		return -1;
	
#if 1
	luaL_openlibs(this->lua);
	luaopen_libluapcre(this->lua);
	luaopen_libluautf8(this->lua);
	luaopen_libluahangul(this->lua);
#endif
#if 0
	luaopen_libluaetc(this->lua);
	luaopen_libluasock(this->lua);
	luaopen_libluathread(this->lua);
	luaopen_libluasqlite3(this->lua);
#endif
	
	return 0;
}

int luacpp::close()
{
	if(this->lua)
	{
		lua_close(this->lua);
		this->lua = NULL;
		return 0;
	}
	return -1;
}

int luacpp::pcall(int nargs, int nresults)
{
	int ret;
	CHECK_LUA_STATE_PTR;
	
	ret = lua_pcall(this->lua, nargs, nresults, 0);
	if(ret != 0)
	{
		const char *errmsg = lua_tostring(this->lua, -1);
		this->lasterr_str = errmsg;
		lua_pop(this->lua, 1);
		return ret;
	}
	
	return 0;
}

std::string luacpp::popstring()
{
	CHECK_LUA_STATE_PTR;
	const char *buf = lua_tostring(this->lua, -1);
	std::string str(buf);
	lua_pop(this->lua, 1);
	return str;
}

int luacpp::dofile(const char *file)
{
	int ret;
	CHECK_LUA_STATE_PTR;
	
	ret = luaL_loadfile(this->lua, file);
	if(ret)
	{
		this->lasterr_str = this->popstring();
		return ret;
	}
	
	ret = this->pcall(0, 0);
	return ret;
}

int luacpp::dostring(const char *code)
{
	int ret;
	CHECK_LUA_STATE_PTR;
	
	ret = luaL_loadstring(this->lua, code);
	if(ret)
	{
		this->lasterr_str = this->popstring();
		return ret;
	}
	
	ret = this->pcall(0, 0);
	return ret;
}

int luacpp::vpcallarg(int nresults, const char *argfmt, va_list &ap)
{
	CHECK_LUA_STATE_PTR;
	int nargs = 0, len;
	const char *p, *next;
	p = argfmt;
	do
	{
		next = strchr(p, ',');
		if(next)
			len = next - p;
		else
			len = strlen(p);
		if(!len)
		{
			lua_pop(this->lua, nargs);
			this->lasterr_str = "invalid argument format string: `";
			this->lasterr_str.append(p, len);
			this->lasterr_str += "'";
			return -1;
		}
		
		if(!strncmp(p, "integer", len))
			lua_pushinteger(this->lua, va_arg(ap, lua_Integer));
		else if(!strncmp(p, "string", len))
			lua_pushstring(this->lua, va_arg(ap, const char *));
		else if(!strncmp(p, "null", len))
			va_arg(ap, void *), lua_pushnil(this->lua);
		else if(!strncmp(p, "boolean", len))
			lua_pushboolean(this->lua, va_arg(ap, int));
		else if(!strncmp(p, "number", len))
			lua_pushnumber(this->lua, va_arg(ap, lua_Number));
		else if(!strncmp(p, "lightuserdata", len))
			lua_pushlightuserdata(this->lua, va_arg(ap, void *));
		else
		{
			lua_pop(this->lua, nargs);
			this->lasterr_str = "invalid argument format string: `";
			this->lasterr_str.append(p, len);
			this->lasterr_str += "'";
			return -1;
		}
		nargs++;
		p = next + 1;
	} while(next);
	
	int ret = this->pcall(nargs, nresults);
	return ret;
}

int luacpp::pcallarg(int nresults, const char *argfmt, ...)
{
	CHECK_LUA_STATE_PTR;
	va_list ap;
	va_start(ap, argfmt);
	int ret = this->vpcallarg(nresults, argfmt, ap);
	va_end(ap);
	return ret;
}

int luacpp::callfxn(const char *name, int nresults, const char *argfmt, ...)
{
	CHECK_LUA_STATE_PTR;
	lua_getfield(this->lua, LUA_GLOBALSINDEX, name); // get function
	va_list ap;
	va_start(ap, argfmt);
	int ret = this->vpcallarg(nresults, argfmt, ap);
	va_end(ap);
	return ret;
}



////////// lua reference class

int luaref::ref(lua_State *L, int stack_n)
{
	this->L = L;
	if(this->ref_id != LUA_NOREF)
		this->unref();
	
	lua_pushvalue(this->L, stack_n);
	this->ref_id = luaL_ref(this->L, LUA_REGISTRYINDEX);
	return this->ref_id;
}

void luaref::unref()
{
	luaL_unref(this->L, LUA_REGISTRYINDEX, this->ref_id);
	this->ref_id = LUA_NOREF;
}

void luaref::pushref(lua_State *L) const
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, this->ref_id);
}
void luaref::pushref() const
{
	lua_rawgeti(this->L, LUA_REGISTRYINDEX, this->ref_id);
}

void luaref::clearref()
{
	this->ref_id = LUA_NOREF;
}

bool luaref::operator==(const luaref &o) const
{
	if((this->L == o.L) && (this->ref_id == o.ref_id))
		return true;
	else
		return false;
}



////////////////////////////////////// lua helper funcs

bool lua_my_tostdstring(lua_State *L, int index, std::string *dest)
{
	int type = lua_type(L, index);
	switch(type)
	{
	case LUA_TNIL:
	{
		return false;
		//*dest = "nil";
		//return true;
	}
	case LUA_TNUMBER:
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", lua_tointeger(L, index));
		*dest = buf;
		return true;
	}
	case LUA_TBOOLEAN:
	{
		bool ret = lua_toboolean(L, index);
		*dest = ret?"true":"false";
		return true;
	}
	case LUA_TSTRING:
	{
		const char *data;
		size_t len;
		data = lua_tolstring(L, index, &len);
		assert(data != NULL);
		dest->assign(data, len);
		return true;
	}
	
	//case LUA_TTABLE:
	//case LUA_TFUNCTION:
	//case LUA_TTUSERDATA:
	//case LUA_TTHREAD
	//case LUA_TLIGHTUSERDATA:
	default:
		return false;
	}
}

std::string lua_my_tostdstring(lua_State *L, int index)
{
	std::string res;
	bool ret = lua_my_tostdstring(L, index, &res);
	if(!ret)
		res.clear();
	return res;
}

bool lua_my_toint(lua_State *L, int index, int *dest)
{
	int type = lua_type(L, index);
	switch(type)
	{
	case LUA_TNIL:
	{
		return false;
		//*dest = 0;
		//return true;
	}
	case LUA_TNUMBER:
	{
		*dest = lua_tointeger(L, index);
		return true;
	}
	case LUA_TBOOLEAN:
	{
		*dest = lua_toboolean(L, index);
		return true;
	}
	case LUA_TSTRING:
	{
		const char *data = lua_tostring(L, index);
		assert(data != NULL);
		*dest = my_atoi(data);
		return true;
	}
	
	//case LUA_TTABLE:
	//case LUA_TFUNCTION:
	//case LUA_TTUSERDATA:
	//case LUA_TTHREAD
	//case LUA_TLIGHTUSERDATA:
	default:
		return false;
	}
}

int lua_my_toint(lua_State *L, int index)
{
	int res;
	bool ret = lua_my_toint(L, index, &res);
	if(!ret)
		res = 0;
	return res;
}














// vim: set tabstop=4 textwidth=80:

