
#include <iostream>
#include <string>

#include "base64_impl.h"
#include "base64.h"


std::string base64_decode(const std::string &data)
{
	unsigned int size = data.length();
	char *buf = new char[size];
	std::string result;
	int ret;
	
	ret = base64_decode(reinterpret_cast<const unsigned char *>(data.c_str()), data.length(), 
		reinterpret_cast<unsigned char *>(buf), &size);
	if(!ret)
	{
		buf[size] = 0;
		result = buf;
	}
	else
	{
		result = "";
		if(ret == -2)
		{
			std::cerr << "base64_decode() failed: " << ret << " (buffer size: " << size << ")" << std::endl;
		}
	}
	delete[] buf;
	
	return result;
}


std::string base64_encode(const std::string &data)
{
	unsigned int size = 4 * ((data.length() + 2) / 3) + 1;
	char *buf = new char[size];
	std::string result;
	int ret;
	
	ret = base64_encode(reinterpret_cast<const unsigned char *>(data.c_str()), data.length(), 
		reinterpret_cast<unsigned char *>(buf), &size);
	if(!ret)
	{
		result = buf;
	}
	else
	{
		result = "";
		if(ret == -2)
		{
			std::cerr << "base64_encode() failed: " << ret << " (buffer size: " << size << ")" << std::endl;
		}
	}
	delete[] buf;
	
	return result;
}









