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

cfgfile = './data/automode_list.txt'
autovoice_list = {}
autoop_list = {}
autohop_list = {}


noautoop_list = 
{
	'#meowbot', '#pjm0616', '#SIGSEGV', '#pmplab', '#\\', '#/', '#.', '#-', '#프겔'
}



function is_user_have_owner_priv(mode)
	if mode:find('~') then
		return true
	else
		return false
	end
end
function is_user_protected(mode)
	if mode:find('&') then
		return true
	else
		return false
	end
end
function is_user_have_op_priv(mode)
	if mode:find('[~@]') then
		return true
	else
		return false
	end
end
function is_user_have_hop_priv(mode)
	if mode:find('[~@%%]') then
		return true
	else
		return false
	end
end
function is_user_have_voice_priv(mode)
	if mode:find('[~@%%+]') then
		return true
	else
		return false
	end
end






function wc_match_nocase_csv(csmasks, str)
	for mask in string.gmatch(csmasks, '([^ ,]+)') do
		if wc_match_nocase(mask, str) then
			return true
		end
	end
	return false
end

-- modechar: one of 'q', 'a', 'o', 'h', 'v'
function give_mode(isrv, chan, modechar, nicklist)
	
	local buf = ''
	local n = 1
	for i, nick in pairs(nicklist) do
		buf = buf .. nick .. ' '
		if n == 6 then
			isrv:quote(string.format(
				'MODE %s +%s %s', 
				chan, string.rep(modechar, n), buf
				))
			buf = ''
			n = 1
		else
			n = n + 1
		end
	end
	n = n - 1
	if n ~= 0 then
		isrv:quote(string.format(
			'MODE %s +%s %s', 
			chan, string.rep(modechar, n), buf
			))
	end
end



function cmd_autovoice(isrv, who, msginfo, mdest, cmd, carg)
	
	local netname = isrv:get_netname()
	local chan = msginfo:getchan()
	local ulist = isrv:get_chanuserlist(chan)
	if not is_user_have_hop_priv(ulist[isrv:my_nick()].chanusermode) then
		isrv:privmsg_nh(mdest, '봇에게 옵이 없으면 사용할 수 없습니다.')
		return
	end
	if not is_user_have_op_priv(ulist[who:getnick()].chanusermode) then
		isrv:privmsg_nh(mdest, '권한이 없습니다. 옵 이상을 가진 사람만 자동보이스를 설정할 수 있습니다.')
		return
	end
	
	if not autovoice_list[netname] then autovoice_list[netname] = {} end
	if autovoice_list[netname][chan] then
		autovoice_list[netname][chan] = nil
		isrv:privmsg_nh(mdest, '자동보이스가 해제되었습니다.')
	else
		autovoice_list[netname][chan] = true
		isrv:privmsg_nh(mdest, '자동보이스가 설정되었습니다.')
	end
	
end

function cmd_autoop(isrv, who, msginfo, mdest, cmd, carg)
	
	local netname = isrv:get_netname()
	local chan = msginfo:getchan()
	carg = string.gsub(carg, ' ', '')
	if carg == '' then
		isrv:privmsg_nh(mdest, '예) !자동옵 *: 모두에게 자동옵, asdf!*@1.1.1.*,dd*: IP가 1.1.1.*이고 닉이 asdf인 사람과 닉이 dd로 시작하는 사람한테 자동옵, !자동옵 해제, !자동옵 목록')
		return
	end
	if msginfo:getchantype() ~= 'chan' then
		isrv:privmsg_nh(mdest, '채널이 아닙니다')
		return
	end
	
	local ulist = isrv:get_chanuserlist(chan)
	if not ulist then
		isrv:privmsg_nh(mdest, '봇이 없는 채널입니다')
		return
	end
	if not is_user_have_op_priv(ulist[isrv:my_nick()].chanusermode) then
		isrv:privmsg_nh(mdest, '봇에게 옵이 없으면 사용할 수 없습니다.')
		return
	end
	for i, v in pairs(noautoop_list) do
		if v == chan then
			isrv:privmsg_nh(mdest, '자동옵이 금지된 채널입니다.')
			return
		end
	end
	
	if not autoop_list[netname] then autoop_list[netname] = {} end
	if carg == '목록' then
		if autoop_list[netname][chan] then
			isrv:privmsg_nh(mdest, '현재 자동옵 목록: ' .. autoop_list[netname][chan])
		else
			isrv:privmsg_nh(mdest, '자동옵이 설정되어있지 않습니다.')
		end
		return
	end
	
	if not is_user_have_op_priv(ulist[who:getnick()].chanusermode) then
		isrv:privmsg_nh(mdest, '권한이 없습니다. 옵 이상을 가진 사람만 자동옵을 설정할 수 있습니다.')
		return
	end
	if carg == '해제' then
		if autoop_list[netname][chan] then
			isrv:privmsg_nh(mdest, '자동옵이 해제되었습니다. (' .. autoop_list[netname][chan] .. ')')
			autoop_list[netname][chan] = nil
		else
			isrv:privmsg_nh(mdest, '자동옵이 설정되어있지 않습니다.')
		end
	else
		autoop_list[netname][chan] = carg
		isrv:privmsg_nh(mdest, '자동옵이 설정되었습니다. (' .. carg .. ')')
	end
	
