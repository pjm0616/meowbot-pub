/*
	Copyright (C) 2008 Park Jeong Min
	
	general i/o class
*/

#include <string>
#include <iostream>
#include <vector>
#include <map>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

#include <pthread.h>
#include <pcrecpp.h> // etc.h

#include "defs.h"
#include "encoding_converter.h"
#include "etc.h"
#include "tcpsock.h"


//#define TCP_PROTOCOL_NUMBER (getprotobyname("tcp")->p_proto)
#define TCP_PROTOCOL_NUMBER 6

#ifdef __CYGWIN__
#define pthread_mutex_timedlock2(mutex, timeout) pthread_mutex_lock(mutex)
#else
#define pthread_mutex_timedlock2(mutex, timeout) \
	({ \
		timeval tv; \
		gettimeofday(&tv, NULL); \
		timespec ts; \
		TIMEVAL_TO_TIMESPEC(&tv, &ts); \
		ts.tv_sec = 0; ts.tv_nsec += timeout * 1000000; \
		int ret = pthread_mutex_timedlock(mutex, &ts); \
		ret; \
	})
#endif



////////////////
bool split_host_port(const std::string &hostport, std::string *host, int *port)
{
	static pcrecpp::RE re_split_addr_port("^([^:]*+):([0-9]++)$");
	if(re_split_addr_port.FullMatch(hostport, host, port))
		return true;
	else
		return false;
}



static std::string g_default_bind_address("");
void set_default_sock_bind_address(const std::string &addr)
{
	g_default_bind_address = addr;
}
const std::string &get_default_sock_bind_address()
{
	return g_default_bind_address;
}





struct hostent *my_gethostbyname_r(const char *hostname, 
	struct hostent *he_result_buf, char *gh_buf, size_t gh_buf_size)
{
#ifdef __CYGWIN__
	return gethostbyname(hostname);
#else
	struct hostent *he_res;
	int gh_errno, ret;
	ret = gethostbyname_r(hostname, he_result_buf, gh_buf, gh_buf_size, &he_res, &gh_errno);
	if(ret < 0)
	{
		errno = gh_errno;
		return NULL;
	}
	return he_res;
#endif
}
struct hostent *my_gethostbyname_tls(const char *hostname)
{
#ifdef HAVE_TLS___THREAD
	static __thread struct hostent he_result_buf;
	static __thread char gh_buf[8192];
#else
	static struct hostent he_result_buf;
	static char gh_buf[8192];
#endif
	struct hostent *he;
	he = my_gethostbyname_r(hostname, &he_result_buf, gh_buf, sizeof(gh_buf));
	
	return he;
}

std::string my_gethostbyname_s(const char *hostname)
{
	struct hostent *he;
	char buf[16];
	
	he = my_gethostbyname(hostname);
	if(!he)
		return "";
	
	in_addr ia_tmp = {*(in_addr_t *)he->h_addr_list[0]};
	strncpy(buf, inet_ntoa(ia_tmp), sizeof(buf));
	buf[15] = 0;
	
	return buf;
}

int my_gethostbyname4(const char *hostname, uint32_t *addr)
{
	struct hostent he_result_buf, *he;
	char gh_buf[4096];
	he = my_gethostbyname_r(hostname, &he_result_buf, gh_buf, sizeof(gh_buf));
	if(!he)
		return -1;
	if(he->h_length != 4)
		return -2;
	*addr = *(uint32_t *)he->h_addr_list[0];
	return 0;
}





tcpsock::tcpsock()
	:	fd(-1),
		linebuf(""),
		enc_me(""),
		enc_srv(""),
		conv_m2s(NULL),
		conv_s2m(NULL), 
#ifndef STATIC_MODULES // @@@@@@@@@@@@@@@@@@@@@@@@@@ FIXME @@@@@@@@@@@@@@@@@
		bind_address(g_default_bind_address), //
#endif
		mode(MODE_SOCK), 
		status(SOCKSTATUS_DISCONNECTED), 
		fdattr(0), 
		default_timeout(-1)
{
	pthread_mutex_init(&this->read_mutex, NULL);
	pthread_mutex_init(&this->write_mutex, NULL);
	pthread_mutex_init(&this->read_buffer_lock, NULL);
	
	this->s_remote_addr[0] = 0;
}

tcpsock::tcpsock(const tcpsock &ts)
{
	this->fd = ts.fd;
	this->linebuf = "";
	this->set_encoding(ts.enc_srv, ts.enc_me);
	this->bind_address = ts.bind_address;
	this->mode = ts.mode;
	this->status = ts.status;
	this->e_pid = ts.e_pid;
	this->e_sendp = ts.e_sendp;
	this->e_recvp = ts.e_recvp;
	this->fdattr = ts.fdattr;
	
	this->default_timeout = ts.default_timeout;
	
	memcpy(this->s_remote_addr, ts.s_remote_addr, sizeof(this->s_remote_addr));
	
	pthread_mutex_init(&read_mutex, NULL);
	pthread_mutex_init(&write_mutex, NULL);
	pthread_mutex_init(&this->read_buffer_lock, NULL);
}

tcpsock::~tcpsock()
{
	this->close();
	if(this->conv_m2s)
	{
		delete this->conv_m2s;
		this->conv_m2s = NULL;
	}
	if(this->conv_s2m)
	{
		delete this->conv_s2m;
		this->conv_s2m = NULL;
	}
}

int tcpsock::setfd(int fd)
{
	if(unlikely(this->status == SOCKSTATUS_CONNECTED))
		return -1;
	
	this->fd = fd;
	this->mode = MODE_FD;
	this->status = SOCKSTATUS_CONNECTED;
	this->set_ndelay(true);

	return 0;
}

int tcpsock::open(const std::string &filename, int openmode /*=RDWR*/)
{
	int fd, mode;
	
	if(unlikely(this->status == SOCKSTATUS_CONNECTED))
		return -1;
	
	mode = 0;
	if((openmode & RDWR) == O_RDWR) mode |= O_RDWR;
	else
	{
		if(openmode & RDONLY) mode |= O_RDONLY;
		if(openmode & WRONLY) mode |= O_WRONLY;
	}
	if(openmode & CREATE) mode |= O_CREAT;
	
	fd = ::open(filename.c_str(), mode);
	if(fd < 0)
	{
		this->set_last_error_with_errno("open() failed: ");
		return -1;
	}
	
	this->fd = fd;
	this->mode = MODE_FILE;
	this->status = SOCKSTATUS_CONNECTED;
	this->set_ndelay(true);

	return 0;
}

int tcpsock::execute(int (*cb_fxn)(void *arg), void *arg)
{
	int pid, sendp[2], recvp[2];
	
	if(unlikely(this->status == SOCKSTATUS_CONNECTED))
		return -1;
	
	pipe(sendp);
	pipe(recvp);
	
	pid = fork();
	if(pid < 0)
	{
		this->set_last_error_with_errno("fork() failed: ");
		::close(sendp[0]);
		::close(sendp[1]);
		::close(recvp[0]);
		::close(recvp[1]);
		
		return -1;
	}
	else if(pid == 0) // child
	{
		::close(sendp[1]);
		::close(recvp[0]);
		
		dup2(sendp[0], 0);
		dup2(recvp[1], 1);
		dup2(recvp[1], 2);
		
		cb_fxn(arg);
		
		::close(sendp[0]);
		::close(recvp[1]);
		
		exit(0);
	}
	::close(sendp[0]);
	::close(recvp[1]);
	
	this->e_sendp = sendp[1];
	this->e_recvp = recvp[0];
	this->fd = recvp[0]; // 
	this->e_pid = pid;
	this->mode = MODE_EXEC;
	this->status = SOCKSTATUS_CONNECTED;
	this->set_ndelay(true);
	
	return 0;
}

static int execute_cb_system(void *cmdline)
{
	return system(static_cast<const char *>(cmdline));
}
int tcpsock::execute(const std::string &cmdline)
{
	return this->execute(execute_cb_system, (void *)cmdline.c_str());
}

static int execute_cb_execv(void *arg)
{
#if 0
	int pid = fork();
	if(pid < 0)
		return -1;
	else if(pid == 0)
	{
		char **argv = reinterpret_cast<char **>(arg);
		const char *path = argv[0];
		
		execv(path, (char *const *)argv);
		perror("execv");
		exit(EXIT_FAILURE);
	}
	else
	{
		int status;
		wait(&status);
		
		return status;
	}
#else
	char **argv = reinterpret_cast<char **>(arg);
	const char *path = argv[0];
	execv(path, (char *const *)argv);
	perror("execv");
	exit(EXIT_FAILURE);
#endif
}
int tcpsock::execute(const char *path, const char *argv[])
{
	int ret;
	const char *orig_argv0 = argv[0];
	
	argv[0] = const_cast<char *>(path);
	ret = this->execute(execute_cb_execv, (void *)(argv));
	argv[0] = orig_argv0;
	
	return ret;
}

int tcpsock::connect_sock(const std::string &host, int port, int timeout)
{
	int sock, ret;
	struct sockaddr_in sa;
	struct hostent *he;
	
	if(unlikely(this->status == SOCKSTATUS_CONNECTED))
		return -1;
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	
	he = my_gethostbyname(host.c_str());
	if(!he)
	{
		this->set_last_error_with_errno("gethostbyname() failed: ");
		return -1;
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);
	
	sock = socket(PF_INET, SOCK_STREAM, TCP_PROTOCOL_NUMBER);
	if(sock < 0)
	{
		this->set_last_error_with_errno("socket() failed: ");
		return -1;
	}
	
	if(!this->bind_address.empty())
	{
		struct sockaddr_in sa2;
		memset(&sa2, 0, sizeof(sa2));
		sa2.sin_family = AF_INET;
		sa2.sin_addr.s_addr = inet_addr(this->bind_address.c_str());
		if(bind(sock, (struct sockaddr *)&sa2, sizeof(sa2)) < 0)
		{
			this->set_last_error_with_errno("bind() failed: ");
			::close(sock);
			return -1;
		}
	}
	
	this->fd = sock; // HACK!!!
	this->mode = MODE_SOCK; // HACK!!!
	this->status = SOCKSTATUS_CONNECTED; // HACK!!!
	if(timeout >= 0)
		this->set_ndelay(true);
	this->status = SOCKSTATUS_DISCONNECTED; // HACK!!!
	this->fd = -1;
	
	ret = ::connect(sock, reinterpret_cast<const struct sockaddr *>(&sa), 
		sizeof(struct sockaddr_in));
	if(ret < 0)
	{
		if(errno == EINPROGRESS) // timeout >= 0
		{
			pollfd fdlist;
			fdlist.fd = sock;
			fdlist.events = POLLIN | POLLOUT;
			ret = poll(&fdlist, 1, timeout);
			if(ret < 0)
			{
				this->set_last_error_with_errno("poll() failed: ");
				::close(sock);
				return -1;
			}
			else if(ret == 0)
			{
				errno = ETIMEDOUT;
				this->set_last_error_with_errno("delayed connect() failed: ");
				::close(sock);
				return -1;
			}
			else
			{
				socklen_t ilen = sizeof(int);
				if(getsockopt(sock, SOL_SOCKET, SO_ERROR, &ret, &ilen) < 0)
				{
					this->set_last_error_with_errno("getsockopt() failed: ");
					::close(sock);
					return -1;
				}
				else
				{
					if(ret != 0)
					{
						this->set_last_error_with_errno(
							"delayed connect() failed: ");
						::close(sock);
						return -1;
					}
				}
			}
		}
		else
		{
			this->set_last_error_with_errno("connect() failed: ");
			::close(sock);
			return -1;
		}
	}
	
	this->fd = sock;
	this->mode = MODE_SOCK;
	this->status = SOCKSTATUS_CONNECTED;
	if(timeout < 0)
		this->set_ndelay(true);
	
	return 0;
}


int tcpsock::connect(const std::string &host, int port, int timeout /*=-1*/)
{
	int ret;
	ret = this->connect_sock(host, port, timeout);
	return ret;
}

int tcpsock::listen(int port, int backlog)
{
	int sock, ret;
	struct sockaddr_in sa;
	
	if(unlikely(this->status == SOCKSTATUS_CONNECTED))
		return -1;
	
	sock = ::socket(PF_INET, SOCK_STREAM, TCP_PROTOCOL_NUMBER);
	if(sock < 0)
	{
		this->set_last_error_with_errno("socket() failed: ");
		
		return -1;
	}
	
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	if(this->bind_address.empty())
		sa.sin_addr.s_addr = 0;
	else
		sa.sin_addr.s_addr = inet_addr(this->bind_address.c_str());
	sa.sin_port = htons(port);
	if(::bind(sock, (struct sockaddr *)&sa, sizeof(sa))<0)
	{
		this->set_last_error_with_errno("bind() failed: ");
		
		::close(sock);
		return -1;
	}
	
	ret = ::listen(sock, backlog);
	if(ret < 0)
	{
		this->set_last_error_with_errno("listen() failed: ");
		
		return -1;
	}
	
	this->fd = sock;
	this->mode = MODE_SERVER;
	this->status = SOCKSTATUS_CONNECTED;
	this->set_ndelay(true);
	
	return 0;
}

int tcpsock::set_ndelay(bool onoff /*=true*/)
{
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	
	if(onoff)
	{
		this->fdattr |= O_NDELAY;
		if(this->mode == MODE_EXEC)
		{
			fcntl(this->e_recvp, F_SETFL, 
				fcntl(this->e_recvp, F_GETFL, 0) | O_NDELAY);
			// fcntl(this->e_sendp, F_SETFL, 
			//	fcntl(this->e_sendp, F_GETFL, 0) | O_NDELAY);
		}
		else if(mode == MODE_SERVER)
		{
			fcntl(this->fd, F_SETFL, 
				fcntl(this->fd, F_GETFL, 0) | O_NDELAY);
		}
		else
		{
			fcntl(this->fd, F_SETFL, 
				fcntl(this->fd, F_GETFL, 0) | O_NDELAY);
		}
	}
	else
	{
		this->fdattr &= ~O_NDELAY;
		if(this->mode == MODE_EXEC)
		{
			fcntl(this->e_recvp, F_SETFL, 
				fcntl(this->e_recvp, F_GETFL, 0) & ~O_NDELAY);
			fcntl(this->e_sendp, F_SETFL, 
				fcntl(this->e_sendp, F_GETFL, 0) & ~O_NDELAY);
		}
		else if(mode == MODE_SERVER)
		{
			fcntl(this->fd, F_SETFL, 
				fcntl(this->fd, F_GETFL, 0) & ~O_NDELAY);
		}
		else
		{
			fcntl(this->fd, F_SETFL, 
				fcntl(this->fd, F_GETFL, 0) & ~O_NDELAY);
		}
	}
	
	return 0;
}

int tcpsock::close()
{
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	
	if(this->mode == MODE_EXEC)
	{
		kill(this->e_pid, SIGKILL);
		::close(this->e_recvp);
		::close(this->e_sendp);
		this->e_recvp = -1;
		this->e_sendp = -1;
		fflush(stdout); //
		fflush(stderr); //
	}
	else if(this->mode == MODE_SERVER)
	{
		::close(this->fd);
		this->fd = -1;
	}
	else
	{
		::close(this->fd);
		this->fd = -1;
	}
	this->s_remote_addr[0] = 0; // MODE_SERVER가 만든 MODE_SOCK일 경우
	this->read_buffer.clear();
	this->mode = MODE_SOCK;
	this->status = SOCKSTATUS_DISCONNECTED;
	
	return 0;
}

int tcpsock::read_nolock(char *buf, size_t size)
{
	int ret;
	
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	
	if(this->mode == MODE_EXEC)
		ret = ::read(this->e_recvp, buf, size);
	else if(this->mode == MODE_SERVER)
	{
		ret = -1;
		errno = EINVAL; // XXX
	}
	else
		ret = ::read(this->fd, buf, size);
	
	if(ret < 0)
	{
		this->set_last_error_with_errno("read() failed: ");
	}
	
	return ret;
}

int tcpsock::read(char *buf, size_t size)
{
	int ret;
	
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	
	ret = this->lock_read((this->fdattr & O_NDELAY) ? 0 : -1);
	if(ret < 0)
		return -1;
	ret = this->read_nolock(buf, size);
	this->unlock_read();
	
	return ret;
}

int tcpsock::write_nolock(const char *buf, size_t size)
{
	int ret;
	
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	
	if(this->mode == MODE_EXEC)
		ret = ::write(this->e_sendp, buf, size);
	else if(this->mode == MODE_SERVER)
	{
		ret = -1;
		errno = EINVAL; // XXX
	}
	else
		ret = ::write(this->fd, buf, size);
	
	if(ret < 0)
	{
		this->set_last_error_with_errno("write() failed: ");
	}
	
	return ret;
}

int tcpsock::write(const char *buf, size_t size)
{
	int ret;
	
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	
	ret = this->lock_write((this->fdattr & O_NDELAY) ? 0 : -1);
	if(ret < 0)
		return -1;
	ret = this->write_nolock(buf, size);
	this->unlock_write();
	
	return ret;
}

int tcpsock::accept(tcpsock **peer, int timeout)
{
	pollfd fdlist;
	int ret, client;
	struct sockaddr_in csa;
	socklen_t sl;
	
	if(this->status != SOCKSTATUS_CONNECTED || this->mode != MODE_SERVER)
	{
		return -1;
	}
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	
	fdlist.fd = this->fd;
	fdlist.events = POLLIN;
	ret = poll(&fdlist, 1, timeout);
	if(ret < 0)
	{
		this->last_errno = errno;
		this->set_last_error_with_errno("poll() failed: ");
		
		this->close();
		return -1;
	}
	else if(ret == 0)
	{
		this->last_errno = ETIMEDOUT;
		return 0;
	}
	else
	{
		sl = sizeof(struct sockaddr_in);
		client = ::accept(this->fd, 
			reinterpret_cast<sockaddr *>(&csa), &sl);
		if(client < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return 0;
			}
			else
			{
				this->last_errno = errno;
				this->set_last_error_with_errno("accept() failed: ");
				
				this->close();
				return -1;
			}
		}
		else
		{
			tcpsock *client_sock = new tcpsock;
			client_sock->mode = MODE_SOCK;
			client_sock->status = SOCKSTATUS_CONNECTED;
			client_sock->fd = client;
			strncpy(client_sock->s_remote_addr, inet_ntoa(csa.sin_addr), 
				sizeof(client_sock->s_remote_addr));
			client_sock->s_remote_addr[15] = 0;
			client_sock->set_ndelay(true);
			*peer = client_sock;
			return 1;
		}
	}
	
	return -1; /* never reached */
}

// returns read size(>0) on success, 0 if timed out, -1 if failed
int tcpsock::timed_read_nolock(char *dest, size_t size, int timeout)
{
	pollfd fdlist;
	int nread;

	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	if(size == 0)
		return 0; // FIXME
	
	pthread_mutex_lock(&this->read_buffer_lock);
	if(!this->read_buffer.empty())
	{
		const char *p = this->read_buffer.data();
		size_t bufsize = this->read_buffer.size();
		if(bufsize >= size)
			nread = size;
		else // bufsize < size
			nread = bufsize;
		assert(nread > 0);
		memcpy(dest, p, nread);
		p += nread;
		
		if(bufsize > (size_t)nread)
			this->read_buffer.assign(p, bufsize - nread);
		else if(bufsize == (size_t)nread)
			this->read_buffer.clear();
		
		pthread_mutex_unlock(&this->read_buffer_lock);
		return nread;
	}
	pthread_mutex_unlock(&this->read_buffer_lock);
	
	if(this->mode == MODE_EXEC)
		fdlist.fd = this->e_recvp;
	else
		fdlist.fd = this->fd;
	if(fdlist.fd < 0)
	{
		errno = EINVAL;
		set_last_error_with_errno("sock < 0: not connected");
		return -1;
	}
	
	fdlist.events = POLLIN;
	nread = poll(&fdlist, 1, timeout);
	if(nread == 0)
	{
		return 0;
	}
	else if(nread < 0)
	{
		if(errno == EINTR)
			return 0;
		
		set_last_error_with_errno("poll()");
		this->close();
		
		return -1;
	}
	else
	{
		// if(fdlist.revents & (POLLERR | POLLHUP | POLLNVAL))
		if((fdlist.revents & (POLLERR | POLLHUP | POLLNVAL)) && 
			!(fdlist.revents & POLLIN))
		{
			// This cannot happen (output only)
			this->close();
			return -1;
		}
#ifdef POLLRDHUP
		if(fdlist.revents & POLLRDHUP)
		{
			this->set_last_error(
				"Connection reset by peer (POLLRDHUP received)");
			this->close();
			return -1;
		}
#endif
		else if(fdlist.revents & POLLIN)
		{
			nread = this->read_nolock(dest, size);
			if(nread < 0)
			{
				if(this->last_errno == EAGAIN || this->last_errno == EINTR || 
					this->last_errno == EWOULDBLOCK || 
					this->last_errno == ETIMEDOUT)
				{
					return 0;
				}
				else if(this->last_errno == ENOTCONN)
				{
					printf("timed_read(): Not connected");
				}
				else // ECONNRESET, etc
				{
					this->close();
					return -1;
				}
			}
			else if(nread == 0)
			{
				this->set_last_error_with_errno("EOF (nread == 0)");
				this->close();
				return -1;
			}
		}
	}
	
	return nread;
}

int tcpsock::timed_read(char *dest, size_t size, int timeout)
{
	int ret;

	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	
	ret = this->lock_read(timeout);
	if(ret < 0)
		return 0; // timed out
	ret = this->timed_read_nolock(dest, size, timeout);
	this->unlock_read();
	
	return ret;
}

// returns written size(>0) on success, 0 if timed out, -1 if failed
int tcpsock::timed_write_nolock(const char *data, size_t size, int timeout)
{
	pollfd fdlist;
	int ret;
	
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	if(size == 0)
		return 0; // FIXME
	
	//////// -_- HACK
	if(this->mode == MODE_EXEC)
	{
		ret = this->write_nolock(data, size);
		if(ret < 0)
		{
			if(this->last_errno == EAGAIN || this->last_errno == EINTR || 
				this->last_errno == EWOULDBLOCK || 
				this->last_errno == ETIMEDOUT)
			{
				return 0;
			}
			else // EPIPE, ECONNRESET, etc
			{
				this->close();
				return -1;
			}
		}
		return ret;
	}
	
	if(this->mode == MODE_EXEC)
		fdlist.fd = this->e_recvp;
	else
		fdlist.fd = this->fd;
	if(fdlist.fd < 0)
	{
		errno = EINVAL;
		set_last_error_with_errno("sock < 0: not connected");
		return -1;
	}
	
	fdlist.events = POLLOUT;
	ret = poll(&fdlist, 1, timeout);
	if(ret == 0)
	{
		return 0;
	}
	else if(ret < 0)
	{
		if(errno == EINTR)
			return 0;
		
		set_last_error_with_errno("poll()");
		this->close();
		
		return -1;
	}
	else
	{
		// if(fdlist.revents & (POLLERR | POLLHUP | POLLNVAL))
		if((fdlist.revents & (POLLERR | POLLHUP | POLLNVAL)) && 
			!(fdlist.revents & POLLOUT))
		{
			//return 0;
			this->close();
			return -1;
		}
		else if(fdlist.revents & POLLOUT)
		{
			ret = this->write_nolock(data, size);
			if(ret < 0)
			{
				if(this->last_errno == EAGAIN || this->last_errno == EINTR || 
					this->last_errno == EWOULDBLOCK || 
					this->last_errno == EBUSY)
				{
					return 0;
				}
				else // EPIPE, ECONNRESET, etc
				{
					this->close();
					return -1;
				}
			}
			else if(ret == 0)
			{
				this->close();
				return -1;
			}
		}
	}
	// assert(ret > 0);
	if((size_t)ret < size)
	{
		// 처리 귀찮음=_=
		fprintf(stderr, "WARNING: timed_write: ret(%d) < size(%d)\n", ret, size);
	}
	
	return ret;
}

int tcpsock::timed_write(const char *data, size_t size, int timeout)
{
	int ret;

	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	
	ret = this->lock_write(timeout);
	if(ret < 0)
		return 0; // timed out
	ret = this->timed_write_nolock(data, size, timeout);
	this->unlock_write();
	
	return ret;
}

int tcpsock::timed_read_acc(char *dest, size_t size, int timeout)
{
	int ret;

	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	
	ret = this->lock_read(timeout);
	if(ret < 0)
		return 0; // timed out
	
	time_t starttime = time(NULL);
	size_t totalread = 0;
	do
	{
		ret = this->timed_read_nolock(dest, size - totalread, timeout);
		if(ret <= 0)
			break;
		totalread += ret;
		if(totalread > size)
		{
			fprintf(stderr, "ERROR: timed_read_acc: totalread(%d) > size(%d)\n", totalread, size);
			totalread = size;
		}
		else if(totalread == size)
		{
			break;
		}
	} while(time(NULL)-starttime <= timeout/1000);
	if(totalread < size)
	{
		if(this->status == SOCKSTATUS_CONNECTED)
		{
			this->append_to_read_buffer(dest, totalread);
			ret = 0;
		}
		else
			ret = -1;
	}
	this->unlock_read();
	
	return ret;
}


int tcpsock::send(const char *data, size_t len, int timeout)
{
	int ret;
	char *newdata = NULL;
	
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -1;
	if(timeout == DEFAULT_TIMEOUT)
		timeout = this->default_timeout;
	
	if(this->conv_m2s)
	{
		newdata = this->conv_m2s->convert(data, len);
		len = strlen(newdata);
	}
	
	ret = this->timed_write(newdata?newdata:data, len, timeout);
	if(ret < 0)
	{
		if(this->last_errno == EAGAIN || this->last_errno == EWOULDBLOCK || 
			this->last_errno == EINTR)
		{
			ret = 0;
			goto end;
		}
		else
		{
			fprintf(stderr, "write() failed: %s\n", this->lasterr_str.c_str());
			this->close();
			ret = -1;
			goto end;
		}
	}
	else if(ret != (int)len)
	{
		std::cerr << "write() error: size not matched" << std::endl;
		// this->close();
		// ret = -1;
		// goto end;
	}
	
end:
	if(newdata)
		delete[] newdata;
	
	return ret;
}

int tcpsock::send(const char *str, int timeout)
{
	return this->send(str, strlen(str), timeout);
}

int tcpsock::send(const std::string &str, int timeout)
{
	return this->send(str.c_str(), str.size(), timeout);
}

int tcpsock::printf(const char *format, ...)
{
	va_list ap;
	char *dest = NULL, buf[1024];
	int len, ret;
	
	va_start(ap, format);
	len = vsnprintf(buf, sizeof(buf), format, ap);
	if(len >= (int)sizeof(buf))
	{
		dest = new char[len + 1];
		len = vsnprintf(dest, len + 1, format, ap);
		ret = this->send(dest, len);
		delete[] dest;
	}
	else
		ret = this->send(buf, len);
	va_end(ap);
	
	return ret;
}

tcpsock &tcpsock::operator<<(const std::string &str)
{
	this->send(str);

	return *this;
}

tcpsock &tcpsock::operator<<(const char *str)
{
	this->send(str, strlen(str));

	return *this;
}

size_t tcpsock::append_to_read_buffer(const char *data, size_t size)
{
	pthread_mutex_lock(&this->read_buffer_lock);
	this->read_buffer.append(data, size);
	pthread_mutex_unlock(&this->read_buffer_lock);
	return size;
}

int tcpsock::clear_read_buffer()
{
	pthread_mutex_lock(&this->read_buffer_lock);
	this->read_buffer.clear();
	pthread_mutex_unlock(&this->read_buffer_lock);
	return 0;
}


int tcpsock::readline_delim(char delim, std::string *dest, int timeout, int maxsize)
{
	int result = 0;
	int ret;
	char buf[512];
	
	if(dest)
		dest->clear();
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -2; // socket error
	
	ret = this->lock_read(timeout);
	if(ret < 0)
		return -1; // timed out
	
	if(maxsize < 0)
		maxsize = INT_MAX;
	
	while(1)
	{
		ret = this->timed_read_nolock(buf, sizeof(buf), timeout);
		if(ret < 0) // socket error
		{
			if(!this->linebuf.empty()) // 마지막 줄까지 읽음
			{
				break;
			}
			else
			{
				if(dest)
					dest->clear();
				result = -2; // socket error
				goto end;
			}
		}
		else if(ret == 0) // timed out
		{
			if(dest) dest->clear();
			result = -1; // timed out
			goto end;
		}
		assert((ret > 0) && ((size_t)ret <= sizeof(buf)));
		
		for(int i = 0; i < ret; i++)
		{
			//if(buf[i] == '\r' || buf[i] == '\0')
			//	continue;
			if(buf[i] == delim || 
				(/*maxsize >= 0 && */this->linebuf.size() > (size_t)maxsize))
			{
				i++;
				int remained = ret - i;
				assert(remained >= 0);
				if(remained > 0)
					this->append_to_read_buffer(&buf[i], remained);
				
				goto read_finished;
			}
			else
			{
				this->linebuf += buf[i];
			}
		}
	}
read_finished:

	if(this->conv_s2m)
	{
		if(dest)
			*dest = this->conv_s2m->convert(this->linebuf);
	}
	else
	{
		if(dest)
			*dest = this->linebuf;
	}
	result = (int)this->linebuf.size();
	this->linebuf.clear();
	
end:
	this->unlock_read();
	
	return result;
}

// FIXME: 중복 코드 제거
int tcpsock::readline(std::string *dest, int timeout, int maxsize)
{
	int result = 0;
	int ret;
	char buf[512];
	
	if(dest)
		dest->clear();
	if(unlikely(this->status != SOCKSTATUS_CONNECTED))
		return -2; // socket error
	
	ret = this->lock_read(timeout);
	if(ret < 0)
		return -1; // timed out
	
	if(maxsize < 0)
		maxsize = INT_MAX;
	
	while(1)
	{
		ret = this->timed_read_nolock(buf, sizeof(buf), timeout);
		if(ret < 0) // socket error
		{
			if(!this->linebuf.empty()) // 마지막 줄까지 읽음
			{
				break;
			}
			else
			{
				if(dest)
					dest->clear();
				result = -2; // socket error
				goto end;
			}
		}
		else if(ret == 0) // timed out
		{
			if(dest) dest->clear();
			result = -1; // timed out
			goto end;
		}
		assert((ret > 0) && ((size_t)ret <= sizeof(buf)));
		
		for(int i = 0; i < ret; i++)
		{
			if(buf[i] == '\r' || buf[i] == '\0')
				continue;
			else if(buf[i] == '\n' || 
				(/*maxsize >= 0 && */this->linebuf.size() > (size_t)maxsize))
			{
				i++;
				int remained = ret - i;
				assert(remained >= 0);
				if(remained > 0)
					this->append_to_read_buffer(&buf[i], remained);
				
				goto read_finished;
			}
			else
			{
				this->linebuf += buf[i];
			}
		}
	}
read_finished:

	if(this->conv_s2m)
	{
		if(dest)
			*dest = this->conv_s2m->convert(this->linebuf);
	}
	else
	{
		if(dest)
			*dest = this->linebuf;
	}
	result = (int)this->linebuf.size();
	this->linebuf.clear();
	
end:
	this->unlock_read();
	
	return result;
}

void tcpsock::operator>>(std::string &str)
{
	while(this->readline(str) == -1){} // break on timeout
}

int tcpsock::get_status()
{
	return this->status;
}

int tcpsock::set_encoding(const std::string &srv, const std::string &me)
{
	this->enc_me = me;
	this->enc_srv = srv;
	if(this->conv_m2s)
	{
		delete this->conv_m2s;
		this->conv_m2s = NULL;
	}
	if(this->conv_s2m)
	{
		delete this->conv_s2m;
		this->conv_s2m = NULL;
	}
	if(!me.empty() && !srv.empty())
	{
		this->conv_m2s = new encconv(me, srv);
		this->conv_s2m = new encconv(srv, me);
	}
	
	return 0;
}

void tcpsock::set_bind_address(const std::string &bind_addr)
{
	this->bind_address = bind_addr;
}


int tcpsock::set_last_error(const std::string &errmsg)
{
	this->lasterr_str = errmsg;
	return 0;
}

int tcpsock::set_last_error_with_errno(const std::string &errmsg)
{
	this->last_errno = errno;
	this->lasterr_str = errmsg + ": " + int2string(this->last_errno) + 
		"(" + strerror(this->last_errno) + ")";
	return 0;
}

int tcpsock::lock_read(int timeout /*=-1*/)
{
	int ret;
	
	if(timeout < 0)
		ret = pthread_mutex_lock(&this->read_mutex);
	else if(timeout == 0)
		ret = pthread_mutex_trylock(&this->read_mutex);
	else
		ret = pthread_mutex_timedlock2(&this->read_mutex, timeout);
	if(ret < 0)
	{
		if(errno == EAGAIN || /* maximum number recursive locks exceeded */
			errno == EBUSY || /* already locked - pthread_mutex_trylock */
			errno == ETIMEDOUT)
		{
			errno = EAGAIN;
			this->set_last_error_with_errno("lock_read - pthread_mutex_lock");
			return -1;
		}
	}
	
	return 0;
}

int tcpsock::unlock_read()
{
	pthread_mutex_unlock(&this->read_mutex);
	return 0;
}

int tcpsock::lock_write(int timeout /*=-1*/)
{
	int ret;
	
	if(timeout < 0)
		ret = pthread_mutex_lock(&this->write_mutex);
	else if(timeout == 0)
		ret = pthread_mutex_trylock(&this->write_mutex);
	else
		ret = pthread_mutex_timedlock2(&this->write_mutex, timeout);
	if(ret < 0)
	{
		if(errno == EAGAIN || /* maximum number recursive locks exceeded */
			errno == EBUSY || /* already locked - pthread_mutex_trylock */
			errno == ETIMEDOUT)
		{
			errno = EAGAIN;
			this->set_last_error_with_errno("lock_write - pthread_mutex_lock");
			return -1;
		}
	}
	
	return 0;
}

int tcpsock::unlock_write()
{
	pthread_mutex_unlock(&this->write_mutex);
	return 0;
}












bool v_pollfd::resize(size_t newsize, bool do_lock)
{
	if(do_lock) this->wrlock_fdlist();
	if(newsize == 0)
	{
		if(this->fdlist)
			free(this->fdlist);
		this->fdlist = NULL;
		this->size = 0;
		if(do_lock) this->unlock_fdlist();
		return true;
	}
	this->fdlist = (pollfd *)realloc(this->fdlist, sizeof(pollfd) * newsize);
	if(!this->fdlist)
	{
		this->size = 0;
		if(do_lock) this->unlock_fdlist();
		return false;
	}
	if(newsize > this->size)
	{
		for(size_t i = this->size; i < newsize; i++)
		{
			fdlist[i].fd = -1;
			fdlist[i].events = 0;
		}
	}
	this->size = newsize;
	if(do_lock) this->unlock_fdlist();
	return true;
}
bool v_pollfd::check_size(size_t n, bool do_lock)
{
	if(do_lock) this->wrlock_fdlist();
	if(this->size < n)
	{
		this->resize(n, false);
		if(do_lock) this->unlock_fdlist();
		return false;
	}
	if(do_lock) this->unlock_fdlist();
	return true;
}
int v_pollfd::find_fd(int fd, bool do_lock)
{
	if(do_lock) this->rdlock_fdlist();
	for(size_t i = 0; i < this->size; i++)
	{
		if(this->fdlist[i].fd == fd)
		{
			if(do_lock) this->unlock_fdlist();
			return i;
		}
	}
	if(do_lock) this->unlock_fdlist();
	return -1;
}
int v_pollfd::add_fd(int fd, short events, bool do_lock)
{
	if(unlikely(fd < 0))
		return -1;
	int result = 0;
	if(do_lock) this->wrlock_fdlist();
	int pos = this->find_fd(-1, false);
	if(pos < 0)
	{
		bool ret = this->resize(this->size + 1, false);
		if(!ret)
		{
			result = -1;
			goto out;
		}
		pos = this->size - 1;
	}
	this->fdlist[pos].fd = fd;
	this->fdlist[pos].events = events;
	result = pos;
out:
	if(do_lock) this->unlock_fdlist();
	return result;
}
bool v_pollfd::del_fd(int fd, bool do_lock)
{
	if(unlikely(fd < 0))
		return -1;
	if(do_lock) this->wrlock_fdlist();
	int pos = this->find_fd(fd, false);
	if(pos < 0)
	{
		if(do_lock) this->unlock_fdlist();
		return false;
	}
	// this->fdlist[pos].fd = -1;
	// this->fdlist[pos].events = 0;
	if((size_t)pos < this->size)
		memmove(&this->fdlist[pos], &this->fdlist[pos + 1], sizeof(pollfd) * (this->size - 1 - pos));
	this->resize(this->size - 1, false);
	
	if(do_lock) this->unlock_fdlist();
	return true;
}
bool v_pollfd::del_fd_close(int fd, bool do_lock)
{
	if(unlikely(fd < 0))
		return -1;
	::close(fd);
	return this->del_fd(fd, do_lock);
}

int v_pollfd::poll(int timeout, bool do_lock)
{
	if(do_lock) this->rdlock_fdlist();
	int ret = ::poll(this->fdlist, this->size, timeout);
	if(do_lock) this->unlock_fdlist();
	return ret;
}

int v_pollfd::clear(bool do_lock)
{
	this->resize(0, do_lock);
	return 0;
}
int v_pollfd::clear_closeall(bool do_lock)
{
	if(do_lock) this->wrlock_fdlist();
	for(size_t i = 0; i < this->size; i++)
	{
		if(this->fdlist[i].fd >= 0)
			::close(this->fdlist[i].fd);
	}
	this->resize(0, false);
	if(do_lock) this->unlock_fdlist();
	return 0;
}





int v_pollfd_ts::add_ts(tcpsock *ts, short events, bool do_lock)
{
	if(do_lock) this->wrlock_fdlist();
	if(this->tslist.find(ts->fd) != this->tslist.end())
	{
		// 중복방지
		return -1;
	}
	bool ret = this->add_fd(ts->fd, events, false);
	if(ret)
	{
		this->tslist[ts->fd] = ts;
	}
	if(do_lock) this->unlock_fdlist();
	return ret;
}
bool v_pollfd_ts::del_ts(tcpsock *ts, bool do_lock)
{
	if(do_lock) this->wrlock_fdlist();
	bool ret = this->del_fd(ts->fd, false);
	std::map<int, tcpsock *>::iterator it = this->tslist.find(ts->fd);
	if(it != this->tslist.end())
		this->tslist.erase(it);
	if(do_lock) this->unlock_fdlist();
	return ret;
}
bool v_pollfd_ts::del_ts_close(tcpsock *ts, bool do_lock)
{
	bool ret = this->del_ts(ts, do_lock);
	delete ts;
	return ret;
}

tcpsock *v_pollfd_ts::find_ts_by_fd(int fd, bool do_lock)
{
	tcpsock *p = NULL;
	if(do_lock) this->rdlock_fdlist();
	std::map<int, tcpsock *>::const_iterator it = this->tslist.find(fd);
	if(it != this->tslist.end())
		p = it->second;
	if(do_lock) this->unlock_fdlist();
	return p;
}
tcpsock *v_pollfd_ts::find_ts_by_n(int n, bool do_lock)
{
	if(unlikely(n < 0 || (size_t)n >= this->size))
		return NULL;
	if(do_lock) this->rdlock_fdlist();
	tcpsock *p = this->find_ts_by_fd(this->fdlist[n].fd, false);
	if(do_lock) this->unlock_fdlist();
	return p;
}
int v_pollfd_ts::clear(bool do_lock) // fix.me
{
	if(do_lock) this->wrlock_fdlist();
	this->resize(0, false);
	this->tslist.clear();
	if(do_lock) this->unlock_fdlist();
	return 0;
}
int v_pollfd_ts::clear_closeall(bool do_lock) // fix.me
{
	if(do_lock) this->wrlock_fdlist();
	for(std::map<int, tcpsock *>::const_iterator it = this->tslist.begin(); 
		it != this->tslist.end(); it++)
	{
		this->del_fd(it->second->fd, false);
		delete it->second;
	}
	for(size_t i = 0; i < this->size; i++)
	{
		if(this->fdlist[i].fd >= 0)
			::close(this->fdlist[i].fd);
	}
	this->resize(0, false);
	this->tslist.clear();
	if(do_lock) this->unlock_fdlist();
	return 0;
}











// vim: set tabstop=4 textwidth=80:

