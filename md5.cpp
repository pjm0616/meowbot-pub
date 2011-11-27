
#include <string>
#include <string.h>
#include <assert.h>

#include "md5_impl.h"
#include "md5.h"



void md5(const char *data, unsigned int len, char dest[16])
{
	MD5_CTX ctx;
	
	MD5_Init(&ctx);
	MD5_Update(&ctx, data, len);
	MD5_Final(reinterpret_cast<unsigned char *>(dest), &ctx);
}

// char output[33]
void md5_hex_conv(const char hash[16], char *output)
{
	const char *hexconv = "0123456789abcdef";
	int i, j;
	
	for(i = 0; i < 16 ; i++)
	{
		j = hash[i];
		output[i*2] = hexconv[(j >> 4) & 0x0f];
		output[i*2+1] = hexconv[j & 0x0f];
	}
	output[32] = '\0';
}

std::string md5_hex(const std::string &data)
{
	char md5buf[16], hexbuf[33];
	
	md5(data.c_str(), data.length(), md5buf);
	md5_hex_conv(md5buf, hexbuf);
	
	return std::string(hexbuf);
}







