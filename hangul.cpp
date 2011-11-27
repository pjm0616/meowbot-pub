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
#include <vector>
#include <inttypes.h>
#include <stdbool.h>

#include <pcrecpp.h>


#include "utf8_impl.h"
#include "utf8.h"
#include "hangul_impl.h"
#include "hangul.h"



std::string hangul_join_utf8str(std::string initial, std::string medial, std::string final)
{
	unsigned int ch;
	char buf[8] = {0, };
	
	ch = hangul_join_utf8char(initial.c_str(), medial.c_str(), final.c_str());
	if(ch == 0)
		return "";
	
	utf8_encode(ch, buf, sizeof(buf));
	buf[3] = 0;
	
	return buf;
}

int hangul_split_utf8str(std::string ch, std::string *initial, 
	std::string *medial, std::string *final)
{
	int ret;
	char ibuf[8] = {0, };
	char mbuf[8] = {0, };
	char fbuf[8] = {0, };
	
	ret = hangul_split_utf8char(ch.c_str(), ibuf, mbuf, fbuf);
	if(ret < 0)
		return -1;
	ibuf[3] = mbuf[3] = fbuf[3] = 0;
	
	if(initial)
		*initial = ibuf;
	if(medial)
		*medial = mbuf;
	if(final)
		*final = fbuf;
	
	return 0;
}

/*
	* ㄴ이나 ㄹ이 ㅇ으로 바뀌는 경우
		o 한자음 '녀, 뇨, 뉴, 니', '랴, 려, 례, 료, 류, 리'가 단어 첫머리에 올 때 '여, 요, 유, 이', '야, 여, 예, 요, 유, 이'로 발음한다.
		o 한자음 '라, 래, 로, 뢰, 루, 르'가 단어 첫머리에 올 때 '나, 내, 노, 뇌, 누, 느'로 발음한다.
	* 모음이나 ㄴ 받침 뒤에 이어지는 '렬, 률'은 '열, 율'로 발음한다.
*/

std::string apply_head_pron_rule(std::string word)
{
	const char *orig = word.c_str();
	std::string result, firstchar, remainedstr, i, m, f;
	
	int hlen = utf8_getsize_char(orig);
	if(hlen < 0)
		return word;
	
	firstchar = utf8_strpos_s(word, 0);
	remainedstr = word.substr(hlen);
	
	if(hangul_split_utf8str(firstchar, &i, &m, &f) < 0)
		return word;
	
	if(i == "ㄴ")
	{
		if(m == "ㅕ" || m == "ㅛ" || m == "ㅠ" || m == "ㅣ")
			i = "ㅇ"; // U+3147
	}
	else if(i == "ㄹ") // U+3139
	{
		if(m == "ㅑ" || m == "ㅕ" || m == "ㅖ" || m == "ㅛ" || m == "ㅠ" || m == "ㅣ")
			i = "ㅇ"; // U+3147
		else if(m == "ㅏ" || m == "ㅐ" || m == "ㅗ" || m == "ㅚ" || m == "ㅜ" || m == "ㅡ")
			i = "ㄴ";
	}
	result = hangul_join_utf8str(i, m, f) + remainedstr;
	
	// 모음이나 ㄴ 받침 뒤에 이어지는 '렬, 률'은 '열, 율'로 발음한다.
	// 라렬(羅列) -> 나열
	if(f == "" || f == "ㄴ")
	{
		std::string secondchar, i2, m2, f2;
		int hlen2;
		hlen2 = utf8_getsize_char(orig + hlen);
		if(hlen2 > 0)
		{
			hlen2 += hlen;
			firstchar = utf8_strpos_s(result, 0);
			secondchar = utf8_strpos_s(result, 1);
			remainedstr = result.substr(hlen2);
			if(secondchar == "렬")
				secondchar = "열";
			else if(secondchar == "률")
				secondchar = "율";
			result = firstchar + secondchar + remainedstr;
		}
	}
	
	return result;
}


static std::string get_alternative_josa(const std::string &josa)
{
	if(josa == "은는")
		return "은(는)";
	else if(josa == "이가")
		return "이(가)";
	else if(josa == "을를")
		return "을(를)";
	else if(josa == "과와")
		return "과(와)";
	else if(josa == "아야")
		return "아(야)";
	else if(josa == "이")
		return "(이)";
	else if(josa == "으")
		return "(으)";
	else if(josa == "s")
		return "(s)";
	else
		return josa;
}

