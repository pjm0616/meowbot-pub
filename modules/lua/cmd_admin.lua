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





function check_tbl_val(tbl, val)
	for idx, v in pairs(tbl) do
		if v == val then
			return true
		end
	end
	return false
end




function cmd_chans(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		return
	end
	
	local chanlist = isrv:get_chanlist()
	local liststr = #chanlist .. ': ' .. table.concat(chanlist, ', ')
	isrv:privmsg(mdest, liststr)
	
end

function cmd_mpart(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		return
	end
	
	carg = carg:gsub(', +', ',')
	carg = carg:gsub(' ', ',')
	if carg == '' then
		isrv:privmsg(mdest, 'Usage: !mpart chan1, chan2, ...')
		return
	end
	
	local clist = isrv:get_chanlist()
	for chan in carg:gmatch('([^, ]+)') do
		if check_tbl_val(clist, chan) then
			isrv:quote('PART '..chan)
		end
	end
	
end

function cmd_mjoin(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		return
	end
	
	carg = carg:gsub(', +', ',')
	carg = carg:gsub(' ', ',')
	if carg == '' then
		isrv:privmsg(mdest, 'Usage: !mjoin chan1, chan2, ...')
		return
	end
	
	for chan in carg:gmatch('([^, ]+)') do
		isrv:quote('JOIN '..chan)
	end
	
end

function cmd_amsg(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		isrv:privmsg(mdest, '관리자가 아닙니다.')
		return
	end
	
	if carg == '' then
		isrv:privmsg(mdest, 'Usage: !amsg message')
		return
	end
	
	local clist = isrv:get_chanlist()
	for _,chan in pairs(clist) do
		-- 동시에 여러개씩 보내는건 귀찮기 때문에 skip
		isrv:privmsg_nh(chan, carg)
	end
	
end

function cmd_rejoin(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		isrv:privmsg(mdest, '관리자가 아닙니다.')
		return
	end
	
	if carg == '' then
		isrv:privmsg(mdest, 'Usage: !rejoin chan')
		return
	end
	local chan = carg
	
	isrv:quote('PART '..chan)
	isrv:quote('JOIN '..chan)
	
end

function cmd_reconnect(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		isrv:privmsg(mdest, '관리자가 아닙니다.')
		return
	end
	
	isrv:reconnect(carg)
	
end

function cmd_quotef(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		isrv:privmsg(mdest, '관리자가 아닙니다.')
		return
	end
	
	local msg = carg
	msg = msg:gsub('%%my_nick%%', isrv:my_nick())
	isrv:quote(msg)
	
end

function cmd_msg(isrv, who, msginfo, mdest, cmd, carg)
	if not isrv:getbot():is_botadmin(who:getident()) then
		--isrv:privmsg(mdest, '관리자가 아닙니다.')
		return
	end
	
	local chan, msg = carg:match('([^ ]+) (.*)')
	if not chan or not msg then
		isrv:privmsg(mdest, 'Usage: !msg chan message')
		return
	end
	
	isrv:privmsg_nh(chan, msg)
	
end



function init(bot)
	bot:register_cmd('chans', cmd_chans);
	bot:register_cmd('mpart', cmd_mpart);
	bot:register_cmd('mjoin', cmd_mjoin);
	bot:register_cmd('amsg', cmd_amsg);
	bot:register_cmd('rejoin', cmd_rejoin);
	bot:register_cmd('reconnect', cmd_reconnect);
	bot:register_cmd('qf', cmd_quotef);
	bot:register_cmd('msg', cmd_msg);
end

function cleanup(bot)
	bot:unregister_cmd('chans');
	bot:unregister_cmd('mpart');
	bot:unregister_cmd('mjoin');
	bot:unregister_cmd('amsg');
	bot:unregister_cmd('rejoin');
	bot:unregister_cmd('reconnect');
	bot:unregister_cmd('qf');
	bot:unregister_cmd('msg');
end





