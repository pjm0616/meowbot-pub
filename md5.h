#ifndef MD5_H_INCLUDED
#define MD5_H_INCLUDED



void md5(const char *data, unsigned int len, char dest[16]);
inline void md5(const char *data, unsigned int len, int dest[4])
{
	md5(data, len, (char *)dest);
}
void md5_hex_conv(const char hash[16], char *output);
std::string md5_hex(const std::string &data);
static inline void md5(const std::string &data, unsigned int dest[4])
{
	md5(data.c_str(), data.length(), (char *)dest);
}



#endif // MD5_H_INCLUDED

