#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include "luacpp.h"


// Lua-based config
class lconfig
{
public:
	lconfig();
	~lconfig();
	
	int clear(bool do_lock = true);
	int open(const std::string &filename);
	int save(const std::string &filename);
	
	std::string getstr(const std::string &code, const std::string &default_val = "");
	int getint(const std::string &code, int default_val = 0);
	bool getbool(const std::string &code, bool default_val = false);
	bool getarray(std::vector<std::string> &dest, const std::string &code, 
		const std::vector<std::string> *default_val = NULL);
	
	std::string operator[](const std::string &key){return this->getstr(key);}
	
	const std::string &get_filename(){return this->filename;}
	
	void lock_ls(){pthread_mutex_lock(&this->lua_lock);}
	void unlock_ls(){pthread_mutex_unlock(&this->lua_lock);}
	
	luacpp Lw;
private:
	pthread_mutex_t lua_lock;
	std::string filename;
	
	int getfield_1(const char *key);
	int getfield_c_1(const char *code);
	int getfield_c(const char *code);
	
};

// this class is NOT thread safe
class config
{
public:
	config();
	~config(){}
	
	int open(const std::string &filename);
	int save(const std::string &filename);
	
	std::map<std::string, std::string> &getdata(){return this->cfgdata;}
	const std::map<std::string, std::string> &getdata() const{return this->cfgdata;}
	
	const std::string &getval(const std::string &key, const std::string &default_val = "") const;
	int getarray(std::vector<std::string> &dest, const std::string &key, 
		const std::string &default_val = "", const std::string &delimeter =",[ \t]*") const;
	int getint(const std::string &key, int default_val = 0) const;
	
	int setval(const std::string &key, const std::string &value);
	int setint(const std::string &key, int value);
	
	std::string &operator[](const std::string &key){return this->cfgdata[key];}
	const std::string &get_filename(){return this->filename;}
	
private:
	
	int parse_file(const std::string &filename);
	
	std::string filename;
	std::map<std::string, std::string> cfgdata;
	pthread_rwlock_t cfgdata_lock;
	
};

#endif // CONFIG_H_INCLUDED

