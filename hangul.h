#ifndef HANGUL_H_INCLUDED
#define HANGUL_H_INCLUDED

#include "utf8.h"
#include "hangul_impl.h"

std::string hangul_join_utf8str(std::string initial, std::string medial, std::string final);
int hangul_split_utf8str(std::string ch, std::string *initial, std::string *medial, std::string *final);

std::string apply_head_pron_rule(std::string word);
std::string select_josa(const std::string &ctx, const std::string &josa);


#endif // HANGUL_H_INCLUDED

