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


#ifndef IRCDEFS_H_INCLUDED
#define IRCDEFS_H_INCLUDED

// from rfc2812
enum irc_codes
{
	IRC_RPL_WELCOME = 001, /* "Welcome to the Internet Relay Network */
	IRC_RPL_YOURHOST = 002, /* "Your host is <servername>, running version <ver>" */
	IRC_RPL_CREATED = 003, /* "This server was created <date>" */
	IRC_RPL_MYINFO = 004, /* "<servername> <version> <available user modes> */
	IRC_RPL_BOUNCE = 005, /* "Try server <server name>, port <port number>" */
	IRC_RPL_USERHOST = 302, /* ":*1<reply> *( " " <reply> )" */
	IRC_RPL_ISON = 303, /* ":*1<nick> *( " " <nick> )" */
	IRC_RPL_AWAY = 301, /* "<nick> :<away message>" */
	IRC_RPL_UNAWAY = 305, /* ":You are no longer marked as being away" */
	IRC_RPL_NOWAWAY = 306, /* ":You have been marked as being away" */
	IRC_RPL_WHOISUSER = 311, /* "<nick> <user> <host> * :<real name>" */
	IRC_RPL_WHOISSERVER = 312, /* "<nick> <server> :<server info>" */
	IRC_RPL_WHOISOPERATOR = 313, /* "<nick> :is an IRC operator" */
	IRC_RPL_WHOISIDLE = 317, /* "<nick> <integer> :seconds idle" */
	IRC_RPL_ENDOFWHOIS = 318, /* "<nick> :End of WHOIS list" */
	IRC_RPL_WHOISCHANNELS = 319, /* "<nick> :*( ( "@" / "+" ) <channel> " " )" */
	IRC_RPL_WHOWASUSER = 314, /* "<nick> <user> <host> * :<real name>" */
	IRC_RPL_ENDOFWHOWAS = 369, /* "<nick> :End of WHOWAS" */
	IRC_RPL_LISTSTART = 321, /* Obsolete. Not used. */
	IRC_RPL_LIST = 322, /* "<channel> <# visible> :<topic>" */
	IRC_RPL_LISTEND = 323, /* ":End of LIST" */
	IRC_RPL_UNIQOPIS = 325, /* "<channel> <nickname>" */
	IRC_RPL_CHANNELMODEIS = 324, /* "<channel> <mode> <mode params>" */
	IRC_RPL_NOTOPIC = 331, /* "<channel> :No topic is set" */
	IRC_RPL_TOPIC = 332, /* "<channel> :<topic>" */
	IRC_RPL_INVITING = 341, /* "<channel> <nick>" */
	IRC_RPL_SUMMONING = 342, /* "<user> :Summoning user to IRC" */
	IRC_RPL_INVITELIST = 346, /* "<channel> <invitemask>" */
	IRC_RPL_ENDOFINVITELIST = 347, /* "<channel> :End of channel invite list" */
	IRC_RPL_EXCEPTLIST = 348, /* "<channel> <exceptionmask>" */
	IRC_RPL_ENDOFEXCEPTLIST = 349, /* "<channel> :End of channel exception list" */
	IRC_RPL_VERSION = 351, /* "<version>.<debuglevel> <server> :<comments>" */
	IRC_RPL_WHOREPLY = 352, /* "<channel> <user> <host> <server> <nick> */
	IRC_RPL_ENDOFWHO = 315, /* "<name> :End of WHO list" */
	IRC_RPL_NAMREPLY = 353, /* "( "=" / "*" / "@" ) <channel> */
	IRC_RPL_ENDOFNAMES = 366, /* "<channel> :End of NAMES list" */
	IRC_RPL_LINKS = 364, /* "<mask> <server> :<hopcount> <server info>" */
	IRC_RPL_ENDOFLINKS = 365, /* "<mask> :End of LINKS list" */
	IRC_RPL_BANLIST = 367, /* "<channel> <banmask>" */
	IRC_RPL_ENDOFBANLIST = 368, /* "<channel> :End of channel ban list" */
	IRC_RPL_INFO = 371, /* ":<string>" */
	IRC_RPL_ENDOFINFO = 374, /* ":End of INFO list" */
	IRC_RPL_MOTDSTART = 375, /* ":- <server> Message of the day - " */
	IRC_RPL_MOTD = 372, /* ":- <text>" */
	IRC_RPL_ENDOFMOTD = 376, /* ":End of MOTD command" */
	IRC_RPL_YOUREOPER = 381, /* ":You are now an IRC operator" */
	IRC_RPL_REHASHING = 382, /* "<config file> :Rehashing" */
	IRC_RPL_YOURESERVICE = 383, /* "You are service <servicename>" */
	IRC_RPL_TIME = 391, /* "<server> :<string showing server's local time>" */
	IRC_RPL_USERSSTART = 392, /* ":UserID   Terminal  Host" */
	IRC_RPL_USERS = 393, /* ":<username> <ttyline> <hostname>" */
	IRC_RPL_ENDOFUSERS = 394, /* ":End of users" */
	IRC_RPL_NOUSERS = 395, /* ":Nobody logged in" */
	IRC_RPL_TRACELINK = 200, /* "Link <version & debug level> <destination> */
	IRC_RPL_TRACECONNECTING = 201, /* "Try. <class> <server>" */
	IRC_RPL_TRACEHANDSHAKE = 202, /* "H.S. <class> <server>" */
	IRC_RPL_TRACEUNKNOWN = 203, /* "???? <class> [<client IP address in dot form>]" */
	IRC_RPL_TRACEOPERATOR = 204, /* "Oper <class> <nick>" */
	IRC_RPL_TRACEUSER = 205, /* "User <class> <nick>" */
	IRC_RPL_TRACESERVER = 206, /* "Serv <class> <int>S <int>C <server> */
	IRC_RPL_TRACESERVICE = 207, /* "Service <class> <name> <type> <active type>" */
	IRC_RPL_TRACENEWTYPE = 208, /* "<newtype> 0 <client name>" */
	IRC_RPL_TRACECLASS = 209, /* "Class <class> <count>" */
	IRC_RPL_TRACERECONNECT = 210, /* Unused. */
	IRC_RPL_TRACELOG = 261, /* "File <logfile> <debug level>" */
	IRC_RPL_TRACEEND = 262, /* "<server name> <version & debug level> :End of TRACE" */
	IRC_RPL_STATSLINKINFO = 211, /* "<linkname> <sendq> <sent messages> */
	IRC_RPL_STATSCOMMANDS = 212, /* "<command> <count> <byte count> <remote count>" */
	IRC_RPL_ENDOFSTATS = 219, /* "<stats letter> :End of STATS report" */
	IRC_RPL_STATSUPTIME = 242, /* ":Server Up %d days %d:%02d:%02d" */
	IRC_RPL_STATSOLINE = 243, /* "O <hostmask> * <name>" */
	IRC_RPL_UMODEIS = 221, /* "<user mode string>" */
	IRC_RPL_SERVLIST = 234, /* "<name> <server> <mask> <type> <hopcount> <info>" */
	IRC_RPL_SERVLISTEND = 235, /* "<mask> <type> :End of service listing" */
	IRC_RPL_LUSERCLIENT = 251, /* ":There are <integer> users and <integer> */
	IRC_RPL_LUSEROP = 252, /* "<integer> :operator(s) online" */
	IRC_RPL_LUSERUNKNOWN = 253, /* "<integer> :unknown connection(s)" */
	IRC_RPL_LUSERCHANNELS = 254, /* "<integer> :channels formed" */
	IRC_RPL_LUSERME = 255, /* ":I have <integer> clients and <integer> */
	IRC_RPL_ADMINME = 256, /* "<server> :Administrative info" */
	IRC_RPL_ADMINLOC1 = 257, /* ":<admin info>" */
	IRC_RPL_ADMINLOC2 = 258, /* ":<admin info>" */
	IRC_RPL_ADMINEMAIL = 259, /* ":<admin info>" */
	IRC_RPL_TRYAGAIN = 263, /* "<command> :Please wait a while and try again." */
	IRC_ERR_NOSUCHNICK = 401, /* "<nickname> :No such nick/channel" */
	IRC_ERR_NOSUCHSERVER = 402, /* "<server name> :No such server" */
	IRC_ERR_NOSUCHCHANNEL = 403, /* "<channel name> :No such channel" */
	IRC_ERR_CANNOTSENDTOCHAN = 404, /* "<channel name> :Cannot send to channel" */
	IRC_ERR_TOOMANYCHANNELS = 405, /* "<channel name> :You have joined too many channels" */
	IRC_ERR_WASNOSUCHNICK = 406, /* "<nickname> :There was no such nickname" */
	IRC_ERR_TOOMANYTARGETS = 407, /* "<target> :<error code> recipients. <abort message>" */
	IRC_ERR_NOSUCHSERVICE = 408, /* "<service name> :No such service" */
	IRC_ERR_NOORIGIN = 409, /* ":No origin specified" */
	IRC_ERR_NORECIPIENT = 411, /* ":No recipient given (<command>)" */
	IRC_ERR_NOTEXTTOSEND = 412, /* ":No text to send" */
	IRC_ERR_NOTOPLEVEL = 413, /* "<mask> :No toplevel domain specified" */
	IRC_ERR_WILDTOPLEVEL = 414, /* "<mask> :Wildcard in toplevel domain" */
	IRC_ERR_BADMASK = 415, /* "<mask> :Bad Server/host mask" */
	IRC_ERR_UNKNOWNCOMMAND = 421, /* "<command> :Unknown command" */
	IRC_ERR_NOMOTD = 422, /* ":MOTD File is missing" */
	IRC_ERR_NOADMININFO = 423, /* "<server> :No administrative info available" */
	IRC_ERR_FILEERROR = 424, /* ":File error doing <file op> on <file>" */
	IRC_ERR_NONICKNAMEGIVEN = 431, /* ":No nickname given" */
	IRC_ERR_ERRONEUSNICKNAME = 432, /* "<nick> :Erroneous nickname" */
	IRC_ERR_NICKNAMEINUSE = 433, /* "<nick> :Nickname is already in use" */
	IRC_ERR_NICKCOLLISION = 436, /* "<nick> :Nickname collision KILL from <user>@<host>" */
	IRC_ERR_UNAVAILRESOURCE = 437, /* "<nick/channel> :Nick/channel is temporarily unavailable" */
	IRC_ERR_USERNOTINCHANNEL = 441, /* "<nick> <channel> :They aren't on that channel" */
	IRC_ERR_NOTONCHANNEL = 442, /* "<channel> :You're not on that channel" */
	IRC_ERR_USERONCHANNEL = 443, /* "<user> <channel> :is already on channel" */
	IRC_ERR_NOLOGIN = 444, /* "<user> :User not logged in" */
	IRC_ERR_SUMMONDISABLED = 445, /* ":SUMMON has been disabled" */
	IRC_ERR_USERSDISABLED = 446, /* ":USERS has been disabled" */
	IRC_ERR_NOTREGISTERED = 451, /* ":You have not registered" */
	IRC_ERR_NEEDMOREPARAMS = 461, /* "<command> :Not enough parameters" */
	IRC_ERR_ALREADYREGISTRED = 462, /* ":Unauthorized command (already registered)" */
	IRC_ERR_NOPERMFORHOST = 463, /* ":Your host isn't among the privileged" */
	IRC_ERR_PASSWDMISMATCH = 464, /* ":Password incorrect" */
	IRC_ERR_YOUREBANNEDCREEP = 465, /* ":You are banned from this server" */
	IRC_ERR_YOUWILLBEBANNED = 466, /*  */
	IRC_ERR_KEYSET = 467, /* "<channel> :Channel key already set" */
	IRC_ERR_CHANNELISFULL = 471, /* "<channel> :Cannot join channel (+l)" */
	IRC_ERR_UNKNOWNMODE = 472, /* "<char> :is unknown mode char to me for <channel>" */
	IRC_ERR_INVITEONLYCHAN = 473, /* "<channel> :Cannot join channel (+i)" */
	IRC_ERR_BANNEDFROMCHAN = 474, /* "<channel> :Cannot join channel (+b)" */
	IRC_ERR_BADCHANNELKEY = 475, /* "<channel> :Cannot join channel (+k)" */
	IRC_ERR_BADCHANMASK = 476, /* "<channel> :Bad Channel Mask" */
	IRC_ERR_NOCHANMODES = 477, /* "<channel> :Channel doesn't support modes" */
	IRC_ERR_BANLISTFULL = 478, /* "<channel> <char> :Channel list is full" */
	IRC_ERR_NOPRIVILEGES = 481, /* ":Permission Denied- You're not an IRC operator" */
	IRC_ERR_CHANOPRIVSNEEDED = 482, /* "<channel> :You're not channel operator" */
	IRC_ERR_CANTKILLSERVER = 483, /* ":You can't kill a server!" */
	IRC_ERR_RESTRICTED = 484, /* ":Your connection is restricted!" */
	IRC_ERR_UNIQOPPRIVSNEEDED = 485, /* ":You're not the original channel operator" */
	IRC_ERR_NOOPERHOST = 491, /* ":No O-lines for your host" */
	IRC_ERR_UMODEUNKNOWNFLAG = 501, /* ":Unknown MODE flag" */
	IRC_ERR_USERSDONTMATCH = 502, /* ":Cannot change mode for other users" */
	IRC_CODES_END
};

