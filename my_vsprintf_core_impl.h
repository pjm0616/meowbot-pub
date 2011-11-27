
#ifdef MY_VSPRINTF_LUA_VERSION
int my_vsnprintf_lua(char *buf, size_t size, const char *fmt, lua_State *L, int st_idx)
#else
int my_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
#endif
{
	int len;
	unsigned long long num;
	int i, base;
	char *str, *end, c;
	const char *s;

	int flags;		/* flags to number() */
	int dotflag;

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */
				/* 'z' support added 23/7/1999 S.H.    */
				/* 'z' changed to 'Z' --davidm 1/25/99 */

	/* Reject out-of-range values early */
	if (unlikely((int) size < 0)) {
		//fprintf(stderr, "");
		return 0;
	}
#define APPEND_CHAR(ch_) if(str <= end){*str = (ch_);} str++;

#ifdef MY_VSPRINTF_LUA_VERSION
	int argcnt = lua_gettop(L);
#endif

	str = buf;
	end = buf + size - 1;

	if (end < buf - 1) {
		end = ((char *) -1);
		size = end - buf + 1;
	}

	for (; *fmt ; ++fmt) {
		if (*fmt != '%') {
			if (str <= end)
				*str = *fmt;
			++str;
			continue;
		}

		/* process flags */
		flags = 0;
		dotflag = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
			}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
#ifdef MY_VSPRINTF_LUA_VERSION
			field_width = ((st_idx <= argcnt) ? luaL_checkint(L, st_idx++) : 0);
#else
			field_width = va_arg(args, int);
#endif
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;	
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
#ifdef MY_VSPRINTF_LUA_VERSION
				precision = ((st_idx <= argcnt) ? luaL_checkint(L, st_idx++) : 0);
#else
				precision = va_arg(args, int);
#endif
			}
			
#if 0
			/* NB: the default precision value is conversion dependent */
			if(precision < 0)
				precision = 0;
#else
			dotflag = 1;
