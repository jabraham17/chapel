ifndef CHPL_MAKE_HOME
export CHPL_MAKE_HOME=$(shell pwd)/../..
endif

CHPL_MAKE_HOST_TARGET = --target
include $(CHPL_MAKE_HOME)/make/Makefile.base

ifeq ($(CHPL_LIB_PIC),pic)
CFLAGS += $(SHARED_LIB_CFLAGS)
endif

default: $(CHPL_MAKE_TARGET_RPMALLOC)
all: $(CHPL_MAKE_TARGET_RPMALLOC)

none:

bundled: rpmalloc

clean: FORCE
	rm -rf $(RPMALLOC_BUILD_SUBDIR)

cleanall: FORCE
	rm -rf build

clobber: FORCE
	rm -rf build install

rpmalloc: configure-rpmalloc build-rpmalloc install-rpmalloc

configure-rpmalloc: FORCE
	mkdir -p $(RPMALLOC_BUILD_DIR)

build-rpmalloc: FORCE
	cd $(RPMALLOC_BUILD_DIR) && $(CC) $(CFLAGS) -c $(RPMALLOC_SUBDIR)/rpmalloc/rpmalloc.c -o rpmalloc.o
	cd $(RPMALLOC_BUILD_DIR) && $(AR) rcs librpmalloc.a rpmalloc.o

install-rpmalloc: FORCE
	mkdir -p $(RPMALLOC_LIB_DIR)
	cp $(RPMALLOC_BUILD_DIR)/librpmalloc.a $(RPMALLOC_LIB_DIR)
	mkdir -p $(RPMALLOC_INCLUDE_DIR)
	cp $(RPMALLOC_SUBDIR)/rpmalloc/rpmalloc.h $(RPMALLOC_INCLUDE_DIR)


FORCE:

.NOTPARALLEL:
