/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; see the file COPYING.LIB.  If
 * not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite
 * 330, Boston, MA  02111-1307, USA.
 */

// linux & proftpd vsprintf implementation mixed version + some patches


#include "defs.h"
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>

#include "utf8.h"
#include "hangul.h"
#include "luacpp.h"
#include "my_vsprintf.h"


#define DECIMAL_STRING_LENGTH 512
#ifdef __CYGWIN__ // 20090701 pjm0616
#define HAVE_FCVT
#else
#define HAVE_FCVT_R
#endif


static int skip_atoi(const char **s)
{
	int i=0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

static char * number(char * buf, char * end, unsigned long long num, int base, int size, int precision, int type)
{
	char c,sign,tmp[66];
	const char *digits;
	static const char small_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	static const char large_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	digits = (type & LARGE) ? large_digits : small_digits;
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return NULL;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if ((signed long long) num < 0) {
			sign = '-';
			num = - (signed long long) num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num != 0)
		{tmp[i++] = digits[num % base]; num /= base;} // XXX ORIGNAL CODE tmp[i++] = digits[do_div(num,base)];
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT))) {
		while(size-->0) {
			if (buf <= end)
				*buf = ' ';
			++buf;
		}
	}
	if (sign) {
		if (buf <= end)
			*buf = sign;
		++buf;
	}
	if (type & SPECIAL) {
		if (base==8) {
			if (buf <= end)
				*buf = '0';
			++buf;
		} else if (base==16) {
			if (buf <= end)
				*buf = '0';
			++buf;
			if (buf <= end)
				*buf = digits[33];
			++buf;
		}
	}
	if (!(type & LEFT)) {
		while (size-- > 0) {
			if (buf <= end)
				*buf = c;
			++buf;
		}
	}
	while (i < precision--) {
		if (buf <= end)
			*buf = '0';
		++buf;
	}
	while (i-- > 0) {
		if (buf <= end)
			*buf = tmp[i];
		++buf;
	}
	while (size-- > 0) {
		if (buf <= end)
			*buf = ' ';
		++buf;
	}
	return buf;
}

/**
 * vsnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The return value is the number of characters which would
 * be generated for the given input, excluding the trailing
 * '\0', as per ISO C99. If you want to have the exact
 * number of characters written into @buf as return value
 * (not including the trailing '\0'), use vscnprintf. If the
 * return is greater than or equal to @size, the resulting
 * string is truncated.
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want snprintf instead.
 */

#include "my_vsprintf_core_impl.h"

#define MY_VSPRINTF_LUA_VERSION
#include "my_vsprintf_core_impl.h"
#undef MY_VSNPRIN_LUA_VERSION


/**
 * vscnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The return value is the number of characters which have been written into
 * the @buf not including the trailing '\0'. If @size is <= 0 the function
 * returns 0.
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want scnprintf instead.
 */
int my_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int i;

	i = my_vsnprintf(buf,size,fmt,args);
	return (i >= 0 && (size_t)i >= size) ? (size - 1) : i;
}


/**
 * snprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters which would be
 * generated for the given input, excluding the trailing null,
 * as per ISO C99.  If the return is greater than or equal to
 * @size, the resulting string is truncated.
 */
int my_snprintf(char * buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = my_vsnprintf(buf,size,fmt,args);
	va_end(args);
	return i;
}


/**
 * scnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters written into @buf not including
 * the trailing '\0'. If @size is <= 0 the function returns 0. If the return is
 * greater than or equal to @size, the resulting string is truncated.
 */

int my_scnprintf(char * buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = my_vsnprintf(buf, size, fmt, args);
	va_end(args);
	return (i >= 0 && (size_t)i >= size) ? (size - 1) : i;
}

/**
 * vsprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf. Use vsnprintf or vscnprintf in order to avoid
 * buffer overflows.
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want sprintf instead.
 */
int my_vsprintf(char *buf, const char *fmt, va_list args)
{
	return my_vsnprintf(buf, (~0U)>>1, fmt, args);
}


/**
 * sprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf. Use snprintf or scnprintf in order to avoid
 * buffer overflows.
 */
int my_sprintf(char * buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = my_vsprintf(buf,fmt,args);
	va_end(args);
	return i;
}






