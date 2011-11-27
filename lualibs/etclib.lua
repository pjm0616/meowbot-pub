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




useragent_list = {
	'Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.9) Gecko/2008061015 Firefox/3.0', 
	'Mozilla/4.0 (PSP (PlayStation Portable); 2.00)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0; .NET CLR 2.0.50727)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; DigExt; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; InfoPath.1)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) )', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/; .NET CLR 1.1.4322; InfoPath.1)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/; .NET CLR 2.0.50727; .NET CLR 1.1.4322; .NET CLR 2.0.50215)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1;  Embedded Web Browser from: http://bsalsa.com/)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1;  Embedded Web Browser from: http://bsalsa.com/; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.0.3705; .NET CLR 1.1.4322; Media Center PC 4.0)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; InfoPath.1)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322; InfoPath.2; .NET CLR 2.0.50727; .NET CLR 3.0.04506.648; .NET CLR 3.5.21022)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 2.0.50215)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 2.0.50727; .NET CLR 1.1.4322; .NET CLR 3.0.04506.30)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; EmbeddedWB 14.52 from: http://www.bsalsa.com/ EmbeddedWB 14.52)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; EmbeddedWB 14.52 from: http://www.bsalsa.com/ EmbeddedWB 14.52; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; FunWebProducts; .NET CLR 2.0.50727; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; InfoPath.1)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; InfoPath.1; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; InfoPath.1; .NET CLR 2.0.50727; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; InfoPath.1; .NET CLR 2.0.50727; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; InfoPath.2)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; InfoPath.2; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; InfoPath.2; .NET CLR 2.0.50727; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; KDS 2.3.014; .NET CLR 2.0.50727)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; LG_UA; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ; .NET CLR 2.0.50727; LG_UA)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; LupinV2.2/20090204; LupinV2.2/20090204)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; LupinV2.u2/20090204; LupinV2.u2/20090204)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; MathPlayer 2.0; GTB5; .NET CLR 2.0.50727)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; Media Center PC 3.0; .NET CLR 1.0.3705; .NET CLR 1.1.4322; .NET CLR 2.0.50727)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) )', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/; .NET CLR 1.0.3705)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/; .NET CLR 1.0.3705; .NET CLR 1.1.4322; Media Center PC 4.0; .NET CLR 2.0.50727)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ; Media Center PC 3.0; .NET CLR 1.0.3705; .NET CLR 2.0.50727; InfoPath.2)', 
	'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; Mozilla/4.0(Compatible-EmbeddedWB 14.57 http://bsalsa.com/  Embedded Web Browser from: http://bsalsa.com/)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.0.3705; .NET CLR 1.1.4322; Media Center PC 4.0)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.0.3705; .NET CLR 1.1.4322; Media Center PC 4.0; .NET CLR 2.0.50727)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.0.3705; .NET CLR 1.1.4322; Media Center PC 4.0; .NET CLR 2.0.50727; InfoPath.2)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.0.3705; .NET CLR 1.1.4322; Media Center PC 4.0; InfoPath.2; .NET CLR 2.0.50727)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.0.3705; Media Center PC 3.1; .NET CLR 2.0.50727; InfoPath.2)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.1.4322)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.1.4322; .NET CLR 2.0.50727; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729; InfoPath.2)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 2.0.50727; .NET CLR 3.0.04506.30; .NET CLR 1.1.4322; .NET CLR 3.0.04506.648)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ;  Embedded Web Browser from: http://bsalsa.com/; .NET CLR 1.1.4322; .NET CLR 2.0.50727; InfoPath.2)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ; InfoPath.2; .NET CLR 2.0.50727; OfficeLiveConnector.1.3; OfficeLivePatch.0.0)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ; InfoPath.2; .NET CLR 2.0.50727; OfficeLiveConnector.1.3; OfficeLivePatch.0.0; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.2; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ; .NET CLR 1.1.4322; .NET CLR 2.0.50727; .NET CLR 3.0.04506.30)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.2; WOW64; GTB5; .NET CLR 2.0.50727; InfoPath.2; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0;  Embedded Web Browser from: http://bsalsa.com/; SLCC1; .NET CLR 2.0.50727; Media Center PC 5.0; .NET CLR 3.0.04506)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ; SLCC1; .NET CLR 2.0.50727; Media Center PC 5.0; .NET CLR 3.0.04506; InfoPath.2)', 
	'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1) ; SLCC1; .NET CLR 2.0.50727; Media Center PC 5.0; .NET CLR 3.0.04506; InfoPath.2; OfficeLiveConnector.1.3; OfficeLivePatch.0.0)', 
	'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.5; en-US; rv:1.9.0.6) Gecko/2009011912 Firefox/3.0.6', 
	'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.5; ko; rv:1.9.0.5) Gecko/2008120121 Firefox/3.0.5', 
	'Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_5_6; ko-kr) AppleWebKit/525.27.1 (KHTML, like Gecko) Version/3.2.1 Safari/525.27.1', 
	'Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/419.2.1 (KHTML, like Gecko) Safari/419.3', 
	'Mozilla/5.0 (Macintosh; U; PPC Mac OS X 10.4; ko; rv:1.9.0.5) Gecko/2008120121 Firefox/3.0.5', 
	'Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.8.1.12) Gecko/20080201 Firefox/2.0.0.12', 
	'Mozilla/5.0 (Windows; U; Windows NT 5.1; de; rv:1.9.0.5) Gecko/2008120122 Firefox/3.0.5 (.NET CLR 3.5.30729)', 
	'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) AppleWebKit/525.19 (KHTML, like Gecko) Chrome/1.0.154.48 Safari/525.19', 
	'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.7.2) Gecko/20040804 Netscape/7.2 (ax)', 
	'Mozilla/5.0 (Windows; U; Windows NT 5.1; ko; rv:1.9) Gecko/2008052906 Firefox/3.0', 
	'Mozilla/5.0 (Windows; U; Windows NT 5.1; ko; rv:1.9.0.3) Gecko/2008092417 Firefox/3.0.3', 
	'Mozilla/5.0 (Windows; U; Windows NT 5.2; ko; rv:1.9.0.5) Gecko/2008120122 Firefox/3.0.5 (.NET CLR 3.5.30729)', 
	'Mozilla/5.0 (Windows; U; Windows NT 6.0; en-US) AppleWebKit/525.19 (KHTML, like Gecko) Chrome/1.0.154.46 Safari/525.19', 
	'Mozilla/5.0 (Windows; U; Windows NT 6.0; ko; rv:1.9.0.6) Gecko/2009011913 Firefox/3.0.6', 
	'Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US; rv:1.9.0.5) Gecko/2008120122 Firefox/3.0.5', 
	'Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.0.14eol) Gecko/20070505 (Debian-1.8.0.15~pre080614i-0etch1) Epiphany/2.14', 
	'Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.0.5) Gecko/2008121622 Linux Mint/6 (Felicia) Firefox/3.0.5', 
	'Mozilla/5.0 (X11; U; Linux i686; fr-FR; rv:1.9.0.6) Gecko/2009012616 Ubuntu/8.10 (intrepid) Firefox/3.0.6 Ubiquity/0.1.5', 
	'Mozilla/5.0 (X11; U; Linux i686; fr; rv:1.9.0.5) Gecko/2008122011 Iceweasel/3.0.5 (Debian-3.0.5-1)', 
	'Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.8.1.19) Gecko/20081216 Ubuntu/7.10 (gutsy) Firefox/2.0.0.19', 
	'Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.8.1.6) Gecko/20071008 Ubuntu/7.10 (gutsy) Firefox/2.0.0.6', 
	'Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.9) Gecko/2008061015 Firefox/3.0', 
	'Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.9.0.5) Gecko/2008121622 Fedora/3.0.5-1.fc10 Firefox/3.0.5', 
	'Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.9.0.5) Gecko/2008121622 Ubuntu/8.10 (intrepid) Firefox/3.0.5', 
	'Mozilla/5.0 (X11; U; Linux i686; ko_KR; rv:1.9.0.5) Gecko/2008123017 Firefox/2.0.0.9', 
	'Mozilla/5.0 (X11; U; Linux x86_64; ko-KR; rv:1.9.0.5) Gecko/2008122010 Iceweasel/3.0.5 (Debian-3.0.5-1)', 
	'Mozilla/5.0 (compatible; Konqueror/4.1; Linux) KHTML/4.1.4 (like Gecko)', 
	'Opera/9.22 (Windows NT 5.1; U; en)', 
	'User-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1)', 
};

