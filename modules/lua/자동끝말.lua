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





g_autochan = '#asdfgh'

function luatest_cb(isrv, who, msginfo, mdest, cmd, carg)
	
	g_autochan = carg
	isrv:privmsg_nh(mdest, '채널: ' .. carg);
end

function privmsg_handler(isrv, who, msginfo, msg)
	local chan = msginfo:getchan()
	
	do
		if chan == g_autochan and who:getuser() == '~inklbot' then
			local firstchar = strip_irc_colors(msg):utf8_substr(-1, 1) -- 마지막 한 글자
			local w = get_random_word(firstchar);
			if w ~= '' then
				isrv:privmsg(chan, w)
				local w2 = get_random_word(firstchar);
				if w2 ~= '' and w2 ~= w then
					isrv:privmsg(chan, w2)
				end
			end
		end
	end
	
	return true
end


function init(bot)
	bot:register_cmd('자동끝말설정', luatest_cb);
	bot:register_event_handler('privmsg', privmsg_handler);
end

function cleanup(bot)
	bot:unregister_cmd('자동끝말설정');
	bot:unregister_event_handler('privmsg');
end





