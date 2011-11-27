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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

#include "utf8_impl.h"

#define inline inline __attribute__((gnu_inline))


/*

// http://en.wikipedia.org/wiki/Utf8#Description
U+000000~U+00007F	0zzzzzzz
U+000080~U+0007FF	110yyyyy 10zzzzzz
U+000800~U+00D7FF	1110xxxx 10yyyyyy 10zzzzzz
U+00E000~U+00FFFF	1110xxxx 10yyyyyy 10zzzzzz
U+010000~U+10FFFF	11110www 10xxxxxx 10yyyyyy 10zzzzzz

00000000	0	0x00
10000000	128	0x80
11000000	192	0xC0
11100000	224	0xE0
11110000	240	0xF0
11111000	248	0xF8
11111100	252	0xFC
11111110	254	0xFE
11111111	255	0xFF
01111111	127	0x7F
00111111	63	0x3F
00011111	31	0x1F
00001111	15	0x0F
00000111	7	0x07
00000011	3	0x03
00000001	1	0x01
00000000	0	0x00


11111111 11111111 11111111

*/



#if 0
static const char utf8_skip_table[256] = 
{
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

int utf8_char_len(const char *p)
{
    return utf8_skip_table[*(const unsigned char*)p];
}
#endif


int utf8_encode(unsigned int unicode_value, char *dest, size_t size)
{
	char utf8buf[4];
	size_t len;
	
	if(unicode_value <= 0x00007F)
	{
		len = 1;
		utf8buf[0] = unicode_value & 0x7F;
	}
	else if((unicode_value >= 0x000080) && (unicode_value <= 0x0007FF))
	{
		len = 2;
		utf8buf[0] = 0xC0 | (unicode_value >> 6);
		utf8buf[1] = 0x80 | (unicode_value & 0x3F);
	}
	else if(((unicode_value >= 0x000800) && (unicode_value <= 0x00D7FF))
		|| ((unicode_value >= 0x00E000) && (unicode_value <= 0x00FFFF)))
	{
		len = 3;
		utf8buf[0] = 0xE0 | (unicode_value >> 12);
		utf8buf[1] = 0x80 | ((unicode_value >> 6) & 0x3F);
		utf8buf[2] = 0x80 | (unicode_value & 0x3F);
	}
	else if((unicode_value >= 0x010000) && (unicode_value <= 0x10FFFF))
	{
		len = 4;
		utf8buf[0] = 0xF0 | (unicode_value >> 15);
		utf8buf[1] = 0x80 | ((unicode_value >> 12) & 0x3F);
		utf8buf[2] = 0x80 | ((unicode_value >> 6) & 0x3F);
		utf8buf[3] = 0x80 | (unicode_value & 0x3F);
	}
	else
	{
		return 0;
	}
	
	if(size < len)
		return -len;
	
	for(size_t i = 0; i < len; i++)
		dest[i] = utf8buf[i];
	
	return len;
}

unsigned int utf8_decode_v2(const char *utf8char, size_t size, size_t *charlen)
{
	if(size == 0 || utf8char[0] == 0)
	{
		if(charlen)
			*charlen = 0;
		
		return 0xFFFFFFFF;
	}
	else if((utf8char[0] & 0x80) == 0x00)
	{
		if(charlen)
			*charlen = 1;
		
		return (unsigned int)(utf8char[0] & 0x7F);
	}
	else if((utf8char[0] & 0xE0) == 0xC0)
	{
		if(charlen)
			*charlen = 2;
		if(size < 2 || utf8char[1] == 0)
			return 0xFFFFFFFF;
		
		return (unsigned int)(
				((utf8char[0] & 0x1F) << 6) | 
				(utf8char[1] & 0x3F)
			);
	}
	else if((utf8char[0] & 0xF0) == 0xE0)
	{
		if(charlen)
			*charlen = 3;
		if(size < 3 || utf8char[1] == 0 || utf8char[2] == 0)
			return 0xFFFFFFFF;
		
		return (unsigned int)(
				((utf8char[0] & 0x0F) << 12) | 
				((utf8char[1] & 0x3F) << 6) | 
				(utf8char[2] & 0x3F)
			);
	}
	else if((utf8char[0] & 0xF8) == 0xF0)
	{
		if(charlen)
			*charlen = 4;
		if(size < 4 || utf8char[1] == 0 || utf8char[2] == 0 || utf8char[3] == 0)
			return 0xFFFFFFFF;
		
		return (unsigned int)(
				((utf8char[0] & 0x07) << 15) | 
				((utf8char[1] & 0x3F) << 12) | 
				((utf8char[2] & 0x3F) << 6) | 
				(utf8char[3] & 0x3F)
			);
	}
	else
	{
		if(charlen)
			*charlen = 0;
		
		return 0xFFFFFFFF;
	}
}

inline int utf8_getsize(unsigned int unicode_value)
{
	int len;
	
	if(unicode_value <= 0x00007F)
	{
		len = 1;
	}
	else if((unicode_value >= 0x000080) && (unicode_value <= 0x0007FF))
	{
		len = 2;
	}
	else if(((unicode_value >= 0x000800) && (unicode_value <= 0x00D7FF))
		|| ((unicode_value >= 0x00E000) && (unicode_value <= 0x00FFFF)))
	{
		len = 3;
	}
	else if((unicode_value >= 0x010000) && (unicode_value <= 0x10FFFF))
	{
		len = 4;
	}
	else
	{
		len = -1;
	}
	
	return len;
}

int utf8_getsize_char_nc(const char *utf8char)
{
	int len;
	if(utf8char[0] == 0)
		return 0;
	else if((utf8char[0] & 0x80) == 0x00)
		return 1;
	else if((utf8char[0] & 0xE0) == 0xC0)
		len = 2;
	else if((utf8char[0] & 0xF0) == 0xE0)
		len = 3;
	else if((utf8char[0] & 0xF8) == 0xF0)
		len = 4;
	else
		return -1;
	
	return len;
}

int utf8_getsize_char(const char *utf8char)
{
	int len = utf8_getsize_char_nc(utf8char);
	for(int i = 1; i < len; i++)
	{
		if((utf8char[i] == 0) || ((utf8char[i] & 0x80) != 0x80))
		{
			return -len;
		}
	}
	
	return len;
}

int utf8_strlen(const char *str)
{
	int len, c;
	
	for(len = 0; *str; )
	{
		c = utf8_getsize_char(str);
		if(c < 0)
			return -1;
		str += c;
		len++;
	}
	
	return len;
}

int utf8_strpos_getpos(const char *str, int pos, int *startpos_out, size_t *len_out)
{
	int i, n, len;
	
	if(pos >= 0)
	{
		for(i = 0, n = 0; str[i]; )
		{
			len = utf8_getsize_char(&str[i]);
			if(len < 0)
				return -1;
			else if(pos == n)
			{
				if(startpos_out)
					*startpos_out = i;
				if(len_out)
					*len_out = len;
				
				return i;
			}
			i += len;
			n++;
		}
		
		return -1;
	}
	else
	{
		len = utf8_strlen(str);
		if(len <= 0)
			return -1;
		int newpos = len + pos;
		if(newpos < 0)
			return -1;
		
		return utf8_strpos_getpos(str, newpos, startpos_out, len_out);
	}
}

int utf8_strpos(const char *str, int pos, char *dest)
{
	int ret, startpos;
	size_t len;
	const size_t size = 8;
	
	ret = utf8_strpos_getpos(str, pos, &startpos, &len);
	if(ret < 0)
		return -1;
	
	if(len < size)
	{
		memcpy(dest, &str[startpos], len);
		dest[len] = 0;
	}
	else
	{
		memcpy(dest, &str[startpos], size - 1);
		dest[size - 1] = 0;
	}
	
	return len;
}

int utf8_substr_getpos(const char *str, int start, int nchar, 
	int *startpos_out, size_t *len_out)
{
	int pos;
	int n, len, startpos = -1, endpos = -1;
	
	if(start >= 0)
	{
		for(pos = 0, n = 0; str[pos]; )
		{
			len = utf8_getsize_char(&str[pos]);
			if(len > 0)
			{
				if(n == start)
				{
					startpos = pos;
					if(nchar < 0)
					{
						endpos = strlen((const char *)str);
						break;
					}
				}
				if(nchar >= 0 && n >= start + nchar)
				{
					endpos = pos;
					break;
				}
			}
			else
				break;
			pos += len;
			n++;
		}
		if(startpos >= 0)
		{
			if(endpos < 0)
				endpos = pos;
			assert(endpos >= startpos);
			len = endpos - startpos;
			if(startpos_out)
				*startpos_out = startpos;
			if(len_out)
				*len_out = len;
			
			return 0;
		}
		
		return -1;
	}
	else
	{
		len = utf8_strlen(str);
		if(len <= 0)
			return -1;
		int newpos = len + start;
		if(newpos < 0)
			return -1;
		
		return utf8_substr_getpos(str, newpos, nchar, startpos_out, len_out);
	}
}

int utf8_substr(const char *str, int start, int nchar, char *dest, size_t size)
{
	int ret, startpos;
	size_t len;
	
	ret = utf8_substr_getpos(str, start, nchar, &startpos, &len);
	if(ret < 0)
		return -1;
	
	if(len < size)
	{
		memcpy(dest, &str[startpos], len);
		dest[len] = 0;
	}
	else
	{
		memcpy(dest, &str[startpos], size - 1);
		dest[size - 1] = 0;
	}
	
	return len;
}

bool utf8_validate(const char *str)
{
	int c;
	
	for(; *str; )
	{
		c = utf8_getsize_char(str);
		if(c < 0)
			return false;
		str += c;
	}
	
	return true;
}

// utf8str: null-terminated valid utf-8 string, len = strlen(utf8buf)
// maxsize: max string size (without null terminator: sizeof(strbuf) - 1)
// returns: length(in bytes) of valid utf-8 string that fits to maxsize. always >=0
int utf8_cut_by_size(const char *utf8str, int len, size_t maxsize)
{
	if(len < 0)
		len = strlen(utf8str);
	if((size_t)len <= maxsize)
		return len;
	
	const char *p = utf8str + maxsize - 1; // maxsize < len
	for(; p >= utf8str; p--)
	{
		unsigned int c = (*p & 0xc0);
		if(c == 0xc0)
		{
			int cs = utf8_getsize_char(p);
			if(cs > 0)
			{
				// assert(cs > 1);
				int res = (p + (cs - 1)) - utf8str + 1;
				if(res > maxsize)
					return p - utf8str;
				else
					return res;
			}
		}
		else if(c != 0x80)
		{
			return p - utf8str + 1;
		}
	}
	return 0; // p+1 == utf8str
}




char *utf8_enc_tmp(unsigned int unicode_value)
{
#ifdef HAVE_TLS___THREAD
	static __thread char tmp[8];
#else
	static char tmp[8];
#endif
	
	if(unicode_value == 0xFFFFFFFF)
		tmp[0] = 0;
	else
	{
		int len = utf8_encode(unicode_value, tmp, sizeof(tmp));
		tmp[len] = 0;
	}
	
	return (char *)tmp;
}













/* vim: set tabstop=4 textwidth=80: */

