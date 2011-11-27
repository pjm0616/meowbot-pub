#ifndef ETC_H_INCLUDED
#define ETC_H_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


unsigned int get_ticks(void);

long int ts_rand();
void my_srand(long int seed);
long int my_rand();

std::string gettimestr(void);
std::string gettimestr_simple(void);
std::string int2string(int n);
int my_atoi(const char *str);
int my_atoi_base(const char *str, int base);

int urlencode(const char *str, char *dst, int size);
std::string urlencode(const std::string &str);
int urldecode(const char *str, char *dst, int size);
std::string urldecode(const std::string &str);
std::string decode_html_entities(const std::string &str);

std::string prevent_irc_highlighting_1(const std::string &nick);
std::string prevent_irc_highlighting(const std::string &word, std::string msg);
std::string prevent_irc_highlighting_all(const std::vector<std::string> &list, std::string msg);

int pcre_csplit(const pcrecpp::RE &re_delimeter, const std::string &data, std::vector<std::string> &dest);
int pcre_split(const std::string &delimeter, const std::string &data, std::vector<std::string> &dest);
static inline int pcre_split(const char *delimeter, const std::string &data, std::vector<std::string> &dest)
{
	return pcre_csplit(std::string(delimeter), data, dest);
}

char *readfile(const char *path, int *readsize = NULL);
int readfile(const char *path, std::string &dest);
int s_vsprintf(std::string *dest, const char *format, va_list &ap);
std::string s_vsprintf(const char *format, va_list &ap);

std::string strip_irc_colors(const std::string &msg);
std::string strip_spaces(const std::string &str);
bool is_bot_command(const std::string &str);


class url_parser
{
public:
	url_parser()
		:is_valid_url(false)
	{}
	url_parser(const std::string &url);
	~url_parser(){}
	
	bool is_valid() const{return this->is_valid_url;}
	const std::string &get_protocol() const{return this->protocol;}
	const std::string &get_host() const{return this->host;}
	int get_port() const{return this->port;}
	const std::string &get_location() const{return this->location;}
	
private:
	bool is_valid_url;
	std::string protocol, host, location;
	int port;
	
	
};






template <typename T>
class pobj_vector
{
public:
	pobj_vector()
		:size(0), 
		plist(NULL)
	{
		pthread_rwlock_init(&this->plist_lock, NULL);
	}
	pobj_vector(const pobj_vector &obj)
		:size(obj.size)
	{
		//this->plist = obj.duplicate();
		this->plist = obj.plist; // FIXME
	}
	~pobj_vector()
	{
		this->clear();
	}
	
	bool clear(bool do_lock = true);
	bool resize(size_t n, bool do_lock = true);
	bool check_size(size_t n, bool do_lock = true);
	inline T **getptrlist() const{return this->plist;} // for sort, etc.
	inline size_t getsize() const{return this->size;}
	inline bool empty() const{return this->size == 0;}
	
	int find_by_ptr(T *ptr, bool do_lock = true);
	int addobj(T *ptr, bool do_lock = true);
	T *newobj(bool do_lock = true);
	bool remove_by_id(int n, bool do_lock = true);
	bool del_by_id(int n, bool do_lock = true);
	bool delobj(T *ptr, bool do_lock = true);
	bool removeobj(T *ptr, bool do_lock = true);
	
	T *at(int n)
	{
		if(unlikely(n < 0 || (size_t)n >= this->size))
			return NULL;
		return this->plist[n];
	}
	inline T *operator[](int n) {return this->at(n);}
	
	inline void wrlock_plist() {pthread_rwlock_wrlock(&this->plist_lock);}
	inline void rdlock_plist() {pthread_rwlock_rdlock(&this->plist_lock);}
	inline int trywrlock_plist() {return pthread_rwlock_trywrlock(&this->plist_lock);}
	inline int tryrdlock_plist() {return pthread_rwlock_tryrdlock(&this->plist_lock);}
	inline void unlock_plist() {pthread_rwlock_unlock(&this->plist_lock);}
#if 1
	inline void wrlock() {pthread_rwlock_wrlock(&this->plist_lock);}
	inline void rdlock() {pthread_rwlock_rdlock(&this->plist_lock);}
	inline int trywrlock() {return pthread_rwlock_trywrlock(&this->plist_lock);}
	inline int tryrdlock() {return pthread_rwlock_tryrdlock(&this->plist_lock);}
	inline void unlock() {pthread_rwlock_unlock(&this->plist_lock);}
#endif
	
private:
	size_t size;
	T **plist;
	pthread_rwlock_t plist_lock;
	
};