end

function cmd_autohop(isrv, who, msginfo, mdest, cmd, carg)
	
	local netname = isrv:get_netname()
	if netname == 'HanIRC' then
		isrv:privmsg_nh(mdest, 'halfop을 지원하지 않는 HanIRC에선 사용할 수 없다에요')
		return
	end
	
	local chan = msginfo:getchan()
	carg = string.gsub(carg, ' ', '')
	if carg == '' then
		isrv:privmsg_nh(mdest, '예) !자동반옵 *: 모두에게 자동옵, asdf!*@1.1.1.*,dd*: IP가 1.1.1.*이고 닉이 asdf인 사람과 닉이 dd로 시작하는 사람한테 자동옵, !자동옵 해제, !자동옵 목록')
		return
	end
	if msginfo:getchantype() ~= 'chan' then
		isrv:privmsg_nh(mdest, '채널이 아닙니다')
		return
	end
	
	local ulist = isrv:get_chanuserlist(chan)
	if not ulist then
		isrv:privmsg_nh(mdest, '봇이 없는 채널입니다')
		return
	end
	if not is_user_have_op_priv(ulist[isrv:my_nick()].chanusermode) then
		isrv:privmsg_nh(mdest, '봇에게 옵이 없으면 사용할 수 없습니다.')
		return
	end
	for i, v in pairs(noautoop_list) do
		if v == chan then
			isrv:privmsg_nh(mdest, '자동옵이 금지된 채널입니다.')
			return
		end
	end
	
	if not autohop_list[netname] then autohop_list[netname] = {} end
	if carg == '목록' then
		if autohop_list[netname][chan] then
			isrv:privmsg_nh(mdest, '현재 자동옵 목록: ' .. autohop_list[netname][chan])
		else
			isrv:privmsg_nh(mdest, '자동옵이 설정되어있지 않습니다.')
		end
		return
	end
	
	if not is_user_have_op_priv(ulist[who:getnick()].chanusermode) then
		isrv:privmsg_nh(mdest, '권한이 없습니다. 옵 이상을 가진 사람만 자동옵을 설정할 수 있습니다.')
		return
	end
	if carg == '해제' then
		if autohop_list[netname][chan] then
			isrv:privmsg_nh(mdest, '자동옵이 해제되었습니다. (' .. autohop_list[netname][chan] .. ')')
			autohop_list[netname][chan] = nil
		else
			isrv:privmsg_nh(mdest, '자동옵이 설정되어있지 않습니다.')
		end
	else
		autohop_list[netname][chan] = carg
		isrv:privmsg_nh(mdest, '자동옵이 설정되었습니다. (' .. carg .. ')')
	end
	
end

