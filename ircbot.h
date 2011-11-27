#ifndef IRCBOT_H_INCLUDED
#define IRCBOT_H_INCLUDED

#include <vector>
#include <list>
#include <deque>
#include <map>


#include "defs.h"
#include "etc.h"
#include "module.h"
#include "config.h"
#include "irc.h"

enum HANDLER_RESULT
{
	HANDLER_FINISHED, 
	HANDLER_GO_ON, 
	HANDLER_RESULT_END
};

class ircbot;
class ircbot_conn;
typedef int (ircbot_event_handler_t)(ircbot_conn *isrv, ircevent *evdata, void *userdata);
typedef int (ircbot_cmd_handler_t)(ircbot_conn *isrv, irc_privmsg &pmsg, irc_msginfo &mdest, std::string &cmd, std::string &carg, void *userdata);


////////////////////////////////////////////////////////////////////////////////////////////////
enum IRCBOT_TIMER_ATTRIBUTE
{
	IRCBOT_TIMERATTR_NONE = 0x0000, 
	IRCBOT_TIMERATTR_ONCE = 0x0001, 
	IRCBOT_TIMERATTR_ATTIME = 0x0002 | IRCBOT_TIMERATTR_ONCE, 
};

typedef int (ircbot_timer_cb_t)(ircbot *bot, const std::string &name, void *userdata);
struct ircbot_timer_info
{
	ircbot_timer_info()
		:id(-1), name(""), fxn(NULL), interval(0), next(0), attr(IRCBOT_TIMERATTR_NONE), userdata(NULL)
	{
	}
	ircbot_timer_info(const ircbot_timer_info &o)
		:id(o.id), name(o.name), fxn(o.fxn), interval(o.interval), next(o.next), attr(o.attr), userdata(o.userdata)
	{
	}
	ircbot_timer_info(int id, const std::string &name, ircbot_timer_cb_t *fxn, time_t interval, unsigned int attr, void *userdata)
		:id(id), name(name), fxn(fxn), interval(interval), attr(attr), userdata(userdata)
	{
		if((attr & IRCBOT_TIMERATTR_ATTIME) == IRCBOT_TIMERATTR_ATTIME)
			this->next = interval;
		else
			this->next = time(NULL) + interval/1000;
	}
	~ircbot_timer_info()
	{
	}
	
	int id;
	std::string name;
	ircbot_timer_cb_t *fxn;
	time_t interval, next; // next = crnttime + interval
	unsigned int attr;
	void *userdata;
};
////////////////////////////////////////////////////////////////////////////////////////////////


struct cmd_handler
{
	cmd_handler()
		:handler(NULL), userdata(NULL)
	{
	}
	cmd_handler(ircbot_cmd_handler_t *handler, void *userdata = NULL)
		:handler(handler), userdata(userdata)
	{
	}
	~cmd_handler(){}
	
	ircbot_cmd_handler_t *handler;
	void *userdata;
};

struct event_handler
{
	event_handler()
		:handler(NULL), priority(IRC_PRIORITY_NORMAL), userdata(NULL)
	{}
	event_handler(ircbot_event_handler_t *handler, 
		int priority = IRC_PRIORITY_NORMAL, void *userdata = NULL)
		:handler(handler), priority(priority), userdata(userdata)
	{}
	~event_handler(){}
	
	ircbot_event_handler_t *handler;
	int priority;
	void *userdata;
};


enum IRCEVTHREAD_STATE
{
	IRCEVTHREAD_STATE_IDLE = 0, 
	IRCEVTHREAD_STATE_WAITING, 
	IRCEVTHREAD_STATE_RUNNING, 
	IRCEVTHREAD_STATE_PAUSED, 
	IRCEVTHREAD_STATE_END
};
struct ircevent_thread_state
{
	ircevent_thread_state()
		:state(IRCEVTHREAD_STATE_IDLE), 
		evdata(NULL), 
		tid((pthread_t)-1), 
		bot(NULL), 
		kill_on(false)
	{
	}
	~ircevent_thread_state()
	{
	}
	
	// event thread must be killed by this fxn
	void kill(bool do_lock = true);
	
	volatile IRCEVTHREAD_STATE state;
	ircevent *evdata; // 처리중이 아닐 경우에는 NULL
	pthread_t tid;
	ircbot *bot;
	bool kill_on;
};




class ircbot_conn : public irc
{
	friend class ircbot;
	
public:
	ircbot_conn();
	~ircbot_conn();
	
	// prevents nickname highlighting by adding a dot between nicks
	std::string prevent_irc_highlighting_all(std::string chan, std::string msg);
	
	// irc::privmsg wrapper
	int privmsg_real(const irc_msginfo &chan, const std::string &msg);
	int privmsg(const irc_msginfo &chan, const std::string &msg);
	int privmsgf(const irc_msginfo &chan, const char *fmt, ...);
	int privmsg_nh(const irc_msginfo &chan, const std::string &msg);
	int privmsgf_nh(const irc_msginfo &chan, const char *fmt, ...);
	
