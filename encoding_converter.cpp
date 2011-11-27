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
#include <iostream>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <iconv.h>

#include "defs.h"
#include "encoding_converter.h"




encconv::encconv()
	:converter((iconv_t)-1), 
	fill_char('?')
{
}

encconv::encconv(const std::string &from, const std::string &to, iconv_type_t type)
	:converter((iconv_t)-1), 
	fill_char('?')
{
	this->open(from, to, type);
}

encconv::~encconv()
{
	this->close();
}

int encconv::open(const std::string &from, const std::string &to, iconv_type_t type)
{
	this->close();
	
	this->from = from;
	if(type == ICONV_TYPE_TRANSLIT)
		this->to = to + "//TRANSLIT";
	else if(type == ICONV_TYPE_IGNORE)
		this->to = to + "//IGNORE";
	else
		this->to = to;
	
	this->converter = iconv_open(this->to.c_str(), from.c_str());
	if(this->converter == (iconv_t)-1)
		return -1;
	return 0;
}

int encconv::close()
{
	if(this->converter == (iconv_t)-1)
		return -1;
	
	iconv_close(this->converter);
	this->converter = (iconv_t)-1;
	return 0;
}

int encconv::convert(char **resultp, const char *data, unsigned int len, size_t *cvlen, 
	bool fail_on_invalid_seq) const
{
	char *buf;
	const char *inbuf;
	char *outbuf;
	size_t inbytesleft, outbytesleft, size;
	int ret;
	
	if(unlikely(this->converter == (iconv_t)-1))
		return -1;
	
	size = len * 4 + 16;
	buf = new char[size];
	
	inbuf = data;
	outbuf = buf;
	inbytesleft = len;
	outbytesleft = size;
	
	while(inbytesleft)
	{
#ifdef __CYGWIN__
		ret = iconv(this->converter, &inbuf, &inbytesleft, 
			&outbuf, &outbytesleft);
#else
		//								??
		ret = iconv(this->converter, (char **)&inbuf, &inbytesleft, 
			&outbuf, &outbytesleft);
#endif
		if(ret < 0)
		{
			if(errno == EILSEQ || errno == EINVAL)
			{
				if(fail_on_invalid_seq)
				{
					delete[] buf;
					return -2;
				}
				else
				{
					*outbuf = this->fill_char;
					outbuf++; outbytesleft--;
					inbuf++; inbytesleft--;
				}
			}
			else if(errno == E2BIG)
			{
				// TODO
				break;
			}
			else if(unlikely(errno == EBADF))
			{
				// cannot happen
				delete[] buf;
				return -3;
			}
			else
			{
				perror("iconv");
				break;
			}
		}
	}
	*outbuf = 0;
	if(cvlen)
		*cvlen = outbuf - buf;
	*resultp = buf;
	
	return 0;
}

char *encconv::convert(const char *data, unsigned int len, size_t *cvlen, 
	bool fail_on_invalid_seq) const
{
	char *result;
	int ret = this->convert(&result, data, len, cvlen, fail_on_invalid_seq);
	if(ret < 0)
		result = NULL;
	return result;
}

int encconv::convert(std::string &dest, const std::string &str, bool fail_on_invalid_seq) const
{
#if 0
	char buf[1024];
	//char buf[8]; // 버퍼 사이즈가 작을 경우 문제발생(multi byte char이 깨짐)
	const char *inbuf;
	char *outbuf;
	size_t inbytesleft, outbytesleft;
	int ret;
	
	if(unlikely(this->converter == (iconv_t)-1))
		return "";
	
	inbuf = str.data();
	size_t datalen = str.length();
	
	std::string result;
	while(datalen > 0)
	{
		outbuf = buf;
		outbytesleft = sizeof(buf);
		if(datalen >= sizeof(buf))
			inbytesleft = sizeof(buf);
		else
			inbytesleft = datalen;
		ret = iconv(this->converter, (char **)&inbuf, &inbytesleft, &outbuf, &outbytesleft);
		if(ret < 0)
		{
			if(errno == EILSEQ || errno == EINVAL)
			{
				*outbuf = '?';
				outbuf++; outbytesleft--;
				inbuf++; inbytesleft--;
			}
			else if(errno == E2BIG)
			{
				continue;
			}
			else if(unlikely(errno == EBADF))
			{
				// cannot happen
				return NULL;
			}
			else
			{
				perror("iconv");
				break;
			}
		}
		else
		{
			size_t cvlen = sizeof(buf) - outbytesleft;
			datalen -= cvlen;
			if(unlikely(cvlen > sizeof(buf)))
			{
				fprintf(stderr, "iconv result %d is bigger than buffer size %d\n", cvlen, sizeof(buf));
				break;
			}
			result.append(buf, cvlen);
		}
	}
	dest = result;
	
	return 0;
#else
	size_t cvlen;
	char *converted = NULL;
	int ret = this->convert(&converted, str.c_str(), str.length(), &cvlen, fail_on_invalid_seq);
	if(converted == NULL || ret < 0)
		return -1;
	dest.assign(converted, cvlen);
	delete[] converted;
	return 0;
#endif
}

std::string encconv::convert(const std::string &str, bool fail_on_invalid_seq) const
{
	std::string buf;
	int ret = this->convert(buf, str, fail_on_invalid_seq);
	if(ret < 0)
		return "";
	return buf;
}

/* [static] */ int encconv::convert(std::string &dest, const std::string &from, 
	const std::string &to, const std::string &str, bool fail_on_invalid_seq)
{
	return encconv(from, to).convert(dest, str, fail_on_invalid_seq);
}
/* [static] */ std::string encconv::convert(const std::string &from, 
	const std::string &to, const std::string &str, bool fail_on_invalid_seq)
{
	return encconv(from, to).convert(str, fail_on_invalid_seq);
}





// vim: set tabstop=4 textwidth=80:

