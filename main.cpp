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



#include <iostream>
#include <string>
#include <vector>
#include <map>


#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cassert>
#include <csetjmp>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
//#include <execinfo.h> // backtrace()
//#include <ucontext.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <pcrecpp.h>
#include <sqlite3.h>
//#include <curl/curl.h>


#include "defs.h"
#include "etc.h"
#include "config.h"
#include "irc.h"
#include "ircbot.h"
#include "my_vsprintf.h"
#include "main.h"


static jmp_buf g_restart_jmp;
static ircbot *g_bot;

struct bot_config_t g_bot_config;
int g_program_argc;
char **g_program_argv;
bool g_restart;



static const char *getsigcodestr(int sig, int code)
{
	const char *str;
	
	if(code == 0)
		return NULL;
	
	switch(sig)
	{
	case SIGILL:
		switch(code)
		{
		case ILL_ILLOPC:
			str = "Illegal opcode";
			break;
		case ILL_ILLOPN:
			str = "Illegal operand";
			break;
		case ILL_ILLADR:
			str = "Illegal addressing mode";
			break;
		case ILL_ILLTRP:
			str = "Illegal trap";
			break;
		case ILL_PRVOPC:
			str = "Privileged opcode";
			break;
		case ILL_PRVREG:
			str = "Privileged register";
			break;
		case ILL_COPROC:
			str = "Coprocessor error";
			break;
		case ILL_BADSTK:
			str = "Internal stack error";
			break;
		default:
			str = "Unknown code";
			break;
		}
		break;
	case SIGFPE:
		switch(code)
		{
		case FPE_INTDIV:
			str = "Integer divide by zero";
			break;
		case FPE_INTOVF:
			str = "Integer overflow";
			break;
		case FPE_FLTDIV:
			str = "Floating point divide by zero";
			break;
		case FPE_FLTOVF:
			str = "Floating point overflow";
			break;
		case FPE_FLTUND:
			str = "Floating point underflow";
			break;
		case FPE_FLTRES:
			str = "Floating point inexact result";
			break;
		case FPE_FLTINV:
			str = "Floating point invalid operation";
			break;
		case FPE_FLTSUB:
			str = "Subscript out of range";
			break;
		default:
			str = "Unknown code";
			break;
		}
		break;
	case SIGSEGV:
		switch(code)
		{
		case SEGV_MAPERR:
			str = "Address not mapped to object";
			break;
		case SEGV_ACCERR:
			str = "Invalid permissions for mapped object";
			break;
		default:
			str = "Unknown code";
			break;
		}
		break;
	case SIGBUS:
		switch(code)
		{
		case BUS_ADRALN:
			str = "Invalid address alignment";
			break;
		case BUS_ADRERR:
			str = "Non-existant physical address";
			break;
		case BUS_OBJERR:
			str = "Object specific hardware error";
			break;
		default:
			str = "Unknown code";
			break;
		}
		break;
	default:
		// str = "Unknown signal";sinfo->errno
		str = NULL;
		break;
	}
	
	return str;
}

#if 0
int do_backtrace(void)
{
#define BACKTRACE_BUFFER_SIZE 100
	void *buffer[BACKTRACE_BUFFER_SIZE];
	
	int nptrs = backtrace(buffer, BACKTRACE_BUFFER_SIZE);
	printf("* backtrace() returned %d addresses\n", nptrs);
	
	/* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
		would produce similar output to the following: */
	
	char **strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL)
	{
		perror("backtrace_symbols");
		
		return -1;
	}
	
	for (int i = 0; i < nptrs; i++)
	{
		printf("  * %s\n", strings[i]);
	}
	free(strings);
	puts("* end of backtrace");
	
	return 0;
}
#endif



void restart()
{
	std::cerr << "Restarting..." << std::endl;
	execvp(g_program_argv[0], g_program_argv);
	perror("execvp");
	std::cerr << "Restart failed, I will try longjmp() to main()" << std::endl;
	
	longjmp(g_restart_jmp, 3); // goto main(), and continue
}

static void signal_handler(int sig, siginfo_t *sinfo, void *vucp)
{
	//ucontext_t *uctx = (ucontext_t *)vucp;
	bool do_restart = false;
	bool do_exit = false;
	bool printdetail = false;
	static bool do_exit_called = false;
	const char *signame, *sigdesc;
	
	//(void)uctx;
	(void)vucp;
	
	switch(sig)
	{
	case SIGPIPE: // ignore
		break;
	case SIGCHLD: // removes zombie process
		// child pid: sinfo->si_pid
		int status;
		wait(&status);
		return;
		break;
	case SIGALRM:
		signame = "SIGALRM";
		sigdesc = "Alarmed";
		return;
		break;
	
	case SIGINT:
		signame = "SIGINT";
		sigdesc = "Interrupted";
		do_exit = true;
		break;
	case SIGTERM:
		signame = "SIGTERM";
		sigdesc = "Terminated";
		do_exit = true;
		break;
	case SIGHUP:
		signame = "SIGHUP";
		sigdesc = "Hangup";
		// do_exit = true;
		break;
	
	case SIGSEGV:
		signame = "SIGSEGV";
		sigdesc = "Segmentation fault";
		do_restart = true;
		do_exit = true;
		printdetail = true;
		break;
	case SIGILL:
		signame = "SIGILL";
		sigdesc = "Illegal instruction";
		do_restart = true;
		do_exit = true;
		printdetail = true;
		break;
	case SIGBUS:
		signame = "SIGBUS";
		sigdesc = "Bus error";
		do_restart = true;
		do_exit = true;
		printdetail = true;
		break;
	case SIGFPE:
		signame = "SIGFPE";
		sigdesc = "Floating point exception";
		do_restart = true;
		do_exit = true;
		printdetail = true;
		break;
	
	case SIGABRT:
		signame = "SIGABRT";
		sigdesc = "Aborted";
		do_restart = true;
		do_exit = true;
		break;
	
	case SIGUSR1:
		signame = "SIGUSR1";
		sigdesc = "User defined signal 1";
		do_restart = false;
		do_exit = false;
		break;
	
	default:
		signame = "";
		sigdesc = "";
		std::cerr << "Unhandled signal " << sig << " received" << std::endl;
		return;
		break;
	}
	fprintf(stderr, "SIGNAL: %d:%s(%s)\n", sig, signame, sigdesc);
	if(printdetail)
	{
		const char *sigcodestr = getsigcodestr(sig, sinfo->si_code);
		if(sigcodestr)
		{
			printf("detail: %s\n", sigcodestr);
		}
		if(sinfo->si_addr)
		{
			printf("address: %p\n", sinfo->si_addr);
		}
	}
	
	if((do_exit || do_restart) && do_exit_called)
	{
		std::cerr << "SIGNAL CAUGHT BUT do_exit_called IS 1" << std::endl;
		if(do_restart)
			restart();
		_exit(1);
	}
	if(do_restart)
	{
		if(do_exit)
			do_exit_called = true;
		g_restart = true;
		g_bot->quit_all(sigdesc);
		return;
	}
	if(do_exit)
	{
		do_exit_called = true;
		
		g_restart = false;
		g_bot->quit_all(sigdesc);
		return;
	}
	
	switch(sig)
	{
	case SIGUSR1:
		g_bot->print_bot_status(1);
	}
}

static int setup_signal_handler()
{
	struct sigaction sig_h;
	sigset_t set;
	
	sigemptyset(&set);
	// sigaddset(&set, SIGHUP);
	
	sig_h.sa_flags = SA_SIGINFO;
	sig_h.sa_mask = set;
	sig_h.sa_sigaction = signal_handler;
	
	sigaction(SIGPIPE, &sig_h, NULL);
	sigaction(SIGCHLD, &sig_h, NULL);
	sigaction(SIGALRM, &sig_h, NULL);
	
	sigaction(SIGABRT, &sig_h, NULL);
	sigaction(SIGINT, &sig_h, NULL);
	sigaction(SIGTERM, &sig_h, NULL);
	sigaction(SIGHUP, &sig_h, NULL);
	
	sigaction(SIGUSR1, &sig_h, NULL);
	sigaction(SIGUSR2, &sig_h, NULL);
	
	if(!g_bot_config.enable_coredump)
	{
		sigaction(SIGSEGV, &sig_h, NULL);
		sigaction(SIGILL, &sig_h, NULL);
		sigaction(SIGBUS, &sig_h, NULL);
		sigaction(SIGFPE, &sig_h, NULL);
	}
	
	return 0;
}

