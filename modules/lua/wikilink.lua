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


re_wikilink = pcre.new('\\[\\[([^\\]]+)\\]\\]')
function privmsg_handler(isrv, who, msginfo, msg)
	
	if msginfo:getchan() == '#IRPG-chat' then
		local matches = re_wikilink:match_all(msg)
		for _,m2 in pairs(matches) do
			local aname = m2[1]:gsub(' ', '_')
			isrv:privmsg(msginfo:getchan(), "["..aname..'] http://idlerpg.kr/wiki/'..urlencode(aname))
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





