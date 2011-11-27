#ifndef IRCUTILS_H_INCLUDED
#define IRCUTILS_H_INCLUDED

#include "ircdefs.h"
#include "etc.h"



bool split_nick_usermode(const std::string &str, std::string *mode, std::string *nick);


class irc_ident
{
	friend class irc_privmsg;
	friend class irc_user;
	
public:
	irc_ident()
	{
		this->set_ident("");
	}
	irc_ident(const std::string &ident)
	{
		this->set_ident(ident);
	}
	irc_ident(const irc_ident &obj)
	{
		this->ident = obj.ident;
		this->nick = obj.nick;
		this->user = obj.user;
		this->host = obj.host;
	}
	~irc_ident(){}
	
	bool operator==(const irc_ident &obj)
	{
		if(obj.ident == this->ident)
			return true;
		else
			return false;
	}
	
	int set_ident(const std::string &ident)
	{
		static pcrecpp::RE re_split_ident("^([^!]+)!([^@]+)@(.*)\\z");
		static pcrecpp::RE re_remove_modechar_from_nick("^[~&@%+]*");
		
		this->ident = ident;
		if(ident.empty())
		{
			this->nick.clear();
			this->user.clear();
			this->host.clear();
			this->ident = "!@";
			
			return 0;
		}
		else if(re_split_ident.FullMatch(ident, &this->nick, &this->user, &this->host))
		{
			re_remove_modechar_from_nick.GlobalReplace("", &this->nick);
			return 0;
		}
		else
		{
			this->nick = ident;
			this->user = "";
			this->host = ident;
			
			return -1;
		}
	}
	void set_nick(const std::string &nick)
	{
		static pcrecpp::RE re_remove_modechar_from_nick("^[~&@%+]*");
		this->nick = nick;
		re_remove_modechar_from_nick.GlobalReplace("", &this->nick);
		this->reconstruct_ident();
	}
	void set_user(const std::string &user)
	{
		this->user = user;
		this->reconstruct_ident();
	}
	void set_host(const std::string &host)
	{
		this->host = host;
		this->reconstruct_ident();
	}
	
	const std::string &getident() const{return this->ident;}
	const std::string &getnick() const{return this->nick;}
	const std::string &getuser() const{return this->user;}
	const std::string &gethost() const{return this->host;}
	
private:
	std::string ident, nick, user, host;
	
	void reconstruct_ident()
	{
		this->ident = this->nick + "!" + this->user + "@" + this->host;
	}
	
};


class irc_msginfo
{
public:
	irc_msginfo()
		:chantype(IRC_CHANTYPE_CHAN), msgtype(IRC_MSGTYPE_NORMAL), 
			priority(IRC_PRIORITY_BNORMAL), line_converted(false)
	{
	}
	irc_msginfo(const std::string &chan, 
			irc_msgtype msgtype = IRC_MSGTYPE_NORMAL, 
			irc_chantype chantype = IRC_CHANTYPE_CHAN, 
			bool line_converted = false)
		:chantype(chantype), msgtype(msgtype), chan(chan), 
			priority(IRC_PRIORITY_BNORMAL), line_converted(line_converted)
	{
	}
	irc_msginfo(const char *chan, 
			irc_msgtype msgtype = IRC_MSGTYPE_NORMAL, 
			irc_chantype chantype = IRC_CHANTYPE_CHAN, 
			bool line_converted = false)
		:chantype(chantype), msgtype(msgtype), chan(chan), 
			priority(IRC_PRIORITY_BNORMAL), line_converted(line_converted)
	{
	}
	irc_msginfo(const irc_msginfo &msginfo, irc_msgtype msgtype)
		:chantype(msginfo.chantype), msgtype(msgtype), chan(msginfo.chan), 
			priority(msginfo.priority), line_converted(msginfo.line_converted)
	{
	}
	irc_msginfo(const irc_msginfo &msginfo)
		:chantype(msginfo.chantype), msgtype(msginfo.msgtype), chan(msginfo.chan), 
			priority(msginfo.priority), line_converted(msginfo.line_converted)
	{
	}
	~irc_msginfo()
	{
	}
	
	// 비교시 priority는 무시됨
	const std::string &operator=(const std::string &chan){return this->chan = chan;}
	bool operator==(const irc_msginfo &obj) const;
	bool operator!=(const irc_msginfo &obj) const
	{
		return !(obj == *this);
	}
	
