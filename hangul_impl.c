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


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stdbool.h>

#include "utf8_impl.h"
#include "hangul_impl.h"



static const char jamo_table[51][3] = {
	{1, 0, 1},
	{2, 0, 2},
	{0, 0, 3},
	{3, 0, 4},
	{0, 0, 5},
	{0, 0, 6},
	{4, 0, 7},
	{5, 0, 0},
	{6, 0, 8},
	{0, 0, 9},
	{0, 0, 10},
	{0, 0, 11},
	{0, 0, 12},
	{0, 0, 13},
	{0, 0, 14},
	{0, 0, 15},
	{7, 0, 16},
	{8, 0, 17},
	{9, 0, 0},
	{0, 0, 18},
	{10, 0, 19},
	{11, 0, 20},
	{12, 0, 21},
	{13, 0, 22},
	{14, 0, 0},
	{15, 0, 23},
	{16, 0, 24},
	{17, 0, 25},
	{18, 0, 26},
	{19, 0, 27},
	{0, 1, 0},
	{0, 2, 0},
	{0, 3, 0},
	{0, 4, 0},
	{0, 5, 0},
	{0, 6, 0},
	{0, 7, 0},
	{0, 8, 0},
	{0, 9, 0},
	{0, 10, 0},
	{0, 11, 0},
	{0, 12, 0},
	{0, 13, 0},
	{0, 14, 0},
	{0, 15, 0},
	{0, 16, 0},
	{0, 17, 0},
	{0, 18, 0},
	{0, 19, 0},
	{0, 20, 0},
	{0, 21, 0},
};



unsigned int hangul_join(unsigned int lead, unsigned int vowel, unsigned int tail)
{
	unsigned int ch = 0;
	
	/* 0x3131: ㄱ */
	/* 0x3163: ㅣ */
	
	if(lead < 0x3131 || lead > 0x3163)
		return 0;
	lead = jamo_table[lead-0x3131][0];
	
	if(vowel < 0x3131 || vowel > 0x3163)
		return 0;
	vowel = jamo_table[vowel-0x3131][1];
	
	if(tail)
	{
		if(tail < 0x3131 || tail > 0x3163)
			return 0;
		tail = jamo_table[tail-0x3131][2];
		if(!tail)
			return 0;
	}
	
	if(lead && vowel)
	{
		ch = tail + (vowel - 1)*28 + (lead - 1)*588 + 44032;
	}
	
	return ch;
}

unsigned int hangul_join_utf8char(const char *lead_utf8char, 
	const char *vowel_utf8char, const char *tail_utf8char)
{
	unsigned int lead = 0, vowel = 0, tail = 0;
	
	if(lead_utf8char)
		lead = utf8_decode(lead_utf8char, NULL);
	if(vowel_utf8char)
		vowel = utf8_decode(vowel_utf8char, NULL);
	if(tail_utf8char && tail_utf8char[0])
		tail = utf8_decode(tail_utf8char, NULL);
	
	return hangul_join(lead, vowel, tail);
}

int hangul_split(unsigned int chr, unsigned int *lead, unsigned int *vowel, 
	unsigned int *tail)
{
	// ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ
	static const unsigned int initials[] = 
	{
		0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 
		0x3142, 0x3143, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 
		0x314a, 0x314b, 0x314c, 0x314d, 0x314e
	};
	// ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ
	static const unsigned int medials[] = 
	{
		0x314f, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155, 
		0x3156, 0x3157, 0x3158, 0x3159, 0x315a, 0x315b, 0x315c, 
		0x315d, 0x315e, 0x315f, 0x3160, 0x3161, 0x3162, 0x3163
	};
	// ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ
	static const unsigned int finals[] = 
	{
		0x0000, 0x3131, 0x3132, 0x3133, 0x3134, 0x3135, 0x3136, 
		0x3137, 0x3139, 0x313a, 0x313b, 0x313c, 0x313d, 0x313e, 
		0x313f, 0x3140, 0x3141, 0x3142, 0x3144, 0x3145, 0x3146, 
		0x3147, 0x3148, 0x314a, 0x314b, 0x314c, 0x314d, 0x314e
	};
	unsigned int a, b, c;
	
	// "AC00:가" ~ "D7A3:힣" 에 속한 글자면 분해.
	if(chr >= 0xAC00 && chr <= 0xD7A3)
	{
		c = chr - 0xAC00;
		a = c / (21 * 28);
		c = c % (21 * 28);
		b = c / 28;
		c = c % 28;
		
		if(lead)
			*lead = initials[a];
		if(vowel)
			*vowel = medials[b];
		if(tail)
			*tail = finals[c];
		
		return 0;
	}
	
	return -1;
}

