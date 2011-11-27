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


#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <cmath>
#include <assert.h>

#include <pcrecpp.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/classification.hpp> // boost::algorithm::is_any_of
#include <boost/algorithm/string/split.hpp>

#include "defs.h"
#include "hangul.h"
#include "module.h"
#include "tcpsock.h"
#include "etc.h"
#include "ircbot.h"

#include "base64.h"
#include "md5.h"

struct tarot_data_s
{
	const char *name, *name_eng, *direction, *description;
};

tarot_data_s tarot_data[] = 
{
#include "mod_strutils_tarot_data.h"
};





static unsigned int md5rand(const irc_privmsg &pmsg, std::string msg)
{
	unsigned int md5buf[4], n;
	
	if(msg.empty())
		return rand();
	
	pcrecpp::RE("[^가-힣]?(나|내)[은는이가을를과와 ]").GlobalReplace(pmsg.getnick(), &msg);
	std::string buf(msg);
	
	pcrecpp::RE("[^-_+=()[\\]*&|^$#@<>{}가-힣一-龥0-9a-zA-Zぁ-んァ-ヶ]", pcrecpp::UTF8()).GlobalReplace("", &buf);
	if(buf.empty())
	{
		buf = msg;
		pcrecpp::RE("[^-_+=()[\\]*&|^$#@<>{}가-힣一-龥0-9a-zA-Zぁ-んァ-ヶㄱ-ㅣㅥ-ㆎ]", pcrecpp::UTF8()).GlobalReplace("", &buf);
		if(buf.empty())
		{
			buf = msg;
		}
	}
	md5(int2string(time(NULL) / 3600) + buf, md5buf);
	n = md5buf[0] ^ md5buf[1] ^ md5buf[2] ^ md5buf[3];
	
	return n;
}

static bool is_negative_msg(std::string msg)
{
	static const char *nw_list[] = 
	{
		"아", "안", "않", "외", "빼", "뺀", "제한", "말고", "없", "업", "부", "불", "비", "반대", "거짓", 
		"not", "false", "틀", "못", 
		NULL
	};
	int factor = 0;
	
	pcrecpp::RE("[^가-힣a-zA-Z]", pcrecpp::UTF8()).GlobalReplace("", &msg);
	
	for(int i = 0; nw_list[i]; i++)
	{
		//factor += count_word(msg, nw_list[i]);
		factor += !!strstr(msg.c_str(), nw_list[i]);
		
		//const char *p = msg.c_str();
		//while((p = strstr(p, nw_list[i])))
		//{
		//	factor++;
		//}
	}
	
	return (factor % 2 == 1) ? true : false;
}


///// 궁합 START
static int get_char_stroke(const std::string &str)
{
	static const struct
	{
		const char *str;
		int stroke;
	} stroke_list[] = 
	{
		{"ㄱ", 1}, {"ㄴ", 1}, {"ㄷ", 2}, {"ㄹ", 3}, {"ㅁ", 3}, {"ㅂ", 4}, {"ㅅ", 2}, {"ㅇ", 1}, 
		{"ㅈ", 2}, {"ㅊ", 3}, {"ㅋ", 2}, {"ㅌ", 3}, {"ㅍ", 4}, {"ㅎ", 3}, {"ㄲ", 4}, {"ㄸ", 4}, 
		{"ㅃ", 8}, {"ㅆ", 4}, {"ㅉ", 6}, {"ㄳ", 3}, {"ㄵ", 3}, {"ㄶ", 4}, {"ㄺ", 4}, {"ㄻ", 6}, 
		{"ㄼ", 7}, {"ㄽ", 5}, {"ㄾ", 6}, {"ㄿ", 7}, {"ㅀ", 6}, {"ㅄ", 6}, {"ㅽ", 6}, {"ㅏ", 2}, 
		{"ㅓ", 2}, {"ㅗ", 2}, {"ㅜ", 2}, {"ㅡ", 1}, {"ㅣ", 1}, {"ㅑ", 3}, {"ㅕ", 3}, {"ㅛ", 3}, 
		{"ㅠ", 3}, {"ㅐ", 3}, {"ㅒ", 4}, {"ㅔ", 3}, {"ㅖ", 4}, {"ㅘ", 4}, {"ㅙ", 5}, {"ㅚ", 3}, 
		{"ㅝ", 4}, {"ㅞ", 5}, {"ㅟ", 3}, {"ㅢ", 2}, 
		{NULL, -1}
	};
	
	char ch = str[0];
	if(ch >= 'A' && ch <= 'Z')
		ch = tolower(ch);
	if(ch >= 'a' && ch <= 'z')
	{
		return 
		(ch >= 'a' && ch <= 'e')?1:
		(ch == 'f')?2:
		(ch == 'g' || ch == 'h')?1:
		(ch >= 'i' && ch <= 'k')?2:
		(ch >= 'l' && ch <= 's')?1:
		(ch == 't')?2:
		(ch == 'u' || ch == 'v')?1:
		(ch == 'x' || ch == 'y')?2:
		(ch == 'z')?1:
		0;
	}
#if 0
	if(ch >= '0' && ch <= '9')
	{
		
	}
#endif
	{
		std::string i, m, f;
		if(!hangul_split_utf8str(str, &i, &m, &f))
		{
			return get_char_stroke(i) + get_char_stroke(m) + get_char_stroke(f);
		}
	}
	
	for(int i = 0; stroke_list[i].str; i++)
	{
		if(stroke_list[i].str == str)
			return stroke_list[i].stroke;
	}
	
	return 0;
}

static int get_str_stroke(const std::string &str)
{
	int i, stroke = 0;
	std::string ch;
	
	for(i = 0; ; i++)
	{
		ch = utf8_strpos_s(str, i);
		if(ch.empty())
			break;
		stroke += get_char_stroke(ch);
	}
	
	return stroke;
}

// "가나다라", "마바사아" -> "가마나바다사라아"
static std::string merge_name(const std::string &s1, const std::string &s2)
{
	std::string result, s;
	int side = 0, i1 = 0, i2 = 0;
	
	for(; ; )
	{
		if(side == 0)
		{
			s = utf8_strpos_s(s1, i1++);
			if(s.empty())
				i1 = -1;
			else
				result.append(s);
			if(i1 < 0 && i2 < 0)
				break;
			if(i2 >= 0)
				side = 1;
		}
		else
		{
			s = utf8_strpos_s(s2, i2++);
			if(s.empty())
				i2 = -1;
			else
				result.append(s);
			if(i1 < 0 && i2 < 0)
				break;
			if(i1 >= 0)
				side = 0;
		}
	}
	
	return result;
}

static int merge_num_array(const std::vector<int> &na)
{
	std::vector<int> na2;
	std::vector<int>::const_iterator it;
	if(na.size() < 1 || na.size() > 30)
		return -1;
	int n = 0, multiplier = 0;
	if(na.size() <= 3)
		multiplier = ({ int r = 1; for(unsigned int i = 1; i < na.size(); i++) r *= 10; r; });
	else
		n = 101;
	
	for(it = na.begin(); it != na.end(); it++)
	{
		if(na.size() <= 3)
		{
			n += *it * multiplier;
			multiplier /= 10;
		}
		if(it + 1 != na.end())
			na2.push_back((*it + *(it + 1)) % 10);
	}
	if(n <= 100)
		return n;
	else
		return merge_num_array(na2);
}

static int get_gp(std::string s1, std::string s2)
{
	if(get_str_stroke(s1) < get_str_stroke(s2))
	{
		std::string tmp = s2;
		s2 = s1;
		s1 = tmp;
	}
	std::string mname = merge_name(s1, s2);
	std::vector<int> na;
	
	for(int i = 0; ; i++)
	{
		std::string ch = utf8_strpos_s(mname, i);
		if(ch.empty())
			break;
		na.push_back(get_char_stroke(ch));
	}
	
	return merge_num_array(na);
}
///// END

//////////////////////////////

