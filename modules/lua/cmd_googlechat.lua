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



re_google_search_result = pcre.new("<nobr>[0-9]+\\. <a title=\"([^\"]*)\" href=([^>]+)>([^<]*)</a><br></nobr>")
function google_search(keyword, page)
	
	if not page then page = 1 end
	data = get_http("http://www.google.co.kr/ie?q=" .. 
		urlencode(keyword) .. "&hl=ko&start=" .. tostring(page * 10))
	if not data then return {} end
	data = data:gsub("</?em>", '')
	
	result = {}
	matches = re_google_search_result:match_all(data)
	for n, r in pairs(matches) do
		result[n] = {
			['content'] = decode_html_entities(decode_html_entities(r[1])), 
			['url'] = r[2], 
			['title'] = decode_html_entities(decode_html_entities(r[3]))
		};
	end
	
	return result
end



-- type1은 ' '가 무조건 포함
-- type2는 ' '가 없을 수도 있음

re_get_last_two_word = pcre.new('([^ ]+) +([^ ]+) *$', 'u')
gg_random_pagelist = {1,1,1,1,2,2,3}
function ggchat_getnext(str)
	
	local matches = re_get_last_two_word:match(str)
	local srchstr
	if matches then
		srchstr = matches[1] .. ' ' ..matches[2]
	else
		srchstr = str
	end
	srchstr = srchstr:gsub('.!?\'"', '')
	
	local page = gg_random_pagelist[math.random(#gg_random_pagelist)]
	local sres = google_search(srchstr, page)
	local wdslist = {}
	for _,item in pairs(sres) do
		local m1 = pcre.new(pcre.quote_meta(srchstr)..' *([0-9a-zA-Z가-힣ㄱ-ㅎㅏ-ㅣ_.!?\'"]+)') -- type1
		--local m1 = pcre.new(pcre.quote_meta(srchstr)..' *?( ?[0-9a-zA-Z가-힣ㄱ-ㅎㅏ-ㅣ_.!?\'"]+)') -- type2
			:match_all(item.content:lower())
		if m1 then
			for _,wd in pairs(m1) do
				wdslist[#wdslist+1] = wd[1]
			end
		end
	end
	--for i,v in pairs(wdslist) do print(i, v) end
	
	if #wdslist == 0 then
		return nil
	else
		return wdslist[math.random(#wdslist)]
	end
end


function googlechat(word)
	local result = word
	for i = 1, 16 do
		word = ggchat_getnext(result)
		if not word then break end
		result = result .. ' ' .. word -- type1
		--result = result .. word -- type2
	end
	
	return result
end

--print(googlechat('미안하지만'))




is_googlechat_active = false
function cmd_googlechat(isrv, who, msginfo, mdest, cmd, carg)
	if carg == '' then
		isrv:privmsg(mdest, '사용법: !구글대화 <문장> | 문장은 단어 1~3개 정도로. 한글, 영어만 사용')
		return
	end
	if is_googlechat_active then
		isrv:privmsg(mdest, '이 기능이 이미 작동중입니다. 끝날때까지 기다려주세요')
		return
	end
	
	is_googlechat_active = true
	local msg = googlechat(carg)
	isrv:privmsg_nh(mdest, '=> ' .. msg)
	is_googlechat_active = false
	
	return
end




function init(bot)
	bot:register_cmd('구글대화', cmd_googlechat);
end

function cleanup(bot)
	bot:unregister_cmd('구글대화');
end













