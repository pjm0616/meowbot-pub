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

#include <pcrecpp.h>

#include "defs.h"
#include "etc.h" // pcre_split
#include "luacpp.h"
#include "luapcrelib.h"



static pcrecpp::RE *lua_pcre_topcre(lua_State *L, int index)
{
	pcrecpp::RE **data = (pcrecpp::RE **)lua_touserdata(L, index);
	if(!data)
	{
		luaL_typerror(L, index, "pcre");
	}
	return *data;
}

static pcrecpp::RE *lua_pcre_checkpcre(lua_State *L, int index)
{
	pcrecpp::RE **data, *re;
	luaL_checktype(L, index, LUA_TUSERDATA);
	data = (pcrecpp::RE **)luaL_checkudata(L, index, "pcre");
	if(!data)
	{
		luaL_typerror(L, index, "pcre");
	}
	re = *data;
	if(!re)
	{
		luaL_error(L, "Null pcre object");
	}
	return re;
}

static pcrecpp::RE **lua_pcre_push_re(lua_State *L, pcrecpp::RE *re)
{
	pcrecpp::RE **data = (pcrecpp::RE **)lua_newuserdata(L, sizeof(pcrecpp::RE *));
	*data = re;
	luaL_getmetatable(L, "pcre");
	lua_setmetatable(L, -2);
	return data;
}

static pcrecpp::RE *lua_pcre_check_pcre_or_string(lua_State *L, int index, bool *do_free)
{
	if(lua_type(L, index) == LUA_TSTRING)
	{
		const char *regexp = luaL_checkstring(L, index);
		if(!regexp)
		{
			luaL_error(L, "Null regexp string");
		}
		*do_free = true;
		return new pcrecpp::RE(regexp);
	}
	else
	{
		*do_free = false;
		return lua_pcre_checkpcre(L, index);
	}
}

/////////////////////////////////////////////

static int lua_pcre_new(lua_State *L)
{
	std::string pattern = luaL_checkstring(L, 1);
	const char *opts = luaL_optstring(L, 2, "");
	pcrecpp::RE_Options re_opts;
	bool opt_full_match = false;
	
	for(; *opts; opts++)
	{
		switch(*opts)
		{
		case 'i': re_opts.set_caseless(true); break;
		case 'm': re_opts.set_multiline(true); break;
		case 's': re_opts.set_dotall(true); break;
		case 'x': re_opts.set_extended(true); break;
		case 'u': re_opts.set_utf8(true); break;
		case 'f': opt_full_match = true; break;
		default: luaL_error(L, "invalid pcre option character");
		}
	}
	if(opt_full_match)
	{
		pattern = "\\A" "(?:" + pattern + ")\\z";
	}
	
	pcrecpp::RE *re = new pcrecpp::RE(pattern, re_opts);
	lua_pcre_push_re(L, re);
	
	return 1;
}

static int lua_pcre_quotemeta(lua_State *L)
{
	const char *text = luaL_checkstring(L, 1);
	lua_pushstdstring(L, pcrecpp::RE::QuoteMeta(text));
	
	return 1;
}

/////////////////////////////////////////////

static int lua_pcre_match(lua_State *L)
{
	//pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	bool do_free_re;
	pcrecpp::RE *re = lua_pcre_check_pcre_or_string(L, 1, &do_free_re);
	const char *text = luaL_checkstring(L, 2);
	
	std::vector<std::string> groups;
	if(re->PartialMatchAll(text, &groups))
	{
		lua_createtable(L, groups.size(), 0);
		std::vector<std::string>::const_iterator it = groups.begin();
		for(int n = 1; it != groups.end(); it++, n++)
		{
			lua_pushstdstring(L, *it);
			lua_rawseti(L, -2, n);
		}
	}
	else
	{
		lua_pushboolean(L, false);
	}
	
	if(do_free_re)
		delete re;
	return 1;
}

