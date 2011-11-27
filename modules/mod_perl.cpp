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
#include <deque>

#include <pthread.h>

#include <pcrecpp.h>

#include "defs.h"
#include "etc.h"
#include "hangul.h"
#include "module.h"
#include "ircbot.h"

#undef ENABLE_PERL /////////////////////// TODO: FIXME !!!

#ifdef ENABLE_PERL


#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>


static ircbot *g_bot;
static caseless_strmap<std::string, SV *> g_cmd_callbacks;
static pthread_mutex_t g_perl_global_lock;
static pthread_rwlock_t g_cmdlist_lock;






static int cmd_cb_perl(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *userdata);


// 20081130 removed `static' in XSUBs - causes compiler error in perl 5.10

XS(xs_register_cmd)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	
	dXSARGS;
	if(items != 2)
	{
		puts("usage: meowbot_register_cmd(cmdname, callback)");
		XSRETURN_NO;
	}
	
	const char *cmd = SvPV_nolen(ST(0));
	SV *callback = sv_mortalcopy(ST(1));
	SvREFCNT_inc(callback);
	pthread_rwlock_wrlock(&g_cmdlist_lock);
	g_cmd_callbacks[cmd] = callback;
	pthread_rwlock_unlock(&g_cmdlist_lock);
	g_bot->register_cmd(cmd, cmd_cb_perl, (void *)my_perl);
	
	XSRETURN_YES;
}

XS(xs_unregister_cmd)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	(void)my_perl;
	
	dXSARGS;
	if(items != 1)
	{
		puts("usage: meowbot_unregister_cmd(cmdname)");
		XSRETURN_NO;
	}
	
	const char *cmd = SvPV_nolen(ST(0));
	pthread_rwlock_wrlock(&g_cmdlist_lock);
	caseless_strmap<std::string, SV *>::iterator it = g_cmd_callbacks.find(cmd);
	if(it == g_cmd_callbacks.end())
	{
		pthread_rwlock_unlock(&g_cmdlist_lock);
		XSRETURN_NO;
	}
	SV *cb = it->second;
	if(!cb)
	{
		pthread_rwlock_unlock(&g_cmdlist_lock);
		XSRETURN_NO;
	}
	SvREFCNT_dec(cb);
	g_cmd_callbacks.erase(it);
	pthread_rwlock_unlock(&g_cmdlist_lock);
	g_bot->unregister_cmd(cmd);
	
	XSRETURN_YES;
}

XS(xs_privmsg)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	(void)my_perl;
	
	dXSARGS;
	if(items != 2)
	{
		puts("usage: meowbot_privmsg(chan, msg)");
		XSRETURN_NO;
	}
	
	const char *chan = SvPV_nolen(ST(0));
	const char *msg = SvPV_nolen(ST(1));
	g_bot->privmsg(irc_msginfo(chan), msg);
	
	XSRETURN_YES;
}

XS(xs_prevent_irc_highlighting_all)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	(void)my_perl;
	
	dXSARGS;
	if(items != 2)
	{
		puts("usage: meowbot_prevent_irc_highlighting_all(chan, msg)");
		XSRETURN_UNDEF;
	}
	
	const char *chan = SvPV_nolen(ST(0));
	const char *msg = SvPV_nolen(ST(1));
	std::string newmsg = g_bot->prevent_irc_highlighting_all(chan, msg);
	
	ST(0) = sv_2mortal(newSVpv(newmsg.c_str(), newmsg.length()));
	XSRETURN(1);
}

XS(xs_privmsg_nh)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	(void)my_perl;
	
	dXSARGS;
	if(items != 2)
	{
		puts("usage: meowbot_privmsg_nh(chan, msg)");
		XSRETURN_NO;
	}
	
	const char *chan = SvPV_nolen(ST(0));
	const char *msg = SvPV_nolen(ST(1));
	g_bot->privmsg_nh(irc_msginfo(chan), msg);
	
	XSRETURN_YES;
}


XS(xs_select_josa)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	(void)my_perl;
	
	dXSARGS;
	if(items != 2)
	{
		puts("usage: select_josa(ctx, josa)");
		XSRETURN_NO;
	}
	
	const char *ctx = SvPV_nolen(ST(0));
	const char *josa = SvPV_nolen(ST(1));
	std::string result = select_josa(ctx, josa);
	
	ST(0) = sv_2mortal(newSVpv(result.c_str(), result.length()));
	XSRETURN(1);
}