function cmd_allop(isrv, who, msginfo, mdest, cmd, carg)
	
	local netname = isrv:get_netname()
	if msginfo:getchantype() ~= 'chan' then
		isrv:privmsg_nh(mdest, '채널이 아닙니다')
		return
	end
	
	local chan = msginfo:getchan()
	local ulist = isrv:get_chanuserlist(chan)
	if not is_user_have_op_priv(ulist[isrv:my_nick()].chanusermode) then
		isrv:privmsg_nh(mdest, '봇에게 옵이 없으면 사용할 수 없습니다.')
		return
	end
	for i, v in pairs(noautoop_list) do
		if v == chan then
			isrv:privmsg_nh(mdest, '올옵이 금지된 채널입니다.')
			return
		end
	end
	if not is_user_have_op_priv(ulist[who:getnick()].chanusermode) then
		isrv:privmsg_nh(mdest, '권한이 없습니다. 옵 이상을 가진 사람만 올옵을 할 수 있습니다.')
		return
	end
	
	local oplist = {}
	for nick, v in pairs(ulist) do
		if not v.chanusermode:find('@') then
			table.insert(oplist, nick)
		end
	end
	if #oplist == 0 then
		isrv:privmsg_nh(mdest, '모두가 옵을 가지고 있습니다')
	else
		give_mode(isrv, chan, 'o', oplist)
	end
	
end

function cmd_allhop(isrv, who, msginfo, mdest, cmd, carg)
	
	local netname = isrv:get_netname()
	if netname == 'HanIRC' then
		isrv:privmsg_nh(mdest, 'halfop을 지원하지 않는 HanIRC에선 사용할 수 없다에요')
		return
	end
	if msginfo:getchantype() ~= 'chan' then
		isrv:privmsg_nh(mdest, '채널이 아닙니다')
		return
	end
	
	local chan = msginfo:getchan()
	local ulist = isrv:get_chanuserlist(chan)
	if not is_user_have_op_priv(ulist[isrv:my_nick()].chanusermode) then
		isrv:privmsg_nh(mdest, '봇에게 옵이 없으면 사용할 수 없습니다.')
		return
	end
	for i, v in pairs(noautoop_list) do
		if v == chan then
			isrv:privmsg_nh(mdest, '올옵이 금지된 채널입니다.')
			return
		end
	end
	if not is_user_have_op_priv(ulist[who:getnick()].chanusermode) then
		isrv:privmsg_nh(mdest, '권한이 없습니다. 옵 이상을 가진 사람만 올옵을 할 수 있습니다.')
		return
	end
	
	local hoplist = {}
	for nick, v in pairs(ulist) do
		--if not v.chanusermode:find('%') then
			table.insert(hoplist, nick)
		--end
	end
	if #hoplist == 0 then
		isrv:privmsg_nh(mdest, '모두가 옵을 가지고 있습니다')
	else
		give_mode(isrv, chan, 'h', hoplist)
	end
	
end

function cmd_allvoice(isrv, who, msginfo, mdest, cmd, carg)
	
	local netname = isrv:get_netname()
	if msginfo:getchantype() ~= 'chan' then
		isrv:privmsg_nh(mdest, '채널이 아닙니다')
		return
	end
	
	local chan = msginfo:getchan()
	local ulist = isrv:get_chanuserlist(chan)
	if not is_user_have_hop_priv(ulist[isrv:my_nick()].chanusermode) then
		isrv:privmsg_nh(mdest, '봇에게 옵이 없으면 사용할 수 없습니다.')
		return
	end
	if not is_user_have_hop_priv(ulist[who:getnick()].chanusermode) then
		isrv:privmsg_nh(mdest, '권한이 없습니다. halfop 이상을 가진 사람만 올옵을 할 수 있습니다.')
		return
	end
	
	local voicelist = {}
	for nick, v in pairs(ulist) do
		if not v.chanusermode:find('+') then
			table.insert(voicelist, nick)
		end
	end
	if #voicelist == 0 then
		isrv:privmsg_nh(mdest, '모두가 보이스를 가지고 있습니다')
	else
		give_mode(isrv, chan, 'v', voicelist)
	end
	
end


-- 채널에 봇 혼자 옵을 가지고 있을경우에도 옵을 자동으로 주지 않는 채널 목록
no_nouser_autoop_list = 
{
	'#meowbot', '#pjm0616', '#SIGSEGV', '#pmplab', '#\\', '#/', '#.', '#Only'
}
function join_handler(isrv, who, chan)
	
	local netname = isrv:get_netname()
	local nick = who:getnick()
	local my_nick = isrv:my_nick()
	if nick ~= my_nick then
		local ulist = isrv:get_chanuserlist(chan)
		if ulist and ({ulist[isrv:my_nick()].chanusermode:gsub('[^~&@]', '')})[1] == '' then
			-- 봇한테 옵이 없다면
			return true
		end
		
		local identstr = who:getident()
		local give_op, give_hop, give_voice = false, false, false
		
		if not autoop_list[netname] then autoop_list[netname] = {} end
		if not autohop_list[netname] then autohop_list[netname] = {} end
		if not autovoice_list[netname] then autovoice_list[netname] = {} end
		if autoop_list[netname][chan] and wc_match_nocase_csv(autoop_list[netname][chan], identstr) then
			give_op = true
		end
		if autohop_list[netname][chan] and wc_match_nocase_csv(autohop_list[netname][chan], identstr) then
			give_hop = true
		end
		if autovoice_list[netname][chan] then
			give_voice = true
		end
		if give_op == false then
			give_op = true
			for nick2, item in pairs(ulist) do
				if nick2 ~= my_nick and nick2 ~= nick then
					-- 내가 아닌 다른 사람이 있을 경우 옵을 주지 않음
					give_op = false
				end
			end
			if give_op == true and autoop_list[netname][chan] then
				give_op = false
				isrv:privmsg_nh(chan, '자동옵이 설정되어있는 채널이기 때문에, 옵을 드리지 않습니다. 자동옵 목록을 보려면 !자동옵 목록 을 입력하세요')
			end
		end
		
		if give_op then
			if give_voice then
				if give_hop then
					isrv:quote('MODE ' .. chan .. ' +ohv ' .. nick .. ' ' .. nick .. ' ' .. nick)
				else
					isrv:quote('MODE ' .. chan .. ' +ov ' .. nick .. ' ' .. nick)
				end
			else
				if give_hop then
					isrv:quote('MODE ' .. chan .. ' +oh ' .. nick .. ' ' .. nick)
				else
					isrv:quote('MODE ' .. chan .. ' +o ' .. nick)
				end
			end
		else
			if give_voice then
				if give_hop then
					isrv:quote('MODE ' .. chan .. ' +hv ' .. nick .. ' ' .. nick)
				else
					isrv:quote('MODE ' .. chan .. ' +v ' .. nick)
				end
			else
				if give_hop then
					isrv:quote('MODE ' .. chan .. ' +h ' .. nick)
				else
					
				end
			end
		end
	end
	
	return true
end




function init(bot)
	
	bot:register_cmd('자동보이스', cmd_autovoice);
	bot:register_cmd('오토보이스', cmd_autovoice);
	bot:register_cmd('자동옵', cmd_autoop);
	bot:register_cmd('오토옵', cmd_autoop);
	bot:register_cmd('자동반옵', cmd_autohop);
	bot:register_cmd('오토반옵', cmd_autohop);
	
	bot:register_cmd('올옵', cmd_allop);
	bot:register_cmd('@', cmd_allop);
	bot:register_cmd('올보이스', cmd_allvoice);
	bot:register_cmd('+', cmd_allvoice);
	bot:register_cmd('올반옵', cmd_allhop);
	bot:register_cmd('%', cmd_allhop);
	
	bot:register_event_handler('join', join_handler);
	
	dofile_nr(cfgfile) -- load automode data
end

function cleanup(bot)
	
	bot:unregister_cmd('자동보이스');
	bot:unregister_cmd('오토보이스');
	bot:unregister_cmd('자동옵');
	bot:unregister_cmd('오토옵');
	bot:unregister_cmd('자동반옵');
	bot:unregister_cmd('오토반옵');
	
	bot:unregister_cmd('올옵');
	bot:unregister_cmd('@');
	bot:unregister_cmd('올보이스');
	bot:unregister_cmd('+');
	bot:unregister_cmd('올반옵');
	bot:unregister_cmd('%');
	
	bot:unregister_event_handler('join');
	
	save_var_to_file(cfgfile, {
			autovoice_list = autovoice_list, 
			autoop_list = autoop_list, 
			autohop_list = autohop_list
		})
end





