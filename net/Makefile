include ../Makefile.pub
AR = ar r
CC = gcc
DEFS =
CFLAGS = -Werror -g -funsigned-char $(DEFS) -I.. -Iinclude -I../util/include

CYGWIN := $(shell uname -a | grep -o Cygwin)

CPP = gcc -E
LIBS = -lssl -lcrypto -lresolv -lcrypt -ldl -lz \
        -lrt -lutil -L../util/$(CROSS_PATH) -llogger -L../logger/$(CROSS_PATH) -lpthread \
        -Wl,--rpath=. -Wl,--rpath=lib 

ifeq ($(CYGWIN),Cygwin)
CFLAGS += -D_WIN32_WINNT=0x0600
CC = $(CROSS_COMPILE)gcc  
CXX = $(CROSS_COMPILE)g++  
LD = $(CROSS_COMPILE)ld  
STRIP = $(CROSS_COMPILE)strip  
OBJCOPY = $(CROSS_COMPILE)objcopy  
OBJDUMP = $(CROSS_COMPILE)objdump  
AR = $(CROSS_COMPILE)ar cr
RANLIB = $(CROSS_COMPILE)ranlib 
else
CFLAGS += -fPIC
endif

CXXFLAGS:=$(CFLAGS)

HEADERS = 
SOURCES_REACTOR = $(wildcard reactor/*.c)
SOURCES_PROTOCOL = $(wildcard protocol/*.c)
SOURCES_APP = $(wildcard app/*.c)
SOURCES_TEST = $(wildcard test/*.c)
OBJS_REACTOR = ${SOURCES_REACTOR:reactor/%.c=obj/reactor_%.o}
OBJS_PROTOCOL = ${SOURCES_PROTOCOL:protocol/%.c=obj/protocol_%.o}
OBJS_APP = ${SOURCES_APP:app/%.c=obj/app_%.o}
OBJS_TEST = ${SOURCES_TEST:test/%.c=obj/test_%.o}

LIBRARY_DIST = $(CROSS_PATH)/libnet.a
TEST_BINARY = ${SOURCES_TEST:test/%.c=test_%}


CFLAGS += -I../../../libevent/include
LIBPATH += -L../../../libevent/$(CROSS_PATH)


.PHONY :prepare all clean test

all: prepare $(LIBRARY_DIST) test

test : $(TEST_BINARY)

prepare:
	@if ! [ -d $(CROSS_PATH) ]; then mkdir -p $(CROSS_PATH); fi;
	@if ! [ -d obj ]; then mkdir obj; fi;
	
obj/reactor_%.o : reactor/%.c
	$(COMPILE_CC)
obj/protocol_%.o : protocol/%.c
	$(COMPILE_CC)
obj/app_%.o : app/%.c
	$(COMPILE_CC)
obj/test_%.o : test/%.c
	$(COMPILE_CC)
	
test_%:obj/test_%.o $(LIBRARY_DIST)
	$(call ASSEMBLE_EXTRA,CC,-lz -lutil  -L../util/$(CROSS_PATH) -L../logger/$(CROSS_PATH) -lpthread -levent_core -levent_pthreads -lexpat)

$(LIBRARY_DIST):$(OBJS_REACTOR) $(OBJS_PROTOCOL) $(OBJS_APP) 
	$(call ARCHIVE)

clean:
	@rm -rf obj $(LIBRARY_DIST)  $(TEST_BINARY)