int hangul_split_utf8char(const char *utf8char, 
	char *lead_utf8char, char *vowel_utf8char, 
	char *tail_utf8char)
{
	unsigned int lead = 0, vowel = 0, tail = 0, ch;
	int ret;
	
	ch = utf8_decode(utf8char, NULL);
	ret = hangul_split(ch, &lead, &vowel, &tail);
	
	if(ret == 0)
	{
		if(lead_utf8char)
			utf8_encode_nt(lead, lead_utf8char, 4);
		if(vowel_utf8char)
			utf8_encode_nt(vowel, vowel_utf8char, 4);
		if(tail_utf8char)
		{
			if(tail)
				utf8_encode_nt(tail, tail_utf8char, 4);
			else
				tail_utf8char[0] = 0;
		}
		
		return 0;
	}
	
	return -1;
}


unsigned int hangul_merge_consonant(unsigned int cons1, unsigned int cons2)
{
	static const struct hangul_consonant_merge_table_t
	{
		ucschar c1, c2, c;
	} hangul_consonant_merge_table[] = 
	{
		{HANGUL_CONSONANT_G, HANGUL_CONSONANT_G, HANGUL_CONSONANT_GG}, /* ㄲ */
		{HANGUL_CONSONANT_G, HANGUL_CONSONANT_S, HANGUL_CONSONANT_GS}, /* ㄳ */
		{HANGUL_CONSONANT_N, HANGUL_CONSONANT_J, HANGUL_CONSONANT_NJ}, /* ㄵ */
		{HANGUL_CONSONANT_N, HANGUL_CONSONANT_H, HANGUL_CONSONANT_NH}, /* ㄶ */
		{HANGUL_CONSONANT_D, HANGUL_CONSONANT_D, HANGUL_CONSONANT_DD}, /* ㄸ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_G, HANGUL_CONSONANT_LG}, /* ㄺ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_M, HANGUL_CONSONANT_LM}, /* ㄻ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_B, HANGUL_CONSONANT_LB}, /* ㄼ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_S, HANGUL_CONSONANT_LS}, /* ㄽ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_T, HANGUL_CONSONANT_LT}, /* ㄾ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_P, HANGUL_CONSONANT_LP}, /* ㄿ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_H, HANGUL_CONSONANT_LH}, /* ㅀ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_B, HANGUL_CONSONANT_BB}, /* ㅃ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_S, HANGUL_CONSONANT_BS}, /* ㅄ */
		{HANGUL_CONSONANT_S, HANGUL_CONSONANT_S, HANGUL_CONSONANT_SS}, /* ㅆ */
		{HANGUL_CONSONANT_J, HANGUL_CONSONANT_J, HANGUL_CONSONANT_JJ}, /* ㅉ */
		{0, 0, 0}
	};
	
	const struct hangul_consonant_merge_table_t *p;
	for(p = hangul_consonant_merge_table; p->c; p++)
	{
		if(p->c1 == cons1 && p->c2 == cons2)
			return p->c;
	}
	
	return 0;
}

unsigned int hangul_merge_vowel(unsigned int vowel1, unsigned int vowel2)
{
	static const struct hangul_vowel_merge_table_t
	{
		ucschar v1, v2, v;
	} hangul_vowel_merge_table[] = 
	{
		{HANGUL_VOWEL_O, HANGUL_VOWEL_A, HANGUL_VOWEL_WA}, /* ㅘ */
		{HANGUL_VOWEL_O, HANGUL_VOWEL_AE, HANGUL_VOWEL_WAE}, /* ㅙ */
		{HANGUL_VOWEL_O, HANGUL_VOWEL_I, HANGUL_VOWEL_OE}, /* ㅚ */
		{HANGUL_VOWEL_U, HANGUL_VOWEL_EO, HANGUL_VOWEL_WO}, /* ㅝ */
		{HANGUL_VOWEL_U, HANGUL_VOWEL_E, HANGUL_VOWEL_WE}, /* ㅞ */
		{HANGUL_VOWEL_U, HANGUL_VOWEL_I, HANGUL_VOWEL_WI}, /* ㅟ */
		{HANGUL_VOWEL_EU, HANGUL_VOWEL_I, HANGUL_VOWEL_UI}, /* ㅢ */
		{0, 0, 0}
	};
	
	const struct hangul_vowel_merge_table_t *p;
	for(p = hangul_vowel_merge_table; p->v; p++)
	{
		if(p->v1 == vowel1 && p->v2 == vowel2)
			return p->v;
	}
	
	return 0;
}

