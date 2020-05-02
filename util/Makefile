include ../Makefile.pub

DEFS = -DDEBUG
CFLAGS = -Wall -DJABBERD2_EXPORTS -g -funsigned-char $(DEFS) -I.. -Iinclude

CPP = gcc -E
LIB = -lssl -lcrypto -lresolv -lcrypt -ldl -lz -lrt

HEADERS = 
SOURCES = $(wildcard src/*.c)

LIBRARY_DIST = $(CROSS_PATH)/vmsutil.dll
LIBRARY_ARCH = $(CROSS_PATH)/libutil.a

CYGWIN := $(shell uname -a | grep -o Cygwin)
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
LIBRARY_DIST :=
endif

CXXFLAGS:=$(CFLAGS)
OBJS=${SOURCES:src/%.c=obj/%.o}

CPPSOURCES = $(wildcard src/*.cpp)
CPPOBJS=${CPPSOURCES:src/%.cpp=obj/%.obj}

all: prepare $(LIBRARY_DIST) $(LIBRARY_ARCH) 

prepare:
	@if ! [ -d obj ]; then mkdir obj; fi;
	@if ! [ -d $(CROSS_PATH) ]; then mkdir -p $(CROSS_PATH); fi;
	
obj/%.o : src/%.c
	$(COMPILE_CC)

obj/%.obj : src/%.cpp
	$(COMPILE_CXX)
	
$(LIBRARY_ARCH):$(OBJS) $(CPPOBJS)
	$(call ARCHIVE)

$(LIBRARY_DIST):CC = $(CROSS_COMPILE)gcc -shared
$(LIBRARY_DIST):$(OBJS) $(CPPOBJS)
	$(call ASSEMBLE_EXTRA,CC, -lvmslogger -L../logger/$(CROSS_PATH) -lws2_32 -lpthread -lz)
	$(call copyto,../$(CROSS_PATH_BIN))
clean:
	@rm -f $(LIBRARY_DIST) $(OBJS) $(CPPOBJS)
	@rm -rf obj

