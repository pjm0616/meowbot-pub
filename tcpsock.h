#ifndef TCPSOCK_H_INCLUDED
#define TCPSOCK_H_INCLUDED

#include "defs.h"

#include <string>
#include <vector>
#include <map>
#include <inttypes.h>

#include <pthread.h>
#include <poll.h>

#include "encoding_converter.h"




bool split_host_port(const std::string &hostport, std::string *host, int *port);

void set_default_sock_bind_address(const std::string &addr);
const std::string &get_sock_default_bind_address();



struct hostent *my_gethostbyname_r(const char *hostname, 
	struct hostent *he_result_buf, char *gh_buf, size_t gh_buf_size);
struct hostent *my_gethostbyname_tls(const char *hostname);
#ifndef HAVE_TLS___THREAD
# define my_gethostbyname(_hostname) \
	({ \
		struct hostent he_result_buf, *he; \
		char gh_buf[4096]; \
		he = my_gethostbyname_r((_hostname), &he_result_buf, gh_buf, sizeof(gh_buf)); \
		he; \
	})
#else
# define my_gethostbyname(_hostname) my_gethostbyname_tls(_hostname)
#endif
std::string my_gethostbyname_s(const char *hostname);
int my_gethostbyname4(const char *hostname, uint32_t *addr);





class tcpsock
{
public:
	tcpsock();
	tcpsock(const tcpsock &ts);
	~tcpsock();

	enum
	{
		SOCKSTATUS_DISCONNECTED,
		SOCKSTATUS_CONNECTED
	};
	enum
	{
		MODE_SOCK, 
		MODE_FD, 
		MODE_EXEC, 
		MODE_FILE, 
		MODE_SERVER, 
		MODE_END
	};
	enum
	{
		RDONLY = 1, 
		WRONLY = 2, 
		RDWR = 3, // RDONLY | WRONLY
		CREATE = 4
	};
	enum
	{
		DEFAULT_TIMEOUT = -2,
		INFINITE = -1
	};
	
	int setfd(int fd);
	int open(const std::string &filename, int openmode = RDWR);
	int execute(int (*cb_fxn)(void *arg), void *arg);
	int execute(const std::string &cmdline);
	int execute(const char *path, const char *argv[]);
	int connect_sock(const std::string &host, int port, int timeout); // internal fxn
	int connect(const std::string &host, int port, int timeout = -1);
	int listen(int port, int backlog);
	
	// mode == MODE_SERVER
	// 접속 성공시 *peer에 새로 할당된 이 객체 포인터가 들어있음
	int accept(tcpsock **peer, int timeout =-1);
	
	int close();
	int set_ndelay(bool onoff = true);
	
	int lock_read(int timeout = -1);
	int unlock_read();
	int lock_write(int timeout = -1);
	int unlock_write();
	
	// low-level read/write function
	// read_buffer를 사용하지 않음: timed_(read|write)* 를 사용
	int read_nolock(char *buf, size_t size);
	int read(char *buf, size_t size);
	int write_nolock(const char *buf, size_t size);
	int write(const char *buf, size_t size);
	
	// timeout == -1: infinite
	// timeout == -2: default timeout
	// call set_ndelay() first if timeout != -1
	// returns nread
	// Return values: 
	//  <0: socket error
	//  ==0: timed out
	//  >0: nread
	// read_buffer를 사용함
	int timed_read_nolock(char *dest, size_t size, int timeout = -1);
	int timed_read(char *dest, size_t size, int timeout = -1);
	int timed_write_nolock(const char *data, size_t size, int timeout = -1);
	int timed_write(const char *data, size_t size, int timeout = -1);
	// 정확히 size만큼 읽음. 타임아웃일 경우 read buffer에 넣고 0 리턴
	int timed_read_acc(char *dest, size_t size, int timeout = -1);
	
	// charset 변환
	// timeout: default_timeout
	int send(const char *data, size_t len, int timeout = tcpsock::DEFAULT_TIMEOUT);
	int send(const char *str, int timeout = tcpsock::DEFAULT_TIMEOUT);
	int send(const std::string &str, int timeout = tcpsock::DEFAULT_TIMEOUT);
	int printf(const char *format, ...);
	tcpsock &operator<<(const std::string &str);
	tcpsock &operator<<(const char *str);
	