// 512 - strlen("\r\n")
#define IRC_MAX_LINE_SIZE 510

#define IRCTEXT_BOLD "\x02"
#define IRCTEXT_UNDERLINE "\x1F"
#define IRCTEXT_NORMAL "\x0F"
#define IRCTEXT_COLOR "\x03"

#define IRCTEXT_COLOR_WHITE "00"
#define IRCTEXT_COLOR_BLACK "01"
#define IRCTEXT_COLOR_NAVY "02"
#define IRCTEXT_COLOR_GREEN "03"
#define IRCTEXT_COLOR_RED "04"
#define IRCTEXT_COLOR_BROWN "05"
#define IRCTEXT_COLOR_PURPLE "06"
#define IRCTEXT_COLOR_ORANGE "07"
#define IRCTEXT_COLOR_YELLOW "08"
#define IRCTEXT_COLOR_LIME "09"
#define IRCTEXT_COLOR_TEAL "10"
#define IRCTEXT_COLOR_SKYBLUE "11"
#define IRCTEXT_COLOR_BLUE "12"
#define IRCTEXT_COLOR_PINK "13"
#define IRCTEXT_COLOR_GRAY "14"
#define IRCTEXT_COLOR_SILVER "15"

#define IRCTEXT_COLOR_FG(_color) IRCTEXT_COLOR _color
#define IRCTEXT_COLOR_BG(_color) IRCTEXT_COLOR "," _color
#define IRCTEXT_COLOR_FGBG(_fgcolor, _bgcolor) IRCTEXT_COLOR _fgcolor "," _bgcolor



