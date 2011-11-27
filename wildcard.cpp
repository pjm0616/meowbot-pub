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
#include <cstring>

#include "defs.h"
#include "ircstring.h"
#include "wildcard.h"



// public domain코드를 가져와서 수정

template <typename CASE_CONVERTER>
bool wc_match_tmpl(const char *mask, const char *str)
{
	const char *cp = NULL, *mp = NULL;
	CASE_CONVERTER case_converter;
	
	while((*str) && (*mask != '*'))
	{
		if((case_converter(*mask) != case_converter(*str)) && 
			(*mask != '?'))
		{
			return false;
		}
		mask++;
		str++;
	}
	while (*str)
	{
		if (*mask == '*')
		{
			if (!*++mask)
			{
				return true;
			}
			mp = mask;
			cp = str+1;
		}
		else if((case_converter(*mask) == case_converter(*str)) || 
			(*mask == '?'))
		{
			mask++;
			str++;
		}
		else
		{
			mask = mp;
			str = cp++;
		}
	}
	while(*mask == '*')
	{
		mask++;
	}
	
	return *mask?false:true;
}



struct case_conv
{
	inline char operator()(char c) const
	{
		return c;
	}
};
bool wc_match(const char *mask, const char *str)
{
	return wc_match_tmpl<case_conv>(mask, str);
}

struct nocase_conv
{
	inline char operator()(char c) const
	{
		return tolower(c);
	}
};
bool wc_match_nocase(const char *mask, const char *str)
{
	return wc_match_tmpl<nocase_conv>(mask, str);
}

struct irccase_conv
{
	inline char operator()(char c) const
	{
		return irc_tolower(c);
	}
};
bool wc_match_irccase(const char *mask, const char *str)
{
	return wc_match_tmpl<irccase_conv>(mask, str);
}



// vim: set tabstop=4 textwidth=80:
