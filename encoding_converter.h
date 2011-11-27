#ifndef ENCODING_CONVERTER_H_INCLUDED
#define ENCODING_CONVERTER_H_INCLUDED

#include <iconv.h>

class encconv
{
public:
	enum iconv_type_t
	{
		ICONV_TYPE_NONE, 
		ICONV_TYPE_TRANSLIT, 
		ICONV_TYPE_IGNORE, 
	};
	
	encconv();
	encconv(const std::string &from, const std::string &to, 
		iconv_type_t type = ICONV_TYPE_TRANSLIT);
	~encconv();
	
	int open(const std::string &from, const std::string &to, 
		iconv_type_t type = ICONV_TYPE_TRANSLIT);
	int close();
	
	int convert(char **resultp, const char *data, unsigned int len, size_t *cvlen, 
		bool fail_on_invalid_seq = false) const;
	char *convert(const char *data, unsigned int len, size_t *cvlen = NULL, 
		bool fail_on_invalid_seq = false) const;
	int convert(std::string &dest, const std::string &str, 
		bool fail_on_invalid_seq = false) const;
	std::string convert(const std::string &str, bool fail_on_invalid_seq = false) const;
	
	static int convert(std::string &dest, const std::string &from, const std::string &to, 
		const std::string &str, bool fail_on_invalid_seq = false);
	static std::string convert(const std::string &from, const std::string &to, 
		const std::string &str, bool fail_on_invalid_seq = false);
	
	inline char set_fill_char(char c)
	{
		char b = this->fill_char;
		this->fill_char = c;
		return b;
	}
	
private:
	std::string from, to;
	iconv_t converter;
	
	char fill_char;
};

#endif // ENCODING_CONVERTER_H_INCLUDED

