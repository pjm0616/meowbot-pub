#ifndef HANGUL_IMPL_H_INCLUDED
#define HANGUL_IMPL_H_INCLUDED



enum HANGUL_CODES
{
	HANGUL_CODE_NONE = 0x0, 
	
	HANGUL_CONSONANT_G = 0x3131, // ㄱ
	HANGUL_CONSONANT_GG = 0x3132, // ㄲ
	HANGUL_CONSONANT_GS = 0x3133, // ㄳ
	HANGUL_CONSONANT_N = 0x3134, // ㄴ
	HANGUL_CONSONANT_NJ = 0x3135, // ㄵ
	HANGUL_CONSONANT_NH = 0x3136, // ㄶ
	HANGUL_CONSONANT_D = 0x3137, // ㄷ
	HANGUL_CONSONANT_DD = 0x3138, // ㄸ
	HANGUL_CONSONANT_L = 0x3139, // ㄹ
	HANGUL_CONSONANT_LG = 0x313a, // ㄺ
	HANGUL_CONSONANT_LM = 0x313b, // ㄻ
	HANGUL_CONSONANT_LB = 0x313c, // ㄼ
	HANGUL_CONSONANT_LS = 0x313d, // ㄽ
	HANGUL_CONSONANT_LT = 0x313e, // ㄾ
	HANGUL_CONSONANT_LP = 0x313f, // ㄿ
	HANGUL_CONSONANT_LH = 0x3140, // ㅀ
	HANGUL_CONSONANT_M = 0x3141, // ㅁ
	HANGUL_CONSONANT_B = 0x3142, // ㅂ
	HANGUL_CONSONANT_BB = 0x3143, // ㅃ
	HANGUL_CONSONANT_BS = 0x3144, // ㅄ
	HANGUL_CONSONANT_S = 0x3145, // ㅅ
	HANGUL_CONSONANT_SS = 0x3146, // ㅆ
	HANGUL_CONSONANT_NG = 0x3147, // ㅇ
	HANGUL_CONSONANT_J = 0x3148, // ㅈ
	HANGUL_CONSONANT_JJ = 0x3149, // ㅉ
	HANGUL_CONSONANT_CH = 0x314a, // ㅊ
	HANGUL_CONSONANT_K = 0x314b, // ㅋ
	HANGUL_CONSONANT_T = 0x314c, // ㅌ
	HANGUL_CONSONANT_P = 0x314d, // ㅍ
	HANGUL_CONSONANT_H = 0x314e, // ㅎ
	
	HANGUL_VOWEL_A = 0x314f, // ㅏ
	HANGUL_VOWEL_AE = 0x3150, // ㅐ
	HANGUL_VOWEL_YA = 0x3151, // ㅑ
	HANGUL_VOWEL_YAE = 0x3152, // ㅒ
	HANGUL_VOWEL_EO = 0x3153, // ㅓ
	HANGUL_VOWEL_E = 0x3154, // ㅔ
	HANGUL_VOWEL_YEO = 0x3155, // ㅕ
	HANGUL_VOWEL_YE = 0x3156, // ㅖ
	HANGUL_VOWEL_O = 0x3157, // ㅗ
	HANGUL_VOWEL_WA = 0x3158, // ㅘ
	HANGUL_VOWEL_WAE = 0x3159, // ㅙ
	HANGUL_VOWEL_OE = 0x315a, // ㅚ
	HANGUL_VOWEL_YO = 0x315b, // ㅛ
	HANGUL_VOWEL_U = 0x315c, // ㅜ
	HANGUL_VOWEL_WO = 0x315d, // ㅝ
	HANGUL_VOWEL_WE = 0x315e, // ㅞ
	HANGUL_VOWEL_WI = 0x315f, // ㅟ
	HANGUL_VOWEL_YU = 0x3160, // ㅠ
	HANGUL_VOWEL_EU = 0x3161, // ㅡ
	HANGUL_VOWEL_UI = 0x3162, // ㅢ
	HANGUL_VOWEL_I = 0x3163, // ㅣ
	
	HANGUL_OLDCONSONANT_NN = 0x3165, // ㅥ
	HANGUL_OLDCONSONANT_ND = 0x3166, // ㅦ
	HANGUL_OLDCONSONANT_NS = 0x3167, // ㅧ
	HANGUL_OLDCONSONANT_NZ = 0x3168, // ㅨ
	HANGUL_OLDCONSONANT_LGS = 0x3169, // ㅩ
	HANGUL_OLDCONSONANT_LD = 0x316a, // ㅪ
	HANGUL_OLDCONSONANT_LBS = 0x316b, // ㅫ
	HANGUL_OLDCONSONANT_LZ = 0x316c, // ㅬ
	HANGUL_OLDCONSONANT_LNG2 = 0x316d, // ㅭ
	HANGUL_OLDCONSONANT_MB = 0x316e, // ㅮ
	HANGUL_OLDCONSONANT_MS = 0x316f, // ㅯ
	HANGUL_OLDCONSONANT_MZ = 0x3170, // ㅰ
	HANGUL_OLDCONSONANT_MNG = 0x3171, // ㅱ
	HANGUL_OLDCONSONANT_BG = 0x3172, // ㅲ
	HANGUL_OLDCONSONANT_BD = 0x3173, // ㅳ
	HANGUL_OLDCONSONANT_BSG = 0x3174, // ㅴ
	HANGUL_OLDCONSONANT_BSD = 0x3175, // ㅵ
	HANGUL_OLDCONSONANT_BJ = 0x3176, // ㅶ
	HANGUL_OLDCONSONANT_BT = 0x3177, // ㅷ
	HANGUL_OLDCONSONANT_BNG = 0x3178, // ㅸ
	HANGUL_OLDCONSONANT_BBNG = 0x3179, // ㅹ
	HANGUL_OLDCONSONANT_SG = 0x317a, // ㅺ
	HANGUL_OLDCONSONANT_SN = 0x317b, // ㅻ
	HANGUL_OLDCONSONANT_SD = 0x317c, // ㅼ
	HANGUL_OLDCONSONANT_SB = 0x317d, // ㅽ
	HANGUL_OLDCONSONANT_SJ = 0x317e, // ㅾ
	HANGUL_OLDCONSONANT_Z = 0x317f, // ㅿ
	HANGUL_OLDCONSONANT_NGNG = 0x3180, // ㆀ
	HANGUL_OLDCONSONANT_NNG = 0x3181, // ㆁ
	HANGUL_OLDCONSONANT_NNGS = 0x3182, // ㆂ
	HANGUL_OLDCONSONANT_NNGZ = 0x3183, // ㆃ
	HANGUL_OLDCONSONANT_PNG = 0x3184, // ㆄ
	HANGUL_OLDCONSONANT_HH = 0x3185, // ㆅ
	HANGUL_OLDCONSONANT_NG2 = 0x3186, // ㆆ
	