template <typename T>
bool pobj_vector<T>::clear(bool do_lock)
{
	if(do_lock) this->wrlock_plist();
	if(this->plist)
	{
		for(size_t i = 0; i < this->size; i++)
			if(this->plist[i])
				delete this->plist[i];
		free(this->plist);
		this->plist = NULL;
	}
	this->size = 0;
	if(do_lock) this->unlock_plist();
	return true;
}


template <typename T>
bool pobj_vector<T>::resize(size_t newsize, bool do_lock)
{
	if(unlikely(newsize == this->size))
		return true;
	if(newsize == 0)
	{
		this->clear(false);
		return true;
	}
	
	if(do_lock) this->wrlock_plist();
	if(newsize < this->size)
	{
		for(size_t i = newsize; i < this->size; i++)
		{
			//if(this->plist[i])
			//	delete this->plist[i];
			//// this->plist[i] = NULL;
			if(this->plist[i])
				fprintf(stderr, "pobj_vector::resize: BUG FOUND\n");
		}
	}
	this->plist = (T **)realloc(this->plist, sizeof(T) * newsize);
	if(!this->plist)
	{
		fprintf(stderr, "pobj_vector::resize: FATAL: realloc() failed. "
			"memory leak: %d*%d = %d byte(s)\n", sizeof(T), this->size, sizeof(T *) * this->size);
		this->size = 0;
		if(do_lock) this->unlock_plist();
		return false;
	}
	if(newsize > this->size)
	{
		for(size_t i = this->size; i < newsize; i++)
		{
			this->plist[i] = NULL;
		}
	}
	this->size = newsize;
	
	if(do_lock) this->unlock_plist();
	return true;
}

template <typename T>
bool pobj_vector<T>::check_size(size_t n, bool do_lock)
{
	if(do_lock) this->wrlock_plist();
	if(this->size < n)
	{
		this->resize(n, false);
		if(do_lock) this->unlock_plist();
		return false;
	}
	if(do_lock) this->unlock_plist();
	return true;
}

template <typename T>
int pobj_vector<T>::find_by_ptr(T *ptr, bool do_lock)
{
	if(do_lock) this->rdlock_plist();
	for(size_t i = 0; i < this->size; i++)
	{
		if(this->plist[i] == ptr)
		{
			if(do_lock) this->unlock_plist();
			return i;
		}
	}
	if(do_lock) this->unlock_plist();
	return -1;
}

template <typename T>
int pobj_vector<T>::addobj(T *ptr, bool do_lock)
{
	int n, result = -1;
	if(do_lock) this->wrlock_plist();
	
	n = this->find_by_ptr(ptr, false);
	if(n >= 0)
	{
		fprintf(stderr, "pobj_vector::addobj: ERROR: same object already exists in then list(%p, %d)\n", ptr, n);
		goto out;
	}
	n = this->find_by_ptr(NULL, false);
	if(n < 0)
	{
		// ALWAYS reaches here
		bool ret = this->resize(this->size + 1, false);
		if(!ret)
			goto out;
		n = this->size - 1;
	}
	else
	{
		fprintf(stderr, "pobj_vector::addobj: BUG: should not reach here\n");
	}
	result = n;
	this->plist[n] = ptr;
out:
	if(do_lock) this->unlock_plist();
	return result;
}

template <typename T>
T *pobj_vector<T>::newobj(bool do_lock)
{
	T *ptr = new T;
	int ret = this->addobj(ptr, do_lock);
	if(ret < 0)
	{
		delete ptr;
		return NULL;
	}
	return ptr;
}

template <typename T>
bool pobj_vector<T>::remove_by_id(int n, bool do_lock)
{
	if(unlikely(n < 0 || (size_t)n >= this->size))
		return false;
	if(do_lock) this->wrlock_plist();
	
	if((size_t)n < this->size)
		memmove(&this->plist[n], &this->plist[n + 1], sizeof(T) * (this->size - 1 - n));
	this->plist[this->size - 1] = NULL;
	this->resize(this->size - 1, false);
	
	if(do_lock) this->unlock_plist();
	return true;
}

template <typename T>
bool pobj_vector<T>::del_by_id(int n, bool do_lock)
{
	if(unlikely(n < 0 || (size_t)n >= this->size))
		return false;
	if(do_lock) this->wrlock_plist();
	
	if(this->plist[n])
		delete this->plist[n];
	this->remove_by_id(n, false);
	
	if(do_lock) this->unlock_plist();
	return true;
}

template <typename T>
bool pobj_vector<T>::delobj(T *ptr, bool do_lock)
{
	assert(ptr != NULL);
	if(do_lock) this->wrlock_plist();
	int pos = this->find_by_ptr(ptr, false);
	if(pos < 0)
	{
		if(do_lock) this->unlock_plist();
		return false;
	}
	bool ret = this->del_by_id(pos, false);	
	if(do_lock) this->unlock_plist();
	return ret;
}