std::string select_josa(const std::string &ctx, const std::string &josa)
{
	static pcrecpp::RE	re_usable_chars_level1("[^가-힣0-9a-zA-Z]", pcrecpp::UTF8()), 
						re_usable_chars_level2("[^가-힣0-9a-zA-Zㄱ-ㅎㅏ-ㅣㅥ-ㆆㆇ-ㆎ]", pcrecpp::UTF8()), 
						re_check_consonants("^[ㄱ-ㅎㅥ-ㆆ]$", pcrecpp::UTF8()), 
						re_check_vowels("^[ㅏ-ㅣㆇ-ㆎ]$", pcrecpp::UTF8()), 
						re_extract_realjosa("^([^은는이가을를과와아야으]*)([은는이가을를과와아야으]+)(.*)", pcrecpp::UTF8());
	
	if(ctx.empty() || josa.empty())
		return get_alternative_josa(josa);
	
	if(josa == "s")
	{
		const char *start = ctx.c_str();
		const char *p = start + ctx.length() - 1;
		
		for(; p >= start; p--)
		{
			if(isdigit(*p))
			{
				if(*p != '1' || (p > start && isdigit(*(p-1))))
					return "s";
				else
					return "";
			}
		}
		// number not found
		return "(s)";
	}
	
	std::string pctx(ctx);
	re_usable_chars_level1.GlobalReplace("", &pctx);
	if(pctx.empty())
	{
		pctx = ctx;
		re_usable_chars_level2.GlobalReplace("", &pctx);
		if(pctx.empty())
			return get_alternative_josa(josa);
	}
	int ret, jongseong = -1;
	
	std::string lastchar = utf8_strpos_s(pctx, -1);
	std::string lastchar2 = utf8_strpos_s(pctx, -2);
	// if(jongseong < 0)
	{
		char c = lastchar[0];
		// FIXME: 일단 대문자 유지, 소문자 변환은 나중에
		if(c >= 'A' && c <= 'Z')
			c = tolower(c);
		char c2 = lastchar2.empty() ? 0 : lastchar2[0];
		
		if(c2 == 'l' && c == 'e')
			jongseong = 1;
		else if(c2 == 'n' && c == 'g')
			jongseong = 1;
		else
		{
			jongseong = 
			(
				c == '0' ? 1 : c == '1' ? 1 : c == '2' ? 0 : c == '3' ? 1 : c == '4' ? 0 : 
				c == '5' ? 0 : c == '6' ? 1 : c == '7' ? 1 : c == '8' ? 1 : c == '9' ? 0 : 
				((c >= 'a' && c <= 'z') && (c2 >= 'a' && c2 <= 'z'))? // 소문자 2개가 연달아 있으면 발음으로 취급
				(
					((c >= 'a' && c <= 'j') || c == 'o' || c == 'q' || c == 's' || (c >= 'u' && c <= 'z')) ? 0 : 
					(c == 'k' || c == 'p' || c == 'l' || c == 'm' || c == 'n' || c == 'r' || c == 't') ? 1 : -1
				):(
					((c >= 'a' && c <= 'k') || c == 'o' || c == 'p' || c == 'q' || (c >= 's' && c <= 'z')) ? 0 : 
					(c == 'l' || c == 'm' || c == 'n' || c == 'r') ? 1 : -1
				)
			);
		}
	}
	if(jongseong < 0)
	{
		std::string f;
		ret = hangul_split_utf8str(lastchar, NULL, NULL, &f);
		if(ret == 0)
			jongseong = f.empty() ? 0 : 1;
	}
	if(jongseong < 0)
	{
		if(re_check_consonants.FullMatch(lastchar))
			jongseong = 1;
		else if(re_check_vowels.FullMatch(lastchar))
			jongseong = 0;
	}
	if(jongseong < 0)
		return get_alternative_josa(josa);
	
	std::string realjosa, pre, suf;
	if(!re_extract_realjosa.FullMatch(josa, &pre, &realjosa, &suf))
	{
		realjosa = josa;
	}
	
	if(realjosa == "은는")
		realjosa = jongseong ? "은" : "는";
	else if(realjosa == "이가")
		realjosa = jongseong ? "이" : "가";
	else if(realjosa == "을를")
		realjosa = jongseong ? "을" : "를";
	else if(realjosa == "과와")
		realjosa = jongseong ? "과" : "와";
	else if(realjosa == "아야")
		realjosa = jongseong ? "아" : "야";
	else if(realjosa == "이") // (이)라고
		realjosa = jongseong ? "이" : "";
	else if(realjosa == "으") // (으)로
		realjosa = jongseong ? "으" : "";
	
	return pre + realjosa + suf;
}






// vim: set tabstop=4 textwidth=80:

