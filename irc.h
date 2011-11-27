#ifndef IRC_H_INCLUDED
#define IRC_H_INCLUDED

#include <list>
#include <deque>
#include <pthread.h>

#include "ircdefs.h"
#include "ircstring.h"
#include "ircutils.h"
#include "ircevent.h"
#include "encoding_converter.h"
#include "tcpsock.h"


#define IRC_IRCBOT_MOD // 필수: quit_handler에서 종료한 사람의 채널 목록을 얻기 위해 필요함


// internal struct
struct irc_sendq_data
{
	irc_sendq_data(const std::string &line, int priority)
		: line(line), priority(priority)
	{
	}
	~irc_sendq_data()
	{
	}
	std::string line;
	int priority;
};


class ircbot_conn;
class ircbot;

class irc
{
	friend class ircbot_conn;
	
public:
	irc();
	~irc();
	
	// initialize and connect to irc server.
	// change_nick -> set_username -> set_realname -> connect -> login_to_server 
	int connect(const std::string &server, int port);
	int disconnect();
	void set_event_callback(irc_event_cb_t *fxn, void *data = NULL);
	
	int quote(const char *line, int priority = IRC_PRIORITY_ANORMAL, bool convert_enc = true);
	int quote(const std::string &line, int priority = IRC_PRIORITY_ANORMAL, bool convert_enc = true);
	int quotef(const char *fmt, ...);
	int quotef(int priority, const char *fmt, ...);
	int quotef(int priority, bool convert_enc, const char *fmt, ...);
	
	int change_nick(const std::string &nick);
	int join_channel(const std::string &channel);
	int leave_channel(const std::string &channel, const std::string &reason = "");
	int main_loop();
	int quit(const std::string &quitmsg = "");
	int privmsg(const irc_msginfo &dest, const std::string &msg);
	
	int set_encoding(const std::string &enc);
	const std::string &get_encoding() const {return this->srv_encoding;} // may cause segfault?
	int set_username(const std::string &username){this->username = username; return 0;}
	int set_realname(const std::string &realname){this->realname = realname; return 0;}
	int set_server_pass(const std::string &server_pass){this->server_pass = server_pass; return 0;}

	void set_server(const std::string &host, int port = 6667) {this->server = host; this->port = port;}
	const std::string &get_server() const {return this->server;} // may cause segfault?
	int get_port() const {return this->port;}
	int get_status() const {return this->status;}
	
	const std::string &my_nick() const {return this->nick;} // may cause segfault?
	const std::vector<std::string> &get_chanlist() const {return this->chans;}
	irc_strmap<std::string, irc_user> &get_chanuserlist(const std::string &chan) {return this->chan_users[chan];}
	int refresh_chanuserlist(const std::string &chan);
	bool is_joined(const std::string &chan, bool autolock = true);
	bool is_user_joined(const std::string &chan, const std::string &nick, bool autolock = true);
	const irc_user *get_user_data(const std::string &chan, const std::string &nick);
	
	int rdlock_chanlist(){return pthread_rwlock_rdlock(&this->chanlist_lock);}
	int wrlock_chanlist(){return pthread_rwlock_wrlock(&this->chanlist_lock);}
	int unlock_chanlist(){return pthread_rwlock_unlock(&this->chanlist_lock);}
	int rdlock_chanuserlist(){return pthread_rwlock_rdlock(&this->chanuserlist_lock);}
	int wrlock_chanuserlist(){return pthread_rwlock_wrlock(&this->chanuserlist_lock);}
	int unlock_chanuserlist(){return pthread_rwlock_unlock(&this->chanuserlist_lock);}
	
	static int parse_irc_line(const std::string &line, std::string &prefix, std::string &cmd, std::vector<std::string> &params);
#ifdef IRC_IRCBOT_MOD
	static int ircbot_event_handler(ircbot_conn *isrv, ircevent *evdata, void *userdata);
	volatile int pause_irc_main_thread_on; // FIXME
#endif
	
	//////////////////
	tcpsock sock; // should be private

private:
	int status;
	int port;
	std::string server;
	std::string nick;
	std::string username, realname;
	std::string server_pass;
	time_t last_ping, last_pong;
	time_t quit_on_timer;
	
	std::vector<std::string> chans;
	irc_strmap<std::string, irc_strmap<std::string, irc_user> > chan_users;
	pthread_rwlock_t chanlist_lock, chanuserlist_lock;
	
public: // FIXME
	std::string srv_encoding; //
private:
	encconv *conv_m2s, *conv_s2m;
	pthread_rwlock_t encconv_lock;
	
	int send_event(ircevent *evdata);
	
	int add_to_chanlist(const std::string &chan); // return 0 if succeed
	int del_from_chanlist(const std::string &chan); // return 0 if succeed
	
	int add_to_chanuserlist(const std::string &chan, const std::string &nick, const std::string &ident = "");
	int del_from_chanuserlist(const std::string &chan, const std::string &nick);
	void del_from_chanuserlist_all(const std::string &nick);
	int clear_chanuserlist(const std::string &chan);
	
	irc_event_cb_t *event_cb;
	void *event_cb_userdata;

	int login_to_server();
	
	// send queue, flood protection related
	std::deque<irc_sendq_data> send_queue;
	pthread_t send_thread_id;
	pthread_cond_t send_cond;
	pthread_mutex_t send_mutex;
	volatile bool send_ready, sendq_added;
	static void *send_thread(void *arg);
	int create_send_thread();
	int kill_send_thread();
	int add_to_send_queue(const std::string &line, int priority);
	
public:
	struct
	{
		bool uhnames_on;
		bool namesx_on;
		std::string netname;
		std::string chantypes;
		std::string chanmodes;
		std::string chanusermode_prefixes;
		std::string chanusermodes;
		size_t max_channels;
		size_t nicklen;
		std::string casemapping;
		std::string charset;
	} srvinfo;
	bool preserve_eilseq_line; // preserve original line if invalid multibyte sequence is found
	bool enable_flood_protection;
};

#endif // IRC_H_INCLUDED

