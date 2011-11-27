--[[
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
]]--




function luatest_cb(isrv, who, msginfo, mdest, cmd, carg)
	isrv:privmsg_nh(mdest, who:getnick() .. ' says ' .. carg .. ' in ' .. msginfo:getchan())
end

function privmsg_handler(isrv, who, msginfo, msg)
	--isrv:privmsg(msginfo:getchan(), 'privmsg: '..who:getident()..','..msginfo:getchan()..','..msginfo:getmsgtype()..','..msg)
	return true
end
function join_handler(isrv, who, chan)
	isrv:privmsg(chan, 'join: '..who:getident())
	return true
end
function part_handler(isrv, who, chan, partmsg)
	isrv:privmsg(chan, 'part: '..who:getident()..','..partmsg)
	return true
end
function kick_handler(isrv, who, chan, kicked_user, kickmsg)
	isrv:privmsg(chan, 'kick: '..who:getident()..','..kicked_user:getident()..','..kickmsg)
	return true
end
function nick_handler(isrv, who, newnick)
	isrv:privmsg(newnick, 'nick: '..who:getident()..','..newnick)
	return true
end
function quit_handler(isrv, who, quitmsg)
	print('quit: '..who:getnick()..': '..quitmsg)
	return true
end
function mode_handler(isrv, who, target, mode, param)
	isrv:privmsg(target, 'mode: '..who:getident()..','..mode..','..param)
	return true
end
function invite_handler(isrv, who, chan)
	isrv:privmsg(chan, 'quit: '..who:getident())
	return true
end
function srvincoming_handler(isrv, line)
	print('srvincoming: '..line)
	return true
end
function idle_handler(isrv)
	return true
end
function connect_handler(isrv)
	return true
end
function login_handler(isrv)
	return true
end
function disconnect_handler(isrv)
	return true
end

function botcommand_handler(isrv, who, msginfo, mdest, cmd_trigger, cmd, carg)
	return true
end



function init(bot)
	bot:register_cmd('luatest', luatest_cb);
	
	bot:register_event_handler('privmsg', privmsg_handler);
	bot:register_event_handler('join', join_handler);
	bot:register_event_handler('part', part_handler);
	bot:register_event_handler('kick', kick_handler);
	bot:register_event_handler('nick', nick_handler);
	bot:register_event_handler('quit', quit_handler);
	bot:register_event_handler('mode', mode_handler);
	bot:register_event_handler('invite', invite_handler);
	bot:register_event_handler('srvincoming', srvincoming_handler);
	bot:register_event_handler('idle', idle_handler);
	bot:register_event_handler('connect', connect_handler);
	bot:register_event_handler('login', login_handler);
	bot:register_event_handler('disconnect', disconnect_handler);
	
	bot:register_event_handler('botcommand', botcommand_handler);
end

function cleanup(bot)
	bot:unregister_cmd('luatest');
	
	bot:unregister_event_handler('privmsg');
	bot:unregister_event_handler('join');
	bot:unregister_event_handler('part');
	bot:unregister_event_handler('kick');
	bot:unregister_event_handler('nick');
	bot:unregister_event_handler('quit');
	bot:unregister_event_handler('mode');
	bot:unregister_event_handler('invite');
	bot:unregister_event_handler('srvincoming');
	bot:unregister_event_handler('idle');
	bot:unregister_event_handler('connect');
	bot:unregister_event_handler('login');
	bot:unregister_event_handler('disconnect');
	
	bot:unregister_event_handler('botcommand');
end