unsigned int hangul_merge_allconsonant(unsigned int cons1, unsigned int cons2, unsigned int cons3)
{
	ucschar result = 0;
	if(cons3 == 0)
	{
		result = hangul_merge_consonant(cons1, cons2);
		if(result)
			return result;
	}
	
	static const struct hangul_oldconsonant_merge_table_t
	{
		ucschar c1, c2, c3, c;
	} hangul_oldconsonant_merge_table[] = 
	{
		{HANGUL_CONSONANT_N, HANGUL_CONSONANT_N, 0, HANGUL_OLDCONSONANT_NN}, /* ㅥ */
		{HANGUL_CONSONANT_N, HANGUL_CONSONANT_D, 0, HANGUL_OLDCONSONANT_ND}, /* ㅦ */
		{HANGUL_CONSONANT_N, HANGUL_CONSONANT_S, 0, HANGUL_OLDCONSONANT_NS}, /* ㅧ */
		{HANGUL_CONSONANT_N, HANGUL_OLDCONSONANT_Z, 0, HANGUL_OLDCONSONANT_NZ}, /* ㅨ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_G, HANGUL_CONSONANT_S, HANGUL_OLDCONSONANT_LGS}, /* ㅩ */
		{HANGUL_CONSONANT_LG, HANGUL_CONSONANT_S, 0, HANGUL_OLDCONSONANT_LGS}, /* ㅩ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_GS, 0, HANGUL_OLDCONSONANT_LGS}, /* ㅩ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_D, 0, HANGUL_OLDCONSONANT_LD}, /* ㅪ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_B, HANGUL_CONSONANT_S, HANGUL_OLDCONSONANT_LBS}, /* ㅫ */
		{HANGUL_CONSONANT_LB, HANGUL_CONSONANT_S, 0, HANGUL_OLDCONSONANT_LBS}, /* ㅫ */
		{HANGUL_CONSONANT_L, HANGUL_CONSONANT_BS, 0, HANGUL_OLDCONSONANT_LBS}, /* ㅫ */
		{HANGUL_CONSONANT_L, HANGUL_OLDCONSONANT_Z, 0, HANGUL_OLDCONSONANT_LZ}, /* ㅬ */
		{HANGUL_CONSONANT_L, HANGUL_OLDCONSONANT_NG2, 0, HANGUL_OLDCONSONANT_LNG2}, /* ㅭ */
		{HANGUL_CONSONANT_M, HANGUL_CONSONANT_B, 0, HANGUL_OLDCONSONANT_MB}, /* ㅮ */
		{HANGUL_CONSONANT_M, HANGUL_CONSONANT_S, 0, HANGUL_OLDCONSONANT_MS}, /* ㅯ */
		{HANGUL_CONSONANT_M, HANGUL_OLDCONSONANT_Z, 0, HANGUL_OLDCONSONANT_MZ}, /* ㅰ */
		{HANGUL_CONSONANT_M, HANGUL_CONSONANT_NG, 0, HANGUL_OLDCONSONANT_MNG}, /* ㅱ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_G, 0, HANGUL_OLDCONSONANT_BG}, /* ㅲ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_D, 0, HANGUL_OLDCONSONANT_BD}, /* ㅳ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_S, HANGUL_CONSONANT_G, HANGUL_OLDCONSONANT_BSG}, /* ㅴ */
		{HANGUL_CONSONANT_BS, HANGUL_CONSONANT_G, 0, HANGUL_OLDCONSONANT_BSG}, /* ㅴ */
		{HANGUL_CONSONANT_B, HANGUL_OLDCONSONANT_SG, 0, HANGUL_OLDCONSONANT_BSG}, /* ㅴ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_S, HANGUL_CONSONANT_D, HANGUL_OLDCONSONANT_BSD}, /* ㅵ */
		{HANGUL_CONSONANT_BS, HANGUL_CONSONANT_D, 0, HANGUL_OLDCONSONANT_BSD}, /* ㅵ */
		{HANGUL_CONSONANT_B, HANGUL_OLDCONSONANT_SD, 0, HANGUL_OLDCONSONANT_BSD}, /* ㅵ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_J, 0, HANGUL_OLDCONSONANT_BJ}, /* ㅶ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_T, 0, HANGUL_OLDCONSONANT_BT}, /* ㅷ */
		{HANGUL_CONSONANT_B, HANGUL_CONSONANT_NG, 0, HANGUL_OLDCONSONANT_BNG}, /* ㅸ */
		{HANGUL_CONSONANT_BB, HANGUL_CONSONANT_NG, 0, HANGUL_OLDCONSONANT_BBNG}, /* ㅹ */
		{HANGUL_CONSONANT_S, HANGUL_CONSONANT_G, 0, HANGUL_OLDCONSONANT_SG}, /* ㅺ */
		{HANGUL_CONSONANT_S, HANGUL_CONSONANT_N, 0, HANGUL_OLDCONSONANT_SN}, /* ㅻ */
		{HANGUL_CONSONANT_S, HANGUL_CONSONANT_D, 0, HANGUL_OLDCONSONANT_SD}, /* ㅼ */
		{HANGUL_CONSONANT_S, HANGUL_CONSONANT_B, 0, HANGUL_OLDCONSONANT_SB}, /* ㅽ */
		{HANGUL_CONSONANT_S, HANGUL_CONSONANT_J, 0, HANGUL_OLDCONSONANT_SJ}, /* ㅾ */
		{HANGUL_CONSONANT_NG, HANGUL_CONSONANT_NG, 0, HANGUL_OLDCONSONANT_NGNG}, /* ㆀ */
		{HANGUL_OLDCONSONANT_NNG, HANGUL_CONSONANT_S, 0, HANGUL_OLDCONSONANT_NNGS}, /* ㆂ */
		{HANGUL_OLDCONSONANT_NNG, HANGUL_OLDCONSONANT_Z, 0, HANGUL_OLDCONSONANT_NNGZ}, /* ㆃ */
		{HANGUL_CONSONANT_P, HANGUL_CONSONANT_NG, 0, HANGUL_OLDCONSONANT_PNG}, /* ㆄ */
		{HANGUL_CONSONANT_H, HANGUL_CONSONANT_H, 0, HANGUL_OLDCONSONANT_HH}, /* ㆅ */
		{0, 0, 0, 0}
	};
	
	const struct hangul_oldconsonant_merge_table_t *p;
	for(p = hangul_oldconsonant_merge_table; p->c; p++)
	{
		if(p->c1 == cons1 && p->c2 == cons2 && p->c3 == cons3)
			return p->c;
	}
	
	return 0;
}

