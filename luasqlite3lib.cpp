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

#include <sqlite3.h>
#include <pcrecpp.h>

#include "defs.h"
#include "etc.h"
#include "luacpp.h"
#include "luasqlite3lib.h"







static int lua_sqlite3_stmt_do_bind_one(sqlite3_stmt *stmt, int n, lua_State *L, int idx)
{
	int res;
	int type = lua_type(L, idx);
	switch(type)
	{
	case LUA_TNIL:
		// int sqlite3_bind_null(sqlite3_stmt*, int);
		res = sqlite3_bind_null(stmt, n);
		break;
	case LUA_TNUMBER:
		res = sqlite3_bind_double(stmt, n, lua_tonumber(L, idx));
		//res = sqlite3_bind_int(stmt, n, lua_tointeger(L, idx));
		//res = sqlite3_bind_int64(stmt, n, lua_tointeger(L, idx));
		break;
	case LUA_TBOOLEAN:
		res = sqlite3_bind_int(stmt, n, lua_toboolean(L, idx));
		break;
	case LUA_TSTRING:
	{
		// int sqlite3_bind_blob(sqlite3_stmt*, int, const void*, int n, void(*)(void*));
		// int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int n, void(*)(void*));
		// int sqlite3_bind_text16(sqlite3_stmt*, int, const void*, int, void(*)(void*));
		const char *data;
		size_t len;
		data = lua_tolstring(L, idx, &len);
		assert(data != NULL);
		//res = sqlite3_bind_text(stmt, n, data, len, SQLITE_STATIC);
		res = sqlite3_bind_text(stmt, n, data, len, SQLITE_TRANSIENT);
		break;
	}
	
	//case LUA_TTABLE:
	//case LUA_TFUNCTION:
	//case LUA_TTUSERDATA:
	//case LUA_TTHREAD
	//case LUA_TLIGHTUSERDATA:
	default:
		luaL_error(L, "Incompatible type in sqlite3_stmt.bind");
		return -1; /* never reached */
	}
	if(res == SQLITE_OK)
		return 0;
	else if(res == SQLITE_RANGE)
		luaL_error(L, "sqlite3_stmt.bind: Bind index out of range");
	else if(res == SQLITE_NOMEM)
		luaL_error(L, "sqlite3_stmt.bind: Not enough memory");
	else if(res == SQLITE_MISUSE)
		luaL_error(L, "sqlite3_stmt.bind: SQLITE_MISUSE returned");
	
	assert(!"Cannot happen");
	return 0; // never reached
}

static int lua_push_sqlite3_value(lua_State *L, sqlite3_stmt *stmt, int col)
{
	int column_type = sqlite3_column_type(stmt, col);
	switch(column_type)
	{
	case SQLITE_INTEGER:
		// int sqlite3_column_int(sqlite3_stmt*, int iCol);
		// sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int iCol);
		lua_pushinteger(L, sqlite3_column_int(stmt, col));
		break;
	case SQLITE_FLOAT:
		// double sqlite3_column_double(sqlite3_stmt*, int iCol);
		lua_pushnumber(L, sqlite3_column_double(stmt, col));
		break;
	case SQLITE_TEXT:
	{
		// const unsigned char *sqlite3_column_text(sqlite3_stmt*, int iCol);
		// int sqlite3_column_bytes(sqlite3_stmt*, int iCol);
		const char *data = (const char *)sqlite3_column_text(stmt, col);
		int len = sqlite3_column_bytes(stmt, col);
		lua_pushlstring(L, data, len);
		break;
	}
	case SQLITE_BLOB:
	{
		// const void *sqlite3_column_blob(sqlite3_stmt*, int iCol);
		// int sqlite3_column_bytes(sqlite3_stmt*, int iCol);
		const char *data = (const char *)sqlite3_column_blob(stmt, col);
		int len = sqlite3_column_bytes(stmt, col);
		lua_pushlstring(L, data, len);
		break;
	}
	case SQLITE_NULL:
		lua_pushnil(L);
		break;
	default:
		break;
	}
	
	return column_type;
}

///////////////////////////////////////////////////////

struct luasqlite3
{
	sqlite3 *db;
};

static luasqlite3 *lua_tosqlite3o(lua_State *L, int index)
{
	luasqlite3 *data = (luasqlite3 *)lua_touserdata(L, index);
	if(!data)
		luaL_typerror(L, index, "sqlite3");
	return data;
}

