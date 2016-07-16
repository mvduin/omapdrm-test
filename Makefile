programs := buf-test

all :: ${programs}

clean ::
	$(RM) ${programs}

# common DRI utils
${programs}: %: %.o omapdrm.o


# all packages
declared_pkgs := libdrm libdrm_omap libudev

# default packages
pkgs = ${declared_pkgs}


include common.mk

flags += -D_FILE_OFFSET_BITS=64
flags += -fno-exceptions
LDLIBS += -lstdc++
