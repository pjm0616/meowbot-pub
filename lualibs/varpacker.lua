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


function escape_string(str)
	str = string.gsub(str, '\\', '\\\\')
	str = string.gsub(str, '\'', '\\\'')
	str = string.gsub(str, '\"', '\\\"')
	str = string.gsub(str, '\n', '\\n')
	str = string.gsub(str, '\t', '\\t')
	str = string.gsub(str, '\r', '\\r')
	return str
end

function pack_var_to_string_oneline(var)
	
	local vartype = type(var)
	if vartype == 'number' or vartype == 'boolean' or vartype == 'nil' then
		return tostring(var)
	elseif vartype == 'string' then
		return "'" .. escape_string(var) .. "'"
	elseif vartype == 'table' then
		local buf = '{'
		local n = 0
		for key, value in pairs(var) do
			local keytype = type(key)
			if n ~= 0 then buf = buf .. ',' end
			if keytype == 'number' then
				buf = buf .. "[" .. key .. "]=" .. 
					pack_var_to_string_oneline(value)
			elseif keytype == 'string' then
				buf = buf .. "['" .. 
					escape_string(key) .. "']=" .. 
					pack_var_to_string_oneline(value)
			elseif keytype == 'table' then
				-- FIXME
			else
				-- FIXME
			end
			n = n + 1
		end
		return buf .. '}'
	else
		return "'[unknown varible type]'"
	end
end

function pack_var_to_string(var, depth)
	
	local vartype = type(var)
	if vartype == 'number' or vartype == 'boolean' or vartype == 'nil' then
		return tostring(var)
	elseif vartype == 'string' then
		return "'" .. escape_string(var) .. "'"
	elseif vartype == 'table' then
		if not depth then depth = 0 end
		local buf
		if depth ~= 0 then buf = '\n' else buf = '' end
		buf = buf .. string.rep('\t', depth) .. '{\n'
		local n = 1
		for key, value in pairs(var) do
			local keytype = type(key)
			if n ~= 1 then buf = buf .. ', ' end
			if n ~= 1 then buf = buf .. '\n' end
			buf = buf .. string.rep('\t', depth + 1)
			if keytype == 'number' then
				buf = buf .. "[" .. key .. "] = " .. 
					pack_var_to_string(value, depth + 1)
			elseif keytype == 'string' then
				buf = buf .. "['" .. 
					escape_string(key) .. "'] = " .. 
					pack_var_to_string(value, depth + 1)
			elseif keytype == 'table' then
				-- FIXME
			else
				-- FIXME
			end
			n = n + 1
		end
		return buf .. '\n' .. string.rep('\t', depth) .. '}'
	else
		return "'[unknown varible type]'"
	end
end

-- usage: a={1,2}; save_var_to_file('vardump.txt', {a=a, b={1,2,3,4}, c='d'})
function save_var_to_file(file, varlist)
	os.remove(file)
	local fh = io.open(file, 'w')
	if not fh then return false end
	for name, var in pairs(varlist) do
		local buf = name .. ' = ' .. pack_var_to_string(var) .. ';\n'
		fh:write(buf)
	end
	fh:write('\n')
	fh:close()
	return true
end

function dofile_nr(file)
	fxn = loadfile(file)
	if not fxn then return false end
	fxn()
	return true
end







