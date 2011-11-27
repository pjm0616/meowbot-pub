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



g_rejoin_chan_list = {}

function part_handler(isrv, who, chan, partmsg)
	
	if g_rejoin_chan_list[chan] then
		isrv:quote('JOIN ' .. chan)
		g_rejoin_chan_list[chan] = nil
	else
		local nick = who:getnick()
		local my_nick = isrv:my_nick()
		if nick ~= my_nick then
			local ulist = isrv:get_chanuserlist(chan)
			if ulist and ulist[isrv:my_nick()] and 
				({ulist[isrv:my_nick()].chanusermode:gsub('[^~&@]', '')})[1] == '' then -- 봇한테 옵이 없다면
				for nick, item in pairs(ulist) do
					if nick ~= my_nick then
						-- 내가 아닌 다른 사람이 있을 경우 rejoin하지 않음
						return true
					end
				end
				g_rejoin_chan_list[chan] = true
				isrv:quote('PART ' .. chan)
				isrv:quote('JOIN ' .. chan)
				print('rejoining channel to regain op: ' .. chan)
			end
		end
	end
	
	return true
end

function init(bot)
	bot:register_event_handler('part', part_handler);
end

function cleanup(bot)
	bot:unregister_event_handler('part');
end





