#!/usr/bin/make

STATIC_MODULES		?=0
DEBUG				?=0
ENABLE_PERL			?=1

CROSSC				=
#CROSSC				= mipsel-linux-

SRCS					= mod_admin.cpp mod_lua.cpp mod_dcc.cpp mod_basic.cpp mod_remember.cpp \
							mod_luaeval.cpp mod_netutils.cpp mod_strutils.cpp mod_extcmd.cpp \
							mod_hanja.cpp mod_query_redir.cpp mod_perl.cpp \
							mod_ewordquiz.cpp mod_kor_orthography.cpp mod_geoip.cpp \
							mod_skel.cpp 
LIBS					= 
INCLUDEDIR			= -I.. -I../pcrecpp -I../lua/src
LIBDIR					=

ifeq ($(DEBUG),1)
CFLAGS_DBG			= -DDEBUG -g
CXXFLAGS_DBG		= -DDEBUG -g
else
CFLAGS_DBG			= -DNDEBUG -O2
CXXFLAGS_DBG		= -DNDEBUG -O2
endif

DEFS				= -D_REENTRANT -D_GNU_SOURCE 

ifeq ($(ENABLE_PERL),1)
CFLAGS_PERL			= -DTHREADS_HAVE_PIDS -fno-strict-aliasing \
						-I/usr/local/include -I/usr/lib/perl/5.10/CORE -D_LARGEFILE_SOURCE \
						-D_FILE_OFFSET_BITS=64 -DENABLE_PERL -Wno-unused-function
CXXFLAGS_PERL		= $(CFLAGS_PERL)
LDFLAGS_PERL		= -Wl,-E -L/usr/local/lib -L/usr/lib/perl/5.10/CORE -lperl 
else
CFLAGS_PERL			=
CXXFLAGS_PERL		=
LDFLAGS_PERL		=
endif

ifeq ($(CROSSC),)
CFLAGS_CROSS		= 
CXXFLAGS_CROSS		=  $(CFLAGS_CROSS)
LDFLAGS_CROSS		= 
endif
ifeq ($(CROSSC),mipsel-linux-)
CFLAGS_CROSS		= 
CXXFLAGS_CROSS		=  $(CFLAGS_CROSS)
LDFLAGS_CROSS		= -L/opt/mipsel-linux-gcc-sdk-3.4.4/cross/mipsel-linux/lib
#LDFLAGS_CROSS		= -L/opt/mipsel-linux-gcc-3.4.6/lib
LDFLAGS_CROSS		+= -liconv
endif

ifeq ($(STATIC_MODULES),1)
CFLAGS_SHLIB		= -DSTATIC_MODULES -export-dynamic
CXXFLAGS_SHLIB		= $(CFLAGS_SHLIB)
LDFLAGS_SHLIB		= -DSTATIC_MODULES
OBJS_TMP			= $(SRCS:.c=.o)
OBJS				= $(OBJS_TMP:.cpp=.o)
else
CFLAGS_SHLIB		=-fPIC
CXXFLAGS_SHLIB		= $(CFLAGS_SHLIB)
LDFLAGS_SHLIB		=-shared
OBJS_TMP			= $(SRCS:.c=.so)
OBJS				= $(OBJS_TMP:.cpp=.so)
endif

CC					=$(CROSSC)gcc
CXX				=$(CROSSC)g++
LD					=$(CROSSC)g++
AR					=$(CROSSC)ar
AS					=$(CROSSC)as
STRIP				=$(CROSSC)strip
OBJDUMP			=$(CROSSC)objdump
RANLIB				=$(CROSSC)ranlib

CFLAGS			= $(CFLAGS_DBG) $(CFLAGS_CROSS) $(INCLUDEDIR) \
						-std=gnu99 -Wall -Wextra $(DEFS) $(CFLAGS_PERL) $(CFLAGS_SHLIB) -pipe #-fgnu89-inline #-std=c99 #-pedantic
CXXFLAGS			= $(CXXFLAGS_DBG) $(CXXFLAGS_CROSS) $(INCLUDEDIR) \
						-std=gnu++98 -Wall -Wextra $(DEFS) $(CXXFLAGS_PERL) $(CXXFLAGS_SHLIB) -pipe #-std=c++98 #-pedantic
LDFLAGS			= $(LIBDIR) $(LDFLAGS_DBG) $(LDFLAGS_CROSS) $(LIBS) $(LDFLAGS_PERL) \
						$(LDFLAGS_SHLIB)

all:	.depend $(OBJS)

ifeq ($(STATIC_MODULES),1)
.SUFFIXES: .c .o
.c.o:
	$(CC) -c $< $(CFLAGS) -o $@

.SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) -c $< $(CXXFLAGS) -o $@
else
.SUFFIXES: .c .so
.c.so:
	$(CC) $< $(CFLAGS) $(LDFLAGS) -Wl,-soname,$(shell echo $@ | sed s/.so//) -o $@

.SUFFIXES: .cpp .so
.cpp.so:
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -Wl,-soname,$(shell echo $@ | sed s/\.so//) -o $@
endif

clean:
	rm -f $(OBJS)
	
distclean:	clean
	rm -f .depend

dep:    depend

depend:
	$(CXX) -MM $(SRCS) $(INCLUDEDIR) >.depend

.depend:
	$(CXX) -MM $(SRCS) $(INCLUDEDIR) >.depend

depclean:
	rm -f .depend


ifneq ($(wildcard .depend),)
include .depend
endif

