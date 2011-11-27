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


-- 타이머 예제
function luatest_cb(isrv, who, msginfo, mdest, cmd, carg)
	local bot = isrv:getbot()
	
	isrv:privmsg(mdest, '5초 후에 그대로 말하겠다에요')
	bot:add_timer('test', 
		function(bot, tmrname, userdata)
			userdata.isrv:privmsg(userdata.chan, userdata.v)
		end, 5000, {isrv=isrv, chan = msginfo:getchan(), v=carg}, 'once')
end




function init(bot)
	bot:register_cmd('luatest', luatest_cb);
end

function cleanup(bot)
	bot:unregister_cmd('luatest');
	
	bot:del_timer('test')
end





