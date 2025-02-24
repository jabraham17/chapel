# chpldbg -- a tool to facilitate "easy" debugging of chapel code

# Chapel config

ifndef CHPL_MAKE_HOME
export CHPL_MAKE_HOME=$(shell pwd)/../..
endif

CHPL_MAKE_HOST_TARGET = --target
include $(CHPL_MAKE_HOME)/make/Makefile.base

CHPL_HOME = $(shell printenv CHPL_HOME)
CHPL_HOST_PLATFORM = $(shell printenv CHPL_HOST_PLATFORM)

CXXFLAGS = -Wall -I. -I cdb/ -g

.SUFFIXES: .o .h .cxx
.cxx.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

SRCS=builtin.cxx cdb_back.cxx chpldbg.cxx
HFILE=cdb/mi_gdb.h builtin.h cdb_back.h chpldbg.h

OFILES=$(SRCS:.cxx=.o)

all: chpldbg_make

backend:
	cd cdb && $(MAKE)

chpldbg_make: $(OFILES) backend 
	g++ -o chpldbg $(OFILES) cdb/libmigdb.a
	cp chpldbg $(CHPL_HOME)/bin/$(CHPL_HOST_PLATFORM)
	cp chpldbg $(CHPL_HOME)/bin/linux64-x86_64

clean:
	rm -f $(OFILES) *.o 
	cd cdb && $(MAKE) clean
clobber: clean
	rm -f chpldbg

