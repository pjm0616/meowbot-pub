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
#include <map>
#include <iostream>
#include <fstream>
#include <pthread.h>

#include <pcrecpp.h>

#include "defs.h"
#include "etc.h"
#include "config.h"
#include "luacpp.h"



lconfig::lconfig()
{
	pthread_mutex_init(&this->lua_lock, NULL);
	this->Lw.init();
}

lconfig::~lconfig()
{
	this->Lw.close();
}

int lconfig::clear(bool do_lock)
{
	if(do_lock) this->lock_ls();
	this->Lw.close();
	int ret = this->Lw.init();
	if(do_lock) this->unlock_ls();
	return ret;
}

int lconfig::open(const std::string &filename)
{
	this->lock_ls();
	this->clear(false);
	int ret = this->Lw.dofile(filename.c_str());
	this->unlock_ls();
	if(ret != 0)
		return -1;
	this->filename = filename;
	return 0;
}

// TODO
int lconfig::save(const std::string &filename)
{
	(void)filename;
	return -1;
}

int lconfig::getfield_1(const char *key)
{
	int result = 0;
	
	lua_getfield(this->Lw.lua, LUA_GLOBALSINDEX, key);
	
	return result;
}

#if 0
static int lconfig_gettbllist()
{
	int cnt = 0;
	lua_State *L;
	int index;
	
	lua_pushnil(L);
	while(lua_next(L, index) != 0)
	{
		// TODO
		lua_type(L, -2); // key
		lua_type(L, -1); // value
		
		lua_pop(L, 1); // pop key
		cnt++;
	}
	
	return cnt;
}
#endif

static int lua_table_to_stdvector(lua_State *L, int index, std::vector<std::string> &dest)
{
	int cnt = 0;
	
	dest.clear();
	lua_pushnil(L);
	while(lua_next(L, index) != 0)
	{
		//lua_type(L, -2); // key
		//lua_type(L, -1); // value
		
		dest.push_back(lua_my_tostdstring(L, -1));
		
		lua_pop(L, 1); // pop key
		cnt++;
	}
	
	return cnt;
}

int lconfig::getfield_c_1(const char *code)
{
	int ret, result = 0;
	
	ret = luaL_loadstring(this->Lw.lua, (std::string("return (") + code + ")").c_str());
	if(ret)
	{
		this->Lw.set_lasterr_str(this->Lw.popstring());
		result = -1;
		goto out;
	}
	ret = this->Lw.pcall(0, 1);
	if(ret != 0)
	{
		result = ret;
	}
	
out:
	return result;
}

// TODO
int lconfig::getfield_c(const char *code)
{
	int ret, result = 0;
	
	ret = luaL_loadstring(this->Lw.lua, (std::string("return (") + code + ")").c_str());
	if(ret)
	{
		std::string errmsg(this->Lw.popstring());
		std::cerr << "lconfig::getfield_c: lua error: " << errmsg << std::endl;
		this->Lw.set_lasterr_str(errmsg);
		result = -1;
		goto out;
	}
	//ret = this->Lw.pcall(0, LUA_MULTRET);
	//int nret = lua_gettop(this->Lw.lua);
	if(ret != 0)
	{
		result = ret;
	}
	// FIXME
	
out:
	return result;
}

//LUA_TNIL, LUA_TNUMBER, LUA_TBOOLEAN, LUA_TSTRING, LUA_TTABLE, LUA_TFUNCTION, LUA_TUSERDATA, LUA_TTHREAD, LUA_TLIGHTUSERDATA
std::string lconfig::getstr(const std::string &code, const std::string &default_val)
{
	this->lock_ls();
	int ret = this->getfield_c_1(code.c_str());
	if(ret != 0)
	{
		this->unlock_ls();
		return default_val;
	}
	std::string res;
	ret = lua_my_tostdstring(this->Lw.lua, -1, &res);
	lua_pop(this->Lw.lua, 1);
	if(!ret)
	{
		this->unlock_ls();
		return default_val;
	}
	this->unlock_ls();
	return res;
}
int lconfig::getint(const std::string &code, int default_val)
{
	this->lock_ls();
	int ret = this->getfield_c_1(code.c_str());
	if(ret != 0)
	{
		this->unlock_ls();
		return default_val;
	}
	int res;
	ret = lua_my_toint(this->Lw.lua, -1, &res);
	lua_pop(this->Lw.lua, 1);
	if(!ret)
	{
		res = default_val;
	}
	this->unlock_ls();
	return res;
}
bool lconfig::getbool(const std::string &code, bool default_val)
{
	this->lock_ls();
	int ret = this->getfield_c_1(code.c_str());
	if(ret != 0)
	{
		this->unlock_ls();
		return default_val;
	}
	bool res;
	if(lua_type(this->Lw.lua, -1) == LUA_TBOOLEAN)
	{
		res = lua_toboolean(this->Lw.lua, -1);
	}
	else
	{
		res = default_val;
	}
	lua_pop(this->Lw.lua, 1);
	this->unlock_ls();
	return (res != 0);
}