#endif
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' ||
		    *fmt =='Z' || *fmt == 'z') {
			qualifier = *fmt;
			++fmt;
			if (qualifier == 'l' && *fmt == 'l') {
				qualifier = 'L';
				++fmt;
			}
		}

		/* default base */
		base = 10;

		switch (*fmt) {
			case 'c':
				if (!(flags & LEFT)) {
					while (--field_width > 0) {
						if (str <= end)
							*str = ' ';
						++str;
					}
				}
#ifdef MY_VSPRINTF_LUA_VERSION
				c = (unsigned char) ((st_idx <= argcnt) ? luaL_checkint(L, st_idx++) : 0);
#else
				c = (unsigned char) va_arg(args, int);
#endif
				if (str <= end)
					*str = c;
				++str;
				while (--field_width > 0) {
					if (str <= end)
						*str = ' ';
					++str;
				}
				continue;

			case 's':
			case 'S':
				if(dotflag && precision < 0)
					precision = 0;
#ifdef MY_VSPRINTF_LUA_VERSION
				s = ((st_idx <= argcnt) ? luaL_checkstring(L, st_idx++) : 0);
#else
				if(*fmt == 'S')
				{
					const std::string *ss = va_arg(args, const std::string *);
					s = ss?ss->c_str():NULL;
				}
				else
				{
					s = va_arg(args, char *);
				}
#endif
				if (s == NULL)
					s = "(null)";

				len = strnlen(s, precision);

				if (!(flags & LEFT)) {
					while (len < field_width--) {
						if (str <= end)
							*str = ' ';
						++str;
					}
				}
				for (i = 0; i < len; ++i) {
					if (str <= end)
						*str = *s;
					++str; ++s;
				}
				while (len < field_width--) {
					if (str <= end)
						*str = ' ';
					++str;
				}
				continue;
			
#if 1
			case 'f':
			{
#ifdef MY_VSPRINTF_LUA_VERSION
				double dval = (double)((st_idx <= argcnt) ? luaL_checknumber(L, st_idx++) : 0);
#else
				double dval = va_arg(args, double);
#endif
				/* Default FP precision */
				if(dotflag && precision < 0)
					precision = 6;
				if(precision < 0) precision = 2; // FIXME !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				/* Let's not press our luck too far */
				int ndigit = precision < 16 ? precision : 16;
				
				/*
				** If available fconvert() is preferred, but fcvt() is
				** more widely available.  It is included in 4.3BSD,
				** the SUS1 and SUS2 standards, Gnu libc.
				*/
				int	decpt, sign;
				char cvtbuf[DECIMAL_STRING_LENGTH] = {'\0'};
#if defined(HAVE_FCONVERT)
				char *cbp = fconvert(dval, ndigit, &decpt, &sign, cvtbuf);
				cvtbuf[sizeof(cvtbuf) - 1] = 0;
#elif defined(HAVE_FCVT_R)
				int fcvt_res = fcvt_r(dval, ndigit, &decpt, &sign, cvtbuf, sizeof(cvtbuf));
				if(fcvt_res < 0)
				{
					// default: case
					if (str <= end)
						*str = '%';
					++str;
					if (*fmt) {
						if (str <= end)
							*str = *fmt;
						++str;
					} else {
						--fmt;
					}
				}
				cvtbuf[sizeof(cvtbuf) - 1] = 0;
				char *cbp = cvtbuf;
#elif defined(HAVE_FCVT)
				char *cbp = fcvt(dval, ndigit, &decpt, &sign);
				strncpy(cvtbuf, cbp, sizeof(cvtbuf));
				cvtbuf[sizeof(cvtbuf) - 1] = 0;
				cbp = cvtbuf;
#endif
				/* XXX Ought to honor field_width, left/right justification */

				/* Result could be "NaN" or "Inf" */
				if ( ! isdigit(*cbp) ) {
					for ( i = 0 ; *cbp != '\0'; i++ ) {
						if (str <= end)
							*str = *cbp;
						str++;
						cbp++;
					}
					continue;
				}

				if ( sign ) {
					APPEND_CHAR('-');
				}
				else if ( flags & PLUS ) {
					APPEND_CHAR('+');
				}
				else if ( flags & SPACE ) {
					APPEND_CHAR(' ');
				}

				/* Leading zeros, if needed */
				if ( decpt <= 0 ) {
					/* Prepend '0' */
					APPEND_CHAR('0');
				}
				if ( decpt < 0 ) {
					/* Prepend '.' */
					APPEND_CHAR('.');
					for ( i = decpt ; i < 0; i++ ) {
						APPEND_CHAR('0');
					}
				}
				/* Significant digits */
				for ( i = 0 ; ; i++ ) {
					if ( i == decpt ) {
						if ( *cbp != '\0' ) {
							APPEND_CHAR('.');
						}
						else {
							/* Tack on "." or ".0"??? */
							break;
						}
					}
					if ( *cbp != '\0' ) {
						APPEND_CHAR(*cbp);
						cbp++;
					}
					else if ( i < decpt ) {
						APPEND_CHAR('0');
					}
					else {
						/* Tack on "." or ".0"??? */
						break;
					}
				}
				
				continue;
			}
#endif
			
			case 'J': // 조사 선택
				{
					if(str > end)
					{
						str += 12; // FIXME: 조사 길이가 될 수 있는 최대값으로 대충 넉넉하게 =_=
						continue; // FIXME
					}
					const char *fmt2 = fmt + 1;
					int josalen = 0;
					if(*fmt2++ != '{')
						continue; // should be fixed
					for(; *fmt2 != 0; fmt2++)
					{
						if(*fmt2 == '}')
						{
							josalen = fmt2 - fmt - 2;
							break;
						}
					}
					if(josalen == 0)
						continue; // should be fixed
					*str = 0;
					std::string josares = select_josa(buf, std::string(fmt+2, josalen));
					s = josares.c_str();
					len = josares.length();
					for (i = 0; i < len; ++i) {
						if (str <= end)
							*str = *s;
						++str; ++s;
					}
					josares.clear();
					fmt = fmt2;
				}
				continue;

			case 'p':
				if(dotflag && precision < 0)
					precision = 0;
				if (field_width == -1) {
					field_width = 2*sizeof(void *);
					flags |= ZEROPAD;
				}
#ifdef MY_VSPRINTF_LUA_VERSION
				str = number(str, end,
						(unsigned long)((st_idx <= argcnt) ? luaL_checkint(L, st_idx++) : 0),
						16, field_width, precision, flags);
#else
				str = number(str, end,
						(unsigned long) va_arg(args, void *),
						16, field_width, precision, flags);
#endif
				continue;
			
#ifdef MY_VSPRINTF_LUA_VERSION
			case 'b':
			{
				bool bval = lua_toboolean(L, st_idx++);
				const char *bstr = bval?"true":"false";
				while(*bstr)
				{
					if(str <= end)
						*str = *bstr;
					++str; ++bstr;
				}
				continue;
			}
#endif
			
#ifndef MY_VSPRINTF_LUA_VERSION
			case 'n':
				/* FIXME:
				* What does C99 say about the overflow case here? */
				if (qualifier == 'l') {
					long * ip = va_arg(args, long *);
					*ip = (str - buf);
				} else if (qualifier == 'Z' || qualifier == 'z') {
					size_t * ip = va_arg(args, size_t *);
					*ip = (str - buf);
				} else {
					int * ip = va_arg(args, int *);
					*ip = (str - buf);
				}
				continue;
#endif

			case '%':
				if (str <= end)
					*str = '%';
				++str;
				continue;

				/* integer number formats - set up the flags and "break" */
			case 'o':
				base = 8;
				break;

			case 'X':
				flags |= LARGE;
			case 'x':
				base = 16;
				break;

			case 'd':
			case 'i':
				flags |= SIGN;
			case 'u':
				break;

			default:
				if (str <= end)
					*str = '%';
				++str;
				if (*fmt) {
					if (str <= end)
						*str = *fmt;
					++str;
				} else {
					--fmt;
				}
				continue;
		}
		if (qualifier == 'L')
		{
#ifdef MY_VSPRINTF_LUA_VERSION
			num = (long long)((st_idx <= argcnt) ? luaL_checklong(L, st_idx++) : 0); // FIXME
#else
			num = va_arg(args, long long);
#endif
		}
		else if (qualifier == 'l') {
#ifdef MY_VSPRINTF_LUA_VERSION
			num = (unsigned long)((st_idx <= argcnt) ? luaL_checklong(L, st_idx++) : 0);
#else
			num = va_arg(args, unsigned long);
#endif
			if (flags & SIGN)
				num = (signed long) num;
		} else if (qualifier == 'Z' || qualifier == 'z') {
#ifdef MY_VSPRINTF_LUA_VERSION
			num = (size_t)((st_idx <= argcnt) ? luaL_checkint(L, st_idx++) : 0);
#else
			num = va_arg(args, size_t);
#endif
		} else if (qualifier == 'h') {
#ifdef MY_VSPRINTF_LUA_VERSION
			num = (unsigned short)((st_idx <= argcnt) ? luaL_checkint(L, st_idx++) : 0);
#else
			num = (unsigned short) va_arg(args, int);
#endif
			if (flags & SIGN)
				num = (signed short) num;
		} else {
#ifdef MY_VSPRINTF_LUA_VERSION
			num = (unsigned int)((st_idx <= argcnt) ? luaL_checkint(L, st_idx++) : 0);
#else
			num = va_arg(args, unsigned int);
#endif
			if (flags & SIGN)
				num = (signed int) num;
		}
		if(dotflag && precision < 0)
			precision = 0;
		str = number(str, end, num, base,
				field_width, precision, flags);
	}
	if (str <= end)
		*str = '\0';
	else if (size > 0)
		/* don't write out a null byte if the buf size is zero */
		*end = '\0';
	/* the trailing null byte doesn't count towards the total
	* ++str;
	*/
#undef APPEND_CHAR
	return str-buf;
}



