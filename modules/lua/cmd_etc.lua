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


require 'etclib'



function check_tbl_val(tbl, val)
	
	for idx, v in pairs(tbl) do
		if v == val then
			return true
		end
	end
	
	return false
end




-- URL, 설명, 크기
re_google_image = pcre.new('\\["[^"]*+","[^"]*+","[^"]*+","([^"]*+)","[^"]*+","[^"]*+","([^"]*+)","[^"]*+","[^"]*+","([^"]*+)",[^\\]]++\\],"[^"]*+"]', 'u');
re_unescape_hex = pcre.new('\\\\x([0-9a-fA-F]{1,2})');
re_strip_tags = pcre.new('<[^>]+>');
function google_image(kwd)
	local url = "http://images.google.co.kr/images?hl=ko&q=" .. urlencode(kwd)
	local data = get_http(url)
	local items = re_google_image:match_all(data)
	local result = {}
	for i, v in pairs(items) do
		v[1] = string.gsub(v[1], '%%3[fF]', '?')
		v[1] = string.gsub(v[1], '%%3[dD]', '=')
		v[1] = string.gsub(v[1], '%%26', '&')
		v[2] = decode_html_entities(
			re_unescape_hex:replace_cb(v[2], 
				function(vec)
					return string.char(atoi(vec[1], 16))
				end
			))
		v[2] = re_strip_tags:replace('', v[2])
		result[i] = 
			{
				['url'] = v[1], 
				['desc'] = v[2], 
				['size'] = v[3]
			}
	end
	return result
end


function cmd_google_image(isrv, who, msginfo, mdest, cmd, carg)
	if carg == '' then
		isrv:privmsg_nh(mdest, '사용법: !ggi <검색어>')
		return
	end
	
	local items = google_image(carg);
	if #items == 0 then
		isrv:privmsg_nh(mdest, '검색 결과가 없습니다.')
	else
		for i,v in pairs(items) do
			--isrv:privmsg(mdest, string.format(
			--	"%s - %s, %s", v.url, v.size, v.desc
			--	))
			isrv:privmsg(mdest, v.url)
			if i >= 3 then break end
		end
	end
end

function cmd_help(isrv, who, msginfo, mdest, cmd, carg)
	isrv:privmsg_nh(mdest, "명령어 목록: !명령어 | '!명령' or ',명령': 채널로 출력, '`명령': 명령어를 입력한 사람한테 노티스로 출력 | DCC로 파일을 보내면 봇 서버에 업로드");
end

function cmd_command_list(isrv, who, msginfo, mdest, cmd, carg)
	isrv:privmsg_nh(mdest, '명령어 목록: !기억해 !알려줘 !찾아줘 !덧붙여 !연결 !본문검색 !계산 !c !cpp !perl !python !ruby !php !javascript !lua !basic !md5 !b64e !b64d !urlenc !urldec !urlencp !정규식 !구글 !시간 !웹서버 !번역 !예전기억 !냐옹이나가 !주민번호 !디씨 !디씨최근글 !디씨댓글 !날씨 !기억실행 !도움말 !인원수 !타로 !퍼센트 !퍼센트2 !골라 !한자 !끝말치트, ');
	isrv:privmsg_nh(mdest, ': !bmi !단어확인 !끝말2 !ipw !asw !ipsn !htmlentdec !맞춤법 !디씨글작성 !디씨댓글작성 !뒤집기 !촛엉 !종올 !운세 !롑흔리나 !자음뒤집기 !초성어 !궁합 !환율 !tv편성표 !ggi !알람 !c2 !로또 !dday !구글대화 !i대화 !i알려 !자동옵 !자동반옵 !자동보이스 !@ !% !+ !geoip !dcrss !라그랑주 !u2c !c2u !ng !세계인구 !부재 !인원 !kgeoip !사전 !예문');
	isrv:privmsg_nh(mdest, ': !!강화 !!마누라 !!씨프트 !!로꾸꺼 !!퍼센트 !!씨프트 !!커널')
end

function cmd_ping(isrv, who, msginfo, mdest, cmd, carg)
	isrv:privmsgf(mdest, '%s, pong!', who:getnick(), carg)
end

