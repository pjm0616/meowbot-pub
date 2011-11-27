#ifndef MODULE_H_INCLUDED
#define MODULE_H_INCLUDED


typedef int (module_init_t)(void *arg);
typedef int (module_cleanup_t)(void *arg);

class module
{
	friend class module_list;
	
public:
	
	enum
	{
		MODTYPE_SHARED, 
		MODTYPE_STATIC, 
		MODTYPE_END
	};
	enum
	{
		MODSTATUS_UNLOADED, 
		MODSTATUS_LOADED, 
		MODSTATUS_END
	};
	
	module() : handle(NULL), type(MODTYPE_SHARED), status(MODSTATUS_UNLOADED){}
	module(const module &obj)
	{
		this->handle = obj.handle;
		this->type = obj.type;
		this->status = obj.status;
		this->name = obj.name;
		this->module_arg = obj.module_arg;
	}
	~module()
	{
#if 0
		if(this->status == MODSTATUS_LOADED)
		{
			this->unload_module();
		}
#endif
	}
	
	int load_module(const std::string &path, void *module_arg = NULL);
	int load_static_module(const std::string &name, void *module_arg /*=NULL*/);
	int unload_module();
	
	bool isloaded() const{return this->status == MODSTATUS_LOADED;}
	const std::string &getname() const{return this->name;}
	void *dlsym(const char *symbol) const;
	
private:
	void *handle;
	int type;
	int status;
	std::string name;
	void *module_arg;
	
};

class module_list
{
public:
	module_list()
	{
		pthread_rwlock_init(&this->modlist_lock, NULL);
	}
	~module_list(){}
	
	int load_module(const std::string &path, void *module_arg = NULL);
	int load_static_module(const std::string &path, void *module_arg = NULL);
	int unload_module(const std::string &name, bool do_lock = true);
	int unload_all();
	
	module *find_by_name(const std::string &name, bool do_lock = true);
	bool is_loaded(const std::string &name);
	void *dlsym(const std::string &modname, const char *symbol);
	
	void rdlock() {pthread_rwlock_rdlock(&this->modlist_lock);}
	void wrlock() {pthread_rwlock_wrlock(&this->modlist_lock);}
	void unlock() {pthread_rwlock_unlock(&this->modlist_lock);}
	
	// XXX
	std::map<std::string, module> modules;
private:
	pthread_rwlock_t modlist_lock;
	
};


#define MODULE_NAME_SYM "_module_name"
#define MODULE_INIT_FXN "_module_init"
#define MODULE_CLEANUP_FXN "_module_cleanup"

#ifdef STATIC_MODULES
# define MODULE_INFO(modname, initfxn, cleanupfxn) \
	extern "C" const char _module_##modname##_name[] = #modname; \
	extern "C" int _module_##modname##_init(void *arg){return reinterpret_cast<module_cleanup_t *>(initfxn)(arg);} \
	extern "C" int _module_##modname##_cleanup(void *arg){return reinterpret_cast<module_cleanup_t *>(cleanupfxn)(arg);}
#else
# define MODULE_INFO(modname, initfxn, cleanupfxn) \
	extern "C" const char _module_name[] = #modname; \
	extern "C" int _module_init(void *arg){return reinterpret_cast<module_init_t *>(initfxn)(arg);} \
	extern "C" int _module_cleanup(void *arg){return reinterpret_cast<module_cleanup_t *>(cleanupfxn)(arg);}
#endif










#endif // MODULE_H_INCLUDED
