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



invite_list = {}

function join_handler(isrv, who, chan)
	
	if who:getnick() == isrv:my_nick() then
		if invite_list[chan] ~= nil then
			isrv:privmsg(chan, prevent_irc_highlighting(invite_list[chan], 
				'안녕하세요. ' .. invite_list[chan] .. '님이 초대한 봇입니다. ' .. 
				'사용법은: !도움말, 문의는 S·IGSEGV에게 쿼리로 해주세요. ' .. 
				'    \2|\15 봇을 혼자 쓸 경우에는, 채널에 초대할 필요 없이 쿼리로 명령을 사용할 수 있습니다.'
				))
			invite_list[chan] = nil
		end
	else
		if not (chan == '#.' or chan == '#-' or chan == '#/' or 
			chan == '#meowbot' or chan == '#lnople' or chan == '#Eaeriaea') then
			local nick = who:getnick()
			local user = who:getuser()
			local host = who:gethost()
			if (user == 'meowbot' or user == '~meowbot') and 
				(host == '61.250.113.98' or host == '61.250.112.4' or host == '61.250.112.9') then
				isrv:quote('MODE ' .. chan .. ' +ov ' .. nick .. ' ' .. nick)
				isrv:quote('PART ' .. chan .. ' :다른 봇이 들어왔기 때문에 퇴장합니다.')
			end
		end
	end
	
	return true
end

function invite_handler(isrv, who, chan)
	
	local cname = isrv:get_name()
	print('['..cname..'] invited to ' .. chan .. ' by ' .. who:getident())
	io.flush();
	
	if cname:sub(1, 10) ~= 'HanIRC-bot' then
		return true
	end
	if cname == 'HanIRC-bot1' then
		if chan ~= '#프겔' and chan ~= '#lnople' and chan ~= '#?' and 
			chan ~= '#Eaeriaea' and chan ~= '#^^Jy!' and chan ~= '#ycc' then
			isrv:privmsg(irc_msginfo.new(who:getnick(), 'notice'), '냐옹이2를 초대해보세요.')
			return true
		end
	end
	
	local clist = isrv:get_chanlist()
	if #clist >= 20 then
		isrv:privmsgf(irc_msginfo.new(who:getnick(), 'notice'), '채널 20개가 찼습니다. 냐옹이2, 냐옹이3, 냐옹이4, 냐옹이5, 봇 %J{을를} 초대해보세요.')
	else
		invite_list[chan] = who:getnick()
		isrv:quote('JOIN ' .. chan)
		--isrv:privmsg('#^', 'INVITED TO '..chan..' BY '..who:getident()) --############### LOGGING #####
	end
	
	return true
end


function part_handler(isrv, who, chan, partmsg)
	--if isrv:my_nick() == who:getnick() then
	--	isrv:privmsg('#^', 'PART '..chan) --############### LOGGING #####
	--end
	return true
end
function kick_handler(isrv, who, chan, kicked_user, kickmsg)
	
	if kicked_user:getnick() == isrv:my_nick() then
		print(isrv:my_nick()..' kicked from '..chan..' by '..who:getident()..' : '..kickmsg)
		io.flush()
	end
	
	--if isrv:my_nick() == kicked_user:getnick() then
	--	isrv:privmsg('#^', 'KICKED FROM '..chan..' BY '..who:getident() .. ' [' .. kickmsg .. ']') --############### LOGGING #####
	--end
	return true
end


function init(bot)
	bot:register_event_handler('join', join_handler);
	--bot:register_event_handler('part', part_handler);
	bot:register_event_handler('kick', kick_handler);
	bot:register_event_handler('invite', invite_handler);
end

function cleanup(bot)
	bot:unregister_event_handler('join');
	--bot:unregister_event_handler('part');
	bot:unregister_event_handler('kick');
	bot:unregister_event_handler('invite');
end





