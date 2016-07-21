programs := buf-test

all :: ${programs}

clean ::
	$(RM) ${programs}

# common DRI utils
${programs}: %: %.cc omapdrm.cc


# all packages
declared_pkgs := libdrm libdrm_omap libudev

# default packages
pkgs = ${declared_pkgs}


include common.mk

flags += -Wno-missing-field-initializers
flags += -D_FILE_OFFSET_BITS=64
flags += -fno-exceptions
LDLIBS += -lstdc++
