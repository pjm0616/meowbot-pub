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
-- 서로 다른 네트워크 간의 대화를 중계해주는 스크립트

-- 네트워크 중계 봇 설정. next 값은 바꾸면 안됩니다.
local relay_cfg = 
{
	{
		chan = '#중계할채널명';
		netlist = {
			{next = 1; netname = 'Luatic'; bots = {'Luatic-rbot1', 'Luatic-rbot2', 'Luatic-rbot3'}}, 
			{next = 1; netname = 'HanIRC'; bots = {'HanIRC-rbot1', 'HanIRC-rbot2'}}, 
			{next = 1; netname = 'DankunNET'; bots = {'DankunNET-rbot1', 'DankunNET-rbot2'}}, 
		};
	}, 
	{
		chan = '#중계할채널명2';
		netlist = {
			{next = 1; netname = 'Luatic'; bots = {'Luatic-rbot1', 'Luatic-rbot2', 'Luatic-rbot3'}}, 
			{next = 1; netname = 'HanIRC'; bots = {'HanIRC-rbot1', 'HanIRC-rbot2'}}, 
			{next = 1; netname = 'DankunNET'; bots = {'DankunNET-rbot1', 'DankunNET-rbot2'}}, 
		};
	}, 
};


function find_relay_idx(cname, chan)
	for ri, v in pairs(relay_cfg) do
		if v.chan == chan then
			for ni, nd in pairs(v.netlist) do
				for _, cname2 in pairs(nd.bots) do
					if cname == cname2 then
						return ri, ni
					end
				end
			end
		end
	end
	return nil
end

function find_relay_idx_firstbot(cname, chan)
	for ri, v in pairs(relay_cfg) do
		if v.chan == chan then
			for ni, nd in pairs(v.netlist) do
				if nd.bots[1] == cname then
					return ri, ni
				end
			end
		end
	end
	return nil
end


function get_nextbot(bot, nd)
	local next1 = nd.next
	while true do
		local isrv_dest = bot:get_conn_by_name(nd.bots[nd.next])
		if nd.next+1 > #nd.bots then
			nd.next = 1
		else
			nd.next = nd.next + 1
		end
		if isrv_dest and isrv_dest:get_status() == 'connected' then
			return isrv_dest
		end
		if nd.next == next1 then
			return false
		end
	end
end

function sendtoall_butone(bot, ridx, nidx, mdest, msg)
	local netlist = relay_cfg[ridx].netlist
	for i, nd in pairs(netlist) do
		if i ~= nidx then
			local isrv_dest = get_nextbot(bot, nd)
			if isrv_dest then
				isrv_dest:privmsg_nh(mdest, '\00315['.. relay_cfg[ridx].netlist[nidx].netname ..']\015 '..msg)
			end
		end
	end
end