// FIXME
static struct qwerty_to_2bul_table_t
{
	ucschar ch;
	char kbdch;
} qwerty_to_2bul_table[] = 
{
	{HANGUL_CONSONANT_G, 'r'}, // ㄱ
	{HANGUL_CONSONANT_GG, 'R'}, // ㄲ
	{HANGUL_CONSONANT_N, 's'}, // ㄴ
	{HANGUL_CONSONANT_D, 'e'}, // ㄷ
	{HANGUL_CONSONANT_DD, 'E'}, // ㄸ
	{HANGUL_CONSONANT_L, 'f'}, // ㄹ
	{HANGUL_CONSONANT_M, 'a'}, // ㅁ
	{HANGUL_CONSONANT_B, 'q'}, // ㅂ
	{HANGUL_CONSONANT_BB, 'Q'}, // ㅃ
	{HANGUL_CONSONANT_S, 't'}, // ㅅ
	{HANGUL_CONSONANT_SS, 'T'}, // ㅆ
	{HANGUL_CONSONANT_NG, 'd'}, // ㅇ
	{HANGUL_CONSONANT_J, 'w'}, // ㅈ
	{HANGUL_CONSONANT_JJ, 'W'}, // ㅉ
	{HANGUL_CONSONANT_CH, 'c'}, // ㅊ
	{HANGUL_CONSONANT_K, 'z'}, // ㅋ
	{HANGUL_CONSONANT_T, 'x'}, // ㅌ
	{HANGUL_CONSONANT_P, 'v'}, // ㅍ
	{HANGUL_CONSONANT_H, 'g'}, // ㅎ
	{HANGUL_VOWEL_A, 'k'}, // ㅏ
	{HANGUL_VOWEL_AE, 'o'}, // ㅐ
	{HANGUL_VOWEL_YA, 'i'}, // ㅑ
	{HANGUL_VOWEL_YAE, 'O'}, // ㅒ
	{HANGUL_VOWEL_EO, 'j'}, // ㅓ
	{HANGUL_VOWEL_E, 'p'}, // ㅔ
	{HANGUL_VOWEL_YEO, 'u'}, // ㅕ
	{HANGUL_VOWEL_YE, 'P'}, // ㅖ
	{HANGUL_VOWEL_O, 'h'}, // ㅗ
	{HANGUL_VOWEL_YO, 'y'}, // ㅛ
	{HANGUL_VOWEL_U, 'n'}, // ㅜ
	{HANGUL_VOWEL_YU, 'b'}, // ㅠ
	{HANGUL_VOWEL_EU, 'm'}, // ㅡ
	{HANGUL_VOWEL_I, 'l'}, // ㅣ
	
	// shift누른 상태로 입력한 경우
	{HANGUL_CONSONANT_N, 'S'}, // ㄴ
	{HANGUL_CONSONANT_L, 'F'}, // ㄹ
	{HANGUL_CONSONANT_M, 'A'}, // ㅁ
	{HANGUL_CONSONANT_NG, 'D'}, // ㅇ
	{HANGUL_CONSONANT_CH, 'C'}, // ㅊ
	{HANGUL_CONSONANT_K, 'Z'}, // ㅋ
	{HANGUL_CONSONANT_T, 'X'}, // ㅌ
	{HANGUL_CONSONANT_P, 'V'}, // ㅍ
	{HANGUL_CONSONANT_H, 'G'}, // ㅎ
	{HANGUL_VOWEL_A, 'K'}, // ㅏ
	{HANGUL_VOWEL_YA, 'I'}, // ㅑ
	{HANGUL_VOWEL_EO, 'J'}, // ㅓ
	{HANGUL_VOWEL_YEO, 'U'}, // ㅕ
	{HANGUL_VOWEL_O, 'H'}, // ㅗ
	{HANGUL_VOWEL_YO, 'Y'}, // ㅛ
	{HANGUL_VOWEL_U, 'N'}, // ㅜ
	{HANGUL_VOWEL_YU, 'B'}, // ㅠ
	{HANGUL_VOWEL_EU, 'M'}, // ㅡ
	{HANGUL_VOWEL_I, 'L'}, // ㅣ
	
	{HANGUL_CODE_END, 0}
};

static ucschar qwerty_to_2bul_onechar(const char *p)
{
	for(qwerty_to_2bul_table_t *t = qwerty_to_2bul_table; t->ch; t++)
	{
		if(t->kbdch == *p)
		{
			return t->ch;
		}
	}
	return 0;
}

static std::string qwerty_to_2bul(const std::string &str)
{
	std::string result;
	const char *p = str.c_str();
	ucschar cho = 0, jung = 0, jong = 0;
	ucschar cho_splitted[2] = {0, 0};
	ucschar jong_splitted[2] = {0, 0};
	
	// 도중에 자모가 아닌 글자가 있을 경우 현재 버퍼로 글자를 완성함
#define COMMIT_HANGUL_BUFFER \
		({ \
			if(cho && jung) \
			{ \
				if(jong != 0 && !hangul_is_jongseong_jamo(jong)) \
				{ \
					result += utf8_encode_s(hangul_join(cho, jung, 0)); \
					result += utf8_encode_s(jong); \
				} \
				else \
					result += utf8_encode_s(hangul_join(cho, jung, jong)); \
				jong = 0; \
			} \
			else if(cho) \
				result += utf8_encode_s(cho); \
			else if(jung) \
				result += utf8_encode_s(jung); \
			/* assert(jong == 0); */ \
			if(jong != 0) \
			{ \
				puts("DEBUG @@@@@@@@@@@@@@@@@@@@@@11111111111"); \
			} \
			cho = jung = jong = 0; \
		})
	for(; *p; p++)
	{
		ucschar ch = qwerty_to_2bul_onechar(p);
		if(ch == 0)
		{
			COMMIT_HANGUL_BUFFER;
			result.append(p, 1);
			continue;
		}
		if(hangul_is_consonant(ch))
		{
			if(cho == 0)
			{
				// 초성 입력
				cho = ch;
			}
			else if(jung == 0) // && cho != 0
			{
				// 초성 조합
				ucschar ch2 = hangul_merge_allconsonant(cho, ch, 0);
				if(ch2 != 0) // 자음 조합 성공
				{
					if(jung == 0)
					{
						// 초성에 사용 불가능한 자음일 경우를 위해 저장
						cho_splitted[0] = cho;
						cho_splitted[1] = ch;
						cho = ch2;
					}
					else
					{
						if(hangul_is_choseong_jamo(ch2))
							cho = ch2; // 초성에 사용 가능함
						else // 초성에 사용할 수 없는 자음. ch2는 버림
						{
							result += utf8_encode_s(cho); // 전에 입력된 자음은 그냥 출력
							cho = ch; // 현재 자음은 다음 글자의 초성에 사용함
							// jung = jong = 0; // 이미 0
						}
					}
				}
				else // 자음 조합 실패
				{
					result += utf8_encode_s(cho);
					cho = ch;
				}
			}
			else if(jong == 0) // && cho != 0 && jung != 0
			{
				// 종성 입력
				jong = ch;
			}
			else // && cho != 0 && jung != 0 && jong != 0
			{
				// 종성 조합
				ucschar ch2 = hangul_merge_consonant(jong, ch);
				if(ch2 != 0) // 자음 조합 성공
				{
					if(hangul_is_jongseong_jamo(ch2))
					{
						// 종성에 사용 가능한 자음
						// 바로 다음에 모음이 나올 경우를 위해 합치기 전의 자음을 저장
						// 예) 삯+ㅣ -> 삭시
						jong_splitted[0] = jong;
						jong_splitted[1] = ch;
						jong = ch2;
					}
					else
					{
						// 조합에 성공했지만 종성에 사용 불가능. ch2는 버리고, 조합 전의 종성으로 글자 완성 후
						// 새로운 자음은 다음 글자의 초성에 사용
						{
							if(jong != 0 && !hangul_is_jongseong_jamo(jong)) // ㅃㅉㄸ
							{
								result += utf8_encode_s(hangul_join(cho, jung, 0)); // 한 글자를 완성하고
								result += utf8_encode_s(jong); // 종성은 따로 출력
							}
							else
								result += utf8_encode_s(hangul_join(cho, jung, jong)); // 한 글자를 완성하고
						}
						cho = ch; // 다음 글자의 초성
						jung = jong = 0;
					}
				}
				else // 종성 조합 실패
				{
					// 조합 불가능한 자음이 연속으로 나온 경우
					// 글자를 완성하고 새로운 자음은 다음 글자의 초성에 사용
					{
						if(jong != 0 && !hangul_is_jongseong_jamo(jong)) // ㅃㅉㄸ
						{
							result += utf8_encode_s(hangul_join(cho, jung, 0)); // 한 글자를 완성하고
							result += utf8_encode_s(jong); // 종성은 따로 출력
						}
						else
							result += utf8_encode_s(hangul_join(cho, jung, jong)); // 한 글자를 완성하고
					}
					cho = ch; // 다음 글자의 초성
					jung = jong = 0;
				}
			}
		} /* if(hangul_is_consonant(ch)) */
		else if(hangul_is_vowel(ch))
		{
			if(cho != 0 && !hangul_is_choseong_jamo(cho))
			{
				result += utf8_encode_s(cho_splitted[0]); // 전에 입력된 자음은 그냥 출력
				cho = cho_splitted[1]; // 현재 자음은 다음 글자의 초성에 사용함
			}
			if(jung == 0)
			{
				// 중성 입력
				jung = ch;
			}
			else
			{
				// 중성 조합
				if(jong == 0) // 아직 종성이 입력되지 않았을 경우
				{
					ucschar ch2;
					if(cho)
						ch2 = hangul_merge_vowel(jung, ch);
					else
						ch2 = hangul_merge_allvowel(jung, ch); // 옛 모음 포함
					if(ch2 != 0) // 모음 조합 성공
					{
						// 오+ㅏ -> 와
						jung = ch2;
					}
					else // 모음 조합 실패
					{
						// 우+ㅏ -> 우ㅏ
						if(cho)
						{
							// 초성이 있다면 초성과 중성으로 한 글자 완성
							result += utf8_encode_s(hangul_join(cho, jung, 0));
							cho = 0;
						}
						else
						{
							// 초성이 없다면 모음 출력
							result += utf8_encode_s(jung);
						}
						// 그리고 다음 글자의 중성을 미리 입력.
						jung = ch;
					}
				}
				else // 종성이 이미 입력된 경우: 글자를 완성하고 새로운 모음은 중성 버퍼에 넣음.
				{
					// 가+ㅏ -> 가ㅏ
					//assert(cho != 0);
					if(cho == 0)
						puts("DEBUG @@@@@@@@@@@@@@@@@@@@222222222");
					// 초성, 중성, 종성이 있는 상태에서 모음 입력시, 
					// 초성, 중성으로 한 글자 완성 후 종성은 다음 글자의 초성으로
					// 간+ㅏ -> 가나
					ucschar cho_b = cho, jung_b = jung, jong_b = 0;
					if(hangul_is_choseong_jamo(jong)) // jong: 다음 글자에서 초성으로 사용됨
					{
						// 현재 종성이 다음 글자의 초성에 사용 가능할 경우
						// 이번 글자는 초성+중성으로 완성시킴
						jong_b = 0;
						cho = jong;
					}
					else
					{
						// 현재 종성이 다음 글자의 초성에 사용 불가능할 경우
						// 이번 글자는 초성+중성+종성1으로 완성시키고
						// 다음 글자의 초성에 종성2를 넣어둠
						jong_b = jong_splitted[0];
						cho = jong_splitted[1];
					}
					jung = ch;
					jong = 0;
					
					{
						if(jong != 0 && !hangul_is_jongseong_jamo(jong_b)) // ㅃㅉㄸ
						{
							result += utf8_encode_s(hangul_join(cho_b, jung_b, 0)); // 한 글자를 완성하고
							result += utf8_encode_s(jong_b); // 종성은 따로 출력
						}
						else
							result += utf8_encode_s(hangul_join(cho_b, jung_b, jong_b)); // 한 글자를 완성하고
					}
				}
			}
		} /* else if(hangul_is_vowel(ch)) */
		else
		{
			// cannot happen
			COMMIT_HANGUL_BUFFER;
			result.append(p, 1);
			assert(!"cannot happen");
			puts("DEBUG @@@@@@@@@@@@@@@@@@@@333333333");
		}
	}
	COMMIT_HANGUL_BUFFER;
#undef COMMIT_HANGUL_BUFFER
	
	return result;
}


