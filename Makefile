#!/usr/bin/make

STATIC_MODULES		?=0
STATIC_BUILD		?=0
DEBUG				?=1
ENABLE_PERL			?=0

CROSSC				=
#CROSSC				= mipsel-linux-

SRCS					= pcrecpp/pcre_stringpiece.cpp pcrecpp/pcre_scanner.cpp pcrecpp/pcrecpp.cpp my_vsprintf.cpp sqlite3.c utf8_impl.c utf8.cpp hangul_impl.c hangul.cpp base64_impl.c base64.cpp md5_impl.c md5.cpp ircstring.cpp wildcard.cpp encoding_converter.cpp etc.cpp tcpsock.cpp sqlite3_db.cpp ircutils.cpp irc.cpp config.cpp module.cpp luacpp.cpp luapcrelib.cpp luautf8lib.cpp luasocklib.cpp luahangullib.cpp luathreadlib.cpp luaetclib.cpp luasqlite3lib.cpp ircbot.cpp main.cpp 
PROGRAM_NAME		= ircbot
LIBS					= ./lua/src/liblua.a -lpcre -lz -ldl -lm -lpthread -lc -lcrypt #-liconv #-lcurl
INCLUDEDIR			= -I. -I./pcrecpp -I./lua/src
LIBDIR					= 

MODULE_SRCS			= mod_skel.cpp mod_admin.cpp mod_dcc.cpp mod_basic.cpp mod_remember.cpp \
							mod_netutils.cpp mod_strutils.cpp mod_extcmd.cpp mod_lua.cpp \
							mod_dcinside.cpp mod_hanja.cpp mod_seen.cpp mod_query_redir.cpp mod_perl.cpp \
							mod_ewordquiz.cpp mod_kor_orthography.cpp mod_inklbot_client.cpp 

DEFS				= -D_REENTRANT -D_GNU_SOURCE -DSQLITE_THREADSAFE=1 

ifeq ($(DEBUG),1)
CFLAGS_DBG			= -DDEBUG -g
CXXFLAGS_DBG		= -DDEBUG -g
else
CFLAGS_DBG			= -DNDEBUG -O2
CXXFLAGS_DBG		= -DNDEBUG -O2 -export-dynamic
endif

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

ifeq ($(STATIC_BUILD),1)
CFLAGS_SHLIB			= -DSTATIC_BUILD -export-dynamic
LDFLAGS_SHLIB		= -DSTATIC_BUILD -export-dynamic
TARGET				=$(PROGRAM_NAME)
else
CFLAGS_SHLIB			= -export-dynamic
LDFLAGS_SHLIB		= -export-dynamic
TARGET				=$(PROGRAM_NAME)
endif


CC					=$(CROSSC)gcc
CXX				=$(CROSSC)g++
#LD					=$(CROSSC)gcc
LD					=$(CROSSC)g++
AR					=$(CROSSC)ar
AS					=$(CROSSC)as
STRIP				=$(CROSSC)strip
OBJDUMP			=$(CROSSC)objdump
RANLIB				=$(CROSSC)ranlib
MAKE				=make #--no-print-directory

RM					=rm -f
LN_S				=ln -s
CP					=cp
MV					=mv

CFLAGS			= $(CFLAGS_DBG) $(CFLAGS_CROSS) $(INCLUDEDIR) \
						-std=gnu99 -Wall -Wextra $(DEFS) $(CFLAGS_PERL) -pipe #-fgnu89-inline #-std=c99 #-pedantic
CXXFLAGS			= $(CXXFLAGS_DBG) $(CXXFLAGS_CROSS) $(INCLUDEDIR) \
						-std=gnu++98 -Wall -Wextra $(DEFS) $(CXXFLAGS_PERL) -pipe #-std=c++98 #-pedantic
LDFLAGS			= $(LIBDIR) $(LDFLAGS_DBG) $(LDFLAGS_CROSS) $(LIBS) $(LDFLAGS_PERL)
OBJS_TMP			= $(SRCS:.c=.o)
OBJS				= $(OBJS_TMP:.cpp=.o)

MODULE_SRCS_REAL = $(shell echo $(MODULE_SRCS) | sed s/mod_/modules\\/mod_/g)
MODULE_OBJS_TMP = $(MODULE_SRCS_REAL:.c=.o)
MODULE_OBJS		 = $(MODULE_OBJS_TMP:.cpp=.o)

ifeq ($(STATIC_MODULES),1)
CFLAGS			+= -DSTATIC_MODULES
CXXFLAGS			+= -DSTATIC_MODULES
LDFLAGS			+= $(MODULE_OBJS)
endif

ifeq ($(STATIC_MODULES),1)
all:	.depend liblua mods $(TARGET)
else
all:	.depend liblua $(TARGET) mods
endif

.SUFFIXES: .c .o
.c.o:
	$(CC) -c $< $(CFLAGS) $(CFLAGS_SHLIB) -o $@

.SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) -c $< $(CXXFLAGS) $(CFLAGS_SHLIB) -o $@


$(TARGET): $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) $(LDFLAGS_SHLIB) -o $@ 
#	$(STRIP) $(TARGET)
	
clean: modclean
	rm -f $(OBJS) $(TARGET)

liblua:
	@$(MAKE) -C lua/src linux DEBUG=$(DEBUG)

luaclean:
	@$(MAKE) -C lua/src clean DEBUG=$(DEBUG)

mods:
	@$(MAKE) -C modules STATIC_MODULES=$(STATIC_MODULES) DEBUG=$(DEBUG) ENABLE_PERL=$(ENABLE_PERL)

modclean:
	@$(MAKE) -C modules STATIC_MODULES=$(STATIC_MODULES) DEBUG=$(DEBUG) ENABLE_PERL=$(ENABLE_PERL) clean

distclean:	clean depclean luaclean
	@$(MAKE) -C modules STATIC_MODULES=$(STATIC_MODULES) DEBUG=$(DEBUG) ENABLE_PERL=$(ENABLE_PERL) distclean

dep: depend
depend:
	$(CXX) -MM $(SRCS) $(INCLUDEDIR) >.depend
	@$(MAKE) -C modules STATIC_MODULES=$(STATIC_MODULES) DEBUG=$(DEBUG) ENABLE_PERL=$(ENABLE_PERL) depend
.depend:
	$(CXX) -MM $(SRCS) $(INCLUDEDIR) >.depend
	@$(MAKE) -C modules STATIC_MODULES=$(STATIC_MODULES) DEBUG=$(DEBUG) ENABLE_PERL=$(ENABLE_PERL) depend

depclean:
	$(RM) .depend
	@$(MAKE) -C modules STATIC_MODULES=$(STATIC_MODULES) DEBUG=$(DEBUG) ENABLE_PERL=$(ENABLE_PERL) depclean


ifneq ($(wildcard .depend),)
include .depend
endif



