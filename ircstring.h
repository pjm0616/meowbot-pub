#ifndef IRCSTRING_H_INCLUDED
#define IRCSTRING_H_INCLUDED

#include <string>
#include <map>

#define IRC_CHARMAP_NONE 0
#define IRC_CHARMAP_ASCII 1
#define IRC_CHARMAP_RFC1459 2
#define IRC_CHARMAP_RFC1459E 3

// aA[]\~^ => aA[]\~^
// #define IRC_CHARMAP IRC_CHARMAP_NONE
// aA[]\~^ => AA[]\~^
// #define IRC_CHARMAP IRC_CHARMAP_ASCII
// aA[]\~^ => AA{}|~^
// #define IRC_CHARMAP IRC_CHARMAP_RFC1459
// aA[]\~ => AA{}|^
#define IRC_CHARMAP IRC_CHARMAP_RFC1459E


// RFC1459 - []\ 와 {}| 를 각각 lower/upper case 처리해서 대소문자 구분 없이 비교
char irc_tolower(char c);
char irc_toupper(char c);
std::string irc_strlower(const std::string &str);
std::string irc_strupper(const std::string &str);
int irc_strcasecmp(const char *s1, const char *s2);
int irc_strcasecmp(const std::string &s1, const std::string &s2);
int irc_strncasecmp(const char *s1, const char *s2, int n);
int irc_strncasecmp(const std::string &s1, const std::string &s2, int n);

struct irc_char_traits: std::char_traits<char>
{
	static bool eq(char c1, char c2){return irc_tolower(c1) == irc_tolower(c2);}
	static bool ne(char c1, char c2){return irc_tolower(c1) != irc_tolower(c2);}
	static bool lt(char c1, char c2){return irc_tolower(c1) < irc_tolower(c2);}
	static int compare(const char *str1, const char *str2, size_t n){return irc_strncasecmp(str1, str2, n);}
	static const char* find(const char *str, int n, char c)
	{
		char c2 = irc_tolower(c);
		while(n-- > 0 && irc_tolower(*str) != c2)
			str++;
		return str;
	}
};
typedef std::basic_string<char, irc_char_traits> irc_string;

struct irc_string_lesscomparator
{
	bool operator()(const std::string &s1, const std::string &s2) const
	{
		return irc_strcasecmp(s1.c_str(), s2.c_str()) < 0;
	}
};

// RFC1459 - []\ 와 {}| 를 각각 lower/upper case 처리해서 key를 대소문자 구분 없이 비교하는 std::map
// std::map과의 호환을 위해 STD_STRING_TYPE은 항상 std::string
template <typename STD_STRING_TYPE, typename T>
class irc_strmap: public std::map<std::string, T, irc_string_lesscomparator>
//class irc_strmap: public std::map<std::string, T>
{
};




/////////

struct caseless_char_traits: std::char_traits<char>
{
	static bool eq(char c1, char c2){return tolower(c1) == tolower(c2);}
	static bool ne(char c1, char c2){return tolower(c1) != tolower(c2);}
	static bool lt(char c1, char c2){return tolower(c1) < tolower(c2);}
	static int compare(const char *str1, const char *str2, size_t n){return strncasecmp(str1, str2, n);}
	static const char* find(const char *str, int n, char c)
	{
		char c2 = tolower(c);
		while(n-- > 0 && tolower(*str) != c2)
			str++;
		return str;
	}
};
typedef std::basic_string<char, caseless_char_traits> caseless_string;

struct caseless_string_lesscomparator
{
	bool operator()(const std::string &s1, const std::string &s2) const
	{
		return strcasecmp(s1.c_str(), s2.c_str()) < 0;
	}
};

// key를 대소문자 구분 없이 비교하는 std::map
// std::map과의 호환을 위해 STD_STRING_TYPE은 항상 std::string
template <typename STD_STRING_TYPE, typename T>
class caseless_strmap: public std::map<std::string, T, caseless_string_lesscomparator>
//class caseless_strmap: public std::map<std::string, T>
{
};







#endif // IRCSTRING_H_INCLUDED