unsigned int hangul_merge_allvowel(unsigned int vowel1, unsigned int vowel2)
{
	ucschar result = hangul_merge_vowel(vowel1, vowel2);
	if(result)
		return result;
	
	static const struct hangul_vowel_merge_table_t
	{
		ucschar v1, v2, v;
	} hangul_vowel_merge_table[] = 
	{
		{HANGUL_VOWEL_YO, HANGUL_VOWEL_YA, HANGUL_OLDVOWEL_YOYA}, /* ㆇ */
		{HANGUL_VOWEL_YO, HANGUL_VOWEL_YAE, HANGUL_OLDVOWEL_YOYAE}, /* ㆈ */
		{HANGUL_VOWEL_YO, HANGUL_VOWEL_I, HANGUL_OLDVOWEL_YOI}, /* ㆉ */
		{HANGUL_VOWEL_YU, HANGUL_VOWEL_YEO, HANGUL_OLDVOWEL_YUYEO}, /* ㆊ */
		{HANGUL_VOWEL_YU, HANGUL_VOWEL_YE, HANGUL_OLDVOWEL_YUYE}, /* ㆋ */
		{HANGUL_VOWEL_YU, HANGUL_VOWEL_I, HANGUL_OLDVOWEL_YUI}, /* ㆌ */
		{HANGUL_OLDVOWEL_OA, HANGUL_VOWEL_I, HANGUL_OLDVOWEL_OAI}, /* ㆎ */
		{0, 0, 0}
	};
	
	const struct hangul_vowel_merge_table_t *p;
	for(p = hangul_vowel_merge_table; p->v; p++)
	{
		if(p->v1 == vowel1 && p->v2 == vowel2)
			return p->v;
	}
	
	return 0;
}


