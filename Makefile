# Determine the operating system
OSTYPE ?=

ifeq ($(OS),Windows_NT)
  OSTYPE = windows
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Linux)
    OSTYPE = linux
  endif

  ifeq ($(UNAME_S),Darwin)
    OSTYPE = osx
  endif
endif

# Default settings (silent debug build).
LIB_EXT ?= a
BUILD_FLAGS = -mcx16 -march=native -Werror -Wall
LINKER_FLAGS =
ALL_CFLAGS = -std=gnu11
ALL_CXXFLAGS = -std=gnu++11

ifeq ($(config),release)
  BUILD_FLAGS += -O3 -DNDEBUG
  LINKER_FLAGS += -fuse-ld=gold
  DEFINES +=
else
  config = debug
  BUILD_FLAGS += -g -DDEBUG
endif

ifneq (,$(shell which llvm-config 2> /dev/null))
  LLVM_CONFIG = llvm-config
endif

ifneq (,$(shell which llvm-config-3.5 2> /dev/null))
  LLVM_CONFIG = llvm-config-3.5
endif

ifndef LLVM_CONFIG
  $(error No LLVM 3.5 installation found!)
endif

PONY_BUILD_DIR   ?= build/$(config)
PONY_SOURCE_DIR  ?= src
PONY_TEST_DIR ?= test

$(shell mkdir -p $(PONY_BUILD_DIR))

lib   := $(PONY_BUILD_DIR)
bin   := $(PONY_BUILD_DIR)
tests := $(PONY_BUILD_DIR)
obj  := $(PONY_BUILD_DIR)/obj

# Libraries. Defined as
# (1) a name and output directory
libponyc  := $(lib)
libponycc := $(lib)
libponyrt := $(lib)

# Define special case rules for a targets source files. By default
# this makefile assumes that a targets source files can be found
# relative to a parent directory of the same name in $(PONY_SOURCE_DIR).
# Note that it is possible to collect files and exceptions with
# arbitrarily complex shell commands, as long as ':=' is used
# for definition, instead of '='.
libponycc.dir := src/libponyc
libponycc.files := src/libponyc/debug/dwarf.cc
libponycc.files += src/libponyc/debug/symbols.cc
libponycc.files += src/libponyc/codegen/host.cc

libponyc.except := $(libponycc.files)
libponyc.except += src/libponyc/platform/signed.cc
libponyc.except += src/libponyc/platform/unsigned.cc
libponyc.except += src/libponyc/platform/vcvars.c

# Handle platform specific code to avoid "no symbols" warnings.
libponyrt.except =

ifneq ($(OSTYPE),windows)
  libponyrt.except += src/libponyrt/asio/iocp.c
  libponyrt.except += src/libponyrt/lang/win_except.c
endif

ifeq ($(OSTYPE),linux)
  libponyrt.except += src/libponyrt/asio/kqueue.c
endif

ifeq ($(OSTYPE),osx)
  libponyrt.except += src/libponyrt/asio/epoll.c
endif

# Third party, but requires compilation. Defined as
# (1) a name and output directory.
# (2) a list of the source files to be compiled.
libgtest := $(lib)
libgtest.dir := lib/gtest
libgtest.files := $(libgtest.dir)/gtest_main.cc $(libgtest.dir)/gtest-all.cc

libraries := libponyc libponycc libponyrt libgtest

# Third party, but prebuilt. Prebuilt libraries are defined as
# (1) a name (stored in prebuilt)
# (2) the linker flags necessary to link against the prebuilt library/libraries.
# (3) a list of include directories for a set of libraries.
# (4) a list of the libraries to link against.
llvm.ldflags := $(shell $(LLVM_CONFIG) --ldflags)
llvm.include := $(shell $(LLVM_CONFIG) --includedir)
llvm.libs    := $(shell $(LLVM_CONFIG) --libs) -lz -lcurses

prebuilt := llvm

# Binaries. Defined as
# (1) a name and output directory.
ponyc := $(bin)

binaries := ponyc

# Tests suites are directly attached to the libraries they test.
libponyc.tests  := $(tests)
libponyrt.tests := $(tests)

tests := libponyc.tests libponyrt.tests

# Define include paths for targets if necessary. Note that these include paths
# will automatically apply to the test suite of a target as well.
libponyc.include := src/common/ $(llvm.include)/
libponycc.include := src/common/ $(llvm.include)/
libponyrt.include := src/common/ src/libponyrt/

libponyc.tests.include := src/common/ src/libponyc/ lib/gtest/
libponyrt.tests.include := src/common/ src/libponyrt/ lib/gtest/

ponyc.include := src/common/
libgtest.include := lib/gtest/

# target specific build options
libponyc.options = -D__STDC_CONSTANT_MACROS
libponyc.options += -D__STDC_FORMAT_MACROS
libponyc.options += -D__STDC_LIMIT_MACROS
libponyc.options += -Wconversion -Wno-sign-conversion

libponyrt.options = -Wconversion -Wno-sign-conversion

libponycc.options = -D__STDC_CONSTANT_MACROS
libponycc.options += -D__STDC_FORMAT_MACROS
libponycc.options += -D__STDC_LIMIT_MACROS

ponyc.options = -Wconversion -Wno-sign-conversion

# Link relationships.
ponyc.links = libponyc libponycc libponyrt llvm
libponyc.tests.links = libgtest libponyc libponycc libponyrt llvm
libponyrt.tests.links = libgtest libponyrt

ifeq ($(OSTYPE),linux)
  ponyc.links += pthread dl
  libponyc.tests.links += pthread dl
  libponyrt.tests.links += pthread
endif

# Overwrite the default linker for a target.
ponyc.linker = $(CXX) #compile as C but link as CPP (llvm)

# make targets
targets := $(libraries) $(binaries) $(tests)

.PHONY: all $(targets)
all: $(targets)

# Dependencies
libponycc:
libponyrt:
libponyc: libponycc libponyrt
libponyc.tests: libponyc gtest
libponyrt.tests: libponyrt gtest
ponyc: libponyc
gtest:

# Generic make section, edit with care.
##########################################################################
#                                                                        #
# DIRECTORY: Determines the source dir of a specific target              #
#                                                                        #
# ENUMERATE: Enumerates input and output files for a specific target     #
#                                                                        #
# PICK_COMPILER: Chooses a C or C++ compiler depending on the target.    #
#                                                                        #
# CONFIGURE_LIBS: Builds a string of libraries to link for a targets     #
#                 link dependency.                                       #
#                                                                        #
# CONFIGURE_LINKER: Assembles the linker flags required for a target.    #
#                                                                        #
# EXPAND_COMMAND: Macro that expands to a proper make command for each   #
#                 target.                                                #
#                                                                        #
##########################################################################
define DIRECTORY
  $(eval sourcedir := )
  $(eval outdir := $(obj)/$(1))

  ifdef $(1).dir
    sourcedir := $($(1).dir)
  else ifneq ($$(filter $(1),$(tests)),)
    sourcedir := $(PONY_TEST_DIR)/$(subst .tests,,$(1))
    outdir := $(obj)/tests/$(subst .tests,,$(1))
  else
    sourcedir := $(PONY_SOURCE_DIR)/$(1)
  endif
endef

define ENUMERATE
  $(eval sourcefiles := )

  ifdef $(1).files
    sourcefiles := $$($(1).files)
  else
    sourcefiles := $$(shell find $$(sourcedir) -type f -name "*.c" -or -name "*.cc" | grep -v '.*/\.')
  endif

  ifdef $(1).except
    sourcefiles := $$(filter-out $($(1).except),$$(sourcefiles))
  endif
endef

define PICK_COMPILER
  $(eval compiler := $(CC))
  $(eval flags:= $(ALL_CFLAGS))

  ifneq ($$(filter $(suffix $(sourcefiles)),.cc),)
    compiler := $(CXX)
    flags := $(ALL_CXXFLAGS)
  endif
