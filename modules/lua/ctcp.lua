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



re_ctcp_parse = pcre.new('([^ ]+) ?(.*)', 'f')
function privmsg_handler(isrv, who, msginfo, msg)
	
	local chan = msginfo:getchan()
	local msgtype, chantype = 
		msginfo:getmsgtype(), msginfo:getchantype()
	
	if chantype == 'query' and msgtype == 'ctcp' then
		local vec = re_ctcp_parse:full_match(msg)
		local cmd, carg = vec[1], vec[2]
		msginfo2 = irc_msginfo.new(chan, 'ctcpreply')
		
		if cmd == 'VERSION' then
			isrv:privmsg(msginfo2, 'VERSION ' .. isrv:getbot():version())
		elseif cmd == 'TIME' then
			--Fri Jan 16 12:11:21 2009
			isrv:privmsg(msginfo2, os.date('TIME %a %b %d %H:%M:%S %Y'))
		elseif cmd == 'PING' then
			isrv:privmsg(msginfo2, 'PONG ' .. carg)
		end
	end
	return true
end


function init(bot)
	bot:register_event_handler('privmsg', privmsg_handler);
end

function cleanup(bot)
	bot:unregister_event_handler('privmsg');
end





