# makefile for building the front end

FLTK_CONFIG=fltk-config
FLTK_FLUID=fluid

CXXFLAGS=-Wall -I. -g

.SUFFIXES: .o .h .cxx

.cxx.o:
	$$($(FLTK_CONFIG) --cxx) -c $$($(FLTK_CONFIG) --cxxflags) -g $(CXXFLAGS)\
	 -pthread	$<


SRCS=LocaleTab.cxx CommInput.cxx CommandLog.cxx 
HFILES=LocaleTab.h CommInput.h CommandLog.h 

# files generated from fluid using chpldbg_fl.fl
GENSRCS=chpldbg_fl.cxx
GENH=chpldbg_fl.h

OFILES=$(SRCS:.cxx=.o) $(GENSRCS:.cxx=.o)

all: chpldbg_fl.h $(OFILES) 

chpldbg_fl.h: chpldbg_fl.fl
	$(FLTK_FLUID) -c chpldbg_fl.fl

clean:
	rm -f $(OFILES) $(GENSRCS) $(GENH)

# dependencies
chpldbg_fl.cxx: chpldbg_fl.h  # weird dependency needed from fluid
chpldbg.o: chpldbg_fl.h
LocaleTab.o: LocaleTab.h
CommInput.o: CommInput.h
CommandLog.o: CommandLog.h