const char *hangul_merge_consonant_utf8char(const char *cons1, const char *cons2)
{
	const char *result;
	
#define CHECK_CONSONANT(a, b) \
	(!strncmp(cons1, a, sizeof(a) - 1) && !strncmp(cons2, b, sizeof(b) - 1))
	if(CHECK_CONSONANT("ㄱ", "ㄱ"))
		result = "ㄲ";
	else if(CHECK_CONSONANT("ㄱ", "ㅅ"))
		result = "ㄳ";
	else if(CHECK_CONSONANT("ㄴ", "ㅈ"))
		result = "ㄵ";
	else if(CHECK_CONSONANT("ㄴ", "ㅎ"))
		result = "ㄶ";
	else if(CHECK_CONSONANT("ㄷ", "ㄷ"))
		result = "ㄸ";
	else if(CHECK_CONSONANT("ㄹ", "ㄱ"))
		result = "ㄺ";
	else if(CHECK_CONSONANT("ㄹ", "ㅁ"))
		result = "ㄻ";
	else if(CHECK_CONSONANT("ㄹ", "ㅂ"))
		result = "ㄼ";
	else if(CHECK_CONSONANT("ㄹ", "ㅅ"))
		result = "ㄽ";
	else if(CHECK_CONSONANT("ㄹ", "ㅌ"))
		result = "ㄾ";
	else if(CHECK_CONSONANT("ㄹ", "ㅍ"))
		result = "ㄿ";
	else if(CHECK_CONSONANT("ㄹ", "ㅎ"))
		result = "ㅀ";
	else if(CHECK_CONSONANT("ㅂ", "ㅂ"))
		result = "ㅃ";
	else if(CHECK_CONSONANT("ㅂ", "ㅅ"))
		result = "ㅄ";
	else if(CHECK_CONSONANT("ㅅ", "ㅅ"))
		result = "ㅆ";
	else if(CHECK_CONSONANT("ㅈ", "ㅈ"))
		result = "ㅉ";
	else
		result = "";
#undef CHECK_CONSONANT
	
	return result;
}

const char *hangul_merge_vowel_utf8char(const char *vowel1, const char *vowel2)
{
	const char *result;
	
#define CHECK_VOWEL(a, b) \
	(!strncmp(vowel1, a, sizeof(a) - 1) && !strncmp(vowel2, b, sizeof(b) - 1))
	if(CHECK_VOWEL("ㅗ", "ㅏ"))
		result = "ㅘ";
	else if(CHECK_VOWEL("ㅗ", "ㅐ"))
		result = "ㅙ";
	else if(CHECK_VOWEL("ㅗ", "ㅣ"))
		result = "ㅚ";
	else if(CHECK_VOWEL("ㅜ", "ㅓ"))
		result = "ㅝ";
	else if(CHECK_VOWEL("ㅜ", "ㅔ"))
		result = "ㅞ";
	else if(CHECK_VOWEL("ㅜ", "ㅣ"))
		result = "ㅟ";
	else if(CHECK_VOWEL("ㅡ", "ㅣ"))
		result = "ㅢ";
	else
		result = "";
#undef CHECK_VOWEL
	
	return result;
}


// 모음 ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ
// 자음 ㄱㄲㄳㄴㄵㄶㄷㄸㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅃㅄㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ
// 초성 ㄱㄲ　ㄴ　　ㄷㄸㄹ　　　　　　　ㅁㅂㅃ　ㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ
// 종성 ㄱㄲㄳㄴㄵㄶㄷ　ㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂ　ㅄㅅㅆㅇㅈ　ㅊㅋㅌㅍㅎ
bool hangul_is_choseong_jamo(ucschar c)
{
	return (c == 0x3131 || c == 0x3132 || c == 0x3134 || 
		(c >= 0x3137 && c <= 0x3139) || 
		(c >= 0x3141 && c <= 0x3143) || 
		(c >= 0x3145 && c <= 0x314e));
}
bool hangul_is_jongseong_jamo(ucschar c)
{
	return ((c >= 0x3131 && c <= 0x3137) || 
		(c >= 0x3139 && c <= 0x3142) || 
		(c >= 0x3144 && c <= 0x3148) || 
		(c >= 0x314a && c <= 0x314e));
}