	HANGUL_OLDVOWEL_YOYA = 0x3187, // ㆇ
	HANGUL_OLDVOWEL_YOYAE = 0x3188, // ㆈ
	HANGUL_OLDVOWEL_YOI = 0x3189, // ㆉ
	HANGUL_OLDVOWEL_YUYEO = 0x318a, // ㆊ
	HANGUL_OLDVOWEL_YUYE = 0x318b, // ㆋ
	HANGUL_OLDVOWEL_YUI = 0x318c, // ㆌ
	HANGUL_OLDVOWEL_OA = 0x318d, // ㆍ
	HANGUL_OLDVOWEL_OAI = 0x318e, // ㆎ
	
	HANGUL_CODE_END = 0xffff
};


/**************************/



#ifdef __cplusplus
extern "C" {
#endif


unsigned int hangul_join(unsigned int lead, unsigned int vowel, unsigned int tail);
unsigned int hangul_join_utf8char(const char *lead_utf8char, const char *vowel_utf8char, const char *tail_utf8char);

int hangul_split(unsigned int chr, unsigned int *lead, unsigned int *vowel, unsigned int *tail);
int hangul_split_utf8char(const char *utf8char, char *lead_utf8char, char *vowel_utf8char, char *tail_utf8char);

unsigned int hangul_merge_consonant(unsigned int cons1, unsigned int cons2);
const char *hangul_merge_consonant_utf8char(const char *cons1, const char *cons2);
unsigned int hangul_merge_vowel(unsigned int vowel1, unsigned int vowel2);
const char *hangul_merge_vowel_utf8char(const char *vowel1, const char *vowel2);

unsigned int hangul_merge_allconsonant(unsigned int cons1, unsigned int cons2, unsigned int cons3);
unsigned int hangul_merge_allvowel(unsigned int vowel1, unsigned int vowel2);



static inline bool hangul_is_consonant(ucschar c){return c >= 0x3131 && c <= 0x314e;}
static inline bool hangul_is_vowel(ucschar c){return c >= 0x314f && c <= 0x3163;}
static inline bool hangul_is_oldconsonant(ucschar c){return c >= 0x3165 && c <= 0x3186;}
static inline bool hangul_is_oldvowel(ucschar c){return c >= 0x3187 && c <= 0x318e;}
static inline bool hangul_is_jamo(ucschar c){return c >= 0x3131 && c <= 0x3163;}
static inline bool hangul_is_oldjamo(ucschar c){return c >= 0x3165 && c <= 0x318e;}
static inline bool hangul_is_jamo_all(ucschar c){return c >= 0x3131 && c <= 0x318e;}

bool hangul_is_choseong_jamo(ucschar c);
bool hangul_is_jongseong_jamo(ucschar c);



static inline bool hangul_is_choseong(ucschar c){return c >= 0x1100 && c <= 0x1159;}
static inline bool hangul_is_jungseong(ucschar c){return c >= 0x1161 && c <= 0x11a2;}
static inline bool hangul_is_jongseong(ucschar c){return c >= 0x11a8 && c <= 0x11f9;}
static inline bool hangul_is_choseong_conjoinable(ucschar c){return c >= 0x1100 && c <= 0x1112;}
static inline bool hangul_is_jungseong_conjoinable(ucschar c){return c >= 0x1161 && c <= 0x1175;}
static inline bool hangul_is_jongseong_conjoinable(ucschar c){return c >= 0x11a7 && c <= 0x11c2;}
static inline bool hangul_is_jaso(ucschar c){return hangul_is_choseong(c) || hangul_is_jungseong(c) || hangul_is_jongseong(c);}

static inline bool hangul_is_syllable(ucschar c){return c >= 0xac00 && c <= 0xd7a3;}


ucschar hangul_jaso_to_jamo(ucschar ch);
ucschar hangul_choseong_to_jongseong(ucschar ch);
ucschar hangul_jongseong_to_choseong(ucschar ch);
int hangul_jongseong_dicompose(ucschar ch, ucschar* jong, ucschar* cho);

#if 1
ucschar hangul_jaso_to_syllable(ucschar choseong, ucschar jungseong, ucschar jongseong);
int hangul_syllable_to_jaso(ucschar syllable, ucschar* choseong, ucschar* jungseong, ucschar* jongseong);
#endif



#ifdef __cplusplus
}
#endif


#endif // HANGUL_IMPL_H_INCLUDED