template <typename T>
bool pobj_vector<T>::removeobj(T *ptr, bool do_lock)
{
	assert(ptr != NULL);
	if(do_lock) this->wrlock_plist();
	int pos = this->find_by_ptr(ptr, false);
	if(pos < 0)
	{
		if(do_lock) this->unlock_plist();
		return false;
	}
	bool ret = this->remove_by_id(pos, false);	
	if(do_lock) this->unlock_plist();
	return ret;
}





#if 0
template <typename Tkey, typename Tval>
class pobj_map
{
public:
	pobj_map()
	{
		pthread_rwlock_init(&this->ptrmap_lock, NULL);
	}
	pobj_map(const pobj_map &obj)
	{
		this->ptrmap = obj.ptrmap; // FIXME
	}
	~pobj_map()
	{
		this->clear();
	}
	
	bool clear(bool do_lock = true);
	inline size_t getsize() const{return this->ptrmap.size();}
	inline bool empty() const{return this->ptrmap.empty();}
	
	Tkey find_by_ptr(Tval *ptr, bool do_lock = true);
	Tval *newobj(const Tkey &key, bool do_lock = true);
	bool del_by_iterator(std::map<Tkey, Tval *>::iterator it, bool do_lock = true);
	bool del_by_key(const Tkey &key, bool do_lock = true);
	bool delobj(Tval *ptr, bool do_lock = true);
	
	Tval *at(const Tkey &key);
	inline Tval *operator[](const Tkey &key) {return this->at(key);}
	
	inline void wrlock() {pthread_rwlock_wrlock(&this->ptrmap_lock);}
	inline void rdlock() {pthread_rwlock_rdlock(&this->ptrmap_lock);}
	inline void unlock() {pthread_rwlock_unlock(&this->ptrmap_lock);}
	
private:
	std::map<Tkey, Tval *> ptrmap;
	pthread_rwlock_t ptrmap_lock;
	
};


template <typename Tkey, typename Tval>
bool pobj_map<Tkey, Tval>::clear(bool do_lock)
{
	if(do_lock) this->wrlock();
	for(std::map<Tkey, Tval *>::iterator it = this->ptrmap.begin(); 
		it != this->ptrmap.end(); it++)
	{
		if(it->second)
			delete it->second;
	}
	this->ptrmap.clear();
	if(do_lock) this->unlock();
	return true;
}

template <typename Tkey, typename Tval>
Tval *pobj_map<Tkey, Tval>::newobj(const Tkey &key, bool do_lock)
{
	if(do_lock) this->wrlock();
	if(this->ptrmap.find(key) != this->ptrmap.end())
		return NULL;
	Tval *obj = new Tval;
	this->ptrmap[key] = obj;
	if(do_lock) this->unlock();
	return obj;
}

template <typename Tkey, typename Tval>
Tval *pobj_map<Tkey, Tval>::at(const Tkey &key)
{
	this->rdlock();
	Tval *ptr = NULL;
	std::map<Tkey, Tval *>::const_iterator it = this->ptrmap.find(key);
	if(it != this->ptrmap.end())
		ptr = it->second;
	this->unlock();
	return ptr;
}

template <typename Tkey, typename Tval>
bool pobj_map<Tkey, Tval>::del_by_iterator(std::map<Tkey, Tval *>::iterator it, bool do_lock)
{
	if(it == this->ptrmap.end())
		return false;
	if(do_lock) this->wrlock();
	if(it->second)
		delete it->second;
	this->ptrmap.erase(it);
	if(do_lock) this->unlock();
	return true;
}

template <typename Tkey, typename Tval>
bool pobj_map<Tkey, Tval>::del_by_key(const Tkey &key, bool do_lock)
{
	if(do_lock) this->wrlock();
	bool ret = this->del_by_iterator(this->ptrmap.find(key));
	if(do_lock) this->unlock();
	return ret;
}

template <typename Tkey, typename Tval>
bool pobj_map<Tkey, Tval>::delobj(Tval *ptr, bool do_lock)
{
	if(do_lock) this->wrlock();
	bool res = false;
	for(std::map<Tkey, Tval *>::iterator it = this->ptrmap.begin(); 
		it != this->ptrmap.end(); it++)
	{
		if(it->second == ptr)
		{
			this->del_by_iterator(it);
			res = true;
			break;
		}
	}
	if(do_lock) this->unlock();
	return res;
}
#endif







#if 0
int curl_get_url(const std::string &url, std::string &dest);
std::string curl_get_url(const std::string &url);
#endif


#endif // ETC_H_INCLUDED

