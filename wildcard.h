#ifndef WILDCARD_H_INCLUDED
#define WILDCARD_H_INCLUDED

bool wc_match(const char *mask, const char *str);
bool wc_match_nocase(const char *mask, const char *str);
bool wc_match_irccase(const char *mask, const char *str);

static inline bool wc_match(const std::string &mask, const std::string &str)
{
	return wc_match(mask.c_str(), str.c_str());
}
static inline bool wc_match_nocase(const std::string &mask, const std::string &str)
{
	return wc_match_nocase(mask.c_str(), str.c_str());
}
static inline bool wc_match_irccase(const std::string &mask, const std::string &str)
{
	return wc_match_irccase(mask.c_str(), str.c_str());
}


#endif // WILDCARD_H_INCLUDED