//////////////////////////////



static int cmd_cb_singlearg_src(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	std::string carg2, enc;
	std::string buf;
	
	static pcrecpp::RE re_split_encoding("^-e ([^ ]+) (.*)");
	if(re_split_encoding.FullMatch(carg, &enc, &carg2))
	{
		encconv cv;
		int ret = cv.open("utf-8", enc);
		if(ret < 0)
		{
			isrv->privmsg_nh(mdest, "지원하지 않는 인코딩입니다.");
			return 0;
		}
		carg2 = cv.convert(carg2);
		cv.close();
	}
	else
	{
		carg2 = carg;
		enc = "utf-8";
	}
	
	if(carg2.empty())
	{
		isrv->privmsg_nh(mdest, "사용법: !" + cmd + " <내용> | !" + cmd + " -e <인코딩> <내용>");
	}
	else if(cmd == "b64e" || cmd == "base64")
	{
		buf = base64_encode(carg2);
		if(buf.empty())
			buf = "실패";
		isrv->privmsg_nh(mdest, buf);
	}
	else if(cmd == "md5")
	{
		isrv->privmsg(mdest, md5_hex(carg2));
	}
	else if(cmd == "urlenc" || cmd == "urlencode")
	{
		isrv->privmsg(mdest, urlencode(carg2));
	}
	else if(cmd == "urlencp")
	{
		buf = urlencode(carg2); // encoded in lowercase
		boost::algorithm::replace_all(buf, "%3a", ":");
		boost::algorithm::replace_all(buf, "%2f", "/");
		boost::algorithm::replace_all(buf, "%3f", "?");
		boost::algorithm::replace_all(buf, "%3d", "=");
		boost::algorithm::replace_all(buf, "%26", "&");
		isrv->privmsg(mdest, buf);
	}
	
	return 0;
}

static int cmd_cb_singlearg_dest(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	std::string carg2, enc;
	std::string buf;
	
	static pcrecpp::RE re_split_encoding("^-e ([^ ]+) (.*)");
	if(!re_split_encoding.FullMatch(carg, &enc, &carg2))
	{
		carg2 = carg;
		enc = "utf-8";
	}
	encconv cv;
	int ret = cv.open(enc, "utf-8");
	if(ret < 0)
	{
		isrv->privmsg_nh(mdest, "지원하지 않는 인코딩입니다.");
		return 0;
	}
	
	if(carg2.empty())
	{
		isrv->privmsg_nh(mdest, "사용법: !" + cmd + " <내용> | !" + cmd + " -e <인코딩> <내용>");
	}
	else if(cmd == "b64d")
	{
		buf = base64_decode(carg2);
		if(buf.empty())
			buf = "실패";
		isrv->privmsg_nh(mdest, cv.convert(buf));
	}
	else if(cmd == "urldec" || cmd == "urldecode")
	{
		isrv->privmsg_nh(mdest, cv.convert(urldecode(carg2)));
	}
	else if(cmd == "htmlentdec")
	{
		isrv->privmsg_nh(mdest, cv.convert(decode_html_entities(carg2)));
	}
	
	return 0;
}

static int cmd_cb_regexp(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	std::string pattern, text, result;
	std::vector<std::string> groups;
	
	if(!pcrecpp::RE("/((?:[^\\\\/]++|\\\\.)*+)/ (.*)").FullMatch(carg, &pattern, &text))
	{
		isrv->privmsg_nh(mdest, "사용법: !" + cmd + " /정규표현식 패턴/ 내용");
		return HANDLER_FINISHED;
	}
	
	if(pcrecpp::RE(pattern, pcrecpp::UTF8()).PartialMatchAll(text, &groups))
	{
		int re_ncgroups = groups.size();
		result = "성공";
		if(re_ncgroups > 0)
			result += ": ";
		for(int i = 0; i < re_ncgroups; i++)
		{
			result += groups[i];
			if(i != re_ncgroups - 1)
			{
				result += ", ";
			}
		}
	}
	else
	{
		result = "실패";
	}
	isrv->privmsg_nh(mdest, result);
	
	return HANDLER_FINISHED;
}


static int cmd_cb_singlearg2(ircbot_conn *isrv,  irc_privmsg &pmsg, 
	irc_msginfo &mdest, std::string &cmd, std::string &carg, void *)
{
	std::string tmp, tmp2;
	
	if(cmd == "시간")
	{
		isrv->privmsg(mdest, gettimestr());
	}
	else if(cmd == "타로" || cmd == "타롯" || cmd == "tarot")
	{
		if(carg.empty())
		{
			isrv->privmsg_nh(mdest, "질문을 입력하세요.");
			return 0;
		}
		int idx = md5rand(pmsg, pmsg.getnick() + carg);
		idx = idx % (sizeof(tarot_data) / sizeof(tarot_data_s));
		tarot_data_s *tdata = &tarot_data[idx];
		
		isrv->privmsgf_nh(mdest, " %s%J{이}라는 질문에 대해서, %s(%s) 카드의 %s이 나왔습니다. (%s)", 
			carg.c_str(), tdata->name, tdata->name_eng, 
			tdata->direction, tdata->description);
	}
	else if(cmd == "퍼센트" || cmd == "percent" || 
		cmd == "퍼센트2")
	{
		if(carg.empty())
		{
			isrv->privmsg_nh(mdest, "질문을 입력하세요.");
			return 0;
		}
		int percent = md5rand(pmsg, carg) % 101;
		
#if 0
		// 예외: 검고에 대한 특별 케이스
		if(strstr(carg.c_str(), "검고") && strstr(carg.c_str(), "게이"))
		{
			if(is_negative_msg(carg))
				percent = 0;
			else
				percent = 100;
		}
#endif
		
		if(cmd == "퍼센트2")
		{
			isrv->privmsgf_nh(mdest, " %s%J{은는} %d%%입니다.", 
				carg.c_str(), percent);
		}
		else
		{
			int i, pc = percent / 2;
			std::string buf;
			char pcstr[16];
			int pi = 0, pcslen = snprintf(pcstr, sizeof(pcstr), "%d%%", percent);
			buf = IRCTEXT_COLOR_FGBG(IRCTEXT_COLOR_SILVER, IRCTEXT_COLOR_NAVY);
			for(i = 0; i < pc; i++)
			{
				if(i >= 25 - (pcslen / 2) && i <= 25 + (pcslen / 2) && pi < pcslen)
					buf += pcstr[pi++];
				else
					buf += " ";
			}
			buf += IRCTEXT_COLOR_FGBG(IRCTEXT_COLOR_NAVY, IRCTEXT_COLOR_SILVER);
			for(; i < 50; i++)
			{
				if(i >= 25 - (pcslen / 2) && i <= 25 + (pcslen / 2) && pi < pcslen)
					buf += pcstr[pi++];
				else
					buf += " ";
			}
			buf += IRCTEXT_NORMAL;
			
			isrv->privmsgf_nh(mdest, " %s%J{은는} %s 입니다.", 
				carg.c_str(), buf.c_str());
		}
	}
	else if(cmd == "골라" || cmd == "선택")
	{
		std::vector<std::string> choices;
		boost::algorithm::split(choices, carg, boost::algorithm::is_any_of(" "));
		if(choices.size() <= 0)
			isrv->privmsg(mdest, "사용법: !" + cmd + " 1 2 3 4 ...");
		else
		{
			int n = rand() % choices.size();
			isrv->privmsg_nh(mdest, " " + choices[n]);
		}
	}
	else if(cmd == "프갤포탈" || cmd == "프겔포탈")
	{
		isrv->privmsg(mdest, "http://gall.dcinside.com/list.php?id=programming");
	}
	else if(cmd == "BMI" || cmd == "bmi" || cmd == "비만계산")
	{
		double height, weight;
		if(!pcrecpp::RE("([0-9.]+) +([0-9.]+)").FullMatch(carg, &height, &weight))
		{
			isrv->privmsg(mdest, "사용법: !BMI 키 몸무게 (단위: cm, kg) 예) !BMI 200 100");
			return 0;
		}
		if(height > 500.0f || height < 1.0f || weight > 1000.0f || weight < 1.0f)
		{
			isrv->privmsg(mdest, "장난하냐능 >_<");
			return 0;
		}
		height /= 100; // cm -> m
		double bmi = weight / (height * height);
		
		const char *bmi_desc = "";
		if(bmi < 16.5f)
			bmi_desc = "심각한 저체중";
		else if(bmi >= 16.5f && bmi < 18.5f)
			bmi_desc = "저체중";
		else if(bmi >= 18.5f && bmi < 25.0f)
			bmi_desc = "정상";
		else if(bmi >= 25.0f && bmi < 30.0f)
			bmi_desc = "과체중";
		else if(bmi >= 30.0f && bmi < 35.0f)
			bmi_desc = "비만 클래스 I";
		else if(bmi >= 35.0f && bmi < 40.0f)
			bmi_desc = "고도비만 클래스 II";
		else if(bmi >= 40.0f && bmi < 45.0f)
			bmi_desc = "초고도비만";
		else if(bmi >= 45.0f && bmi < 50.0f)
			bmi_desc = "병적으로 비만";
		else if(bmi >= 50.0f && bmi < 60.0f)
			bmi_desc = "超超고도비만 - 답이 ㅇ벗다";
		else if(bmi >= 60.0f)
			bmi_desc = "超超超超超高度비만 - 자살춪현";
		else
			bmi_desc = "내부 오류";
		
		isrv->privmsgf_nh(mdest, "BMI: %.02f (%s)", bmi, bmi_desc);
	}
	else if(cmd == "두음법칙")
	{
		if(carg.empty())
			isrv->privmsg(mdest, "!두음법칙 <단어>");
		else
			isrv->privmsg(mdest, " " + apply_head_pron_rule(carg));
	}
	else if(cmd == "궁합")
	{
		std::string s1, s2;
		if(!pcrecpp::RE(" *([^ ]+) +([^ ]+) *").FullMatch(carg, &s1, &s2))
		{
			isrv->privmsg(mdest, "사용법: !궁합 이름1 이름2");
			return 0;
		}
		int p = get_gp(s1, s2);
		if(p < 0)
			isrv->privmsg(mdest, "이름이 너무 깁니다.");
		else
			isrv->privmsgf(mdest, "%d%%\n", p);
	}
	else if(cmd == "c2")
	{
		if(carg.empty())
			isrv->privmsg(mdest, "!c2 <answkd>");
		else
			isrv->privmsg(mdest, qwerty_to_2bul(carg));
	}
	else if(cmd == "말해")
	{
		isrv->privmsg_nh(mdest, carg);
	}
	else
	{
		return 0;
	}
	
end:
	return 0;
}



