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




chanlist_file_pfx = './data/autojoin/chanlist-'

function savechanlist(isrv)
	local cname = isrv:get_name()
	
	os.remove(chanlist_file_pfx..cname..'.txt')
	local fh, errmsg
	fh, errmsg = io.open(chanlist_file_pfx..cname..'.txt', 'w')
	if not fh then
		print('io.open(chanlist_file, \'w\') failed: ' .. errmsg)
		return false
	end
	local clist = isrv:get_chanlist()
	fh:write(table.concat(clist, ',') .. '\n')
	fh:close()
	
	return true
end

function cmd_savechanlist(isrv, who, msginfo, mdest, cmd, carg)
	
	if not isrv:getbot():is_botadmin(who:getident()) then
		return
	end
	savechanlist(isrv)
	isrv:privmsg(mdest, 'chanlist saved')
end

function login_handler(isrv)
	local cname = isrv:get_name()
	
	local fh, errmsg
	fh, errmsg = io.open(chanlist_file_pfx..cname..'.txt', 'r')
	if not fh then
		print('io.open(chanlist_file, \'r\') failed: ' .. errmsg)
		return
	end
	clist = fh:read('*l') -- read one line
	if (clist == nil) or (clist == '') then
		print('autojoin failed: ' .. chanlist_file_pfx..cname..'.txt' .. ' is empty')
	else
		isrv:quote('JOIN ' .. clist)
	end
	
	return true
end

function disconnect_handler(isrv)
	
	--savechanlist(isrv) -- join, part handler로 이동
	
	return true
end

function join_handler(isrv, who, chan)
	savechanlist(isrv)
end
function part_handler(isrv, who, chan)
	savechanlist(isrv)
end


function init(bot)
	bot:register_event_handler('login', login_handler)
	bot:register_event_handler('join', join_handler)
	bot:register_event_handler('part', part_handler)
	bot:register_event_handler('disconnect', disconnect_handler)
	bot:register_cmd('savechanlist', cmd_savechanlist)
end

function cleanup(bot)
	bot:unregister_event_handler('login')
	bot:unregister_event_handler('join')
	bot:unregister_event_handler('part')
	bot:unregister_event_handler('disconnect')
	bot:unregister_cmd('savechanlist')
	
	--savechanlist(bot)
end