function get_random_useragent()
	return useragent_list[math.random(#useragent_list)];
end
function set_default_useragent(ua)
	g_default_useragent = ua
end


function make_cookie_str(cookies)
	if not cookies then return '' end
	local result = ''
	for name, value in pairs(cookies) do
		if result ~= '' then result = result .. '; ' end
		result = result .. name .. '=' .. value
	end
	return result
end



g_useragent_ff = 'Mozilla/5.0 (X11; U; Linux i686; ko-KR; rv:1.9) Gecko/2008061015 Firefox/3.0'
g_useragent_ie = 'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0)'
g_default_useragent = g_useragent_ff

g_http_timeout = 5000

function set_http_timeout(timeout)
	local old_timeout = g_http_timeout
	g_http_timeout = timeout
	return old_timeout
end


function sock_read_all(s, timeout)
	
	local data = ''
	while true do
		local buf = s:read(1024, timeout)
		if not buf then break end
		data = data .. buf
	end
	
	return data
end

re_parse_http_url = pcre.new("^http://([^:/]+):?([0-9]+)?(/.*)")

function parse_http_url(url)
	local r = re_parse_http_url:full_match(url)
	if not r then return nil end
	
	local host, port, location = unpack(r)
	if port == '' then port = 80 end
	
	return host, port, location
end


re_http_status = pcre.new('^HTTP/1\\.. ([0-9]+) (.*)')
re_http_header_var = pcre.new('^([^ ]+): *(.*)')

-- http 요청을 보낸 후 사용
function get_http_header(s)
	
	local header_vars = {}
	local line = s:readline(g_http_timeout)
	if not line then return false end
	local matches = re_http_status:full_match(line)
	if not matches then return false end
	header_vars['status'] = matches[1]+0
	header_vars['status_msg'] = matches[2]
	
	header_vars['cookies'] = {}
	--header_vars['cookie_str'] = ''
	while true do
		local line = s:readline(g_http_timeout)
		if not line then
			return false
		elseif line == '' then
			break
		end
		local matches = re_http_header_var:full_match(line)
		if not matches then return false end
		local name, value = matches[1]:lower(), matches[2]
		header_vars[name] = value
		if name == 'set-cookie' then
			matches = pcre.new('^([^=;]+)=([^;]*)'):match(value)
			if matches then
				header_vars['cookies'][matches[1]] = matches[2]
				
				--if header_vars['cookie_str'] ~= '' then
				--	header_vars['cookie_str'] = header_vars['cookie_str'] .. '; '
				--end
				--header_vars['cookie_str'] = header_vars['cookie_str'] .. matches[1]..'='..matches[2]
			end
		elseif name == 'content-type' then
			matches = pcre.new('charset=([^ ;=]+)'):match(value)
			if matches then
				header_vars['charset'] = matches[1]
			end
		end
	end
	
	return header_vars
end

-- TODO: FIXME: chunked encoding, etc...
function http_read_all(s, header_vars)
	
	local data = sock_read_all(s, g_http_timeout)
	s:close()
	-- charset이 있다면 인코딩 변환
	--if header_vars['charset'] and 
	--	header_vars['charset']:lower() ~= 'utf8' and 
	--	header_vars['charset']:lower() ~= 'utf-8' then
	--	data = data:decode(header_vars['charset'])
	--end
	
	return data, header_vars
end


function get_http(url, headers)
	local host, port, location = parse_http_url(url)
	if not host then return nil end
	
	local s = sock.new()
	local ret = s:connect(host, port, g_http_timeout)
	if ret < 0 then return nil end
	s:send("GET " .. location .. " HTTP/1.0\r\n") -- FIXME: HTTP/1.0 HTTP/1.1
	s:send("Host: " .. host .. "\r\n")
	s:send("Accept-Encoding: *;q=0\r\n")
	s:send("User-Agent: " .. g_default_useragent .. "\r\n")
	if headers then
		for name, value in pairs(headers) do
			s:send(name .. ": " .. value .. "\r\n")
		end
	end
	s:send("Connection: close\r\n\r\n")
	
	local header_vars = get_http_header(s)
	if not header_vars then
		s:close()
		return false
	end
	return http_read_all(s, header_vars), header_vars
end



function post_http_urlencoded(url, postdata, headers)
	local host, port, location = parse_http_url(url)
	if not host then return nil end
	
	local qstr = ''
	for name, value in pairs(postdata) do
		if qstr ~= '' then qstr = qstr .. '&' end
		qstr = qstr .. urlencode(name) .. '=' .. urlencode(value)
	end
	
	local s = sock.new()
	local ret = s:connect(host, port, g_http_timeout)
	if ret < 0 then return nil end
	s:send("POST " .. location .. " HTTP/1.0\r\n") -- FIXME: HTTP/1.0 HTTP/1.1
	s:send("Host: " .. host .. "\r\n")
	s:send("Accept-Encoding: *;q=0\r\n")
	s:send("User-Agent: " .. g_default_useragent .. "\r\n")
	s:send("Content-Type: application/x-www-form-urlencoded\r\n")
	s:send("Content-Length: " .. #qstr .. "\r\n")
	if headers then
		for name, value in pairs(headers) do
			s:send(name .. ": " .. value .. "\r\n")
		end
	end
	s:send("Connection: close\r\n\r\n")
	s:send(qstr)
	
	local header_vars = get_http_header(s)
	if not header_vars then
		s:close()
		return false
	end
	return http_read_all(s, header_vars), header_vars
end


function post_http_multipart(url, postdata, headers)
	local host, port, location = parse_http_url(url)
	if not host then return nil end
	
	local boundary = string.rep('-', 27) .. string.format('%09u%09u%09u', math.random(0x7FFFFFFF), math.random(0x7FFFFFFF), math.random(0x7FFFFFFF)):sub(1, 27)
	local buf = ''
	for name, value in pairs(postdata) do
		buf = buf .. '--'..boundary..'\r\nContent-Disposition: form-data; name="'..name..'"\r\n\r\n'
		buf = buf .. value .. '\r\n'
	end
	buf = buf .. '--'..boundary..'--\r\n'
	
	local s = sock.new()
	local ret = s:connect(host, port, g_http_timeout)
	if ret < 0 then return nil end
	s:send("POST " .. location .. " HTTP/1.0\r\n") -- FIXME: HTTP/1.0 HTTP/1.1
	s:send("Host: " .. host .. "\r\n")
	s:send("Accept-Encoding: *;q=0\r\n")
	s:send("User-Agent: " .. g_default_useragent .. "\r\n")
	s:send("Content-Type: multipart/form-data; boundary=" .. boundary .. "\r\n")
	s:send("Content-Length: " .. tostring(#buf) .. "\r\n")
	if headers then
		for name, value in pairs(headers) do
			s:send(name .. ": " .. value .. "\r\n")
		end
	end
	s:send("Connection: close\r\n\r\n")
	s:send(buf)
	buf = nil
	
	local header_vars = get_http_header(s)
	if not header_vars then
		s:close()
		return false
	end
	return http_read_all(s, header_vars), header_vars
end
















