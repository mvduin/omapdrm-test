################ common flags and rules ########################################

.DELETE_ON_ERROR:

SHELL := /bin/bash

ifndef CROSS_COMPILE
ifneq ($(g++ -dumpmachine),"arm-linux-gnueabihf")
CROSS_COMPILE := arm-linux-gnueabihf-
endif
endif

target-flags =

CXX = ${CROSS_COMPILE}g++ ${target-flags}
CC = ${CROSS_COMPILE}gcc ${target-flags}

target-arch != ${CC} -dumpmachine

ifeq "${target-arch}" "arm-linux-gnueabihf"
  target-flags += -mcpu=cortex-a15 -mfpu=neon-vfpv4 -mfloat-abi=hard -mthumb
else
  ifeq "${target-arch}" "x86_64-linux-gnu"
    ifeq "${CROSS_COMPILE}" ""
      target-flags += -march=native
    endif
  else
    $(error "unknown target: ${target-arch}")
  endif
endif

LD = ${CXX}
LDFLAGS =
LDFLAGS += -z now -z relro
LDLIBS =
flags =
flags += -funsigned-char
#flags += -fno-strict-aliasing -fwrapv
flags += -D_FILE_OFFSET_BITS=64
flags += -Wall -Wextra
#flags += -Werror
flags += -Wno-unused-parameter -Wno-unused-function
ifndef DEBUG
flags += -O3
else
flags += -Og
endif
flags += -g
CFLAGS = -std=gnu11 $(flags)
CXXFLAGS = -std=gnu++1z $(flags)
CXXFLAGS += -fno-operator-names
CPPFLAGS = -I src -I include

CXX += -fmax-errors=3

export GCC_COLORS = 1

clean ::
	${RM} *.o


################ package magic #################################################

define declare_pkg =
${pkg}_CFLAGS != pkg-config --cflags ${pkg}
${pkg}_LDLIBS != pkg-config --libs ${pkg}
endef
$(foreach pkg,${declared_pkgs}, $(eval ${declare_pkg}))

CXXFLAGS += $(foreach pkg,${pkgs},${${pkg}_CFLAGS})
LDLIBS += $(foreach pkg,${pkgs},${${pkg}_LDLIBS})


################ automatic header dependencies #################################

# a place to put them out of sight
depdir := .dep
$(shell mkdir -p ${depdir})

# generate them

dep = ${depdir}/$@.d
depfd := 4
gendep = -MMD -MQ $@ -MP -MF /dev/fd/${depfd} ${depfd}>${dep}.tmp && mv ${dep}.tmp ${dep}

%.o: %.c
	${COMPILE.c} ${OUTPUT_OPTION} $< ${gendep}

%.o: %.cc
	${COMPILE.cc} ${OUTPUT_OPTION} $< ${gendep}

# use them
-include .dep/*.d

# clean them up
clean ::
	${RM} -r ${depdir}

# fix built-in rules that bork because they think all deps are sources
%: %.o
	${LINK.o} ${^:%.h=} ${LDLIBS} ${OUTPUT_OPTION} ${gendep}

%: %.c
	${LINK.c} ${^:%.h=} ${LDLIBS} ${OUTPUT_OPTION} ${gendep}

%: %.cc
	${LINK.cc} ${^:%.h=} ${LDLIBS} ${OUTPUT_OPTION} ${gendep}


################ to check what the compiler is making of your code #############

ifdef use_clang

%.asm: %.c
	$(COMPILE.c) -S -Xclang -masm-verbose $(OUTPUT_OPTION) $< ${gendep}
%.asm: %.cc
	$(COMPILE.cc) -S -Xclang -masm-verbose $(OUTPUT_OPTION) $< ${gendep}

else

%.asm: %.c
	$(COMPILE.c) -S -g0 $(OUTPUT_OPTION) $< ${gendep}
%.asm: %.cc
	$(COMPILE.cc) -S -g0 $(OUTPUT_OPTION) $< ${gendep}

endif

clean ::
	${RM} *.asm