bool lconfig::getarray(std::vector<std::string> &dest, const std::string &code, 
	const std::vector<std::string> *default_val)
{
	bool res = true;
	this->lock_ls();
	int ret = this->getfield_c_1(code.c_str());
	if(ret != 0)
	{
		this->unlock_ls();
		dest = *default_val;
		return false;
	}
	if(lua_type(this->Lw.lua, -1) == LUA_TTABLE)
	{
		lua_table_to_stdvector(this->Lw.lua, lua_gettop(this->Lw.lua), dest);
	}
	else
	{
		if(!default_val)
			dest.clear();
		else
			dest = *default_val;
		res = false;
	}
	lua_pop(this->Lw.lua, 1);
	this->unlock_ls();
	return res;
}




config::config()
{
	pthread_rwlock_init(&this->cfgdata_lock, NULL);
}

// -1: cannot open file
// -2: parsing failed
int config::parse_file(const std::string &filename)
{
	static pcrecpp::RE	re_cfgline("^([^+= \t]+)[ \t]*=[ \t]*(.*)"), 
						re_cfgline_append("^([^+= \t]+)[ \t]*\\+=[ \t]*(.*)"), 
						re_cfgline_include("^include (.+)"), 
						re_comment("^[ \t]*#.*");
	int ln, result;
	std::string buf, key, val;
	std::ifstream reader(filename.c_str());
	if(!reader.good())
	{
		reader.close();
		return -1;
	}
	
	ln = 0;
	result = 0;
	while(reader.good())
	{
		std::getline(reader, buf);
		ln++;
		if(buf.empty() || re_comment.FullMatch(buf))
			continue;
		else if(re_cfgline.FullMatch(buf, &key, &val))
		{
			this->cfgdata[key] = val;
		}
		else if(re_cfgline_append.FullMatch(buf, &key, &val))
		{
			this->cfgdata[key] += val;
		}
		else if(re_cfgline_include.FullMatch(buf, &val))
		{
			int ret = this->parse_file(val);
			if(ret == -1) // cannot open file
			{
				fprintf(stderr, "%s:%d: cannot open file %s\n", filename.c_str(), ln, val.c_str());
				result = -2;
				break;
			}
			else if(ret < 0)
			{
				result = ret;
				break;
			}
		}
		else
		{
			fprintf(stderr, "parsing failed in file %s at line %d\n", filename.c_str(), ln);
			result = -2;
			break;
		}
	}
	reader.close();
	
	return result;
}

int config::open(const std::string &filename)
{
	pthread_rwlock_wrlock(&this->cfgdata_lock);
	this->cfgdata.clear();
	int ret = this->parse_file(filename);
	this->filename = filename;
	pthread_rwlock_unlock(&this->cfgdata_lock);
	
	return ret;
}

int config::save(const std::string &filename)
{
	unlink(filename.c_str());
	std::ofstream writer(filename.c_str());
	
	pthread_rwlock_rdlock(&this->cfgdata_lock);
	this->filename = filename; //
	for(std::map<std::string, std::string>::const_iterator it = 
			this->cfgdata.begin();
		it != this->cfgdata.end(); it++)
	{
		writer << it->first << " = " << it->second << "\n";
	}
	pthread_rwlock_unlock(&this->cfgdata_lock);
	writer << "\n";
	writer.close();
	
	return 0;
}

const std::string &config::getval(const std::string &key, 
	const std::string &default_val /*=""*/) const
{
	//pthread_rwlock_rdlock(&this->cfgdata_lock);
	const std::map<std::string, std::string>::const_iterator it = 
		this->cfgdata.find(key);
	//pthread_rwlock_unlock(&this->cfgdata_lock); // ㅁㄴㅇㄹ
	if(it == this->cfgdata.end())
		return default_val;
	else
		return it->second;
}

int config::getarray(std::vector<std::string> &dest, 
	const std::string &key, const std::string &default_val /*=""*/, 
	const std::string &delimeter /* =",[ \t]*" */) const
{
	const std::string &data = this->getval(key, default_val);
	
	pcre_split(delimeter, data, dest);
	
	return dest.size();
}

int config::getint(const std::string &key, int default_val /*=0*/) const
{
	return my_atoi(this->getval(key, int2string(default_val)).c_str());
}

int config::setval(const std::string &key, const std::string &value)
{
	pthread_rwlock_wrlock(&this->cfgdata_lock);
	this->cfgdata[key] = value;
	pthread_rwlock_unlock(&this->cfgdata_lock);
	
	return 0;
}

int config::setint(const std::string &key, int value)
{
	return this->setval(key, int2string(value));
}




// vim: set tabstop=4 textwidth=80:

