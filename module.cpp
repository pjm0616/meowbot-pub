/*
	Copyright (C) 2008 Park Jeongmin
*/


#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <deque>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>

#include <pcrecpp.h>

#include "defs.h"
#include "module.h"


#ifndef RTLD_DEEPBIND
# define RTLD_DEEPBIND 0
#endif
#ifndef RTLD_LOCAL
# define RTLD_LOCAL 0
#endif



int module::load_module(const std::string &path, void *module_arg /*=NULL*/)
{
	void *handle;
	int ret;
	const char *errmsg;
	
	if(this->status == MODSTATUS_LOADED)
	{
		return -1;
	}
	
	dlerror(); // clear error
	handle = dlopen(path.c_str(), 
		RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	if((errmsg = dlerror()) != NULL)
	{
		std::cerr << "dlopen failed: " << errmsg << std::endl;
		return -1;
	}
	const char *name = (const char *)::dlsym(handle, MODULE_NAME_SYM);
	if(!name)
	{
		std::cerr << "module name not found: " << dlerror() << std::endl;
		dlclose(handle);
		return -1;
	}
	this->module_arg = module_arg;
	module_init_t *init_fxn = 
		(module_init_t *)::dlsym(handle, MODULE_INIT_FXN);
	if(init_fxn)
	{
		ret = init_fxn(module_arg);
		if(ret < 0)
		{
			std::cerr << "init function failed: returned " << ret << std::endl;
			dlclose(handle);
			return -1;
		}
	}
	else
	{
		std::cerr << "init function not found: " << dlerror() << std::endl;
	}
	
	this->name = name;
	this->handle = handle;
	this->type = MODTYPE_SHARED;
	this->status = MODSTATUS_LOADED;
	
	return 0;
}

int module::load_static_module(const std::string &name, void *module_arg)
{
	int ret;
	
	if(this->status == MODSTATUS_LOADED)
	{
		return -1;
	}
	
	const char *modname = reinterpret_cast<const char *>(
		::dlsym(RTLD_DEFAULT, 
			std::string("_module_" + name + "_name").c_str())
		);
	if(!modname)
	{
		std::cerr << "module name not found: " << dlerror() << std::endl;
		return -1;
	}
	this->module_arg = module_arg;
	module_init_t *init_fxn = (module_init_t *)::dlsym(RTLD_DEFAULT, 
		std::string("_module_" + name + "_init").c_str());
	if(init_fxn)
	{
		ret = init_fxn(module_arg);
		if(ret < 0)
		{
			std::cerr << "init function returned " << ret << std::endl;
			return -1;
		}
	}
	else
	{
		std::cerr << "init function not found: " << dlerror() << std::endl;
	}
	
	this->name = modname;
	this->handle = RTLD_DEFAULT;
	this->type = MODTYPE_STATIC;
	this->status = MODSTATUS_LOADED;
	
	return 0;
}

int module::unload_module()
{
	module_cleanup_t *cleanup_fxn;
	
	if(this->type == MODTYPE_SHARED)
	{
		cleanup_fxn = (module_cleanup_t *)::dlsym(this->handle, 
			MODULE_CLEANUP_FXN);
	}
	else
	{
		cleanup_fxn = (module_cleanup_t *)::dlsym(RTLD_DEFAULT, 
			std::string("_module_" + name + "_init").c_str());
	}
	if(cleanup_fxn)
	{
		cleanup_fxn(this->module_arg);
	}
	
	if(this->type == MODTYPE_SHARED)
	{
		dlclose(this->handle);
	}
	this->handle = NULL;
	this->status = MODSTATUS_UNLOADED;
	
	return 0;
}

void *module::dlsym(const char *symbol) const
{
	return ::dlsym(this->handle, symbol);
}






module *module_list::find_by_name(const std::string &name, bool do_lock)
{
	module *res = NULL;
	if(do_lock) this->rdlock();
	std::map<std::string, module>::iterator it = this->modules.find(name);
	if(it != this->modules.end())
		res = &it->second;
	if(do_lock) this->unlock();
	return res;
}

int module_list::load_module(const std::string &path, void *module_arg)
{
	module mod;
	int ret;
	
	ret = mod.load_module(path, module_arg);
	if(ret < 0)
		return ret;
	
	this->wrlock();
	if(this->find_by_name(mod.getname(), false))
	{
		this->unlock();
		mod.unload_module();
		return -1;
	}
	this->modules[mod.getname()] = mod;
	this->unlock();
	
	return 0;
}

int module_list::load_static_module(const std::string &name, void *module_arg)
{
	module mod;
	int ret;
	
	ret = mod.load_static_module(name, module_arg);
	if(ret < 0)
		return ret;
	
	this->wrlock();
	if(this->find_by_name(mod.getname(), false))
	{
		this->unlock();
		mod.unload_module();
		return -1;
	}
	this->modules[mod.getname()] = mod;
	this->unlock();
	
	return 0;
}

int module_list::unload_module(const std::string &name, bool do_lock)
{
	if(do_lock) this->wrlock();
	std::map<std::string, module>::iterator it = this->modules.find(name);
	if(it == this->modules.end())
	{
		if(do_lock) this->unlock();
		return -1;
	}
	
	it->second.unload_module();
	this->modules.erase(it);
	if(do_lock) this->unlock();
	
	return 0;
}

int module_list::unload_all()
{
	std::map<std::string, module>::iterator it;
	int cnt = 0;
	
	this->wrlock();
	for(it = this->modules.begin(); it != this->modules.end(); it++)
	{
		if(it->second.isloaded())
		{
			it->second.unload_module();
			cnt++;
		}
	}
	this->modules.clear();
	this->unlock();
	
	return cnt;
}

bool module_list::is_loaded(const std::string &name)
{
	module *mod = this->find_by_name(name);
	
	if(!mod)
		return false;
	else if(mod->isloaded())
		return true;
	else
		return false;
}

void *module_list::dlsym(const std::string &modname, const char *symbol)
{
	module *mod = this->find_by_name(modname);
	if(!mod)
		return NULL;
	
	return mod->dlsym(symbol);
}





// vim: set tabstop=4 textwidth=80:

