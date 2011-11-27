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




lines_test1 = 
{
	'보ㅇㅕ주ㅓ ㄷㅐㄷㅏㅎㅏ', 
	'         ㄴ ㄴ 개그다 대단한 개그야 분위기를 타기'
}
lines_test2 = 
{
	'to the', 
	'to the men on the ground. At once about fifty of', 
	'the soldiers ran forward and cut the strings that', 
	'held my head and hands. Then the man made a long', 
	'speech. Of course, I did not know what he said,', 
	'but, as I was'
}
lines_test3 = 
{
	'그ㄱㅓ 보ㅇㅕ주ㄱㅔㄷㅏ느ㄱㅓㅇㅑ?', 
	'  ㄹ      ㅆ  ㄴ    ?  여러분들 오늘 정말 잘', 
	'오신거에요 콧방울 콧방울 콧방울 보여줘'
}


-- static varibles
--taja_space = '　' -- U+3000
re_taja_firstline = pcre.new('^  ⓩ   ([^［]+) ［제한 ([0-9.]++)초］', 'u') -- ⓩ: U+24e9
re_taja_line = pcre.new('^  (.)   (.++)', 'u')
re_replace_spaces = pcre.new('　') -- U+3000
re_strip_useless_spaces = pcre.new('[ \t]')

-- 
g_linecnt = 0
g_timelimit = 0
g_starttime = 0
g_lines = nil
g_autochan = '#타자'
g_finished_line = ''


--function cnum2num(nstr)
--	local code = nstr:decode_utf8()
--	-- ①: U+2460
--	if code >= 0x2460 and code <= 0x2473 then
--		return code - 0x245f
--	else
--		return 0
--	end
--end


function parse_lines(lines)
	
	local result = ''
	
	-- 1, 2번째 줄 처리
	local spos = 1
	while true do
		local cho, jung, jong = '', '', ''
		local c = lines[1]:utf8_substr(spos, 1)
		if c == '' then -- 첫줄의 끝
			lines[2] = lines[2]:utf8_substr(spos)
			if lines[2]:sub(1,2) == '  ' then
				-- ' '가 2개 연속으로 있는 경우
				lines[2] = lines[2]:sub(2)
			end
			result = result .. lines[2]
			break
		elseif hangul.is_consonant(c) then
			cho = c
			jung = lines[1]:utf8_substr(spos + 1, 1)
			jong = lines[2]:utf8_substr(spos + 1, 1)
			if jong == ' ' then jong = '' end
			result = result .. hangul.join_utf8char(cho, jung, jong)
			spos = spos + 2
		elseif hangul.is_syllable(c) then
			cho, jung = hangul.split_utf8char(c)
			local vowel2 = lines[1]:utf8_substr(spos + 1, 1)
			-- ㅚ 등의 모음 처리 (오ㅣ)
			if hangul.is_vowel(vowel2) then
				jung = hangul.merge_vowel_utf8char(jung, vowel2)
				jong = lines[2]:utf8_substr(spos + 1, 1)
				spos = spos + 1
			else
				jong = lines[2]:utf8_substr(spos, 1)
			end
			if jong == ' ' then jong = '' end
			result = result .. hangul.join_utf8char(cho, jung, jong)
			spos = spos + 1
		else
			result = result .. c
			spos = spos + 1
		end
	end
	-- 3번째 줄부터는 그대로 이어붙임
	for i = 3, #lines do
		result = result .. ' ' .. lines[i]
	end
	
	return result
end



--pause('')
--print(parse_lines(lines_test1))
--print(parse_lines(lines_test2))
--print(parse_lines(lines_test3))
--debug.debug()



function luatest_cb(isrv, who, msginfo, mdest, cmd, carg)
	g_autochan = carg
	isrv:privmsg_nh(mdest, '채널: ' .. carg);
end

function cmd_print_result(isrv, who, msginfo, mdest, cmd, carg)
	if carg == 'n' then
		isrv:privmsg(irc_msginfo.new('토마토', 'notice'), g_finished_line)
	else
		isrv:privmsg(mdest, g_finished_line)
	end
end

function privmsg_handler(isrv, who, msginfo, msg)
	local chan = msginfo:getchan()
	
	local r, n
	do
		-- (토마토|도마도)!~tomato@타자.users.HanIRC.org
		if chan == g_autochan and who:gethost() == '타자.users.HanIRC.org' and (who:getuser() == 'tomato' or who:getuser() == '~tomato') then
			msg = strip_irc_colors(msg)
			if g_linecnt == 0 then
				r = re_taja_firstline:full_match(msg)
				if r then
					g_linecnt = 1
					g_timelimit = r[2]
					g_starttime = os.time()
					g_lines = {}
					g_lines[1] = re_replace_spaces:replace(' ', re_strip_useless_spaces:replace('', r[1]))
					g_finished_line = ''
				end
			else
				r = re_taja_line:full_match(msg)
				if r then
					g_linecnt = g_linecnt + 1
					g_lines[g_linecnt] = re_replace_spaces:replace(' ', re_strip_useless_spaces:replace('', r[2]))
					if r[1] == '①' then
						-- 내용 출력 끝.
						g_finished_line = parse_lines(g_lines)
						g_linecnt = 0
						isrv:privmsg(chan, g_finished_line) -- 자동출력. 안쓸경우 주석처리
					end
				end
			end
		end
	end
	
	return true
end


function init(bot)
	bot:register_cmd('자동타자설정', luatest_cb);
	bot:register_cmd('타자출력', cmd_print_result);
	bot:register_event_handler('privmsg', privmsg_handler);
end

function cleanup(bot)
	bot:unregister_cmd('자동타자설정');
	bot:unregister_cmd('타자출력');
	bot:unregister_event_handler('privmsg');
end

-----------------
--  ⓩ   ㄴㅓ르　ㄱㅣ으 ［제한 65.7초］
--  ⑦   　　ㄹ　　ㄹㄹ　걷다　멍하니　너를　지금은　내　곁에　없는
--  ⑥   너를　그리워하네　바보처럼　잊을께　잊을께　잊을께
--  ⑤   슬픔은　없을것　같아요　우산없이　비오는　거리를
--  ④   걸어도　나는　행복할　것　같아요　내　안에　그대가
--  ③   왔잖아요　그대와　내가　마주쳤던　순간에　나는　다시
--  ②   태어난거죠　그대가　없던　어제에　나는　없던　것과
--  ①   같아요　기억조차　없는　걸요　어떡하죠	
---------------------
--  ⓩ   쥬스도ㅣㄲㅓㅇㅑ　꾸ㄲㅓ ［제한 17.5초］
--  ①   　　　ㄹ　　　　　ㄹ　ㄱ　나는야　케찹될꺼야　찌익　나는야
---------------------
--  ⓩ   그ㄱㅓ　보ㅇㅕ주ㄱㅔㄷㅏ느ㄱㅓㅇㅑ? ［제한 30.6초］
--  ②   　　ㄹ　　　　　　ㅆ　　ㄴ　　　　? 　여러분들　오늘　정말　잘
--  ①   오신거에요　콧방울　콧방울　콧방울　보여줘
---------------------










