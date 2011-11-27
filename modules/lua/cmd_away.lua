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



away_list = {}

function cmd_away(isrv, who, msginfo, mdest, cmd, carg)
	
	local netname = isrv:get_netname()
	local nick = who:getnick()
	local reason = carg
	
	if #nick < 4 then
		isrv:privmsg_nh(mdest, '닉이 너무 짧습니다. 닉이 4바이트 이상이어야 사용 가능합니다. (영어 1바이트, 한글 3바이트)')
		return
	end
	
	if not away_list[netname] then away_list[netname] = {} end
	away_list[netname][nick] = {
		time = os.time();
		reason = reason;
		chan = msginfo:getchan();
	}
	isrv:privmsg_nh(mdest, nick..', 부재중으로 설정합니다 ('..reason..')')
	
end


function nick_handler(isrv, who, newnick)
	
	local netname = isrv:get_netname()
	local orignick = who:getnick()
	if not away_list[netname] then away_list[netname] = {} end
	if away_list[netname][orignick] then
		away_list[netname][newnick] = away_list[netname][orignick]
		away_list[netname][orignick] = nil
	end
	
	return true
end

function part_handler(isrv, who, chan, partmsg)
	
	local netname = isrv:get_netname()
	local nick = who:getnick()
	if not away_list[netname] then away_list[netname] = {} end
	if away_list[netname][nick] then
		away_list[netname][nick] = nil
	end
	
	return true
end

function kick_handler(isrv, who, chan, kicked_user, kickmsg)
	
	local netname = isrv:get_netname()
	local nick = kicked_user:getnick()
	if not away_list[netname] then away_list[netname] = {} end
	if away_list[netname][nick] then
		away_list[netname][nick] = nil
	end
	
	return true
end

function quit_handler(isrv, who, quitmsg)
	
	local netname = isrv:get_netname()
	local nick = who:getnick()
	if not away_list[netname] then away_list[netname] = {} end
	if away_list[netname][nick] then
		away_list[netname][nick] = nil
	end
	
	return true
end


function cancel_all_away_list(bot)
	
	if not away_list[netname] then away_list[netname] = {} end
	for nick, v in pairs(away_list) do
		
	end
end

function privmsg_handler(isrv, who, msginfo, msg)
	
	local netname = isrv:get_netname()
	local chan = msginfo:getchan()
	local nick = who:getnick()
	
	if not away_list[netname] then away_list[netname] = {} end
	if away_list[netname][nick] then
		local v = away_list[netname][nick]
		local dtime = os.time() - away_list[netname][nick].time
		isrv:privmsg_nh(chan, string.format('%s, 부재중 설정을 해제합니다 (%d초 동안 부재; 이유: %s)', 
			nick, dtime, v.reason))
		away_list[netname][nick] = nil
	end
	
	local lmsg = msg:irc_lower()
	for anick, v in pairs(away_list[netname]) do
		if lmsg:find(anick:irc_lower()) then
			isrv:privmsgf(irc_msginfo.new(nick, 'notice'), 
				'%s 님은 부재중입니다 (%s)', anick, v.reason)
			break -- 도배 방지: 1개만
		end
	end
	
end


function init(bot)
	bot:register_cmd('away', cmd_away);
	bot:register_cmd('부재', cmd_away);
	bot:register_event_handler('nick', nick_handler);
	bot:register_event_handler('part', part_handler);
	bot:register_event_handler('kick', kick_handler);
	bot:register_event_handler('quit', quit_handler);
	bot:register_event_handler('privmsg', privmsg_handler);
end

function cleanup(bot)
	bot:unregister_cmd('away');
	bot:unregister_cmd('부재');
	bot:unregister_event_handler('nick');
	bot:unregister_event_handler('part');
	bot:unregister_event_handler('kick');
	bot:unregister_event_handler('quit');
	bot:unregister_event_handler('privmsg');
	cancel_all_alarms(bot)
end