endef

define CONFIGURE_LIBS
  ifneq (,$$(filter $(1),$(prebuilt)))
    linkcmd += $($(1).ldflags)
    libs += $($(1).libs)
  else
    libs += -l$(subst lib,,$(1))
  endif
endef

define CONFIGURE_LINKER
  $(eval linker := $(compiler))
  $(eval linkcmd := $(LINKER_FLAGS) -L $(lib))
  $(eval libs :=)
  $(foreach lk,$($(1).links),$(eval $(call CONFIGURE_LIBS,$(lk))))
  linkcmd += $(libs)

  ifdef $(1).linker
    linker := $($(1).linker)
  endif
endef

define PREPARE
  $(eval $(call DIRECTORY,$(1)))
  $(eval $(call ENUMERATE,$(1)))
  $(eval $(call PICK_COMPILER,$(sourcefiles)))
  $(eval $(call CONFIGURE_LINKER,$(1)))
  $(eval objectfiles  := $(subst $(sourcedir)/,$(outdir)/,$(addsuffix .o,$(sourcefiles))))
  $(eval dependencies := $(subst .c,,$(subst .cc,,$(subst .o,.d,$(objectfiles)))))
endef

define EXPAND_OBJCMD
$(subst .c,,$(subst .cc,,$(1))): $(subst $(outdir)/,$(sourcedir)/,$(subst .o,,$(1)))
	@echo '$$(notdir $$<)'
	@mkdir -p $$(dir $$@)
	@$(compiler) -MMD -MP $(BUILD_FLAGS) $(flags) -c -o $$@ $$< $($(2).options) $(addprefix -I,$($(2).include))
endef

define EXPAND_COMMAND
$(eval $(call PREPARE,$(1)))
$(eval ofiles := $(subst .c,,$(subst .cc,,$(objectfiles))))

print_$(1):
	@echo "==== Building $(1) ($(config)) ===="

ifneq ($(filter $(1),$(libraries)),)
$(1): print_$(1) $($(1))/$(1).$(LIB_EXT)
$($(1))/$(1).$(LIB_EXT): $(ofiles)
	@echo 'Linking $(1)'
	@$(AR) -rcs $$@ $(ofiles)
else
$(1): print_$(1) $($(1))/$(1)
$($(1))/$(1): print_$(1) $(ofiles)
	@echo 'Linking $(1)'
	@$(linker) -o $$@ $(ofiles) $(linkcmd)
endif

$(foreach ofile,$(objectfiles),$(eval $(call EXPAND_OBJCMD,$(ofile),$(1))))
-include $(dependencies)
endef

$(foreach target,$(targets),$(eval $(call EXPAND_COMMAND,$(target))))

stats:
	@echo
	@echo '------------------------------'
	@echo 'Compiler and standard library '
	@echo '------------------------------'
	@echo
	@cloc --read-lang-def=pony.cloc src packages
	@echo
	@echo '------------------------------'
	@echo 'Test suite:'
	@echo '------------------------------'
	@echo
	@cloc --read-lang-def=pony.cloc test

clean:
	@rm -rf build
	@echo 'Repository cleaned.'

help:
	@echo
	@echo 'Usage: make [config=name] [target]'
	@echo
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  release"
	@echo
	@echo 'TARGETS:'
	@echo '  libponyc          Pony compiler library'
	@echo '  libponycc         Pony compiler host info and debugger support'
	@echo '  libponyrt         Pony runtime'
	@echo '  libponyc.tests    Test suite for libponyc'
	@echo '  libponyrt.tests   Test suite for libponyrt'
	@echo '  ponyc             Pony compiler executable'
	@echo
	@echo '  all               Build all of the above (default)'
	@echo '  stats             Print Pony cloc statistics'
	@echo '  clean             Delete all build files'
	@echo
