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




re_parse_data = pcre.new('^([^ ]+) = (.*)$')
function get_inklbot_api_data(url)
	
	local s = sock.new()
	if not s:connect('barosl.com', 80, 4000) then
		return false
	end
	s:send('GET ' .. url .. ' HTTP/1.0\r\n');
	s:send('Host: barosl.com\r\n');
	s:send('Connection: close\r\n\r\n');
	
	local result = {}
	while true do
		line = s:readline(3000)
		if not line then break end
		
		if line == 'no' then
			result['error'] = 'no'
			break
		end
		local v = re_parse_data:full_match(line)
		if v then
			result[v[1]] = v[2]
		end
	end
	s:close()
	return result
end


function cmd_inklbot_memory(isrv, who, msginfo, mdest, cmd, carg)
	
	if carg == '' then
		isrv:privmsg_nh(mdest, '인클봇의 기억 데이터를 표시합니다. 사용법: !i알려 <단어>')
		return
	end
	
	local vars = get_inklbot_api_data('/inklbot/api/memory.php?enc=utf8&word=' .. urlencode(carg))
	------------------
	--word = 단어
	--desc = 내용
	--who = 작성자
	--time = 2009-01-10 15:29:34
	--time2 = 1231568974
	------------------
	if vars['error'] == 'no' then
		isrv:privmsg_nh(mdest, '찾지 못했습니다.')
	else
		isrv:privmsg_nh(mdest, string.format(
			"%s: %s (by %s at %s)", 
			vars['word'], vars['desc'], vars['who'], vars['time']
			))
	end
end

function cmd_inklbot_talk(isrv, who, msginfo, mdest, cmd, carg)
	
	if carg == '' then
		isrv:privmsg_nh(mdest, '인클봇의 대화 기능. 사용법: !i대화 <문장>')
		return
	end
	
	local vars = get_inklbot_api_data('http://barosl.com/inklbot/api/talk.php?enc=utf8&q=' .. urlencode(carg))
	------------------
	--a = 내용
	--time = 1190638971
	------------------
	if vars['error'] == 'no' then
		isrv:privmsg_nh(mdest, '모르는 말이에요.')
	else
		local msg = string.gsub(vars['a'], '$nick', who:getnick())
		isrv:privmsg_nh(mdest, msg)
	end
end



function init(bot)
	bot:register_cmd('i알려', cmd_inklbot_memory);
	bot:register_cmd('i알려줘', cmd_inklbot_memory);
	bot:register_cmd('i대화', cmd_inklbot_talk);
	bot:register_cmd('대화', cmd_inklbot_talk);
end

function cleanup(bot)
	bot:unregister_cmd('i알려');
	bot:unregister_cmd('i알려줘');
	bot:unregister_cmd('i대화');
	bot:unregister_cmd('대화');
end