//////////////////////////////////////////////
/* libhangul
 * Copyright (C) 2004 - 2007 Choe Hwanjin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 //////////////////////

static const ucschar hangul_base    = 0xac00;
static const ucschar choseong_base  = 0x1100;
static const ucschar jungseong_base = 0x1161;
static const ucschar jongseong_base = 0x11a7;
static const int njungseong = 21;
static const int njongseong = 28;
#define HANGUL_CHOSEONG_FILLER 0x115f /* hangul choseong filler */
#define HANGUL_JUNGSEONG_FILLER 0x1160 /* hangul jungseong filler */


// convert a jaso to the compatibility jamo
ucschar hangul_jaso_to_jamo(ucschar c)
{
	static ucschar choseong[] = 
	{
		0x3131,	    /* 0x1100 */
		0x3132,	    /* 0x1101 */
		0x3134,	    /* 0x1102 */
		0x3137,	    /* 0x1103 */
		0x3138,	    /* 0x1104 */
		0x3139,	    /* 0x1105 */
		0x3141,	    /* 0x1106 */
		0x3142,	    /* 0x1107 */
		0x3143,	    /* 0x1108 */
		0x3145,	    /* 0x1109 */
		0x3146,	    /* 0x110a */
		0x3147,	    /* 0x110b */
		0x3148,	    /* 0x110c */
		0x3149,	    /* 0x110d */
		0x314a,	    /* 0x110e */
		0x314b,	    /* 0x110f */
		0x314c,	    /* 0x1110 */
		0x314d,	    /* 0x1111 */
		0x314e,	    /* 0x1112 */
	};

	static ucschar jungseong[] = 
	{
		0x314f,	    /* 0x1161 */
		0x3150,	    /* 0x1162 */
		0x3151,	    /* 0x1163 */
		0x3152,	    /* 0x1164 */
		0x3153,	    /* 0x1165 */
		0x3154,	    /* 0x1166 */
		0x3155,	    /* 0x1167 */
		0x3156,	    /* 0x1168 */
		0x3157,	    /* 0x1169 */
		0x3158,	    /* 0x116a */
		0x3159,	    /* 0x116b */
		0x315a,	    /* 0x116c */
		0x315b,	    /* 0x116d */
		0x315c,	    /* 0x116e */
		0x315d,	    /* 0x116f */
		0x315e,	    /* 0x1170 */
		0x315f,	    /* 0x1171 */
		0x3160,	    /* 0x1172 */
		0x3161,	    /* 0x1173 */
		0x3162,	    /* 0x1174 */
		0x3163	    /* 0x1175 */
	};

	static ucschar jongseong[] = 
	{
		0x3131,	    /* 0x11a8 */
		0x3132,	    /* 0x11a9 */
		0x3133,	    /* 0x11aa */
		0x3134,	    /* 0x11ab */
		0x3135,	    /* 0x11ac */
		0x3136,	    /* 0x11ad */
		0x3137,	    /* 0x11ae */
		0x3139,	    /* 0x11af */
		0x313a,	    /* 0x11b0 */
		0x313b,	    /* 0x11b1 */
		0x313c,	    /* 0x11b2 */
		0x313d,	    /* 0x11b3 */
		0x313e,	    /* 0x11b4 */
		0x313f,	    /* 0x11b5 */
		0x3140,	    /* 0x11b6 */
		0x3141,	    /* 0x11b7 */
		0x3142,	    /* 0x11b8 */
		0x3144,	    /* 0x11b9 */
		0x3145,	    /* 0x11ba */
		0x3146,	    /* 0x11bb */
		0x3147,	    /* 0x11bc */
		0x3148,	    /* 0x11bd */
		0x314a,	    /* 0x11be */
		0x314b,	    /* 0x11bf */
		0x314c,	    /* 0x11c0 */
		0x314d,	    /* 0x11c1 */
		0x314e	    /* 0x11c2 */
	};

	if(c >= 0x1100 && c <= 0x1112)
		return choseong[c - 0x1100];
	else if(c >= 0x1161 && c <= 0x1175)
		return jungseong[c - 0x1161];
	else if(c >= 0x11a8 && c <= 0x11c2)
		return jongseong[c - 0x11a8];

	return c;
}