static int lua_pcre_full_match(lua_State *L)
{
	//pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	bool do_free_re;
	pcrecpp::RE *re = lua_pcre_check_pcre_or_string(L, 1, &do_free_re);
	const char *text = luaL_checkstring(L, 2);
	
	std::vector<std::string> groups;
	if(re->FullMatchAll(text, &groups))
	{
		lua_createtable(L, groups.size(), 0);
		std::vector<std::string>::const_iterator it = groups.begin();
		for(int n = 1; it != groups.end(); it++, n++)
		{
			lua_pushstdstring(L, *it);
			lua_rawseti(L, -2, n);
		}
	}
	else
	{
		lua_pushboolean(L, false);
	}
	
	if(do_free_re)
		delete re;
	return 1;
}

static int lua_pcre_match_all(lua_State *L)
{
	//pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	bool do_free_re;
	pcrecpp::RE *re = lua_pcre_check_pcre_or_string(L, 1, &do_free_re);
	const char *text = luaL_checkstring(L, 2);
	
	std::string data(text);
	pcrecpp::StringPiece input(data);
	std::vector<std::string> groups;
	
	lua_newtable(L);
	int result_array = lua_gettop(L);
	int n = 1;
	while(re->FindAndConsumeAll(&input, &groups))
	{
		std::vector<std::string>::const_iterator it = groups.begin();
		lua_createtable(L, groups.size(), 0);
		for(int n2 = 1; it != groups.end(); it++, n2++)
		{
			lua_pushstdstring(L, *it);
			lua_rawseti(L, -2, n2);
		}
		lua_rawseti(L, result_array, n);
		n++;
	}
	
	if(do_free_re)
		delete re;
	return 1;
}

static int lua_pcre_replace(lua_State *L)
{
	//pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	bool do_free_re;
	pcrecpp::RE *re = lua_pcre_check_pcre_or_string(L, 1, &do_free_re);
	const char *rewrite = luaL_checkstring(L, 2);
	const char *text = luaL_checkstring(L, 3);
	
	std::string buf(text);
	if(re->GlobalReplace(rewrite, &buf))
		lua_pushstdstring(L, buf);
	else
		lua_pushstring(L, text);
	
	if(do_free_re)
		delete re;
	return 1;
}

static bool lua_pcre_replace_cb_fxn(std::string &dest, const std::vector<std::string> &groups, void *userdata)
{
	lua_State *L = (lua_State *)userdata;
	
	lua_pushvalue(L, 3); // lua callback function
	lua_createtable(L, groups.size(), 0);
	std::vector<std::string>::const_iterator it = groups.begin();
	//for(int n = 1; it != groups.end(); it++, n++)
	for(int n = 0; it != groups.end(); it++, n++)
	{
		lua_pushstdstring(L, *it);
		lua_rawseti(L, -2, n);
	}
	int ret = lua_pcall(L, 1, 1, 0);
	if(ret != 0)
	{
		std::string errmsg(lua_tostring(L, -1)); // make a copy of the message
		lua_pop(L, 1);
		luaL_error(L, errmsg.c_str());
		return false; // never reached
	}
	else
	{
		const char *str = luaL_checkstring(L, -1);
		dest += str;
	}
	
	return true;
}
static int lua_pcre_replace_cb(lua_State *L)
{
	//pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	bool do_free_re;
	pcrecpp::RE *re = lua_pcre_check_pcre_or_string(L, 1, &do_free_re);
	const char *text = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);
	
	std::string buf(text);
	if(re->GlobalReplaceCallback(&buf, lua_pcre_replace_cb_fxn, L))
		lua_pushstdstring(L, buf);
	else
		lua_pushstring(L, text);
	
	if(do_free_re)
		delete re;
	return 1;
}