static luasqlite3 *lua_checksqlite3o(lua_State *L, int index)
{
	luaL_checktype(L, index, LUA_TUSERDATA);
	luasqlite3 *data = (luasqlite3 *)luaL_checkudata(L, index, "sqlite3");
	if(!data)
		luaL_typerror(L, index, "sqlite3");
	return data;
}

static sqlite3 *lua_checksqlite3(lua_State *L, int index)
{
	luasqlite3 *dbs =  lua_checksqlite3o(L, index);
	if(!dbs->db)
		luaL_error(L, "Attemped to access closed sqlite3 db");
	return dbs->db;
}

static luasqlite3 *lua_push_sqlite3(lua_State *L, sqlite3 *db)
{
	luasqlite3 *data = (luasqlite3 *)lua_newuserdata(L, sizeof(luasqlite3 *));
	luaL_getmetatable(L, "sqlite3");
	lua_setmetatable(L, -2);
	
	data->db = db;
	//pthread_mutex_init(&data->db_lock, NULL);
	//new(&data->opened_stmts) pobj_vector<sqlite3_stmt>; 
	
	return data;
}

/////////////////////////////////////////////

static sqlite3_stmt *lua_tosqlite3_stmt(lua_State *L, int index)
{
	sqlite3_stmt **data = (sqlite3_stmt **)lua_touserdata(L, index);
	if(!data)
	{
		luaL_typerror(L, index, "sqlite3_stmt");
	}
	return *data;
}

static sqlite3_stmt *lua_checksqlite3_stmt(lua_State *L, int index)
{
	sqlite3_stmt **data, *stmt;
	luaL_checktype(L, index, LUA_TUSERDATA);
	data = (sqlite3_stmt **)luaL_checkudata(L, index, "sqlite3_stmt");
	if(!data)
	{
		luaL_typerror(L, index, "sqlite3_stmt");
	}
	stmt = *data;
	if(!stmt)
	{
		luaL_error(L, "Null sqlite3_stmt object");
	}
	return stmt;
}

static sqlite3_stmt **lua_push_sqlite3_stmt(lua_State *L, sqlite3_stmt *stmt)
{
	sqlite3_stmt **data = (sqlite3_stmt **)lua_newuserdata(L, sizeof(sqlite3_stmt *));
	*data = stmt;
	luaL_getmetatable(L, "sqlite3_stmt");
	lua_setmetatable(L, -2);
	return data;
}

/////////////////////////////////////////////

static int lua_sqlite3_open(lua_State *L)
{
	const char *filename = luaL_checkstring(L, 1);
	
	sqlite3 *db;
	int rc = sqlite3_open(filename, &db);
	if(rc != SQLITE_OK)
	{
		lua_pushboolean(L, false);
		if(db)
		{
			lua_pushstring(L, sqlite3_errmsg(db));
			sqlite3_close(db);
		}
		else
			lua_pushliteral(L, "Cannot allocate memory");
		return 2;
	}
	lua_push_sqlite3(L, db);
	
	return 1;
}

static int lua_sqlite3_prepare(lua_State *L)
{
	sqlite3 *db = lua_checksqlite3(L, 1);
	const char *query = luaL_checkstring(L, 2);
	size_t querylen = lua_objlen(L, 2);
	
	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare(db, query, querylen, &stmt, NULL);
	if(rc != SQLITE_OK)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	lua_push_sqlite3_stmt(L, stmt);
	
	return 1;
}

static int lua_sqlite3_prepare_and_bind(lua_State *L)
{
	sqlite3 *db = lua_checksqlite3(L, 1);
	const char *query = luaL_checkstring(L, 2);
	size_t querylen = lua_objlen(L, 2);
	int cnt = lua_gettop(L) - 2;
	
	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare(db, query, querylen, &stmt, NULL);
	if(rc != SQLITE_OK)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	
	int stmt_param_cnt = sqlite3_bind_parameter_count(stmt);
	if(stmt_param_cnt != cnt)
	{
		sqlite3_finalize(stmt);
		luaL_error(L, "sqlite3.prepare_and_bind: bind parameter count not matched");
	}
	for(int i = 1; i <= cnt; i++)
		lua_sqlite3_stmt_do_bind_one(stmt, i, L, i + 2);
	
	lua_push_sqlite3_stmt(L, stmt);
	
	return 1;
}

static int lua_sqlite3_errmsg(lua_State *L)
{
	sqlite3 *db = lua_checksqlite3(L, 1);
	lua_pushstring(L, sqlite3_errmsg(db));
	
	return 1;
}

static int luasqlite3_close(luasqlite3 *lsdb)
{
	if(lsdb->db == NULL)
		return -1;
	
	// FIXME: delete all lua references to stmts created by db
#if 0 // finalize here and crash when lua script uses the stmt, or...
	// delect statements created by db. should not be happen
	sqlite3_stmt *stmt;
	while((stmt = sqlite3_next_stmt(lsdb->db, NULL)))
	{
		fprintf(stderr, "Warning: luasqlite3: forced delete of sqlite3_stmt %p opened by %p\n", stmt, lsdb->db);
		sqlite3_finalize(stmt);
	}
#else // memory leak
	sqlite3_stmt *stmt = sqlite3_next_stmt(lsdb->db, NULL);
	if(stmt)
	{
		fprintf(stderr, "WARNING: luasqlite3: You MUST finalize all statements before closing db. At least 1 sqlite3_stmt(s)(opened by %p) not freed\n", lsdb->db);
	}
#endif
	
	sqlite3_close(lsdb->db);
	lsdb->db = NULL;
	
	return 0;
}

static int lua_sqlite3_close(lua_State *L)
{
	luasqlite3 *lsdb = lua_checksqlite3o(L, 1);
	int ret = luasqlite3_close(lsdb);
	lua_pushboolean(L, ret == 0);
	
	return 1;
}

static const luaL_reg sqlite3_methods[] = 
{
	{"open", lua_sqlite3_open}, 
	{"close", lua_sqlite3_close}, 
	{"prepare", lua_sqlite3_prepare}, 
	{"prepare_and_bind", lua_sqlite3_prepare_and_bind}, 
	{"errmsg", lua_sqlite3_errmsg}, 
	
	{NULL, NULL}
};

static int lua_sqlite3_gc(lua_State *L) // destroy
{
	luasqlite3 *lsdb = lua_tosqlite3o(L, 1);
	if(lsdb)
	{
		luasqlite3_close(lsdb);
	}
	
	return 0;
}

static const luaL_reg sqlite3_meta_methods[] = 
{
	{"__gc", lua_sqlite3_gc}, 
	{NULL, NULL}
};




static int lua_sqlite3_stmt_bind_one(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	int idx = luaL_checkint(L, 2);
	lua_sqlite3_stmt_do_bind_one(stmt, idx, L, 3);
	
	return 1;
}

static int lua_sqlite3_stmt_bind(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	int cnt = lua_gettop(L) - 1;
	int stmt_param_cnt = sqlite3_bind_parameter_count(stmt);
	if(stmt_param_cnt != cnt)
	{
		luaL_error(L, "sqlite3_stmt.bind: bind parameter count not matched");
	}
	for(int i = 1; i <= cnt; i++)
		lua_sqlite3_stmt_do_bind_one(stmt, i, L, i + 1);
	
	return 1;
}

static int lua_sqlite3_stmt_reset(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	sqlite3_reset(stmt);
	
	return 1;
}

static int lua_sqlite3_stmt_step(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	
	int res = sqlite3_step(stmt);
	if(res == SQLITE_ROW)
		lua_pushboolean(L, true);
	else if(res == SQLITE_DONE)
		lua_pushboolean(L, true);
	else
		lua_pushboolean(L, false);
	
	return 1;
}

static int lua_sqlite3_stmt_fetch_array(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	
	int rc = sqlite3_step(stmt);
	if(rc != SQLITE_ROW)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	
	int column_count = sqlite3_column_count(stmt);
	lua_createtable(L, column_count, 0);
	for(int col = 0; col < column_count; col++)
	{
		lua_push_sqlite3_value(L, stmt, col);
		lua_rawseti(L, -2, col + 1);
	}
	
	return 1;
}

static int lua_sqlite3_stmt_fetch_array_all(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	
	lua_newtable(L);
	int result_array = lua_gettop(L);
	int n = 1;
	while(sqlite3_step(stmt) == SQLITE_ROW)
	{
		int column_count = sqlite3_column_count(stmt);
		lua_createtable(L, column_count, 0);
		for(int col = 0; col < column_count; col++)
		{
			lua_push_sqlite3_value(L, stmt, col);
			lua_rawseti(L, -2, col + 1);
		}
		lua_rawseti(L, result_array, n++);
	}
	
	return 1;
}

static int lua_sqlite3_stmt_fetch_object(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	
	int rc = sqlite3_step(stmt);
	if(rc != SQLITE_ROW)
	{
		lua_pushboolean(L, false);
		return 1;
	}
	
	int column_count = sqlite3_column_count(stmt);
	lua_createtable(L, column_count, 0);
	for(int col = 0; col < column_count; col++)
	{
		const char *colname = sqlite3_column_name(stmt, col);
		if(!colname)
			luaL_error(L, "Out of memory");
		lua_pushstring(L, colname);
		lua_push_sqlite3_value(L, stmt, col);
		lua_rawset(L, -3);
	}
	
	return 1;
}

static int lua_sqlite3_stmt_fetch_object_all(lua_State *L)
{
	sqlite3_stmt *stmt = lua_checksqlite3_stmt(L, 1);
	
	lua_newtable(L);
	int result_array = lua_gettop(L);
	int n = 1;
	while(sqlite3_step(stmt) == SQLITE_ROW)
	{
		int column_count = sqlite3_column_count(stmt);
		lua_createtable(L, column_count, 0);
		for(int col = 0; col < column_count; col++)
		{
			const char *colname = sqlite3_column_name(stmt, col);
			if(!colname)
				luaL_error(L, "Out of memory");
			lua_pushstring(L, colname);
			lua_push_sqlite3_value(L, stmt, col);
			lua_rawset(L, -3);
		}
		lua_rawseti(L, result_array, n++);
	}
	
	return 1;
}


static int lua_sqlite3_stmt_finalize(lua_State *L)
{
	sqlite3_stmt **stmtp;
	luaL_checktype(L, 1, LUA_TUSERDATA);
	stmtp = (sqlite3_stmt **)luaL_checkudata(L, 1, "sqlite3_stmt");
	if(!stmtp)
		luaL_typerror(L, 1, "sqlite3_stmt");
	
	if(*stmtp)
	{
		sqlite3_finalize(*stmtp);
		*stmtp = NULL;
		lua_pushboolean(L, true);
	}
	else
	{
		lua_pushboolean(L, false);
	}
	
	return 1;
}

static const luaL_reg sqlite3_stmt_methods[] = 
{
	{"finalize", lua_sqlite3_stmt_finalize}, 
	
	{"bind_one", lua_sqlite3_stmt_bind_one}, 
	{"bind", lua_sqlite3_stmt_bind}, 
	{"reset", lua_sqlite3_stmt_reset}, 
	{"step", lua_sqlite3_stmt_step}, 
	{"fetch_array", lua_sqlite3_stmt_fetch_array}, 
	{"fetch_array_all", lua_sqlite3_stmt_fetch_array_all}, 
	{"fetch_object", lua_sqlite3_stmt_fetch_object}, 
	{"fetch_object_all", lua_sqlite3_stmt_fetch_object_all}, 
	
	{NULL, NULL}
};


static int lua_sqlite3_stmt_gc(lua_State *L)
{
	sqlite3_stmt *stmt = lua_tosqlite3_stmt(L, 1);
	if(stmt)
	{
		sqlite3_finalize(stmt);
	}
	
	return 0;
}

static const luaL_reg sqlite3_stmt_meta_methods[] = 
{
	{"__gc", lua_sqlite3_stmt_gc}, 
	{NULL, NULL}
};



int luaopen_libluasqlite3(lua_State *L)
{
	int methods, metatable;
	
	// sqlite3
	luaL_register(L, "sqlite3", sqlite3_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "sqlite3"); metatable = lua_gettop(L); // create metatable for Image, add it to the Lua registry
	luaL_register(L, 0, sqlite3_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // hide metatable: metatable.__metatable = methods
	
	lua_pop(L, 2); // drop metatable, methods table
	
	///// sqlite3_stmt
	luaL_register(L, "sqlite3_stmt", sqlite3_stmt_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "sqlite3_stmt"); metatable = lua_gettop(L); // create metatable for Image, add it to the Lua registry
	luaL_register(L, 0, sqlite3_stmt_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // hide metatable: metatable.__metatable = methods
	
	lua_pop(L, 2); // drop metatable, methods table
	
	return 0;
}






