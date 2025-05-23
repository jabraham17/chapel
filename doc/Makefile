ifndef CHPL_MAKE_HOME
export CHPL_MAKE_HOME=$(shell pwd)/..
endif

ifndef CHPL_MAKE_PYTHON
export CHPL_MAKE_PYTHON := $(shell $(CHPL_MAKE_HOME)/util/config/find-python.sh)
endif

# Makefile for Chapel documentation

# for a release, change the following to 'release/' and 'x.yz' (the
# release's verion number
#
WEB_DOC_DIR=
WEB_DOC_VERSION=main

# Makefile.sphinx interfaces with sphinx build commands
include Makefile.sphinx

CHPLDEPS = $(shell $(CHPL_MAKE_PYTHON) $(CHPL_MAKE_HOME)/util/chplenv/chpl_home_utils.py --chpldeps)

CHPL2RST = $(CHPL_MAKE_PYTHON) $(CHPLDEPS) ./util/chpl2rst.py
COPY_IF_DIFFERENT = $(CHPL_MAKE_PYTHON) $(CHPL_MAKE_HOME)/util/config/update-if-different --copy


CHPL_DOC_PRIMERS_BUILD_DIR = ../build/doc/primers
CHPL_DOC_PRIMERS_RST_DIR = $(SOURCEDIR)/primers
CHPL2RSTOPTS = --output=rst --prefix=$(CHPL_DOC_PRIMERS_BUILD_DIR) --link=$(WEB_DOC_DIR)$(WEB_DOC_VERSION)

CHPL_DOC_EXAMPLES_BUILD_DIR = ../build/doc/examples
CHPL_DOC_EXAMPLES_RST_DIR = $(SOURCEDIR)/examples

CHPL_DOC_HELLO_BUILD_DIR = $(CHPL_DOC_EXAMPLES_BUILD_DIR)/hellos
CHPL_DOC_HELLO_RST_DIR = $(CHPL_DOC_EXAMPLES_RST_DIR)/hellos
CHPL2RSTOPTS_HELLO = --output=rst --prefix=$(CHPL_DOC_HELLO_BUILD_DIR) --link=$(WEB_DOC_DIR)$(WEB_DOC_VERSION)

SPEC_BUILD_DIR = ../build/doc/language/spec
SPEC_RST_DIR = $(SOURCEDIR)/language/spec

COMPILER_INTERNALS_DOXYGEN = ../build/doc/doxygen
COMPILER_INTERNALS = $(SOURCEDIR)/developer/compiler-internals

help: help-sphinx help-source

help-source:
	@echo "Source Help:"
	@echo "  docs           to invoke 'make html'"
	@echo "  checkdocs      to invoke 'make check'"
	@echo "  man-chapel     to invoke 'make man'"
	@echo "  module-docs    to invoke 'make documentation' in ../modules"
	@echo "  primers        to generate primer docs in $(SOURCEDIR)/primers/"
	@echo "  examples       to generate hellos docs in $(SOURCEDIR)/examples/"
	@echo "  symlinks       to create symlinks from ../man and ../test"
	@echo "  clean          to remove all generated files excluding ../build/doc"
	@echo "  clean-build    to remove all generated files in ../build/doc"
	@echo "  clobber        to remove all generated files"
	@echo


docs: FORCE
	@COMP_DOCS_ERROR=0 ; \
	(cd ../compiler && $(MAKE) frontend-docs) || \
	 COMP_DOCS_ERROR=1 ; \
	$(MAKE) html-release && \
	if [ $$COMP_DOCS_ERROR -ne 0 -o ! -d $(COMPILER_INTERNALS_DOXYGEN) ] ; \
	  then \
	  echo ; \
	  echo " This docs build does not include frontend library API docs" ; \
	  echo " Try 'make frontend-docs'" ; \
	fi

docs-release: FORCE
	$(MAKE) docs
	cd html && rm versionButton.php && ln -s ../versionButton.php .