	void setchantype(irc_chantype chantype){this->chantype = chantype;}
	void setmsgtype(irc_msgtype msgtype){this->msgtype = msgtype;}
	void setchan(const std::string &chan){this->chan = chan;}
	void setpriority(int priority){this->priority = priority;}
	void set_line_converted(bool cv){this->line_converted = cv;}
	
	irc_chantype getchantype() const{return this->chantype;}
	irc_msgtype getmsgtype() const{return this->msgtype;}
	const std::string &getchan() const{return this->chan;}
	int getpriority() const{return this->priority;}
	int is_line_converted() const{return this->line_converted;}
	
private:
	irc_chantype chantype;
	irc_msgtype msgtype;
	std::string chan;
	int priority;
	bool line_converted;
};

class irc_privmsg :public irc_ident, public irc_msginfo
{
public:
	irc_privmsg(const irc_ident &ident, const irc_msginfo &msginfo, const std::string &msg);
	irc_privmsg(const irc_privmsg &obj)
		: irc_ident(obj), irc_msginfo(obj), msg(obj.msg)
	{
	}
	irc_privmsg()
		:irc_ident()
	{
	}
	~irc_privmsg()
	{
	}
	
	const irc_msginfo &getmsginfo() const{return *this;}
	
	void setmsg(const std::string &msg){this->msg = msg;}
	const std::string &getmsg() const{return this->msg;}
	
private:
	std::string msg;
	
};


// 정확히 쓰면 irc_chan_user
class irc_user: public irc_ident
{
public:
	irc_user()
	{
	}
	irc_user(const std::string &nick)
	{
		this->set_nick(nick);
	}
	int set_nick(const std::string &nick)
	{
		std::string nick2;
		split_nick_usermode(nick, &this->chanmode, 
			&static_cast<irc_ident *>(this)->nick);
		
		return 0;
	}
	~irc_user()
	{
	}
	
	const std::string &getchanusermode() const
	{
		return this->chanmode;
	}
	int setchanusermode(const std::string &newmode)
	{
		this->chanmode = newmode;
		return 0;
	}
	int add_to_chanusermode(const std::string &newmode)
	{
		for(std::string::const_iterator it = newmode.begin(); 
			it != newmode.end(); it++)
		{
			if(this->chanmode.find(*it) == std::string::npos)
				this->chanmode += *it;
		}
		
		return 0;
	}
	int del_from_chanusermode(const std::string &newmode)
	{
		for(std::string::const_iterator it = newmode.begin(); 
			it != newmode.end(); it++)
		{
			size_t loc = this->chanmode.find(*it);
			if(loc != std::string::npos)
				this->chanmode = this->chanmode.erase(loc, 1);
		}
		
		return 0;
	}
	
	bool is_user_has_owner_priv() const
	{
		if(this->chanmode.find('~') != std::string::npos)
			return true;
		return false;
	}
	bool is_user_protected() const
	{
		if(this->chanmode.find('&') != std::string::npos)
			return true;
		return false;
	}
	bool is_user_has_op_priv() const
	{
		if(this->chanmode.find('~') != std::string::npos)
			return true;
		if(this->chanmode.find('@') != std::string::npos)
			return true;
		return false;
	}
	bool is_user_has_hop_priv() const
	{
		if(this->chanmode.find('~') != std::string::npos)
			return true;
		if(this->chanmode.find('@') != std::string::npos)
			return true;
		if(this->chanmode.find('%') != std::string::npos)
			return true;
		return false;
	}
	bool is_user_has_voice() const
	{
		if(this->chanmode.find('+') != std::string::npos)
			return true;
		return false;
	}
	
	bool add_owner_priv()
	{
		if(this->chanmode.find('~') != std::string::npos)
			return false;
		this->chanmode += '~';
		return true;
	}
	bool add_protected_priv()
	{
		if(this->chanmode.find('&') != std::string::npos)
			return false;
		this->chanmode += '&';
		return true;
	}
	bool add_op_priv()
	{
		if(this->chanmode.find('@') != std::string::npos)
			return false;
		this->chanmode += '@';
		return true;
	}
	bool add_hop_priv()
	{
		if(this->chanmode.find('%') != std::string::npos)
			return false;
		this->chanmode += '%';
		return true;
	}
	bool add_voice_priv()
	{
		if(this->chanmode.find('+') != std::string::npos)
			return false;
		this->chanmode += '+';
		return true;
	}
	
private:
	std::string chanmode;
	
};





#endif // IRCUTILS_H_INCLUDED

