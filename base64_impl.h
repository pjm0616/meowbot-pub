#ifndef BASE64_IMPL_H_INCLUDED
#define BASE64_IMPL_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

int base64_decode(const unsigned char *in, unsigned int inlen, unsigned char *out, unsigned int *outlen);
int base64_encode(const unsigned char *in, unsigned int inlen, unsigned char *out, unsigned int *outlen);

#ifdef __cplusplus
}
#endif


#endif // BASE64_IMPL_H_INCLUDED
