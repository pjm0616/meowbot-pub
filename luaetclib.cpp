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
#include <assert.h>
#include <cmath>
#include <pcrecpp.h>

#include "defs.h"
#include "luacpp.h"
#include "etc.h"
#include "wildcard.h"
#include "base64.h"
#include "md5.h"
#include "my_vsprintf.h"
#include "luaetclib.h"



static int lua_strip_irc_colors(lua_State *L)
{
	const char *msg = luaL_checkstring(L, 1);
	lua_pushstdstring(L, strip_irc_colors(msg));
	return 1;
}

static int lua_prevent_irc_highlighting(lua_State *L)
{
	const char *nick = luaL_checkstring(L, 1);
	const char *msg = luaL_checkstring(L, 2);
	lua_pushstdstring(L, prevent_irc_highlighting(nick, msg));
	return 1;
}

static int lua_wc_match(lua_State *L)
{
	const char *mask = luaL_checkstring(L, 1);
	const char *text = luaL_checkstring(L, 2);
	lua_pushboolean(L, wc_match(mask, text));
	return 1;
}

static int lua_wc_match_nocase(lua_State *L)
{
	const char *mask = luaL_checkstring(L, 1);
	const char *text = luaL_checkstring(L, 2);
	lua_pushboolean(L, wc_match_nocase(mask, text));
	return 1;
}

static int lua_wc_match_irccase(lua_State *L)
{
	const char *mask = luaL_checkstring(L, 1);
	const char *text = luaL_checkstring(L, 2);
	lua_pushboolean(L, wc_match_irccase(mask, text));
	return 1;
}

static int lua_urlencode(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, urlencode(str));
	return 1;
}

static int lua_urldecode(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, urldecode(str));
	return 1;
}

static int lua_decode_html_entities(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, decode_html_entities(str));
	return 1;
}

static int lua_strip_spaces(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, strip_spaces(str));
	return 1;
}

static int lua_atoi(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	int base = luaL_optinteger(L, 2, 0);
	if(base <= 0)
		lua_pushinteger(L, my_atoi(str));
	else
		lua_pushinteger(L, my_atoi_base(str, base));
	return 1;
}

static int lua_base64_encode(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, base64_encode(str));
	return 1;
}

static int lua_base64_decode(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, base64_decode(str));
	return 1;
}

static int lua_md5_hex(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	lua_pushstdstring(L, md5_hex(str));
	return 1;
}

static int lua_md5_int32(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	int res[4];
	md5(str, lua_objlen(L, 1), res);
	lua_pushinteger(L, res[0]);
	lua_pushinteger(L, res[1]);
	lua_pushinteger(L, res[2]);
	lua_pushinteger(L, res[3]);
	return 1;
}

static int lua_sprintf(lua_State *L)
{
	const char *format = luaL_checkstring(L, 1);
	char buf[1024];
	int len;
	
	len = my_vsnprintf_lua(buf, sizeof(buf), format, L, 2);
	assert(len >= 0);
	if((size_t)len >= sizeof(buf))
	{
		char *buf2 = new char[len + 1];
		len = my_vsnprintf_lua(buf2, len + 1, format, L, 2);
		lua_pushlstring(L, buf2, len);
		delete[] buf2;
	}
	else
		lua_pushlstring(L, buf, len);
	
	return 1;
}

static int lua_math_my_rand(lua_State *L)
{
	lua_Number r = (lua_Number)(my_rand()%RAND_MAX) / (lua_Number)RAND_MAX;
	switch (lua_gettop(L)) /* check number of arguments */
	{
		case 0: /* no arguments */
			lua_pushnumber(L, r); /* Number between 0 and 1 */
			break;
		case 1: /* only upper limit */
		{
			int u = luaL_checkint(L, 1);
			luaL_argcheck(L, 1<=u, 1, "interval is empty");
			lua_pushnumber(L, floor(r*u)+1);  /* int between 1 and `u' */
			break;
		}
		case 2: /* lower and upper limits */
		{
			int l = luaL_checkint(L, 1);
			int u = luaL_checkint(L, 2);
			luaL_argcheck(L, l<=u, 2, "interval is empty");
			lua_pushnumber(L, floor(r*(u-l+1))+l);  /* int between `l' and `u' */
			break;
		}
		default:
			return luaL_error(L, "wrong number of arguments");
	}
	return 1;
}

static int lua_math_my_randomseed (lua_State *L)
{
	my_srand(luaL_checkint(L, 1));
	return 0;
}

#if 0
static int lua_curl_get_url(lua_State *L)
{
	const char *url = luaL_checkstring(L, 1);
	lua_pushstdstring(L, curl_get_url(url));
	return 1;
}
#endif



int luaopen_libluaetc(lua_State *L)
{
	lua_register(L, "strip_irc_colors", lua_strip_irc_colors);
	lua_register(L, "prevent_irc_highlighting", lua_prevent_irc_highlighting);
	lua_register(L, "wc_match", lua_wc_match);
	lua_register(L, "wc_match_nocase", lua_wc_match_nocase);
	lua_register(L, "wc_match_irccase", lua_wc_match_irccase);
	lua_register(L, "urlencode", lua_urlencode);
	lua_register(L, "urldecode", lua_urldecode);
	lua_register(L, "decode_html_entities", lua_decode_html_entities);
	lua_register(L, "strip_spaces", lua_strip_spaces);
	lua_register(L, "atoi", lua_atoi);
	lua_register(L, "base64_encode", lua_base64_encode);
	lua_register(L, "base64_decode", lua_base64_decode);
	lua_register(L, "md5_hex", lua_md5_hex);
	lua_register(L, "md5_int32", lua_md5_int32);
	lua_register(L, "sprintf", lua_sprintf);
	
	lua_getfield(L, LUA_GLOBALSINDEX, "math");
	int mathtable = lua_gettop(L);
	lua_pushcfunction(L, lua_math_my_rand);
	lua_setfield(L, mathtable, "random");
	lua_pushcfunction(L, lua_math_my_randomseed);
	lua_setfield(L, mathtable, "randomseed");
	
#if 0
	lua_register(L, "curl_get_url", lua_curl_get_url);
#endif
	
	return 1;
}






