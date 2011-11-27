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
#include "utf8_impl.h"
#include "encoding_converter.h"
#include "luautf8lib.h"



// indices start at 1 (not 0)



static int lua_utf8_encode(lua_State *L)
{
	int code = luaL_checkint(L, 1);
	char buf[8];
	
	int len = utf8_encode(code, buf, sizeof(buf));
	if(len <= 0)
	{
		lua_pushliteral(L, "");
	}
	else
	{
		lua_pushlstring(L, buf, len);
	}
	
	return 1;
}

static int lua_utf8_decode(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	int code = utf8_decode(str, NULL);
	lua_pushinteger(L, code);
	
	return 1;
}

static int lua_utf8_strlen(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	int len = utf8_strlen(str);
	lua_pushinteger(L, len);
	
	return 1;
}

// startpos
static int lua_utf8_strpos(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	int start = luaL_checkint(L, 2);
	int startpos;
	size_t len;
	
	if(start > 0)
		start--;
	int ret = utf8_strpos_getpos(str, start, &startpos, &len);
	if(ret < 0)
	{
		lua_pushliteral(L, "");
		return 1;
	}
	
	lua_pushlstring(L, str + startpos, len);
	return 1;
}

// startpos, length
static int lua_utf8_substr(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	int start = luaL_checkint(L, 2);
	int nchars = luaL_optint(L, 3, -1);
	int startpos;
	size_t len;
	
	if(start > 0)
		start--;
	int ret = utf8_substr_getpos(str, start, nchars, &startpos, &len);
	if(ret < 0)
	{
		lua_pushliteral(L, "");
		return 1;
	}
	
	lua_pushlstring(L, str + startpos, len);
	return 1;
}

// lua-style substr (startpos, endpos)
static int lua_utf8_sub(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	int start = luaL_checkint(L, 2);
	int end = luaL_optint(L, 3, -1);
	int startpos, endpos;
	size_t len;
	
	if(start > 0)
		start--;
	if(end > 0)
		end--;
	int ret = utf8_strpos_getpos(str, start, &startpos, NULL);
	int ret2 = utf8_strpos_getpos(str, end, &endpos, &len);
	if(ret < 0 || ret2 < 0)
	{
		lua_pushliteral(L, "");
		return 1;
	}
	len = endpos + len - startpos;
	
	lua_pushlstring(L, str + startpos, len);
	return 1;
}

static int lua_utf8_cut_by_size(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	size_t len = lua_objlen(L, 1);
	int maxsize = luaL_checkint(L, 2);
	
	int newlen = utf8_cut_by_size(str, len, maxsize);
	lua_pushinteger(L, newlen);
	
	return 1;
}

static int lua_str_iconv(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	size_t len = lua_objlen(L, 1);
	const char *from = luaL_checkstring(L, 2);
	const char *to = luaL_checkstring(L, 3);
	
	encconv cd;
	int ret = cd.open(from, to, encconv::ICONV_TYPE_TRANSLIT);
	if(ret < 0)
		luaL_error(L, "unsupported encoding");
	
	size_t cvlen;
	char *converted = cd.convert(str, len, &cvlen);
	if(!converted)
		lua_pushliteral(L, "");
	else
		lua_pushlstring(L, converted, cvlen);
	delete[] converted;
	
	return 1;
}

static int lua_str_encode(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	size_t len = lua_objlen(L, 1);
	const char *to = luaL_checkstring(L, 2);
	
	encconv cd;
	int ret = cd.open("utf-8", to, encconv::ICONV_TYPE_TRANSLIT);
	if(ret < 0)
		luaL_error(L, "unsupported encoding");
	
	size_t cvlen;
	char *converted = cd.convert(str, len, &cvlen);
	if(!converted)
		lua_pushliteral(L, "");
	else
		lua_pushlstring(L, converted, cvlen);
	delete[] converted;
	
	return 1;
}

static int lua_str_decode(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	size_t len = lua_objlen(L, 1);
	const char *from = luaL_checkstring(L, 2);
	
	encconv cd;
	int ret = cd.open(from, "utf-8", encconv::ICONV_TYPE_TRANSLIT);
	if(ret < 0)
		luaL_error(L, "unsupported encoding");
	
	size_t cvlen;
	char *converted = cd.convert(str, len, &cvlen);
	if(!converted)
		lua_pushliteral(L, "");
	else
		lua_pushlstring(L, converted, cvlen);
	delete[] converted;
	
	return 1;
}



int luaopen_libluautf8(lua_State *L)
{
	// string functions
	lua_getfield(L, LUA_GLOBALSINDEX, "string");
	int strtable = lua_gettop(L);
	
	lua_pushcfunction(L, lua_utf8_encode);
	lua_setfield(L, strtable, "utf8_encode");
	lua_pushcfunction(L, lua_utf8_decode);
	lua_setfield(L, strtable, "utf8_decode");
	lua_pushcfunction(L, lua_utf8_encode);
	lua_setfield(L, strtable, "encode_utf8");
	lua_pushcfunction(L, lua_utf8_decode);
	lua_setfield(L, strtable, "decode_utf8");
	
	lua_pushcfunction(L, lua_utf8_strlen);
	lua_setfield(L, strtable, "utf8_strlen");
	lua_pushcfunction(L, lua_utf8_strpos);
	lua_setfield(L, strtable, "utf8_strpos");
	lua_pushcfunction(L, lua_utf8_substr);
	lua_setfield(L, strtable, "utf8_substr");
	lua_pushcfunction(L, lua_utf8_sub);
	lua_setfield(L, strtable, "utf8_sub");
	lua_pushcfunction(L, lua_utf8_cut_by_size);
	lua_setfield(L, strtable, "utf8_cut_by_size");
	
	lua_pushcfunction(L, lua_str_encode);
	lua_setfield(L, strtable, "encode");
	lua_pushcfunction(L, lua_str_decode);
	lua_setfield(L, strtable, "decode");
	lua_pushcfunction(L, lua_str_iconv);
	lua_setfield(L, strtable, "iconv");
	
	// global functions
	lua_register(L, "utf8_encode", lua_utf8_encode);
	lua_register(L, "utf8_decode", lua_utf8_decode);
	
	return 1;
}






