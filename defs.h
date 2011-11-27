#ifndef DEFS_H_INCLUDED
#define DEFS_H_INCLUDED

#ifndef __GNUC__
# error use GCC
#endif


#if defined(__GNUC__) && !defined(__MIPSEL__) && !defined(__CYGWIN__)
# define HAVE_TLS___THREAD 1
#else
# define __thread
#endif

#define HAVE_RAND48

#ifdef __GNUC__
# define likely(_expr) __builtin_expect((_expr), 1)
# define unlikely(_expr) __builtin_expect((_expr), 0)
#else
# define likely(_expr) (_expr)
# define unlikely(_expr) (_expr)
#endif







#endif // DEFS_H_INCLUDED