ucschar hangul_choseong_to_jongseong(ucschar c)
{
	static ucschar table[] = 
	{
		0x11a8,  /* choseong kiyeok      -> jongseong kiyeok      */
		0x11a9,  /* choseong ssangkiyeok -> jongseong ssangkiyeok */
		0x11ab,  /* choseong nieun       -> jongseong nieun       */
		0x11ae,  /* choseong tikeut      -> jongseong tikeut      */
		0x0,     /* choseong ssangtikeut -> jongseong tikeut      */
		0x11af,  /* choseong rieul       -> jongseong rieul       */
		0x11b7,  /* choseong mieum       -> jongseong mieum       */
		0x11b8,  /* choseong pieup       -> jongseong pieup       */
		0x0,     /* choseong ssangpieup  -> jongseong pieup       */
		0x11ba,  /* choseong sios        -> jongseong sios        */
		0x11bb,  /* choseong ssangsios   -> jongseong ssangsios   */
		0x11bc,  /* choseong ieung       -> jongseong ieung       */
		0x11bd,  /* choseong cieuc       -> jongseong cieuc       */
		0x0,     /* choseong ssangcieuc  -> jongseong cieuc       */
		0x11be,  /* choseong chieuch     -> jongseong chieuch     */
		0x11bf,  /* choseong khieukh     -> jongseong khieukh     */
		0x11c0,  /* choseong thieuth     -> jongseong thieuth     */
		0x11c1,  /* choseong phieuph     -> jongseong phieuph     */
		0x11c2   /* choseong hieuh       -> jongseong hieuh       */
	};

	if(c < 0x1100 || c > 0x1112)
		return 0;

	return table[c - 0x1100];
}

ucschar hangul_jongseong_to_choseong(ucschar c)
{
	static ucschar table[] = 
	{
		0x1100,  /* jongseong kiyeok        -> choseong kiyeok       */
		0x1101,  /* jongseong ssangkiyeok   -> choseong ssangkiyeok  */
		0x1109,  /* jongseong kiyeok-sios   -> choseong sios         */
		0x1102,  /* jongseong nieun         -> choseong nieun        */
		0x110c,  /* jongseong nieun-cieuc   -> choseong cieuc        */
		0x1112,  /* jongseong nieun-hieuh   -> choseong hieuh        */
		0x1103,  /* jongseong tikeut        -> choseong tikeut       */
		0x1105,  /* jongseong rieul         -> choseong rieul        */
		0x1100,  /* jongseong rieul-kiyeok  -> choseong kiyeok       */
		0x1106,  /* jongseong rieul-mieum   -> choseong mieum        */
		0x1107,  /* jongseong rieul-pieup   -> choseong pieup        */
		0x1109,  /* jongseong rieul-sios    -> choseong sios         */
		0x1110,  /* jongseong rieul-thieuth -> choseong thieuth      */
		0x1111,  /* jongseong rieul-phieuph -> choseong phieuph      */
		0x1112,  /* jongseong rieul-hieuh   -> choseong hieuh        */
		0x1106,  /* jongseong mieum         -> choseong mieum        */
		0x1107,  /* jongseong pieup         -> choseong pieup        */
		0x1109,  /* jongseong pieup-sios    -> choseong sios         */
		0x1109,  /* jongseong sios          -> choseong sios         */
		0x110a,  /* jongseong ssangsios     -> choseong ssangsios    */
		0x110b,  /* jongseong ieung         -> choseong ieung        */
		0x110c,  /* jongseong cieuc         -> choseong cieuc        */
		0x110e,  /* jongseong chieuch       -> choseong chieuch      */
		0x110f,  /* jongseong khieukh       -> choseong khieukh      */
		0x1110,  /* jongseong thieuth       -> choseong thieuth      */
		0x1111,  /* jongseong phieuph       -> choseong phieuph      */
		0x1112   /* jongseong hieuh         -> choseong hieuh        */
	};

	if(c < 0x11a8 || c > 0x11c2)
		return 0;

	return table[c - 0x11a8];
}

