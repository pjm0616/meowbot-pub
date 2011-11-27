#ifndef MY_VSPRINTF_H_INCLUDED
#define MY_VSPRINTF_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

struct lua_State;

unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base);
long simple_strtol(const char *cp,char **endp,unsigned int base);
unsigned long long simple_strtoull(const char *cp,char **endp,unsigned int base);
long long simple_strtoll(const char *cp,char **endp,unsigned int base);
int my_vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int my_vsnprintf_lua(char *buf, size_t size, const char *fmt, lua_State *L, int st_idx);
int my_vscnprintf(char *buf, size_t size, const char *fmt, va_list args);
int my_snprintf(char * buf, size_t size, const char *fmt, ...);
int my_scnprintf(char * buf, size_t size, const char *fmt, ...);
int my_vsprintf(char *buf, const char *fmt, va_list args);
int my_sprintf(char * buf, const char *fmt, ...);

#ifdef __cplusplus
} // extern "C:
#endif


#endif // MY_VSPRINTF_H_INCLUDED