function cmd_lotto(isrv, who, msginfo, mdest, cmd, carg)
	function make_lotto_number()
	
		local nums, ret = {}, {}
		for i=1,46 do nums[i] = i end
		for i=1,6 do
			local n = math.random(1, #nums)
			ret[i] = nums[n]
			table.remove(nums, n)
		end
		table.sort(ret)
	
		return ret[1], ret[2], ret[3], ret[4], ret[5], ret[6]
	end
	
	local s = table.concat({make_lotto_number()}, ', ')
	isrv:privmsg(mdest, s);
end

function cmd_world_population(isrv, who, msginfo, mdest, cmd, carg)
	local data = get_http('http://www.census.gov/main/www/popclock.html')
	if not data then
		isrv:privmsg(mdest, '서버 접속 실패')
		return
	end
	
	local matches = pcre.new('<span id="wclocknum">([^<]+)</span>'):match(data)
	isrv:privmsg(mdest, matches[1]..'명')
end

function cmd_utf8_to_cp949(isrv, who, msginfo, mdest, cmd, carg)
	if carg == '' then
		isrv:privmsg(mdest, 'utf-8을 cp949로 변환해서 출력합니다.')
		return
	end
	-- carg is in utf-8, so i can just print without a convert
	isrv:privmsg_nh(mdest, carg)
end

function cmd_cp949_to_utf8(isrv, who, msginfo, mdest, cmd, carg)
	if carg == '' then
		isrv:privmsg(mdest, 'cp949를 utf-8로 변환해서 출력합니다.')
		return
	end
	
	isrv:quote('PRIVMSG '..mdest:getchan():encode('cp949')..' :'..carg, -1, 0) -- priority: -1, do not convert enc
end

function cmd_channickmode(isrv, who, msginfo, mdest, cmd, carg)
	
	if msginfo:getchantype() ~= 'chan' then
		isrv:privmsg_nh(mdest, '채널이 아닙니다')
		return
	end
	local chan = msginfo:getchan()
	local ulist = isrv:get_chanuserlist(chan)
	
	local nuser = 0
	local nuserm = {['~']=0, ['&']=0, ['@']=0, ['%']=0, ['+']=0}
	for nick,uinfo in pairs(ulist) do
		nuser = nuser + 1
		for mchar in uinfo.chanusermode:gmatch('(.)') do
			nuserm[mchar] = nuserm[mchar] + 1
		end
	end
	isrv:privmsgf(mdest, '총 %d명 | 채널주인(~) %d명 | 보호모드(&) %d명 | 옵(@) %d명 | 하프옵(%%) %d명 | 보이스(+) %d명', 
			nuser, nuserm['~'], nuserm['&'], nuserm['@'], nuserm['%'], nuserm['+'])
	
end



local re_ani_data = pcre.new('<td class=cont_01><img src="/images/ani_search/view/info.gif"> *([^<]+)</td>[ \t\r\n]*<td(?: colspan=3)?>[ \t\r\n]*((?:[^<\t]++|<(?!/td>))+)[ \t\r\n]*</td>', 's')
local re_strip_tags = pcre.new('<[^>]*>', 's')
function cmd_anisrch(isrv, who, msginfo, mdest, cmd, carg)
	
	if carg == '' then
		isrv:privmsg(mdest, '사용법: !애니 <검색어>')
		return
	end
	
	local srchstr = carg
	local data = get_http('http://bestanime.co.kr/newAniData/list.php?' .. 
		'searchKey=title&searchTmpKey=title&searchStr=' .. urlencode(srchstr:encode('cp949')) .. '&x=0&y=0')
	if not data then
		isrv:privmsg(mdest, '접속 실패')
		return
	end
	
	local matches = pcre.new('<a href="/newAniData/aniInfo\\.php\\?idx=([0-9]+)[^"]*+">'):match(data)
	if not matches then
		isrv:privmsg(mdest, '찾지 못했습니다.')
		return
	end
	local url2 = 'http://bestanime.co.kr/newAniData/aniInfo.php?idx=' .. matches[1]
	data = get_http(url2)
	if not data then
		isrv:privmsg(mdest, '접속 실패')
		return
	end
	data = data:decode('cp949')
	
	matches = re_ani_data:match_all(data)
	if not matches then
		isrv:privmsg(mdest, '내부 오류')
		return
	end
	
	local aniinfo = {}
	for _,m2 in pairs(matches) do
		m2[2] = re_strip_tags:replace('', decode_html_entities(m2[2]))
		aniinfo[m2[1]] = m2[2]
	end
	
	local print_list = {
		{'제목', '영제', '원제'}, 
		{'구분', '총화수', '음악', '제작국', '상영시간', 'BA 등급', '장르'}, 
		{'감독', '각본', '원작', '제작'}
	}
	isrv:privmsg(mdest, '주소: ' .. url2)
	for _,linelist in pairs(print_list) do
		local pstr = nil
		for _,name in pairs(linelist) do
			if aniinfo[name] then
				if pstr then
					pstr = pstr .. ' \2|\15 '
				else
					pstr = ''
				end
				pstr = pstr .. name .. ': ' .. aniinfo[name]
			end
		end
		if pstr then
			isrv:privmsg_nh(mdest, pstr)
		end
	end
	
end


function init(bot)
	bot:register_cmd('도움말', cmd_help);
	bot:register_cmd('help', cmd_help);
	
	bot:register_cmd('봇명령', cmd_command_list);
	bot:register_cmd('명령어', cmd_command_list);
	bot:register_cmd('명령', cmd_command_list);
	bot:register_cmd('commands', cmd_command_list);
	bot:register_cmd('showcommands', cmd_command_list);
	bot:register_cmd('cmds', cmd_command_list);
	
	bot:register_cmd('로또', cmd_lotto);
	bot:register_cmd('lotto', cmd_lotto);
	bot:register_cmd('ggi', cmd_google_image);
	bot:register_cmd('그림', cmd_google_image);
	bot:register_cmd('ping', cmd_ping);
	bot:register_cmd('핑', cmd_ping);
	
	bot:register_cmd('u2c', cmd_utf8_to_cp949);
	bot:register_cmd('c2u', cmd_cp949_to_utf8);
	
	bot:register_cmd('세계인구', cmd_world_population);
	bot:register_cmd('인원', cmd_channickmode);
	bot:register_cmd('애니', cmd_anisrch);
	
end

function cleanup(bot)
	bot:unregister_cmd('도움말');
	bot:unregister_cmd('help');
	
	bot:unregister_cmd('봇명령');
	bot:unregister_cmd('명령어');
	bot:unregister_cmd('명령');
	bot:unregister_cmd('commands');
	bot:unregister_cmd('showcommands');
	bot:unregister_cmd('cmds');
	
	bot:unregister_cmd('로또');
	bot:unregister_cmd('lotto');
	bot:unregister_cmd('ggi');
	bot:unregister_cmd('그림');
	bot:unregister_cmd('ping');
	bot:unregister_cmd('핑');
	
	bot:unregister_cmd('u2c');
	bot:unregister_cmd('c2u');
	
	bot:unregister_cmd('세계인구');
	bot:unregister_cmd('인원');
	bot:unregister_cmd('애니');
	
end





