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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <arpa/inet.h> // inet_ntoa()
#include <assert.h>

#include <typeinfo> // DEBUG

#include <pthread.h>
#include <pcrecpp.h>

#include "defs.h"
#include "module.h"
#include "ircbot.h"



static irc_strmap<std::string, std::string> g_user_broadcast_chans;
static pthread_rwlock_t g_bcchanlist_lock;



int broadcast_to_joined_channels(ircbot_conn *isrv, const std::string &nick, 
	const std::string &msg, int maxchan = -1)
{
	isrv->rdlock_chanlist();
	const std::vector<std::string> &chans = isrv->get_chanlist();
	std::vector<std::string>::const_iterator it;
	std::string chan, destchans;
	int n;
	
	n = 0;
	for(it = chans.begin(); it != chans.end(); it++)
	{
		chan = *it;
		if(isrv->is_user_joined(chan, nick))
		{
			if(!destchans.empty())
				destchans += ',';
			destchans += chan;
		}
		n++;
		if(maxchan >= 0 && n >= maxchan)
			break;
	}
	isrv->unlock_chanlist();
	isrv->privmsg(destchans, msg);
	
	return n;
}


struct dcc_param
{
	dcc_param(){}
	~dcc_param(){}
	
	ircbot_conn *isrv;
	irc_privmsg pmsg;
	std::string path, filename, ip;
	int port;
	unsigned int size;
};

static void *dcc_get_real(void *arg)
{
	dcc_param *param = static_cast<dcc_param *>(arg);
	int fd, ret, nread;
	tcpsock sock;
	char buf[1024];
	unsigned int total = 0;
	irc_msginfo sender(param->pmsg.getchan(), IRC_MSGTYPE_NOTICE);
	
	const std::string &download_url_prefix = param->isrv->bot->cfg["mod_dcc.download_url_prefix"];
	if(download_url_prefix.empty())
	{
		param->isrv->privmsg(sender, "봇 설정이 되지 않았습니다. 봇 관리자에게 문의하세요.");
		goto end;
	}
	
	printf("DCC RECV: %s: filename: %s, ip: %s, port: %d, size: %d\n", 
		param->pmsg.getident().c_str(), param->filename.c_str(), 
		param->ip.c_str(), param->port, param->size);
	
	if(param->size == 0)
	{
		param->isrv->privmsg(sender, "파일 크기는 0보다 커야합니다.");
		goto end;
	}
	if(param->port == 0)
	{
		param->isrv->privmsg(sender, "passive mode는 지원하지 않습니다.");
		goto end;
	}
	if(param->port <= 80 || param->port > 65535)
	{
		param->isrv->privmsg(sender, "포트는 80 ~ 65535 사이에 있어야 합니다.");
		goto end;
	}
	
	if(sock.connect(param->ip, param->port) < 0)
	{
		param->isrv->privmsgf(sender, 
			"서버에 접속할 수 없습니다. (%s:%d): %s", param->ip.c_str(), param->port, 
			sock.get_last_error_str().c_str());
		fprintf(stderr, "%s\n", sock.get_last_error_str().c_str());
		goto end;
	}
	
	fd = open(param->path.c_str(), O_RDWR | O_CREAT);
	if(fd < 0)
	{
		perror("open");
		param->isrv->privmsgf(sender, 
			"내부 오류 (open() failed: %s)", strerror(errno));
		sock.close();
		goto end;
	}
	fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	
	while((nread = sock.timed_read(buf, sizeof(buf), 10000)) > 0) // timed out || socket error일 경우 break
	{
		ret = write(fd, buf, nread);
		if(ret < 0)
		{
			perror("write");
			param->isrv->privmsgf(sender, "내부 오류 (write() failed: %s)", strerror(errno));
			break;
		}
		total += nread;
		unsigned int totaln = htonl(total);
		sock.write(reinterpret_cast<const char *>(&totaln), 4);
		if(total >= param->size)
		{
			break;
		}
	}
	sock.close();
	close(fd);
	
	printf("finished: %s: %d, %d\n", param->filename.c_str(), total, param->size);
	if(nread == 0)
	{
		printf("timed out");
		param->isrv->privmsg(sender, "timed out");
	}
	else if(nread < 0)
	{
		param->isrv->privmsgf(sender, 
			"%s", sock.get_last_error_str().c_str());
	}
	else if(ret < 0)
	{
		
	}
	else if(total != param->size)
	{
		param->isrv->privmsg(sender, 
			"오류: 파일 크기가 일치하지 않습니다 (" + int2string(total) + " , " + 
			int2string(param->size) + ")");
	}
	else
	{
		std::string encoded_filename = urlencode(param->filename);
		param->isrv->privmsg(sender, 
			"업로드 성공 (" + int2string(total) + " bytes)");
		param->isrv->privmsg(sender, 
			"URL: " + download_url_prefix + encoded_filename);
		
		pthread_rwlock_rdlock(&g_bcchanlist_lock);
		const std::string &broadcast_chans = g_user_broadcast_chans[param->pmsg.getnick()];
		if(broadcast_chans.empty())
		{
			broadcast_to_joined_channels(param->isrv, param->pmsg.getnick(), 
				prevent_irc_highlighting_1(param->pmsg.getnick()) + " - " 
				+ param->filename + " (" + int2string(param->size / 1024) + "." + 
				int2string(param->size % 1024) + "KiB): " + 
				download_url_prefix + encoded_filename);
		}
		else
		{
			param->isrv->privmsgf(broadcast_chans, 
				"%s - %s (%d.%d KiB): %s%s", 
				prevent_irc_highlighting_1(param->pmsg.getnick()).c_str(), param->filename.c_str(), 
				param->size / 1024, param->size % 1024, download_url_prefix.c_str(), encoded_filename.c_str());
		}
		pthread_rwlock_unlock(&g_bcchanlist_lock);
	}
	fflush(stdout);
	fflush(stderr);
	
end:
	delete param;
	
	return NULL;
}

