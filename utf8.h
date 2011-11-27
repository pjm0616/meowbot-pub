#ifndef UTF8_H_INCLUDED
#define UTF8_H_INCLUDED

#include "utf8_impl.h"

// typedef std::basic_string<ucschar> ucsstring;


int utf8_getsize_char_s(std::string utf8str);
int utf8_strlen_s(std::string str);
std::string utf8_encode_s(unsigned int unicode);
unsigned int utf8_decode_s(const std::string &utf8_str);
std::string utf8_strpos_s(std::string str, int pos);
std::string utf8_substr_s(std::string str, int start, int len = -1);
std::vector<std::string> utf8_split_chars(const std::string &str);
// ucsstring utf8string_to_ucsstring(const std::string &str);
// std::string ucsstring_to_utf8string(const ucsstring &str);
std::string utf8_sanitize_s(const std::string &str);



#endif // UTF8_H_INCLUDED