XS(xs_is_botadmin)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	(void)my_perl;
	
	dXSARGS;
	if(items != 1)
	{
		puts("usage: meowbot_is_botadmin(ident)");
		XSRETURN_NO;
	}
	
	const char *ident = SvPV_nolen(ST(0));
	if(g_bot->is_botadmin(ident))
	{
		XSRETURN_YES;
	}
	else
	{
		XSRETURN_NO;
	}
	
	// never reached
}

XS(xs_quote)
{
	PERL_SET_CONTEXT(my_perl);
	(void)cv;
	(void)my_perl;
	
	dXSARGS;
	if(items != 1)
	{
		puts("usage: meowbot_quote(msg)");
		XSRETURN_NO;
	}
	
	const char *msg = SvPV_nolen(ST(0));
	g_bot->irc_client.quote(msg);
	
	XSRETURN_YES;
}




extern "C" void boot_DynaLoader(pTHX_ CV *cv);
static void xs_init(pTHX)
{
	(void)my_perl;
	
	newXS((char *)"DynaLoader::boot_DynaLoader", boot_DynaLoader, (char *)__FILE__);
	
	newXS((char *)"meowbot_register_cmd", xs_register_cmd, (char *)__FILE__);
	newXS((char *)"meowbot_unregister_cmd", xs_unregister_cmd, (char *)__FILE__);
	newXS((char *)"meowbot_privmsg", xs_privmsg, (char *)__FILE__);
	newXS((char *)"meowbot_prevent_irc_highlighting_all", xs_prevent_irc_highlighting_all, (char *)__FILE__);
	newXS((char *)"meowbot_privmsg_nh", xs_privmsg_nh, (char *)__FILE__);
	newXS((char *)"select_josa", xs_select_josa, (char *)__FILE__);
	newXS((char *)"meowbot_is_botadmin", xs_is_botadmin, (char *)__FILE__);
	
	return;
}




int call_cmd_cb(PerlInterpreter *my_perl, const irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg)
{
	PERL_SET_CONTEXT(my_perl);
	dSP;
	
	pthread_rwlock_rdlock(&g_cmdlist_lock);
	SV *cb = g_cmd_callbacks[cmd];
	pthread_rwlock_unlock(&g_cmdlist_lock);
	if(!cb)
	{
		return -1;
	}
	
	ENTER;
	SAVETMPS;
	
	PUSHMARK(SP);
	#define push_string(_str) XPUSHs(sv_2mortal(newSVpv(_str.c_str(), _str.length())))
	push_string(pmsg.getchan());
	push_string(pmsg.getident());
	push_string(cmd);
	push_string(carg);
	#undef push_string
	PUTBACK;
	call_sv(cb, G_EVAL | G_DISCARD);
	
	FREETMPS;
	LEAVE;
	
	return 0;
}

class perlmod
{
public:
	perlmod()
		:my_perl(NULL)
	{
	}
	perlmod(const std::string &filename)
	{
		this->load(filename);
	}
	~perlmod()
	{
		this->unload();
	}
	
	int load(const std::string &filename)
	{
		// char *embedding[] = {(char *)"", (char *)"-e", (char *)"0"};
		char *embedding[] = {(char *)"", (char *)filename.c_str()};
		
		if(this->my_perl)
			return -1;
		
		this->my_perl = perl_alloc();
		PL_perl_destruct_level = 1;
		perl_construct(this->my_perl);
		
		int ret = perl_parse(my_perl, xs_init, 2, embedding, NULL);
		if(ret != 0)
		{
			printf("perl_parse() failed in file %s\n", filename.c_str());
			PL_perl_destruct_level = 1;
			perl_destruct(this->my_perl);
			perl_free(this->my_perl);
			this->my_perl = NULL;
			return -1;
		}
		perl_run(my_perl);
		
		dSP;
		PUSHMARK(SP);
		call_pv("init", G_EVAL | G_DISCARD | G_NOARGS);
		
		return 0;
	}
	
	int unload()
	{
		if(this->my_perl == NULL)
			return -1;
		
		PERL_SET_CONTEXT(my_perl);
		dSP;
		PUSHMARK(SP);
		call_pv("cleanup", G_EVAL | G_DISCARD | G_NOARGS);
		
		PL_perl_destruct_level = 1;
		perl_destruct(this->my_perl);
		perl_free(this->my_perl);
		this->my_perl = NULL;
		
		return 0;
	}
	
private:
	PerlInterpreter *my_perl;
	
};