man-chapel: FORCE
	$(MAKE) man


source: collect-syntax module-docs primers examples symlinks compiler-internals chplcheck-docs


collect-syntax:
	@echo
	@echo "Collecting syntax from '$(SPEC_RST_DIR)' into '$(SPEC_RST_DIR)/syntax.rst'"
	@mkdir -p $(SPEC_BUILD_DIR)
	@touch $(SPEC_BUILD_DIR)/syntax.rst
	@./util/collect-syntax.py rst/language/spec --outdir $(SPEC_BUILD_DIR)
	@$(COPY_IF_DIFFERENT) "$(SPEC_BUILD_DIR)/syntax.rst" "$(SPEC_RST_DIR)/syntax.rst"

module-docs:
	@echo
	@echo "Generating module docs from "'$(CHPL_MAKE_HOME)'"/modules/ into $(SOURCEDIR)/modules"
	@# modules/Makefile will
	@#   store module docs in doc/rst/modules and doc/rst/builtins
	@#   including meta/modules/* and meta/builtins/*
	@(cd ../modules && $(MAKE) documentation)

primers:
	@echo
	@echo "Generating primers from "'$(CHPL_MAKE_HOME)'"/test/release/examples to $(SOURCEDIR)/primers"
	@rm -rf $(CHPL_DOC_PRIMERS_BUILD_DIR)
	@mkdir -p $(CHPL_DOC_PRIMERS_BUILD_DIR)
	@#Note - this assumes that we are not in a release tar ball
	@$(CHPL2RST) $(CHPL2RSTOPTS) ../test/release/examples/primers/*.chpl
	@$(CHPL2RST) $(CHPL2RSTOPTS) ../test/release/examples/primers/*doc.chpl --codeblock
	@mkdir -p $(CHPL_DOC_PRIMERS_RST_DIR)
	@cp -f $(SOURCEDIR)/meta/primers/* $(CHPL_DOC_PRIMERS_BUILD_DIR)
	@$(COPY_IF_DIFFERENT) $(CHPL_DOC_PRIMERS_BUILD_DIR) $(CHPL_DOC_PRIMERS_RST_DIR)

examples:
	@echo
	@echo "Generating hellos from "'$(CHPL_MAKE_HOME)'"/test/release/examples to $(SOURCEDIR)/examples"
	@rm -rf $(CHPL_DOC_EXAMPLES_BUILD_DIR)
	@mkdir -p $(CHPL_DOC_EXAMPLES_BUILD_DIR)
	@$(CHPL2RST) $(CHPL2RSTOPTS_HELLO) ../examples/hello*.chpl
	@cp -f $(SOURCEDIR)/meta/examples/* $(CHPL_DOC_EXAMPLES_BUILD_DIR)
	@mkdir -p $(CHPL_DOC_EXAMPLES_RST_DIR)
	@$(COPY_IF_DIFFERENT) $(CHPL_DOC_EXAMPLES_BUILD_DIR) $(CHPL_DOC_EXAMPLES_RST_DIR)

symlinks:
	@echo
	@echo "Creating symlinks"
	@if [ ! -e $(SOURCEDIR)/usingchapel/man.rst ]; then ln -s $(CHPL_MAKE_HOME)/man/chpl.rst $(SOURCEDIR)/usingchapel/man.rst; fi
	@if [ ! -e $(SOURCEDIR)/tools/chpldoc/man.rst ]; then ln -s $(CHPL_MAKE_HOME)/man/chpldoc.rst $(SOURCEDIR)/tools/chpldoc/man.rst; fi
	@if [ ! -e $(SOURCEDIR)/tools/chplvis/examples ]; then ln -s $(CHPL_MAKE_HOME)/test/release/examples $(SOURCEDIR)/tools/chplvis/examples; fi
	@if [ ! -e $(SOURCEDIR)/users-guide/base/examples ]; then ln -s $(CHPL_MAKE_HOME)/test/release/examples $(SOURCEDIR)/users-guide/base/examples; fi
	@if [ ! -e $(SOURCEDIR)/users-guide/taskpar/examples ]; then ln -s $(CHPL_MAKE_HOME)/test/release/examples $(SOURCEDIR)/users-guide/taskpar/examples; fi
	@if [ ! -e $(SOURCEDIR)/users-guide/datapar/examples ]; then ln -s $(CHPL_MAKE_HOME)/test/release/examples $(SOURCEDIR)/users-guide/datapar/examples; fi
	@if [ ! -e $(SOURCEDIR)/users-guide/locality/examples ]; then ln -s $(CHPL_MAKE_HOME)/test/release/examples $(SOURCEDIR)/users-guide/locality/examples; fi

compiler-internals: FORCE
	@echo "Updating compiler-internals"
	rm -rf "$(COMPILER_INTERNALS)"
	@if [ -d $(COMPILER_INTERNALS_DOXYGEN) ]; then \
	  ln -s ../../../frontend/doc "$(COMPILER_INTERNALS)" ; \
	else \
	  ln -s ../meta/compiler-internals-no-doxygen "$(COMPILER_INTERNALS)" ; \
	fi

chplcheck-docs: FORCE
	@echo
	@echo "Generating docs for chplcheck"
	@(cd ../tools/chplcheck && $(MAKE) chplcheck-docs)

checkdocs: FORCE
	$(MAKE) check

%.docs: FORCE
	$(MAKE) $*.html

prunedocs: FORCE
	rm -f html/_downloads/quickReference.pdf
	if [ -d "html" ]; then \
	ln -s ../../$(SOURCEDIR)/language/quickReference.pdf html/_downloads/quickReference.pdf; \
	fi


clean: clean-source clean-build clean-build-dir clean-doxygen clean-pycache FORCE

cleanall: clean-source clean-build clean-build-dir clean-doxygen clean-pycache FORCE

clobber: clean-source clean-build clean-build-dir clean-doxygen clean-pycache FORCE
	cd ../third-party/chpl-venv/ && make clobber

clean-source: clean-module-docs clean-primers clean-examples clean-symlinks clean-collect-syntax clean-compiler-internals clean-chplcheck-docs FORCE

clean-build-dir: FORCE
	rm -rf ../build/doc

clean-compiler-internals: FORCE
	@echo
	@echo "Removing compiler-internals .rst files"
	rm -rf "$(COMPILER_INTERNALS)"

clean-collect-syntax: FORCE
	@echo
	@echo "Removing generated RST file for syntax productions"
	touch $(SPEC_RST_DIR)/syntax.rst && rm $(SPEC_RST_DIR)/syntax.rst

clean-module-docs: FORCE
	@echo
	@echo "Removing module docs that were generated"
	rm -rf $(SOURCEDIR)/modules/
	rm -rf $(SOURCEDIR)/builtins/

clean-primers: FORCE
	@echo
	@echo "Removing primers generated into $(SOURCEDIR)/primers/"
	rm -rf $(SOURCEDIR)/primers/

clean-examples: FORCE
	@echo
	@echo "Removing hellos generated into $(SOURCEDIR)/examples/"
	rm -rf $(SOURCEDIR)/examples/

clean-symlinks: FORCE
	@echo
	@echo "Removing all symbolic links"
	find $(SOURCEDIR) -type l -delete

clean-doxygen: FORCE
	rm -rf $(CHPL_MAKE_HOME)/build/doc/doxygen

clean-pycache: FORCE
	rm -rf util/__pycache__
	rm -rf $(SOURCEDIR)/__pycache__

clean-chplcheck-docs: FORCE
	cd ../tools/chplcheck && $(MAKE) clean-chplcheck-docs

FORCE:

# Disable parallel builds to prevent race conditions
.NOTPARALLEL:

