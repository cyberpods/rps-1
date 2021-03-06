
OS := $(shell sh -c 'uname -s 2>/dev/null || echo unknow')
ROOT := $(shell sh -c 'cd .. ; pwd')
OPTIMIZATION?=-O2


RM=rm -rf
PREFIX?=/usr/local
#INSTALL_BIN=$(PREFIX)/bin
INSTALL_BIN=$(ROOT)/bin
INSTALL=install

STD=-std=c99 -pedantic
WARN=-Wall -W -Wno-missing-field-initializers
OPT=$(OPTIMIZATION)
DEBUG=-g -ggdb
DYNAMIC=-rdynamic
RPS_DEBUG = $(RPS_DEBUG_OPEN)
GNU_SOURCE=-D_GNU_SOURCE #libuv need this declaration in linux platform.

DEPENDENCY_TARGETS=yaml libuv jansson
LIBYAML=yaml-0.1.5
LIBUV=libuv-v1.9.1
JANSSON=jansson-2.9



FINAL_CFLAGS=$(WARN) $(OPT) $(DEBUG) $(RPS_DEBUG) $(CFLAGS)

FINAL_LDFLAGS=$(LDFLAGS) $(DEBUG) $(DYNAMIC)

ifeq ($(OS), Linux)
	FINAL_CFLAGS+=$(GNU_SOURCE) 
	FINAL_LIBS+= -lm -lrt -lpthread -lcurl
else
ifeq ($(OS), Darwin)
	FINAL_CFLAGS+=$(STD) 
	FINAL_LIBS+= -lm -lcurl
endif
endif

FINAL_CFLAGS+= -I.
FINAL_CFLAGS+= -Iproto
FINAL_CFLAGS+= -I../contrib/$(LIBYAML)/include 
FINAL_CFLAGS+= -I../contrib/$(LIBUV)/include 
FINAL_CFLAGS+= -I../contrib/$(HIREDIS)
FINAL_CFLAGS+= -I../contrib/$(JANSSON)/src

FINAL_LIBS+= ./proto/proto.a
FINAL_LIBS+= ../contrib/$(LIBYAML)/src/.libs/libyaml.a
FINAL_LIBS+= ../contrib/$(LIBUV)/.libs/libuv.a
FINAL_LIBS+= ../contrib/$(JANSSON)/src/.libs/libjansson.a


CC?=gcc
RPS_CC=$(CC) $(FINAL_CFLAGS)
RPS_LD=$(CC) $(FINAL_LDFLAGS)
RPS_INSTALL=$(INSTALL)


RPS_BIN=rps
RPS_OBJ=rps.o log.o config.o util.o array.o queue.o hashmap.o _string.o _signal.o upstream.o server.o \
		b64/cencode.o b64/cdecode.o murmur3/murmur3.o

%.o: %.c
	$(RPS_CC) -c $< -o $@ 

default: single


all: make-contrib make-proto $(RPS_BIN)
.PHONY: all

make-contrib:
	-(cd ../contrib && $(MAKE) $(DEPENDENCY_TARGETS)) 
.PHONY: make-contrib

make-proto:
	-(cd proto && $(MAKE))
.PHONY: make-proto

$(RPS_BIN): $(RPS_OBJ)
	$(RPS_LD) $^  -o $@ $(FINAL_LIBS)

single: make-proto $(RPS_BIN)
.PHONY: single

protoclean:
	-(cd proto && $(MAKE) clean)

clean: protoclean
	$(RM) $(RPS_BIN) *.o *.gch \.*.swp *.i b64/*.o murmur3/*.o
.PHONY: clean

distclean: clean
	-(cd ../contrib && $(MAKE) distclean)

install: 
	@mkdir -p $(INSTALL_BIN)
	$(RPS_INSTALL) $(RPS_BIN) $(INSTALL_BIN)

noopt:
	$(MAKE) OPTIMIZATION="-O0"

debug:
	$(MAKE) OPTIMIZATION="-O0" RPS_DEBUG_OPEN="-DRPS_DEBUG_OPEN"

test:
	@echo $(FINAL_CFLAGS)
	@echo $(FINAL_LIBS)
.PHONY: test


