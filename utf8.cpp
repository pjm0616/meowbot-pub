/*
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
*/


#include <string>
#include <vector>
#include <inttypes.h>
#include <stdbool.h>

#include "utf8_impl.h"
#include "utf8.h"


int utf8_getsize_char_s(std::string utf8str)
{
	return utf8_getsize_char(utf8str.c_str());
}

int utf8_strlen_s(std::string str)
{
	return utf8_strlen(str.c_str());
}

std::string utf8_encode_s(unsigned int unicode)
{
	char buf[8];
	int len;
	
	len = utf8_encode(unicode, buf, sizeof(buf));
	if(len <= 0)
	{
		buf[0] = 0;
	}
	else
	{
		buf[len] = 0;
	}
	
	return std::string(buf, len);
}

unsigned int utf8_decode_s(const std::string &utf8_str)
{
	return utf8_decode(utf8_str.c_str(), NULL);
}

std::string utf8_strpos_s(std::string str, int pos)
{
	char buf[8];
	int ret;
	
	ret = utf8_strpos(str.c_str(), pos, buf);
	if(ret < 0)
		return "";
	
	return buf;
}


std::string utf8_substr_s(std::string str, int start, int len)
{
	int startpos;
	size_t size;
	const char *p = str.c_str();
	
	int ret = utf8_substr_getpos(p, start, len, &startpos, &size);
	if(ret < 0)
		return "";
	
	return std::string(p + startpos, size);
}

std::vector<std::string> utf8_split_chars(const std::string &str)
{
	std::vector<std::string> ret;
	const char *p = str.c_str();
	int len;
	
	for(;;)
	{
		len = utf8_getsize_char(p);
		if(len <= 0)
			break;
		ret.push_back(std::string(p, len));
		p += len;
	}
	
	return ret;
}

std::string utf8_sanitize_s(const std::string &str)
{
	// TODO
	return str;
}





// vim: set tabstop=4 textwidth=80:

