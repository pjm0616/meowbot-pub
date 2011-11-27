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




IRC_PRIORITY_NORMAL = 0

max_alarms_per_nick = 5

alarm_list = {}

re_checkint = pcre.new('([0-9]+)', 'f');


function get_number_of_alarms_by_nick(nick)

	local n, i, v;
	n = 0
	for i, v in pairs(alarm_list) do
		if v['nick'] == nick then
			n = n + 1
		end
	end
	
	return n;
end

function format_time(time)
	
	return os.date('%m/%d %H:%M:%S', time)
end

re_alarm_carg = pcre.new('([^ ]+) *(.*)', 'f');
function cmd_alarm(isrv, who, msginfo, mdest, cmd, carg)
	
	local v = re_alarm_carg:full_match(carg);
	if v == false then
		isrv:privmsg_nh(mdest, '사용법: !알람 <알람할 시간>[단위] [메시지]');
		isrv:privmsg_nh(mdest, '예: !알람 !알람 5분20초 ㅁㄴㅇㄹ');
		return;
	end
	local nick = who:getnick()
	local timestr = v[1]
	local alarm_msg = v[2]
	
	if get_number_of_alarms_by_nick(nick) >= max_alarms_per_nick then
		isrv:privmsg_nh(mdest, '동시에 ' .. max_alarms_per_nick .. '개 이상 알람을 설정 할 수 없습니다.');
		return
	end
	
	local time = timestr_to_time(timestr)
	if time <= 0 or time > 3600 * 24 then
		isrv:privmsg_nh(mdest, '올바르지 않은 시간입니다: ' .. timestr .. ' (24시간 이내만 가능합니다.)');
		return
	end
	
	table.insert(alarm_list, 
		{
			['conn_name'] = isrv:get_name(), 
			['nick'] = nick, 
			['time'] = os.time(), -- 알람을 요청한 시각
			['alarm_sec'] = time, -- 요청한 시간
			['etime'] = os.time() + time, -- 알람을 할 시각
			['chan'] = msginfo, 
			['alarm_msg'] = alarm_msg
		})
	
	isrv:privmsg_nh(mdest, who:getnick() .. ', ' .. time .. '초 후에 설호합니다.');
	--isrv:privmsg_nh(irc_msginfo(who, 'notice'), who:getnick() .. ', ' .. time .. '초 후에 설호합니다.');
end

function cmd_alarm_list(isrv, who, msginfo, mdest, cmd, carg)
	
	local n = 0
	local cname = isrv:get_name()
	local nick = who:getnick()
	for i, v in pairs(alarm_list) do
		if v['conn_name'] == cname and v['nick'] == nick then
			isrv:privmsgf_nh(mdest, '%d번: %s%J{이가} %s에 %s 채널에서 %d초 알람 요청(%s), 예정시각 %s, 남은시간 %d초', 
				i, nick, format_time(v.time), v.chan:getchan(), v.alarm_sec, 
				v.alarm_msg, format_time(v.etime), v.etime - os.time());
			n = n + 1
		end
	end
	
	if n == 0 then
		isrv:privmsg_nh(mdest, nick .. '에 대한 결과가 없습니다.');
	end
	
end

function cmd_cancel_alarm(isrv, who, msginfo, mdest, cmd, carg)
	
	if re_checkint:full_match(carg) == false then
		isrv:privmsg(mdest, '사용법: !알람취소 <번호> | 번호를 모르면 !알람목록')
		return
	end
	local cname = isrv:get_name()
	local n = carg + 0
	local nick = who:getnick()
	if alarm_list[n] == nil then
		isrv:privmsg(mdest, n .. '번 알람을 찾을 수 없습니다.')
		return
	end
	if alarm_list[n].conn_name ~= cname or alarm_list[n].nick ~= nick then
		isrv:privmsg(mdest, '다른 사람이 설정한 알람은 취소할 수 없습니다.')
		return
	end
	alarm_list[n] = nil
	isrv:privmsg(mdest, '알람이 취소되었습니다.')
	
end

function idle_handler(isrv)
	
	local cname = isrv:get_name()
	for i, value in pairs(alarm_list) do
		if value.conn_name == cname and os.time() >= value.etime then
			
			local nick = value.nick
			local alarm_msg = value.alarm_msg
			if alarm_msg ~= '' then
				alarm_msg = ' (' .. alarm_msg .. ')'
			end
			
			local dest = irc_msginfo.new(value.chan)
			-- local dest = irc_msginfo.new(nick, 'notice')
			dest:setpriority(IRC_PRIORITY_NORMAL)
			isrv:privmsg(dest, nick .. ', 알람한 시각입니다.' .. alarm_msg)
			
			alarm_list[i] = nil
		end
	end
	
	return true
end

function nick_handler(isrv, who, newnick)
	
	local cname = isrv:get_name()
	local orignick = who:getnick()
	for i, value in pairs(alarm_list) do
		if value.conn_name == cname and value['nick'] == orignick then
			alarm_list[i]['nick'] = newnick
		end
	end
	
	return true
end

function part_handler(isrv, who, chan, partmsg)
	
	local cname = isrv:get_name()
	local nick = who:getnick()
	for i, value in pairs(alarm_list) do
		if value.conn_name == cname and value['nick'] == nick and value.chan == chan then
			alarm_list[i] = nil
		end
	end
	
	return true
end

function kick_handler(isrv, who, chan, kicked_user, kickmsg)
	
	local cname = isrv:get_name()
	local nick = kicked_user:getnick()
	for i, value in pairs(alarm_list) do
		if value.conn_name == cname and value['nick'] == nick and value.chan == chan then
			alarm_list[i] = nil
		end
	end
	
	return true
end

function quit_handler(isrv, who, quitmsg)
	
	local cname = isrv:get_name()
	local nick = who:getnick()
	for i, value in pairs(alarm_list) do
		if value.conn_name == cname and value['nick'] == nick then
			alarm_list[i] = nil
		end
	end
	
	return true
end


function cancel_all_alarms(bot)
	
	local i, v
	for i, v in pairs(alarm_list) do
		bot:get_conn_by_name(v.conn_name):privmsgf(v.chan, 
			'내부 사정으로 인해 알람이 취소됩니다: %d번: %s%J{이가} %s에 %s 채널에서 %d초 알람 요청(%s), 예정시각 %s, 남은시간 %d초', 
			i, v.nick, format_time(v.time), v.chan:getchan(), v.alarm_sec, 
			v.alarm_msg, format_time(v.etime), v.etime - os.time());
	end
end



function init(bot)
	bot:register_cmd('알람', cmd_alarm);
	bot:register_cmd('알람목록', cmd_alarm_list);
	bot:register_cmd('알람취소', cmd_cancel_alarm);
	bot:register_event_handler('idle', idle_handler);
	bot:register_event_handler('nick', nick_handler);
	bot:register_event_handler('part', part_handler);
	bot:register_event_handler('kick', kick_handler);
	bot:register_event_handler('quit', quit_handler);
end

function cleanup(bot)
	bot:unregister_cmd('알람');
	bot:unregister_cmd('알람목록');
	bot:unregister_cmd('알람취소');
	bot:unregister_event_handler('idle');
	bot:unregister_event_handler('nick');
	bot:unregister_event_handler('part');
	bot:unregister_event_handler('kick');
	bot:unregister_event_handler('quit');
	cancel_all_alarms(bot)
end