color_list = {2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13}
function color_nick(nick)
	local idx = md5_int32(nick)
	local color = color_list[idx%(#color_list) + 1]
	return string.format('\003%02d%s\015', color, nick)
end

function is_nick_joined(isrv, chan, nick)
	local ni = isrv:get_chanuserinfo(chan, nick)
	if ni then
		return true
	else
		return false
	end
end





function join_handler(isrv, who, chan)
	local cname = isrv:get_name()
	local nick = who:getnick()
	
	if nick ~= isrv:my_nick() then
		local ridx, nidx = find_relay_idx_firstbot(cname, chan)
		if ridx then
			sendtoall_butone(isrv:getbot(), ridx, nidx, chan, string.format('%s (%s@%s) 님이 참여하셨습니다', nick, who:getuser(), who:gethost()))
		end
	end
	
	return true
end

function kick_handler(isrv, who, chan, kicked_user, kickmsg)
	local cname = isrv:get_name()
	local nick = who:getnick()
	
	if nick ~= isrv:my_nick() then
		local ridx, nidx = find_relay_idx_firstbot(cname, chan);
		if ridx then
			sendtoall_butone(isrv:getbot(), ridx, nidx, chan, string.format('%s (%s@%s)이(가) %s (%s@%s)을(를) 킥했습니다 (%s)', 
				nick, who:getuser(), who:gethost(), 
				kicked_user:getnick(), kicked_user:getuser(), kicked_user:gethost(), kickmsg))
		end
	end
	
	return true
end


function privmsg_handler(isrv, who, msginfo, msg)
	local cname = isrv:get_name()
	local chan = msginfo:getchan()
	local nick = who:getnick()
	
	if nick ~= isrv:my_nick() then
		local ridx, nidx = find_relay_idx_firstbot(cname, chan)
		if ridx then
			for _,lcname in pairs(relay_cfg[ridx].netlist[nidx].bots) do
				local isrv_tmp = isrv:getbot():get_conn_by_name(lcname)
				if isrv_tmp and isrv_tmp:my_nick() == nick then
					return true
				end
			end
			
			local mdest
			if msginfo:getmsgtype() == 'action' then
				mdest = irc_msginfo.new(chan, 'action')
			else
				mdest = chan
			end
			sendtoall_butone(isrv:getbot(), ridx, nidx, mdest, '<'..color_nick(nick)..'> '..msg)
		end
	end
	
	return true
end

function part_handler(isrv, who, chan, partmsg)
	local cname = isrv:get_name()
	local nick = who:getnick()
	
	if nick ~= isrv:my_nick() then
		local ridx, nidx = find_relay_idx_firstbot(cname, chan)
		if ridx then
			sendtoall_butone(isrv:getbot(), ridx, nidx, chan, string.format('%s (%s@%s) 님이 나가셨습니다 (%s)', 
				nick, who:getuser(), who:gethost(), partmsg))
		end
	end
	
	return true
end

function nick_handler(isrv, who, newnick)
	local cname = isrv:get_name()
	local nick = who:getnick()
	
	if nick ~= isrv:my_nick() then
		for ridx, rd in pairs(relay_cfg) do
			for nidx, nd in pairs(rd.netlist) do
				if cname == nd.bots[1] then
					if is_nick_joined(isrv, rd.chan, newnick) then
						sendtoall_butone(isrv:getbot(), ridx, nidx, rd.chan, string.format('%s (%s@%s) 님이 닉네임을 %s(으)로 변경하셨습니다', 
							nick, who:getuser(), who:gethost(), newnick))
					end
					break
				end
			end
		end
	end
	
	return true
end

function quit_handler(isrv, who, quitmsg)
	local cname = isrv:get_name()
	local nick = who:getnick()
	
	if nick ~= isrv:my_nick() then
		for ridx, rd in pairs(relay_cfg) do
			for nidx, nd in pairs(rd.netlist) do
				if cname == nd.bots[1] then
					if is_nick_joined(isrv, rd.chan, nick) then
						sendtoall_butone(isrv:getbot(), ridx, nidx, chan, string.format('%s (%s@%s) 님이 종료하셨습니다 (%s)', 
							nick, who:getuser(), who:gethost(), quitmsg))
					end
					break
				end
			end
		end
	end
	
	return true
end


-- 중계 봇에서 명령어를 사용하지 않도록 하려면 아래의 bot:(un)register_event_handler('botcommand' 부분을 주석 해제하세요
function botcommand_handler(isrv, who, msginfo, mdest, cmd_trigger, cmd, carg)
	local cname = isrv:get_name()
	local chan = msginfo:getchan()
	
	if find_relay_idx(cname, chan) then
		return false
	end
	
	return true
end



function init(bot)
	--bot:register_event_handler('botcommand', botcommand_handler);
	bot:register_event_handler('privmsg', privmsg_handler);
	bot:register_event_handler('join', join_handler);
	bot:register_event_handler('kick', kick_handler);
	bot:register_event_handler('part', part_handler);
	bot:register_event_handler('nick', nick_handler);
	bot:register_event_handler('quit', quit_handler);
end

function cleanup(bot)
	--bot:unregister_event_handler('botcommand');
	bot:unregister_event_handler('privmsg');
	bot:unregister_event_handler('join');
	bot:unregister_event_handler('kick');
	bot:unregister_event_handler('part');
	bot:unregister_event_handler('nick');
	bot:unregister_event_handler('quit');
end