static int lua_pcre_replace_one(lua_State *L)
{
	//pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	bool do_free_re;
	pcrecpp::RE *re = lua_pcre_check_pcre_or_string(L, 1, &do_free_re);
	const char *rewrite = luaL_checkstring(L, 2);
	const char *text = luaL_checkstring(L, 3);
	
	std::string buf(text);
	if(re->Replace(rewrite, &buf))
		lua_pushstdstring(L, buf);
	else
		lua_pushstring(L, text);
	
	if(do_free_re)
		delete re;
	return 1;
}

// pattern이 "(.*?)(분리할 패턴)" 같은 형식일 경우에만 사용 가능
static int lua_pcre_csplit(lua_State *L)
{
	//pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	bool do_free_re;
	pcrecpp::RE *re = lua_pcre_check_pcre_or_string(L, 1, &do_free_re);
	const char *text = luaL_checkstring(L, 2);
	std::vector<std::string> arr;
	
	if(re->NumberOfCapturingGroups() != 2)
		luaL_error(L, "invalid regexp for csplit");
	
	pcre_csplit(*re, text, arr);
	
	lua_createtable(L, arr.size(), 0);
	int result_tbl = lua_gettop(L), i;
	std::vector<std::string>::const_iterator it;
	for(it = arr.begin(), i = 1; 
		it != arr.end(); 
		it++, i++)
	{
		lua_pushstdstring(L, *it);
		lua_rawseti(L, result_tbl, i);
	}
	
	if(do_free_re)
		delete re;
	return 1;
}


/////////////////////////////////////////////

static int lua_pcre_getpattern(lua_State *L)
{
	pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	lua_pushstdstring(L, re->pattern());
	
	return 1;
}

static int lua_pcre_geterror(lua_State *L)
{
	pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	lua_pushstdstring(L, re->error());
	
	return 1;
}

static int lua_pcre_numberofcapturinggroups(lua_State *L)
{
	pcrecpp::RE *re = lua_pcre_checkpcre(L, 1);
	lua_pushinteger(L, re->NumberOfCapturingGroups());
	
	return 1;
}


static const luaL_reg pcre_methods[] = 
{
	{"new", lua_pcre_new}, 
	{"quote_meta", lua_pcre_quotemeta}, 
	
	{"match", lua_pcre_match}, 
	{"full_match", lua_pcre_full_match}, 
	{"match_all", lua_pcre_match_all}, 
	
	{"replace", lua_pcre_replace}, 
	{"replace_cb", lua_pcre_replace_cb}, 
	{"replace_one", lua_pcre_replace_one},
	
	{"csplit", lua_pcre_csplit},
	
	{"get_pattern", lua_pcre_getpattern}, 
	{"get_error", lua_pcre_geterror}, 
	{"number_of_capturing_groups", lua_pcre_numberofcapturinggroups}, 
	
	{NULL, NULL}
};




static int lua_pcre_gc(lua_State *L) // destroy
{
	pcrecpp::RE *re = lua_pcre_topcre(L, 1);
	if(re)
	{
		delete re;
	}
	
	return 0;
}

static int lua_pcre_tostring(lua_State *L)
{
	pcrecpp::RE *re = lua_pcre_topcre(L, 1);
	if(re)
	{
		lua_pushstdstring(L, re->pattern());
	}
	else
	{
		lua_pushstring(L, "NULL pcre object");
	}
	
	return 1;
}

static const luaL_reg pcre_meta_methods[] = 
{
	{"__gc", lua_pcre_gc}, 
	{"__tostring", lua_pcre_tostring}, 
	{NULL, NULL}
};



int luaopen_libluapcre(lua_State *L)
{
	int methods, metatable;
	
	luaL_register(L, "pcre", pcre_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "pcre"); metatable = lua_gettop(L); // create metatable for Image, add it to the Lua registry
	luaL_register(L, 0, pcre_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // hide metatable: metatable.__metatable = methods
	
	lua_pop(L, 1); // drop metatable
	
	return 1;
}