int hangul_jongseong_dicompose(ucschar c, ucschar* jong, ucschar* cho)
{
	static ucschar table[][2] = 
	{
		{ 0,      0x1100 }, /* jong kiyeok	      = cho  kiyeok               */
		{ 0x11a8, 0x1100 }, /* jong ssangkiyeok   = jong kiyeok + cho kiyeok  */
		{ 0x11a8, 0x1109 }, /* jong kiyeok-sios   = jong kiyeok + cho sios    */
		{ 0,      0x1102 }, /* jong nieun	      = cho  nieun                */
		{ 0x11ab, 0x110c }, /* jong nieun-cieuc   = jong nieun  + cho cieuc   */
		{ 0x11ab, 0x1112 }, /* jong nieun-hieuh   = jong nieun  + cho hieuh   */
		{ 0,      0x1103 }, /* jong tikeut	      = cho  tikeut               */
		{ 0,      0x1105 }, /* jong rieul         = cho  rieul                */
		{ 0x11af, 0x1100 }, /* jong rieul-kiyeok  = jong rieul  + cho kiyeok  */
		{ 0x11af, 0x1106 }, /* jong rieul-mieum   = jong rieul  + cho mieum   */
		{ 0x11af, 0x1107 }, /* jong rieul-pieup   = jong rieul  + cho pieup   */
		{ 0x11af, 0x1109 }, /* jong rieul-sios    = jong rieul  + cho sios    */
		{ 0x11af, 0x1110 }, /* jong rieul-thieuth = jong rieul  + cho thieuth */
		{ 0x11af, 0x1111 }, /* jong rieul-phieuph = jong rieul  + cho phieuph */
		{ 0x11af, 0x1112 }, /* jong rieul-hieuh   = jong rieul  + cho hieuh   */
		{ 0,      0x1106 }, /* jong mieum         = cho  mieum                */
		{ 0,      0x1107 }, /* jong pieup         = cho  pieup                */
		{ 0x11b8, 0x1109 }, /* jong pieup-sios    = jong pieup  + cho sios    */
		{ 0,      0x1109 }, /* jong sios          = cho  sios                 */
		{ 0x11ba, 0x1109 }, /* jong ssangsios     = jong sios   + cho sios    */
		{ 0,      0x110b }, /* jong ieung         = cho  ieung                */
		{ 0,      0x110c }, /* jong cieuc         = cho  cieuc                */
		{ 0,      0x110e }, /* jong chieuch       = cho  chieuch              */
		{ 0,      0x110f }, /* jong khieukh       = cho  khieukh              */
		{ 0,      0x1110 }, /* jong thieuth       = cho  thieuth              */
		{ 0,      0x1111 }, /* jong phieuph       = cho  phieuph              */
		{ 0,      0x1112 }  /* jong hieuh         = cho  hieuh                */
	};

	*jong = table[c - 0x11a8][0];
	*cho  = table[c - 0x11a8][1];

	return 0;
}


#if 1
ucschar hangul_jaso_to_syllable(ucschar choseong, ucschar jungseong, 
	ucschar jongseong)
{
	ucschar c;

	/* we use 0x11a7 like a Jongseong filler */
	if(jongseong == 0)
		jongseong = 0x11a7; /* Jongseong filler */

	if(!hangul_is_choseong_conjoinable(choseong))
		return 0;
	if(!hangul_is_jungseong_conjoinable(jungseong))
		return 0;
	if(!hangul_is_jongseong_conjoinable(jongseong))
		return 0;

	choseong  -= choseong_base;
	jungseong -= jungseong_base;
	jongseong -= jongseong_base;

	c = ((choseong * njungseong) + jungseong) * 
		njongseong + jongseong + hangul_base;
	
	return c;
}

int hangul_syllable_to_jaso(ucschar syllable, ucschar* choseong, 
	ucschar* jungseong, ucschar* jongseong)
{
	if(jongseong != NULL)
		*jongseong = 0;
	if(jungseong != NULL)
		*jungseong = 0;
	if(choseong != NULL)
		*choseong = 0;

	if(!hangul_is_syllable(syllable))
		return -1;

	syllable -= hangul_base;
	if(jongseong != NULL)
	{
		if(syllable % njongseong != 0)
			*jongseong = jongseong_base + syllable % njongseong;
	}
	syllable /= njongseong;

	if(jungseong != NULL)
	{
		*jungseong = jungseong_base + syllable % njungseong;
	}
	syllable /= njungseong;

	if(choseong != NULL)
	{
		*choseong = choseong_base + syllable;
	}

	return 0;
}



#endif



/* vim: set tabstop=4 textwidth=80: */