static int dcc_get(ircbot_conn *isrv, irc_privmsg *pmsg, std::string filename, const std::string &ip, int port, unsigned int size)
{
	int ret;
	pthread_t tid;
	dcc_param *param = new dcc_param;
	irc_msginfo sender(pmsg->getchan(), IRC_MSGTYPE_NOTICE);
	
	std::string upload_dir(isrv->bot->cfg["mod_dcc.upload_dir"]);
	if(upload_dir.empty())
	{
		isrv->privmsg(sender, "봇 설정이 되지 않았습니다. 봇 관리자에게 문의하세요.");
		return -1;
	}
	
	const char *name = basename(filename.c_str());
	if(!name)
	{
		isrv->privmsg(sender, "내부 오류");
		return -1;
	}
	if(strstr(name, "/") || name[0] == '.' || name[0] == 0)
	{
		isrv->privmsg(sender, "파일 이름에 허용되지 않는 문자가 포함되어 있습니다.");
		return -1;
	}
	filename = name;
	
	// /\'"~*?
	// ..
	// pcrecpp::RE("[/\\\\'\"~*?]", pcrecpp::UTF8()).GlobalReplace("_", &filename);
	// pcrecpp::RE("/", pcrecpp::UTF8()).GlobalReplace("_", &filename);
	// pcrecpp::RE("^[./]", pcrecpp::UTF8()).Replace("_", &filename);
	// pcrecpp::RE("\\.\\.", pcrecpp::UTF8()).GlobalReplace("_", &filename);
	
	param->isrv = isrv;
	param->pmsg = *pmsg;
	param->path = upload_dir + filename;
	param->filename = filename;
	param->ip = ip;
	param->port = port;
	param->size = size;
	
	// ret = dcc_get_real(static_cast<void *>(param));
	ret = pthread_create(&tid, NULL, dcc_get_real, static_cast<void *>(param));
	if(ret < 0)
	{
		return -1;
	}
	
	return (int)tid;
}

static int dcc_get(ircbot_conn *isrv, irc_privmsg *pmsg, const std::string &filename, unsigned int ip, int port, unsigned int size)
{
	in_addr tmp = {ntohl(ip)};
	
	return dcc_get(isrv, pmsg, filename, inet_ntoa(tmp), port, size);
}




static int dcc_event_handler(ircbot_conn *isrv, ircevent *evdata, void *)
{
	int etype = evdata->event_id;
	
	if(etype == IRCEVENT_PRIVMSG)
	{
		ircevent_privmsg *pmsg = static_cast<ircevent_privmsg *>(evdata);
		if(pmsg == NULL)
			return HANDLER_GO_ON;
		int msgtype = pmsg->getmsgtype();
		
		if(msgtype == IRC_MSGTYPE_CTCP)
		{
			const std::string &chan = pmsg->getchan();
			if(chan[0] == '#' || chan[0] == '&')
			{
				return HANDLER_GO_ON;
			}
			const std::string &msg = pmsg->getmsg();
			std::string dcc_filename;
			unsigned int dcc_ip, dcc_size;
			int dcc_port;
			
			if(pcrecpp::RE("^DCC SEND \"((?:[^\"\\\\]++|\\.)+)\" ([0-9]+) ([0-9]+) ([0-9]+)")
				.FullMatch(msg, &dcc_filename, &dcc_ip, &dcc_port, &dcc_size))
			{
				dcc_get(isrv, pmsg, dcc_filename, dcc_ip, dcc_port, dcc_size);
				return HANDLER_FINISHED;
			}
			else if(pcrecpp::RE("^DCC SEND ([^ ]+) ([0-9]+) ([0-9]+) ([0-9]+)")
				.FullMatch(msg, &dcc_filename, &dcc_ip, &dcc_port, &dcc_size))
			{
				dcc_get(isrv, pmsg, dcc_filename, dcc_ip, dcc_port, dcc_size);
				return HANDLER_FINISHED;
			}
		}
	}
	
	return HANDLER_GO_ON;
}






static int cmd_cb_dcc_config(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	(void)cmd;
	(void)carg;
	
	pthread_rwlock_wrlock(&g_bcchanlist_lock);
	g_user_broadcast_chans[pmsg.getnick()] = carg;
	isrv->privmsg(mdest, "DCC 업로드 후 " + g_user_broadcast_chans[pmsg.getnick()] + " 채널에 알립니다.");
	pthread_rwlock_unlock(&g_bcchanlist_lock);
	
	return HANDLER_FINISHED;
}


int mod_dcc_init(ircbot *bot)
{
	bot->register_event_handler(dcc_event_handler);
	bot->register_cmd("DCCchan", cmd_cb_dcc_config);
	
	pthread_rwlock_init(&g_bcchanlist_lock, NULL);
	
	return 0;
}

int mod_dcc_cleanup(ircbot *bot)
{
	bot->unregister_event_handler(dcc_event_handler);
	bot->unregister_cmd("DCCchan");
	
	return 0;
}



MODULE_INFO(mod_dcc, mod_dcc_init, mod_dcc_cleanup)