static const char *const cmdlist_singlearg_src[] = 
{
	"b64e", "base64", "md5", "urlenc", "urlencode", "urlencp", 
	NULL
};
static const char *const cmdlist_singlearg_dest[] = 
{
	"b64d", "urldec", "urldecode", "htmlentdec", 
	NULL
};
static const char *const cmdlist_singlearg2[] = 
{
	"시간", "타로", "타롯", "tarot", "퍼센트", "퍼센트2", "percent", "골라", "선택", 
	"프갤포탈", "프겔포탈", "bmi", "비만계산", "두음법칙", "궁합", "말해", "c2", 
	NULL
};

int mod_strutils_init(ircbot *bot)
{
	int i;
	
	for(i = 0; cmdlist_singlearg_src[i]; i++)
		bot->register_cmd(cmdlist_singlearg_src[i], cmd_cb_singlearg_src);
	for(i = 0; cmdlist_singlearg_dest[i]; i++)
		bot->register_cmd(cmdlist_singlearg_dest[i], cmd_cb_singlearg_dest);
	for(i = 0; cmdlist_singlearg2[i]; i++)
		bot->register_cmd(cmdlist_singlearg2[i], cmd_cb_singlearg2);
	bot->register_cmd("정규식", cmd_cb_regexp);
	bot->register_cmd("정규표현식", cmd_cb_regexp);
	bot->register_cmd("regex", cmd_cb_regexp);
	bot->register_cmd("regexp", cmd_cb_regexp);
	
	return 0;
}

int mod_strutils_cleanup(ircbot *bot)
{
	int i;
	
	for(i = 0; cmdlist_singlearg_src[i]; i++)
		bot->unregister_cmd(cmdlist_singlearg_src[i]);
	for(i = 0; cmdlist_singlearg_dest[i]; i++)
		bot->unregister_cmd(cmdlist_singlearg_dest[i]);
	for(i = 0; cmdlist_singlearg2[i]; i++)
		bot->unregister_cmd(cmdlist_singlearg2[i]);
	bot->unregister_cmd("정규식");
	bot->unregister_cmd("정규표현식");
	bot->unregister_cmd("regex");
	bot->unregister_cmd("regexp");
	
	return 0;
}




MODULE_INFO(mod_strutils, mod_strutils_init, mod_strutils_cleanup)