static std::map<std::string, perlmod> perlmods;

static int load_perl_module(const std::string &name)
{
	static pcrecpp::RE re_validate_mod_name("^[a-zA-Z0-9_가-힣-]+\\.pl$");
	if(!re_validate_mod_name.FullMatch(name))
		return -2;
	else
	{
		int ret = perlmods[name].load("./modules/perl/" + name);
		if(ret != 0)
		{
			perlmods.erase(perlmods.find(name));
			return -1;
		}
		return 0;
	}
}


static int cmd_cb_perl(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *userdata)
{
	(void)self;
	(void)cmd;
	(void)carg;
	
	pthread_mutex_lock(&g_perl_global_lock);
	call_cmd_cb((PerlInterpreter *)userdata, pmsg, mdest, cmd, carg);
	pthread_mutex_unlock(&g_perl_global_lock);
	
	return HANDLER_FINISHED;
}


static int cmd_cb_perl_admin1(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)self;
	(void)cmd;
	(void)carg;
	int ret;
	
	if(!isrv->is_botadmin(pmsg.getident()))
	{
		isrv->privmsg(mdest, "당신은 관리자가 아닙니다.");
		return 0;
	}
	
	if(cmd == "plload")
	{
		std::map<std::string, perlmod>::iterator it;
		it = perlmods.find(carg);
		if(it != perlmods.end())
			isrv->privmsgf(mdest, "cannot load %s: already loaded", carg.c_str());
		else
		{
			isrv->pause_event_threads(true);
			ret = load_perl_module(carg);
			isrv->pause_event_threads(false);
			
			if(ret < 0)
				isrv->privmsgf(mdest, "failed to load perl module %s", carg.c_str());
			else
				isrv->privmsgf(mdest, "perl module %s loaded", carg.c_str());
		}
	}
	else if(cmd == "plunload")
	{
		std::map<std::string, perlmod>::iterator it;
		it = perlmods.find(carg);
		if(it == perlmods.end())
			isrv->privmsgf(mdest, "cannot unload %s: not loaded", carg.c_str());
		else
		{
			isrv->pause_event_threads(true);
			perlmods.erase(it);
			isrv->pause_event_threads(false);
			
			isrv->privmsgf(mdest, "perl module %s unloaded", carg.c_str());
		}
	}
	else if(cmd == "plreload")
	{
		std::map<std::string, perlmod>::iterator it;
		it = perlmods.find(carg);
		if(it == perlmods.end())
			isrv->privmsgf(mdest, "cannot unload %s: not loaded", carg.c_str());
		else
		{
			isrv->pause_event_threads(true);
			perlmods.erase(it);
			ret = load_perl_module(carg);
			isrv->pause_event_threads(false);
			
			if(ret < 0)
				isrv->privmsgf(mdest, "failed to load perl module %s", carg.c_str());
			else
				isrv->privmsgf(mdest, "perl module %s reloaded", carg.c_str());
		}
	}
	
	return HANDLER_FINISHED;
}

int mod_perl_init(ircbot *bot)
{
	int ret;
	g_bot = self;
	
	pthread_mutex_init(&g_perl_global_lock, NULL);
	pthread_rwlock_init(&g_cmdlist_lock, NULL);
	
	bot->register_cmd("plload", cmd_cb_perl_admin1);
	bot->register_cmd("plunload", cmd_cb_perl_admin1);
	bot->register_cmd("plreload", cmd_cb_perl_admin1);
	
	std::vector<std::string> autoload_list;
	std::vector<std::string>::const_iterator it;
	isrv->cfg.getarray(autoload_list, "mod_perl.autoload_modules");
	
	for(it = autoload_list.begin(); it != autoload_list.end(); it++)
	{
		if(it->empty())
			continue;
		ret = load_perl_module(*it);
	}
	
	return 0;
}

int mod_perl_cleanup(ircbot *bot)
{
	(void)self;
	
	perlmods.clear();
	bot->unregister_cmd("plload");
	bot->unregister_cmd("plunload");
	bot->unregister_cmd("plreload");
	
	return 0;
}


#else

int mod_perl_init(ircbot *bot)
{
	(void)bot;
	
	return 0;
}

int mod_perl_cleanup(ircbot *bot)
{
	(void)bot;
	
	return 0;
}



#endif


MODULE_INFO(mod_perl, mod_perl_init, mod_perl_cleanup)