	// launches irc thread, and connect
	bool launch_thread();
	
	bool quit(const std::string &quitmsg);
	bool disconnect();
	bool reconnect(const std::string &quitmsg);
	// kills ircbot_conn object from another object/thread
	bool force_kill();
	
	int get_connid() const {return this->conn_id;}
	const std::string &get_name() const {return this->name;}
	
	ircbot *bot;
	pthread_t main_thread_tid;
	volatile bool is_destructing;
	
private:
	int conn_id;
	std::string name;
	std::string nickbuf;
	volatile bool disconnect_on;
	volatile bool reconnect_on;
	// used in force_kill
	pthread_mutex_t connfxn_mutex;
	
	static void main_thread_cleanup(void *arg);
	static void *main_thread(void *data);
	int main_loop();
	
	void set_nick(const std::string &nick) {this->nickbuf = nick;}
	void set_name(const std::string &name) {this->name = name;}
};

// TODO
#if 0
class ircbot_cliconn: public ircbot_conn
{
public:
	ircbot_cliconn()
	{
	}
	virtual ~ircbot_cliconn()
	{
	}
};
enum IRCSRV_PROTOCOLS
{
	IRCSRV_PROTO_END
};
class ircbot_srvconn: public ircbot_conn
{
public:
	ircbot_srvconn()
	{
	}
	virtual ~ircbot_srvconn()
	{
	}
};
#endif


class ircbot
{
	friend class ircbot_conn;
	friend class ircevent_thread_state;
	
public:
	ircbot();
	~ircbot();
	
	int load_module(const std::string &name);
	int unload_module(const std::string &name);
	
	bool register_cmd(const std::string &name, ircbot_cmd_handler_t *handler, void *userdata = NULL);
	bool unregister_cmd(const std::string &name);
	const cmd_handler *get_cmd_handler(const std::string &name);
	
	bool register_event_handler(ircbot_event_handler_t *handler, 
		int priority = IRC_PRIORITY_NORMAL, void *userdata = NULL);
	bool unregister_event_handler(ircbot_event_handler_t *handler);
	int call_event_handlers(ircbot_conn *isrv, ircevent *ev);
	
	int add_timer(const std::string &name, ircbot_timer_cb_t *fxn, time_t interval, void *userdata = NULL, unsigned int attr = 0);
	int del_timer(const std::string &name, bool do_lock = true);
	int del_timer_by_cbptr(ircbot_timer_cb_t *fxn, bool do_lock = true);
	std::vector<ircbot_timer_info>::iterator find_timer(const std::string &name, bool do_lock = false);
	// std::vector<ircbot_timer_info>::const_iterator find_timer(const std::string &name, bool do_lock = false) const;
	std::vector<ircbot_timer_info>::iterator find_cbtimer(ircbot_timer_cb_t *fxn, bool do_lock = false);
	// std::vector<ircbot_timer_info>::const_iterator find_cbtimer(ircbot_timer_cb_t *fxn, bool do_lock = false) const;
	
	int is_module_loaded(const std::string &name)// const
	{
		return modules.find_by_name(name)?true:false;
	}
	
	int initialize();
	int cleanup();
	int run();
	// connects a bot
	int connect_1(const char *name);
	// (re) load config file
	int load_cfg_file(const std::string &path);
	// quits all bots and exit
	int quit_all(const std::string &quitmsg = "");
	// kills all bots and exit. should use quit_all instead.
	int force_kill_all();
	// disconnect all bots and exit (no quit message)
	int disconnect_all();
	// wait for all bots to be quit
	int wait();
	// create a (empty) ircbot_conn object
	ircbot_conn *create_irc_conn();
	// checks if ircbot_conn object is exists
	bool check_ircconn(ircbot_conn *isrv)
	{
		return (this->irc_conns.find_by_ptr(isrv) >= 0);
	}
	
	// returns true if there's a match in botadmin list
	bool is_botadmin(const std::string &ident);
	// returns true, and fills args if msg is bot command
	bool is_botcommand(const std::string &msg, std::string *cmd_trigger = NULL, std::string *cmd = NULL, std::string *carg = NULL);
	
	// enqueues evdata to bot event queue
	int add_to_event_queue(irc *src, ircevent *evdata);
	
