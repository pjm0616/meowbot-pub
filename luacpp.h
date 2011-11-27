
#ifndef LUACPP_H_INCLUDED
#define LUACPP_H_INCLUDED

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}


#include "luapcrelib.h"
#include "luautf8lib.h"
#include "luasocklib.h"
#include "luahangullib.h"
#include "luathreadlib.h"
#include "luaetclib.h"
#include "luasqlite3lib.h"



#define lua_pushstdstring(L_, str_) lua_pushlstring((L_), (str_).c_str(), (str_).size())
#define lua_pushstdstring2(L_, str_) \
	({ \
		const std::string &tmp_ = (str_); \
		lua_pushlstring((L_), tmp_.c_str(), tmp_.size()); \
	})
#define lua_pushstdstring_nl(L_, str_) lua_pushstring((L_), (str_).c_str())





#if 1
#define DEFINE_LUA_TOUSERDATA_FXN(_fxnname, _name, _type) \
	static _type *lua_to ##_fxnname(lua_State *L, int index) \
	{ \
		_type **ptr = (_type **)lua_touserdata(L, index); \
		if(!ptr) \
			luaL_typerror(L, index, #_name); \
		return *ptr; \
	}
#define DEFINE_LUA_CHECKUSERDATA_FXN(_fxnname, _name, _type) \
	static _type *lua_check ##_fxnname(lua_State *L, int index) \
	{ \
		luaL_checktype(L, index, LUA_TUSERDATA); \
		_type **ptr = (_type **)luaL_checkudata(L, index, #_name); \
		if(!ptr) \
			luaL_typerror(L, index, #_name); \
		if(!*ptr) \
			luaL_error(L, "null " #_name " object"); \
		return *ptr; \
	}
#define DEFINE_LUA_PUSHUSERDATA_FXN(_fxnname, _name, _type) \
	static _type **lua_push_ ##_fxnname(lua_State *L, _type *data) \
	{ \
		_type **ptr = (_type **)lua_newuserdata(L, sizeof(_type *)); \
		*ptr = data; \
		luaL_getmetatable(L, #_name); \
		lua_setmetatable(L, -2); \
		return ptr; \
	}
#endif



class luacpp
{
public:
	luacpp()
		:lua(NULL)
	{
		// std::map에서 문제 발생
		// this->init();
	}
	luacpp(const luacpp &obj)
		:lua(obj.lua), // FIXME
		lasterr_str(obj.lasterr_str) // FIXME
	{
	}
	~luacpp()
	{
		this->close();
	}
	
	int init();
	int close();
	
	/*
		luacpp::pcall
		* Return values: 
			0: success
			LUA_ERRRUN: a runtime error.
			LUA_ERRMEM: memory allocation error. For such errors, Lua does not call the error handler function.
			LUA_ERRERR: error while running the error handler function.
	*/
	int pcall(int nargs, int nresults);
	// lua 스택의 top에 있는 문자열을 pop한 후 반환.
	std::string popstring();
	
	// file을 로드 후 실행
	int dofile(const char *file);
	int dofile(const std::string &file){return this->dofile(file.c_str());}
	// code를 실행
	int dostring(const char *code);
	int dostring(const std::string &code){return this->dostring(code.c_str());}
	
	int vpcallarg(int nresults, const char *argfmt, va_list &ap);
	// pcallarg("int,bool,string,null", 3, true, "asdf", NULL);
	int pcallarg(int nresults, const char *argfmt, ...);
	int callfxn(const char *name, int nresults, const char *argfmt, ...);
	
	const std::string &get_lasterr_str() const {return this->lasterr_str;}
	void set_lasterr_str(const std::string &str) {this->lasterr_str = str;}
	
	
	
	
	
	lua_State *lua;
	
private:
	std::string lasterr_str;
	
};



struct luaref
{
	luaref()
		:L(NULL), 
		ref_id(LUA_NOREF)
	{
	}
	luaref(lua_State *L, int stack_n)
		:ref_id(LUA_NOREF)
	{
		this->ref(L, stack_n);
	}
	luaref(const luaref &o)
		:L(o.L), 
		ref_id(o.ref_id)
	{
#if 0
		// multi-threaded 환경에서 문제가 생길것같음
		o.pushref();
		this->ref(o.L, -1);
		luaL_pop(o.L, 1);
#endif
	}
	~luaref()
	{
		// FIXME: stl container 에서 문제 발생
		// 그래서 해제를 수동으로 해야됨
		//if(this->L)
		//	this->unref();
	}
	
	int ref(lua_State *L, int stack_n);
	void unref();
	void pushref(lua_State *L) const; // for threads
	void pushref() const;
	void clearref();
	bool operator==(const luaref &o) const;
	
	lua_State *L;
	int ref_id;
	
};





bool lua_my_tostdstring(lua_State *L, int index, std::string *dest);
std::string lua_my_tostdstring(lua_State *L, int index);
bool lua_my_toint(lua_State *L, int index, int *dest);
int lua_my_toint(lua_State *L, int index);
















#endif /* LUA_H_INCLUDED */