	enum
	{
		READLINE_RESULT_TIMEOUT = -1, 
		READLINE_RESULT_SOCKERR = -2, 
	};
	// charset 변환
	// maxsize < 0: infinite
	// Return values: 
	//	==-2: socket error
	//	==-1: timeout
	// 	>0: nread
	int readline_delim(char delim, std::string *dest, int timeout = -1, int maxsize = 65535);
	int readline(std::string *dest, int timeout = -1, int maxsize = 65535);
	int readline(std::string &dest, int timeout = -1, int maxsize = 65535)
	{
		return this->readline(&dest, timeout, maxsize);
	}
	void operator>>(std::string &str);
	
	int get_status();
	
	int set_encoding(const std::string &srv, const std::string &me = "utf-8");
	void set_bind_address(const std::string &bind_addr);
	
	const std::string &get_last_error_str(){return this->lasterr_str;}
	const char *get_remote_addr(){return this->s_remote_addr;}
	
	int set_default_timeout(int timeout_value){return this->default_timeout = timeout_value;}
	int get_default_timeout(){return this->default_timeout;}
	
	size_t append_to_read_buffer(const char *data, size_t size);
	int clear_read_buffer();
	
	
	
//private:
	int fd; // FIXME
private:
	std::string linebuf;
	std::string enc_me, enc_srv;
	encconv *conv_m2s, *conv_s2m;
	std::string bind_address;
	
	int mode, status;
	int e_pid, e_sendp, e_recvp; // MODE_EXEC
	char s_remote_addr[16]; // MODE_SERVER
	
	int fdattr;
	
	pthread_mutex_t read_mutex, write_mutex;
	
	int last_errno;
	std::string lasterr_str;
	int set_last_error(const std::string &errmsg);
	int set_last_error_with_errno(const std::string &errmsg);
	
	int default_timeout;
	
	std::string read_buffer;
	pthread_mutex_t read_buffer_lock;
};



struct v_pollfd
{
public:
	v_pollfd()
		:size(0), 
		fdlist(NULL)
	{
		pthread_rwlock_init(&this->fdlist_lock, NULL);
	}
	v_pollfd(const v_pollfd &obj)
		:size(obj.size)
	{
		//fdlist = obj.duplicate_fdlist();
		fdlist = obj.fdlist; // FIXME
	}
	~v_pollfd()
	{
		if(this->fdlist)
			free(this->fdlist);
	}
	
	bool resize(size_t newsize, bool do_lock = true);
	bool check_size(size_t n, bool do_lock = true);
	int find_fd(int fd, bool do_lock = true);
	int add_fd(int fd, short events, bool do_lock = true);
	bool del_fd(int fd, bool do_lock = true);
	bool del_fd_close(int fd, bool do_lock = true);
	
	int poll(int timeout, bool do_lock = true);
	int clear(bool do_lock = true);
	int clear_closeall(bool do_lock = true);
	
	pollfd *operator[](int n)
	{
		if(unlikely(n < 0 || (size_t)n >= this->size))
			return NULL;
		return &this->fdlist[n];
	}
	
	inline void wrlock_fdlist() {pthread_rwlock_wrlock(&this->fdlist_lock);}
	inline void rdlock_fdlist() {pthread_rwlock_rdlock(&this->fdlist_lock);}
	inline int trywrlock_fdlist() {return pthread_rwlock_trywrlock(&this->fdlist_lock);}
	inline int tryrdlock_fdlist() {return pthread_rwlock_tryrdlock(&this->fdlist_lock);}
	inline void unlock_fdlist() {pthread_rwlock_unlock(&this->fdlist_lock);}
	
	size_t size;
	pollfd *fdlist;
	pthread_rwlock_t fdlist_lock;
	
};

struct v_pollfd_ts :public v_pollfd
{
public:
	v_pollfd_ts()
	{
	}
	v_pollfd_ts(const v_pollfd_ts &obj)
		:v_pollfd(obj)
	{
	}
	~v_pollfd_ts()
	{
	}
	
	int add_ts(tcpsock *ts, short events, bool do_lock = true);
	bool del_ts(tcpsock *ts, bool do_lock = true);
	bool del_ts_close(tcpsock *ts, bool do_lock = true);
	
	tcpsock *find_ts_by_fd(int fd, bool do_lock = true);
	tcpsock *find_ts_by_n(int n, bool do_lock = true);
	int clear(bool do_lock = true);
	int clear_closeall(bool do_lock = true);
	
	// fd, obj
	std::map<int, tcpsock *> tslist;
	
};










#endif // TCPSOCK_H_INCLUDED

