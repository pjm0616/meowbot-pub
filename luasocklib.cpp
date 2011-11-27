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

#include "defs.h"
#include "luacpp.h"
#include "tcpsock.h"
#include "luasocklib.h"


#define DEFAULT_CONNECT_TIMEOUT 2000
#define DEFAULT_TIMEOUT 1000



static tcpsock *lua_tosock(lua_State *L, int index)
{
	tcpsock **data = (tcpsock **)lua_touserdata(L, index);
	if(!data)
	{
		luaL_typerror(L, index, "sock");
	}
	return *data;
}

static tcpsock *lua_checksock(lua_State *L, int index)
{
	tcpsock **data, *sock;
	luaL_checktype(L, index, LUA_TUSERDATA);
	data = (tcpsock **)luaL_checkudata(L, index, "sock");
	if(!data)
	{
		luaL_typerror(L, index, "sock");
	}
	sock = *data;
	if(!sock)
	{
		luaL_error(L, "Null sock object");
	}
	return sock;
}

static tcpsock **lua_push_sock(lua_State *L, tcpsock *sock)
{
	tcpsock **data = (tcpsock **)lua_newuserdata(L, sizeof(tcpsock *));
	*data = sock;
	luaL_getmetatable(L, "sock");
	lua_setmetatable(L, -2);
	return data;
}


static int lua_sock_new(lua_State *L)
{
	tcpsock *sock = new tcpsock();
	lua_push_sock(L, sock);
	
	return 1;
}

static int lua_sock_connect(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	const char *host = luaL_checkstring(L, 2);
	int port = luaL_checkint(L, 3);
	int timeout = luaL_optint(L, 4, DEFAULT_CONNECT_TIMEOUT);
	
	int ret = sock->connect(host, port, timeout);
	lua_pushinteger(L, ret);
	return 1;
}

static int lua_sock_close(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	
	int ret = sock->close();
	lua_pushinteger(L, ret);
	return 1;
}

static int lua_sock_read(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	int size = luaL_checkint(L, 2);
	int timeout = luaL_optint(L, 3, DEFAULT_TIMEOUT);
	if(size <= 0)
	{
		// FIXME
		return 0;
	}
	
	char *buf = new char[size];
	int nread = sock->timed_read(buf, size, timeout);
	if(nread > 0)
		lua_pushlstring(L, buf, nread);
	else if(nread == 0) // timeout
		lua_pushboolean(L, false);
	else // socket error
		lua_pushnil(L);
	delete[] buf;
	
	return 1;
}

static int lua_sock_readline(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	int timeout = luaL_optint(L, 2, DEFAULT_TIMEOUT);
	
	std::string buf;
	int nread = sock->readline(buf, timeout);
	if(nread >= 0)
		lua_pushlstring(L, buf.c_str(), buf.size());
	else if(nread == -1) // timeout
		lua_pushboolean(L, false);
	else // socket error
		lua_pushnil(L);
	
	return 1;
}

static int lua_sock_write(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	const char *data = luaL_checkstring(L, 2);
	size_t datalen = lua_objlen(L, 2);
	int timeout = luaL_optint(L, 3, DEFAULT_TIMEOUT);
	
	int nwrite = sock->timed_write(data, (int)datalen, timeout);
	if(nwrite > 0)
		lua_pushinteger(L, nwrite);
	else if(nwrite == 0) // timeout
		lua_pushboolean(L, false);
	else // socket error
		lua_pushnil(L);
	
	return 1;
}

static int lua_sock_send(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	const char *data = luaL_checkstring(L, 2);
	size_t datalen = lua_objlen(L, 2);
	int timeout = luaL_optint(L, 3, DEFAULT_TIMEOUT);
	
	int nwrite = sock->send(data, (int)datalen, timeout);
	if(nwrite > 0)
		lua_pushinteger(L, nwrite);
	else if(nwrite == 0) // timeout
		lua_pushboolean(L, false);
	else // socket error
		lua_pushnil(L);
	
	return 1;
}

static int lua_sock_append_to_read_buffer(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	const char *data = luaL_checkstring(L, 2);
	size_t len = lua_objlen(L, 2);
	
	size_t ret = sock->append_to_read_buffer(data, len);
	lua_pushinteger(L, ret);
	return 1;
}

static int lua_sock_clear_read_buffer(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	
	int ret = sock->clear_read_buffer();
	lua_pushinteger(L, ret);
	return 1;
}

static int lua_sock_get_status(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	int status = sock->get_status();
	if(status == tcpsock::SOCKSTATUS_DISCONNECTED)
		lua_pushstring(L, "disconnected");
	else
		lua_pushstring(L, "connected");
	
	return 1;
}

static int lua_sock_get_last_error_msg(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	lua_pushstdstring(L, sock->get_last_error_str());
	return 1;
}

static int lua_sock_set_encoding(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	const char *enc = luaL_checkstring(L, 2);
	
	int ret = sock->set_encoding(enc);
	lua_pushinteger(L, ret);
	return 1;
}

static int lua_sock_set_bind_address(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	const char *bind_addr = luaL_checkstring(L, 2);
	
	sock->set_bind_address(bind_addr);
	lua_pushboolean(L, true);
	return 1;
}

static int lua_sock_set_ndelay(lua_State *L)
{
	tcpsock *sock = lua_checksock(L, 1);
	bool onoff = luaL_checkint(L, 2);
	
	int ret = sock->set_ndelay(onoff);
	lua_pushinteger(L, ret);
	return 1;
}

static int lua_sock_gethostbyname_str(lua_State *L)
{
	const char *hostname = luaL_checkstring(L, 1);
	lua_pushstdstring(L, my_gethostbyname_s(hostname));
	
	return 1;
}



static const luaL_reg sock_methods[] = 
{
	{"new", lua_sock_new}, 
	
	{"connect", lua_sock_connect}, 
	{"close", lua_sock_close}, 
	
	{"read", lua_sock_read}, 
	{"readline", lua_sock_readline}, 
	{"write", lua_sock_write}, 
	{"send", lua_sock_send}, 
	
	{"append_to_read_buffer", lua_sock_append_to_read_buffer}, 
	{"clear_read_buffer", lua_sock_clear_read_buffer}, 
	
	{"get_status", lua_sock_get_status}, 
	{"get_last_error_msg", lua_sock_get_last_error_msg}, 
	
	{"set_encoding", lua_sock_set_encoding}, 
	{"set_ndelay", lua_sock_set_ndelay}, // should not be used
	{"set_bind_address", lua_sock_set_bind_address}, 
	
	// static functions
	{"gethostbyname_str", lua_sock_gethostbyname_str}, 
	
	{NULL, NULL}
};




static int lua_sock_gc(lua_State *L) // destroy
{
	tcpsock *sock = lua_tosock(L, 1);
	if(sock)
	{
		sock->close();
		delete sock;
	}
	
	return 0;
}

static const luaL_reg sock_meta_methods[] = 
{
	{"__gc", lua_sock_gc}, 
	{NULL, NULL}
};



int luaopen_libluasock(lua_State *L)
{
	int methods, metatable;
	
	luaL_register(L, "sock", sock_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "sock"); metatable = lua_gettop(L); // create metatable for Image, add it to the Lua registry
	luaL_register(L, 0, sock_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // hide metatable: metatable.__metatable = methods
	
	lua_pop(L, 1); // drop metatable
	
	return 1;
}






