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


require 'varpacker'

cfgfile = './data/dday_list.txt'
dday_list = {}

--dday_list
--{
--	[1] = 
--	{
--		who = 'ident', 
--		datestr = 'datestr', 
--		time = 'time', 
--		when = 'when'
--	}
--}



function format_time(time)
	return os.date('%Y/%m/%d %H:%M:%S', time)
end


function cmd_dday_add(isrv, who, msginfo, mdest, cmd, carg)
	local name, str = string.match(carg, '([^ ]+) (.+)')
	if not name then
		isrv:privmsg_nh(mdest, '사용법: !dday 추가 <이름> <날짜>')
		return
	end
	if dday_list[name] then
		isrv:privmsg_nh(mdest, '이미 등록되어있습니다. 변경하시려면 먼저 !디데이 삭제 ' .. name .. ' 로 지우세요')
		return
	end
	
	local time, errmsg = datestr_to_time(str)
	if errmsg then
		isrv:privmsg_nh(mdest, '오류: ' .. errmsg)
		return
	end
	dday_list[name] = {
		ident = who:getident(), 
		datestr = str, 
		time = time, 
		when = os.time()
	};
	isrv:privmsg_nh(mdest, '성공 (' .. format_time(time) .. ')')
end

function cmd_dday_del(isrv, who, msginfo, mdest, cmd, carg)
	
	local name = string.gsub(carg, ' ', '')
	local item = dday_list[name]
	if not item then
		isrv:privmsg_nh(mdest, '찾을 수 없습니다.')
		return
	end
	dday_list[name] = nil
	isrv:privmsg_nh(mdest, '삭제되었습니다.')
end

function cmd_dday_view(isrv, who, msginfo, mdest, cmd, carg)

	local name = string.gsub(carg, ' ', '')
	local item = dday_list[name]
	if not item then
		isrv:privmsg_nh(mdest, '찾을 수 없습니다.')
		return
	end
	local time = item.time
	local rtime = time - os.time()
	if rtime > 0 then
		isrv:privmsg_nh(mdest, string.format(
			"%s: %s까지 %s 남았습니다.", 
			name, os.date('%Y/%m/%d %H:%M:%S', time), 
			os.date('%d일 %H시간 %M 분 %S초', rtime)
			))
	elseif rtime < 0 then
		isrv:privmsg_nh(mdest, string.format(
			"%s: %s부터 %s 지났습니다.", 
			name, os.date('%Y/%m/%d %H:%M:%S', time), 
			os.date('%d일 %H시간 %M 분 %S초', -rtime)
			))
	else -- rtime == 0 then
		isrv:privmsg_nh(mdest, string.format(
			"%s: 지금이 %s입니다.", 
			name, os.date('%Y/%m/%d %H:%M:%S', time)
			))
	end
end

re_split_cmd = pcre.new(' *([^ ]+) *(.*)')
function cmd_dday(isrv, who, msginfo, mdest, cmd, carg)
	
	local ret = re_split_cmd:full_match(carg)
	if not ret then
		isrv:privmsg_nh(mdest, '사용법: !dday 추가 <이름> <날짜> | !dday 삭제 이름 | !dday <이름>')
		return
	end
	local cmd2, carg2 = ret[1], ret[2]
	if cmd2 == 'add' or cmd2 == '추가' then
		cmd_dday_add(isrv, who, msginfo, mdest, cmd2, carg2)
	elseif cmd2 == 'del' or cmd2 == '삭제' then
		cmd_dday_del(isrv, who, msginfo, mdest, cmd2, carg2)
	else
		cmd_dday_view(isrv, who, msginfo, mdest, cmd, carg)
	end
	
end


function init(bot)
	
	bot:register_cmd('dday', cmd_dday);
	bot:register_cmd('디데이', cmd_dday);
	
	dofile_nr(cfgfile)
end

function cleanup(bot)
	
	bot:unregister_cmd('dday');
	bot:unregister_cmd('디데이');
	
	save_var_to_file(cfgfile, {
			dday_list = dday_list
		})
end



