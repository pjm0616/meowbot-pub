#ifndef UTF8_IMPL_H_INCLUDED
#define UTF8_IMPL_H_INCLUDED

#include <inttypes.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ucschar;


int utf8_encode(unsigned int unicode_value, char *dest, size_t size);
unsigned int utf8_decode_v2(const char *utf8char, size_t size, size_t *charlen);
int utf8_getsize(unsigned int unicode_value);
int utf8_getsize_char_nc(const char *utf8char);
int utf8_getsize_char(const char *utf8char);
int utf8_strlen(const char *str);
int utf8_strpos_getpos(const char *str, int pos, int *startpos_out, size_t *len_out);
int utf8_strpos(const char *str, int pos, char *dest);
int utf8_substr_getpos(const char *str, int start, int nchar, 
	int *startpos_out, size_t *len_out);
int utf8_substr(const char *str, int start, int nchar, char *dest, size_t size);
bool utf8_validate(const char *str);
int utf8_cut_by_size(const char *utf8str, int len, size_t maxsize);

char *utf8_enc_tmp(unsigned int unicode_value);

static inline int utf8_encode_nt(unsigned int unicode_value, char *dest, size_t size)
{
	int len = utf8_encode(unicode_value, dest, size - 1);
	if(len > 0)
		dest[len] = 0;
	else
		dest[0] = 0;
	return len;
}

static inline int utf8_decode(const char *utf8char, size_t *charlen)
{
	return utf8_decode_v2(utf8char, (size_t)-1, charlen);
}



#ifdef __cplusplus
}
#endif


#endif // UTF8_IMPL_H_INCLUDED