unsigned int dev_rand(void)
{
	int fd, ret;
	unsigned int buf;
	
	fd = open("/dev/urandom", O_RDONLY);
	if(fd < 0)
		return time(NULL);
	
	ret = read(fd, &buf, sizeof(unsigned int));
	close(fd);
	if(ret != sizeof(unsigned int))
		return time(NULL);
	
	return buf;
}


static const struct option bot_opts[] = 
{
	{"config-file", required_argument, NULL, 'c'}, 
	{"enable-coredump", no_argument, NULL, 'C'}, 
	{"help", no_argument, NULL, 'h'}, 
	{NULL, 0, NULL, 0}
};

static int conf_set_default()
{
	g_bot_config.config_file = "./data/meowbot.conf";
	g_bot_config.enable_coredump = false;
	g_bot_config.load_and_exit = false;
	return 0;
}

static int parse_args(int argc, char *argv[])
{
	int c;
	while((c = getopt_long(argc, argv, "c:Cth", bot_opts, NULL)) >= 0)
	{
		switch(c)
		{
		case 'c':
			g_bot_config.config_file = optarg;
			break;
		case 'C':
			g_bot_config.enable_coredump = true;
			break;
		case 't':
			// 모듈 로드 후 바로 종료. 모듈 디버깅시 사용
			g_bot_config.load_and_exit = true;
			break;
		case 'h':
			printf(
				"Usage: %s [options]\n"
				"\t-c <config file> --config-file=<config file>\n"
				"\t\tset config file (default: ./data/meowbot.conf)\n"
				"\t-C --enable-coredump\n"
				"\t-t\n"
				, argv[0]
				);
			exit(EXIT_SUCCESS);
			break;
		default:
			return -1;
			break;
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	g_program_argc = argc;
	g_program_argv = argv;
	int exitcode = EXIT_SUCCESS;
	int ret;
	
	conf_set_default();
	ret = parse_args(argc, argv);
	if(ret < 0)
	{
		exit(EXIT_FAILURE);
	}
	
	setup_signal_handler();
	srand(dev_rand());
#ifdef HAVE_RAND48
	srand48(rand());
#endif

	if(g_bot_config.enable_coredump)
	{
		struct rlimit rlim;
		getrlimit(RLIMIT_CORE, &rlim);
		rlim.rlim_cur = rlim.rlim_max;
		ret = setrlimit(RLIMIT_CORE, &rlim);
		if(ret < 0)
		{
			perror("setrlimit() failed");
		}
	}
	
#if 0
	return 0;
#endif
	
	//assert(sqlite3_threadsafe() != 1);
	
	ircbot bot;
	g_bot = &bot;
l_restart:
	g_restart = false;
	ret = bot.load_cfg_file(g_bot_config.config_file);
	if(ret < 0)
	{
		std::cerr << "couldn't load configuration file `" 
			<< g_bot_config.config_file << "'" << std::endl;
			std::cerr << "Error: " << bot.cfg.Lw.get_lasterr_str() << std::endl;
		exitcode = EXIT_FAILURE;
		goto end;
	}
	bot.initialize();
	if(g_bot_config.load_and_exit)
	{
		goto end;
	}
	
	printf("PID: %d\n", getpid());
	if(getuid() == 0)
		puts("WARNING: do not run as root");
	
	ret = setjmp(g_restart_jmp);
	if(ret)
	{
		std::cerr << "Disconnecting..." << std::endl;
		bot.disconnect_all();
		
		if(ret == 1)
		{
			std::cerr << "main(): I will restart after 30 seconds" << std::endl;
			sleep(30);
			restart();
		}
		else if(ret == 2)
		{
			std::cerr << "main(): Exiting..." << std::endl;
			goto end;
		}
		else if(ret == 3)
		{
			// continue;
			std::cerr << "main(): BUG: Unknown value returned from longjmp" << std::endl;
			std::cerr << "main():  I will restart after 30 seconds" << std::endl;
			sleep(30);
			restart();
		}
	}
	
	bot.run();
	bot.wait();
	std::cerr << "main loop exited" << std::endl;
	if(g_restart)
	{
		restart();
		goto l_restart;
	}
	
end:
	bot.cleanup(); // FIXME: bot1.cleanup() 사용
	
	return exitcode;
}






