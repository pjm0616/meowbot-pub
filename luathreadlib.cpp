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
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>

#include "defs.h"
#include "luacpp.h"
#include "luathreadlib.h"



#ifdef __CYGWIN__
#define pthread_foobar_timedlock2(_lockfxn, _obj, _timeout) (_lockfxn)(_obj)
#else
#define pthread_foobar_timedlock2(_lockfxn, _obj, _timeout) \
	({ \
		timeval tv; \
		gettimeofday(&tv, NULL); \
		timespec ts; \
		TIMEVAL_TO_TIMESPEC(&tv, &ts); \
		ts.tv_sec = 0; ts.tv_nsec += (_timeout) * 1000000; \
		int ret = (_lockfxn)((_obj), &ts); \
		ret; \
	})
#endif

#ifdef __CYGWIN__
#define pthread_mutex_timedlock2(_mutex, _timeout) pthread_mutex_lock(_mutex)
#define pthread_rwlock_timedrdlock2(_rwlock, _timeout) pthread_rwlock_rdlock(_rwlock)
#define pthread_rwlock_timedwrlock2(_rwlock, _timeout) pthread_rwlock_wrlock(_rwlock)
#else
#define pthread_mutex_timedlock2(_mutex, _timeout) pthread_foobar_timedlock2(pthread_mutex_timedlock, _mutex, _timeout)
#define pthread_rwlock_timedrdlock2(_rwlock, _timeout) pthread_foobar_timedlock2(pthread_rwlock_timedrdlock, _rwlock, _timeout)
#define pthread_rwlock_timedwrlock2(_rwlock, _timeout) pthread_foobar_timedlock2(pthread_rwlock_timedwrlock, _rwlock, _timeout)
#endif



#if 0

struct lua_thread_state
{
	lua_State *L;
	luaref child_ref; // reference of the child thread
	
	// internal control data
	lua_thread_state **udata_ptr;
	bool is_started;
};



static lua_thread_state *lua_tolthread(lua_State *L, int index)
{
	lua_thread_state **ptr = (lua_thread_state **)lua_touserdata(L, index);
	if(!ptr)
		luaL_typerror(L, index, "pthread");
	
	return *ptr;
}

static lua_thread_state *lua_checklthread(lua_State *L, int index)
{
	luaL_checktype(L, index, LUA_TUSERDATA);
	lua_thread_state **ptr = (lua_thread_state **)luaL_checkudata(L, index, "pthread");
	if(!ptr)
		luaL_typerror(L, index, "pthread");
	
	// NULL일 경우 스레드가 죽은 것으로 간주
	//if(!*ptr)
	//	luaL_error(L, "null thread object");
	
	return *ptr;
}

static lua_thread_state **lua_push_lthread(lua_State *L, lua_thread_state *data)
{
	lua_thread_state **ptr = (lua_thread_state **)lua_newuserdata(L, sizeof(lua_thread_state *));
	*ptr = data;
	luaL_getmetatable(L, "pthread");
	lua_setmetatable(L, -2);
	
	return ptr;
}




static void lua_lthread_cleanup(void *arg)
{
	lua_thread_state *lts = (lua_thread_state *)arg;
	
	// stack: 1(arg table), 2(function)
	
	// TODO: call child cleanup handlers
	if(lts->udata_ptr)
		lts->udata_ptr = NULL;
	lts->child_ref.unref();
	lt_activedownandsignal(lts->L);
	delete lts;
	
	return;
}
static void *lua_lthread_entry_point(void *arg)
{
	lua_thread_state *lts = (lua_thread_state *)arg;
	pthread_detach(pthread_self());
	
	pthread_cleanup_push(lua_lthread_cleanup, lts);
	{
		while(lts->udata_ptr == NULL) // udata_ptr이 설정될때까지 기다림
			usleep(10);
		lts->is_started = true;
		
		lua_State *L = lts->L;
		// current lua stack: 1: func, 2: arg table
		
		// expand args
		size_t argcnt = lua_objlen(L, 2);
		for(size_t i = 1; i <= argcnt; i++)
		{
			lua_rawgeti(L, 2, i);
		}
		lua_remove(L, 2); // remove arg table from stack
		// current lua stack: 1: func, 2~: expanded args
		int ret = lua_pcall(L, argcnt, 0, 0);
		if(ret != 0)
		{
			const char *errmsg = lua_tostring(L, -1);
			fprintf(stderr, "lua: error: %s\n", errmsg);
			lua_pop(L, 1);
		}
	} pthread_cleanup_pop(1);
	
	return (void *)0;
}

static int lua_lthread_new(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);
	luaL_checktype(L, 2, LUA_TTABLE);
	
	lua_thread_state *lts = new lua_thread_state;
	lts->L = lua_newthread(L);
	if(!lts->L)
	{
		delete lts;
		luaL_error(L, "lua_newthread() failed");
		// never reached
	}
	lts->child_ref.ref(lts->L, -1); // child가 gc에 의해 삭제되지 않도록 reference 생성
	lua_xmove(L, lts->L, 2); // 2개의 인자를 child thread로 옮김
	lts->udata_ptr = NULL;
	lts->is_started = false;
	
	lt_activeup(L);
	pthread_t tid;
	int ret = pthread_create(&tid, NULL, lua_lthread_entry_point, lts);
	if(ret < 0)
	{
		lts->child_ref.unref();
		lt_activedown(L);
		delete lts;
		luaL_error(L, "pthread_create() failed: %d[%s]", errno, strerror(errno));
		// never reached
	}
	
	lua_thread_state **ptr = lua_push_lthread(L, lts);
	lts->udata_ptr = ptr;
	while(lts->is_started == false)
		usleep(10);
	
	return 1;
}

static const luaL_reg lthread_methods[] = 
{
	{"new", lua_lthread_new}, 
	
	{NULL, NULL}
};

static int lua_lthread_gc(lua_State *L) // destroy
{
	lua_thread_state *lts = lua_tolthread(L, 1);
	if(lts)
	{
		lts->udata_ptr = NULL;
		// thread가 종료될때 lts가 해제됨
	}
	
	return 0;
}

static const luaL_reg lthread_meta_methods[] = 
{
	{"__gc", lua_lthread_gc}, 
	{NULL, NULL}
};

#endif


////////////////////////////


#define LUAPTHREAD_DEFINE_UTILFXN(_name_, _type_) \
	static _type_ *lua_to_##_name_(lua_State *L, int index) \
	{ \
		_type_ *ptr = (_type_ *)lua_touserdata(L, index); \
		return ptr; \
	} \
	static _type_ *lua_check_##_name_(lua_State *L, int index) \
	{ \
		luaL_checktype(L, index, LUA_TUSERDATA); \
		_type_ *ptr = (_type_ *)luaL_checkudata(L, index, #_name_); \
		if(!ptr) \
			luaL_typerror(L, index, #_name_); \
		return ptr; \
	} \
	static _type_ *lua_new_##_name_(lua_State *L) \
	{ \
		_type_ *ptr = (_type_ *)lua_newuserdata(L, sizeof(_type_)); \
		luaL_getmetatable(L, #_name_); \
		lua_setmetatable(L, -2); \
		return ptr; \
	}




LUAPTHREAD_DEFINE_UTILFXN(pmutex, pthread_mutex_t)

static int lua_pmutex_new(lua_State *L)
{
	pthread_mutex_t *mutex = lua_new_pmutex(L);
	pthread_mutexattr_t mattr;
	
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(mutex, &mattr);
	//pthread_mutexattr_destroy(&mattr);
	
	return 1;
}
static int lua_pmutex_init(lua_State *L)
{
	pthread_mutex_t *mutex = lua_check_pmutex(L, 1);
	pthread_mutexattr_t mattr;
	
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(mutex, &mattr);
	//pthread_mutexattr_destroy(&mattr);
	
	return 0;
}


static int lua_pmutex_lock(lua_State *L)
{
	pthread_mutex_t *mutex = lua_check_pmutex(L, 1);
	int timeout = luaL_optint(L, 2, -1); // in miliseconds
	
	int ret;
	if(timeout < 0)
		ret = pthread_mutex_lock(mutex);
	else if(timeout == 0)
		ret = pthread_mutex_trylock(mutex);
	else
	{
		ret = pthread_mutex_timedlock2(mutex, timeout);
	}
	
	return 0;
}
static int lua_pmutex_unlock(lua_State *L)
{
	pthread_mutex_t *mutex = lua_check_pmutex(L, 1);
	
	int ret = pthread_mutex_unlock(mutex);
	lua_pushboolean(L, ret == 0);
	
	return 1;
}

static const luaL_reg pmutex_methods[] = 
{
	{"new", lua_pmutex_new}, 
	{"init", lua_pmutex_init}, 
	{"lock", lua_pmutex_lock}, 
	{"unlock", lua_pmutex_unlock}, 
	
	{NULL, NULL}
};
static const luaL_reg pmutex_meta_methods[] = 
{
	//{"__gc", lua_pmutex_gc}, 
	{NULL, NULL}
};

////////////////////////////

LUAPTHREAD_DEFINE_UTILFXN(prwlock, pthread_rwlock_t)

static int lua_prwlock_new(lua_State *L)
{
	pthread_rwlock_t *rwlock = lua_new_prwlock(L);
	pthread_rwlock_init(rwlock, NULL);
	
	return 1;
}
static int lua_prwlock_init(lua_State *L)
{
	pthread_rwlock_t *rwlock = lua_check_prwlock(L, 1);
	pthread_rwlock_init(rwlock, NULL);
	
	return 0;
}

static int lua_prwlock_rdlock(lua_State *L)
{
	pthread_rwlock_t *rwlock = lua_check_prwlock(L, 1);
	int timeout = luaL_optint(L, 2, -1); // in miliseconds
	
	int ret;
	if(timeout < 0)
		ret = pthread_rwlock_rdlock(rwlock);
	else if(timeout == 0)
		ret = pthread_rwlock_tryrdlock(rwlock);
	else
	{
		ret = pthread_rwlock_timedrdlock2(rwlock, timeout);
	}
	
	return 0;
}
static int lua_prwlock_wrlock(lua_State *L)
{
	pthread_rwlock_t *rwlock = lua_check_prwlock(L, 1);
	int timeout = luaL_optint(L, 2, -1); // in miliseconds
	
	int ret;
	if(timeout < 0)
		ret = pthread_rwlock_wrlock(rwlock);
	else if(timeout == 0)
		ret = pthread_rwlock_trywrlock(rwlock);
	else
	{
		ret = pthread_rwlock_timedwrlock2(rwlock, timeout);
	}
	
	return 0;
}
static int lua_prwlock_unlock(lua_State *L)
{
	pthread_rwlock_t *rwlock = lua_check_prwlock(L, 1);
	
	int ret = pthread_rwlock_unlock(rwlock);
	lua_pushboolean(L, ret == 0);
	
	return 1;
}

static const luaL_reg prwlock_methods[] = 
{
	{"new", lua_prwlock_new}, 
	{"init", lua_prwlock_init}, 
	{"rdlock", lua_prwlock_rdlock}, 
	{"wrlock", lua_prwlock_wrlock}, 
	{"unlock", lua_prwlock_unlock}, 
	
	{NULL, NULL}
};
static const luaL_reg prwlock_meta_methods[] = 
{
	//{"__gc", lua_prwlock_gc}, 
	{NULL, NULL}
};
////////////////////////////






#define DEFINE_LUACLASS_REGFXN(_name_) \
	luaL_register(L, #_name_, pmutex_methods); methods = lua_gettop(L); \
	luaL_newmetatable(L, #_name_); metatable = lua_gettop(L); \
	luaL_register(L, 0, _name_##_meta_methods); \
	 \
	lua_pushliteral(L, "__index"); \
	lua_pushvalue(L, methods); \
	lua_rawset(L, metatable); \
	 \
	lua_pushliteral(L, "__metatable"); \
	lua_pushvalue(L, methods); \
	lua_rawset(L, metatable); \
	 \
	lua_pop(L, 1); \


int luaopen_libluathread(lua_State *L)
{
	int methods, metatable;
	
#if 0
	luaL_register(L, "pthread", lthread_methods); methods = lua_gettop(L); // create methods table, add it to the globals
	luaL_newmetatable(L, "pthread"); metatable = lua_gettop(L); // create metatable for Image, add it to the Lua registry
	luaL_register(L, 0, lthread_meta_methods); // fill metatable
	
	lua_pushliteral(L, "__index"); // add index event to metatable
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // metatable.__index = methods
	
	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methods); // dup methods table
	lua_rawset(L, metatable); // hide metatable: metatable.__metatable = methods
	
	lua_pop(L, 1); // drop metatable
#endif
	
	DEFINE_LUACLASS_REGFXN(pmutex)
	DEFINE_LUACLASS_REGFXN(prwlock)
	//DEFINE_LUACLASS_REGFXN(pcond)
	
	return 0;
}