	// lock/unlock functions
	int wrlock_cmd_handler_list(){pthread_rwlock_wrlock(&this->cmd_handler_list_lock); return 0;}
	int rdlock_cmd_handler_list(){pthread_rwlock_rdlock(&this->cmd_handler_list_lock); return 0;}
	int unlock_cmd_handler_list(){pthread_rwlock_unlock(&this->cmd_handler_list_lock); return 0;}
	int wrlock_event_handler_list(){pthread_rwlock_wrlock(&this->event_handler_list_lock); return 0;}
	int rdlock_event_handler_list(){pthread_rwlock_rdlock(&this->event_handler_list_lock); return 0;}
	int unlock_event_handler_list(){pthread_rwlock_unlock(&this->event_handler_list_lock); return 0;}
	int wrlock_timers(){pthread_rwlock_wrlock(&this->timer_lock); return 0;}
	int rdlock_timers(){pthread_rwlock_rdlock(&this->timer_lock); return 0;}
	int unlock_timers(){pthread_rwlock_unlock(&this->timer_lock); return 0;}
	int wrlock_cfgdata(){pthread_rwlock_wrlock(&this->cfgdata_lock); return 0;}
	int rdlock_cfgdata(){pthread_rwlock_rdlock(&this->cfgdata_lock); return 0;}
	int unlock_cfgdata(){pthread_rwlock_unlock(&this->cfgdata_lock); return 0;}
	
	int set_event_thread_cnt(int newcnt);
	int number_of_event_threads() const{return this->event_threads.getsize();}
	// pause한 만큼 unpause해줘야 스레드가 실행됨
	int pause_event_threads(bool pause, unsigned int options = 0);
	
	// this->irc_conns.rdlock(); NEEDED IF do_lock == false
	ircbot_conn *find_conn_by_name(const std::string &name, bool do_lock = true);
	
	// for debugging
	// do_lock: 0(nolock), 1(trylock), 2(lock)
	int print_bot_status(int do_lock = 2);
	// static helper function
	static const char *evthrd_state_to_string(IRCEVTHREAD_STATE n);
	
	module_list modules; // FIX.ME: should be private
	pobj_vector<ircbot_conn> irc_conns;
	
	lconfig cfg;
	
private:
	// ircbot command regexp
	pcrecpp::RE re_command;
	
	// event handler related
	std::vector<event_handler> event_handlers;
	caseless_strmap<std::string, cmd_handler> cmd_handlers;
	pthread_rwlock_t event_handler_list_lock, cmd_handler_list_lock;
	ircbot_conn dummy_ircbot_conn; // event 전달시 ircbot_conn *이 아닌 ircbot *만 필요할 경우 사용
	
	// timer related
	// MUST call sort_timers(), signal_timer_thread() after modifying timer list
public:
	std::vector<ircbot_timer_info> timers;
	int sort_timers();
	int signal_timer_thread();
private:
	pthread_rwlock_t timer_lock;
	pthread_t timer_thread_id;
	int timer_nextid;
	volatile IRCEVTHREAD_STATE timer_thread_state;
	volatile bool kill_timer_thread_on;
	static void *timer_thread(void *data);
	bool create_timer_thread();
	bool kill_timer_thread();
	
	// event thread related
	pobj_vector<ircevent_thread_state> event_threads;
	pthread_mutex_t event_queue_mutex;
	pthread_cond_t event_queue_cond; // signaled if new event is enqueued
	std::deque<std::pair<ircbot_conn *, ircevent *> > events;
	int clear_event_queue();
	static void event_thread_cleanup(void *arg);
	static void *event_thread(void *arg);
	void init_event_threads();
	int create_event_threads(int cnt);
	int kill_event_threads(); // kill all threads
	//int resize_event_threads(int cnt);
	// 0일 경우 이벤트 처리, 
	// 0이 아닐 경우 이벤트 처리 일시중지: 모듈 로드/언로드시 사용됨
	// this->event_thread_pause_level++; pthread_mutex_lock(&this->event_thread_pause_lock);
	// this->event_thread_pause_level--; pthread_mutex_unlock(&this->event_thread_pause_lock);
	volatile int event_thread_pause_level;
	//pthread_mutex_t event_thread_pause_lock; // recursive mutex
	pthread_t event_thread_lock_owner; // lock 된 경우 해당 스레드의 tid, 아닐 경우 0
	int event_cb_real(ircbot_conn *isrv, ircevent *evdata);
	static int event_cb(irc *irc_client, ircevent *evdata, void *userdata);
	
	pthread_rwlock_t cfgdata_lock; // 설정을 읽거나 쓸때 사용
	int next_connid;
	
	pthread_mutex_t pause_event_threads_fxn_lock;
	pthread_mutex_t set_event_thread_cnt_fxn_lock;
	
	// 평소에는 0, disconnect_all or quit_all 이 호출되었을 경우 호출된 시각
	time_t disconnect_all_ts;
	
public:
	int max_lines_per_cmd; // suggested max lines per cmd. XXX FIXME
	
	// 봇 통계 관련
	time_t start_ts; // 봇이 시작된 시간
	void update_statistics();
	time_t bot_statistics_last_update_time;
	struct
	{
		unsigned long long int total_event_cnt; // 봇이 처리한 총 이벤트 수
		float events_per_second;// 초당 이벤트 수(eps)
		/* TEMPVAR */ unsigned int evpersec_counter; // eps계산용 임시 변수
	} statistics;
	
	
};











#endif // IRCBOT_H_INCLUDED
