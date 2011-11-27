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

-- NickServ 자동 로그인 스크립트 || 
-- HanIRC ^^ 자동 로그인&주소 숨김 스크립트
cs_id = '' -- 아이디
cs_pw = '' -- 비밀번호

function login_handler(isrv)
	
	if cs_id ~= '' and cs_pw ~= '' then
		--isrv:privmsg('NickServ', 'IDENTIFY ' .. cs_id .. ' ' .. cs_pw)
		--isrv:privmsg('NickServ', 'IDENTIFY ' .. cs_pw)
		--isrv:quote('NS ' .. cs_id .. ' ' .. cs_pw)
		isrv:privmsg('^^', 'login ' .. cs_id .. ' ' .. cs_pw)
		
		isrv:quote('MODE ' .. isrv:my_nick() .. ' +x')
	end
	
	return true
end



function init(bot)
	bot:register_event_handler('login', login_handler)
end

function cleanup(bot)
	bot:unregister_event_handler('login')
end



