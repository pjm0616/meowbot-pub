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
#include "utf8.h"
#include "hangul.h"
#include "luahangullib.h"



static int lua_hangul_join(lua_State *L)
{
	int cho = luaL_checkint(L, 1);
	int jung = luaL_checkint(L, 2);
	int jong = luaL_optint(L, 3, 0);
	
	int code = hangul_join(cho, jung, jong);
	lua_pushinteger(L, code);
	return 1;
}

static int lua_hangul_join_utf8char(lua_State *L)
{
	const char *cho = luaL_checkstring(L, 1);
	const char *jung = luaL_checkstring(L, 2);
	const char *jong = luaL_optstring(L, 3, NULL);
	
	int code = hangul_join_utf8char(cho, jung, jong);
	char buf[8];
	int len = utf8_encode_nt(code, buf, sizeof(buf));
	if(len <= 0)
		lua_pushliteral(L, "");
	else
		lua_pushlstring(L, buf, len);
	
	return 1;
}

static int lua_hangul_split(lua_State *L)
{
	int code = luaL_checkint(L, 1);
	int cho, jung, jong;
	
	int ret = hangul_split(code, (unsigned int *)&cho, 
		(unsigned int *)&jung, (unsigned int *)&jong);
	if(ret < 0)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	else
	{
		lua_pushinteger(L, cho);
		lua_pushinteger(L, jung);
		lua_pushinteger(L, jong);
		return 3;
	}
}

static int lua_hangul_split_utf8char(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	char cho[8], jung[8], jong[8];
	
	int ret = hangul_split_utf8char(str, cho, jung, jong);
	if(ret < 0)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	else
	{
		lua_pushstring(L, cho);
		lua_pushstring(L, jung);
		lua_pushstring(L, jong);
		return 3;
	}
}

static int lua_hangul_merge_consonant(lua_State *L)
{
	int cons1 = luaL_checkint(L, 1);
	int cons2 = luaL_checkint(L, 2);
	lua_pushinteger(L, 
		hangul_merge_consonant((unsigned int)cons1, (unsigned int)cons2));
	
	return 1;
}

static int lua_hangul_merge_consonant_utf8char(lua_State *L)
{
	const char *cons1 = luaL_checkstring(L, 1);
	const char *cons2 = luaL_checkstring(L, 2);
	lua_pushstring(L, 
		hangul_merge_consonant_utf8char(cons1, cons2));
	
	return 1;
}

static int lua_hangul_merge_vowel(lua_State *L)
{
	int vowel1 = luaL_checkint(L, 1);
	int vowel2 = luaL_checkint(L, 2);
	lua_pushinteger(L, 
		hangul_merge_vowel((unsigned int)vowel1, (unsigned int)vowel2));
	
	return 1;
}

static int lua_hangul_merge_vowel_utf8char(lua_State *L)
{
	const char *vowel1 = luaL_checkstring(L, 1);
	const char *vowel2 = luaL_checkstring(L, 2);
	lua_pushstring(L, hangul_merge_vowel_utf8char(vowel1, vowel2));
	
	return 1;
}

static int lua_hangul_select_josa(lua_State *L)
{
	const char *ctx = luaL_checkstring(L, 1);
	const char *josa = luaL_checkstring(L, 2);
	lua_pushstdstring(L, select_josa(ctx, josa));
	
	return 1;
}



#define DEFINE_LUA_HANGUL_TYPE_FXN(type) \
	static int lua_hangul_is_##type(lua_State *L) \
	{ \
		const char *str = luaL_checkstring(L, 1); \
		lua_pushboolean(L, hangul_is_##type(utf8_decode(str, NULL))); \
		return 1; \
	}
DEFINE_LUA_HANGUL_TYPE_FXN(consonant)
DEFINE_LUA_HANGUL_TYPE_FXN(vowel)
DEFINE_LUA_HANGUL_TYPE_FXN(oldconsonant)
DEFINE_LUA_HANGUL_TYPE_FXN(oldvowel)
DEFINE_LUA_HANGUL_TYPE_FXN(jamo)
DEFINE_LUA_HANGUL_TYPE_FXN(oldjamo)
DEFINE_LUA_HANGUL_TYPE_FXN(jamo_all)
DEFINE_LUA_HANGUL_TYPE_FXN(choseong_jamo)
DEFINE_LUA_HANGUL_TYPE_FXN(jongseong_jamo)

DEFINE_LUA_HANGUL_TYPE_FXN(choseong)
DEFINE_LUA_HANGUL_TYPE_FXN(jungseong)
DEFINE_LUA_HANGUL_TYPE_FXN(jongseong)
DEFINE_LUA_HANGUL_TYPE_FXN(choseong_conjoinable)
DEFINE_LUA_HANGUL_TYPE_FXN(jungseong_conjoinable)
DEFINE_LUA_HANGUL_TYPE_FXN(jongseong_conjoinable)
DEFINE_LUA_HANGUL_TYPE_FXN(jaso)

DEFINE_LUA_HANGUL_TYPE_FXN(syllable)
#undef DEFINE_LUA_HANGUL_TYPE_FXN



static const luaL_reg luahangullib_methods[] = 
{
	{"join", lua_hangul_join}, 
	{"join_utf8char", lua_hangul_join_utf8char}, 
	{"split", lua_hangul_split}, 
	{"split_utf8char", lua_hangul_split_utf8char}, 
	{"merge_consonant", lua_hangul_merge_consonant}, 
	{"merge_consonant_utf8char", lua_hangul_merge_consonant_utf8char}, 
	{"merge_vowel", lua_hangul_merge_vowel}, 
	{"merge_vowel_utf8char", lua_hangul_merge_vowel_utf8char}, 
	
	{"select_josa", lua_hangul_select_josa}, 
	
	{"is_consonant", lua_hangul_is_consonant}, 
	{"is_vowel", lua_hangul_is_vowel}, 
	{"is_oldconsonant", lua_hangul_is_oldconsonant}, 
	{"is_oldvowel", lua_hangul_is_oldvowel}, 
	{"is_jamo", lua_hangul_is_jamo}, 
	{"is_oldjamo", lua_hangul_is_oldjamo}, 
	{"is_jamo_all", lua_hangul_is_jamo_all}, 
	{"choseong_jamo", lua_hangul_is_choseong_jamo},
	{"jongseong_jamo", lua_hangul_is_jongseong_jamo},
	
	{"is_choseong", lua_hangul_is_choseong}, 
	{"is_jungseong", lua_hangul_is_jungseong}, 
	{"is_jongseong", lua_hangul_is_jongseong}, 
	{"is_choseong_conjoinable", lua_hangul_is_choseong_conjoinable}, 
	{"is_jungseong_conjoinable", lua_hangul_is_jungseong_conjoinable}, 
	{"is_jongseong_conjoinable", lua_hangul_is_jongseong_conjoinable}, 
	{"is_jaso", lua_hangul_is_jaso}, 
	
	{"is_syllable", lua_hangul_is_syllable}, 
	
	{NULL, NULL}
};

int luaopen_libluahangul(lua_State *L)
{
	luaL_register(L, "hangul", luahangullib_methods);
	
	return 1;
}