enum IRCSTATUS
{
	IRCSTATUS_DISCONNECTED, 
	IRCSTATUS_CONNECTING, 
	IRCSTATUS_CONNECTED, 
	IRCSTATUS_END
};

enum irc_priority
{
	IRC_PRIORITY_MIN = -99,
	IRC_PRIORITY_LOW = -5,
	IRC_PRIORITY_BNORMAL = -1, 
	IRC_PRIORITY_NORMAL = 0, 
	IRC_PRIORITY_ANORMAL = 1, 
	IRC_PRIORITY_HIGH = 5, 
	IRC_PRIORITY_MAX = 99, 
};

enum irc_chantype
{
	IRC_CHANTYPE_NONE = 0, 
	IRC_CHANTYPE_CHAN, 
	IRC_CHANTYPE_QUERY, 
	IRC_CHANTYPE_DCC, 
	IRC_CHANTYPE_TELNET, 
	IRC_CHANTYPE_END
};

enum irc_msgtype
{
	IRC_MSGTYPE_NONE = 0, 
	IRC_MSGTYPE_NORMAL, 
	IRC_MSGTYPE_NOTICE, 
	IRC_MSGTYPE_ACTION, 
	IRC_MSGTYPE_CTCP, 
	IRC_MSGTYPE_CTCPREPLY, 
	IRC_MSGTYPE_END
};








#endif // IRCDEFS_H_INCLUDED

