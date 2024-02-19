#
# The '/etc/dogtag' file is present on the Beagle and not on the dev machine.
# Grep returns 0 if "Debian" is found in /etc/dogtag, 1 if it isn't and 2 if /etc/dogtag doesn't exist.
# This same mechanism is used in the wrapper shell script (./k) because device tree files need to be
# loaded only on the Beagle.
#

DEBIAN_DEVSYS := $(shell grep -q -s Debian /etc/dogtag; echo $$?)
DEBIAN := 0
NOT_DEBIAN := 1
DEVSYS := 2

# double check for RPI
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
    DEBIAN_DEVSYS := $(shell grep -q -s dtparam /boot/config.txt; echo $$?)
endif

UNAME := $(shell uname)
SYS := $(shell uname -r)
SYS_MAJ := $(shell uname -r | awk '{print $1}' | cut -d. -f1)
SYS_MIN := $(shell uname -r | awk '{print $1}' | cut -d. -f2)
COMPILE_HOST := $(shell hostname)

################################
# compiler/option selection
################################

VIS_OPT =
VIS_UNOPT =

# can't be := because BUILD_DIR not defined yet
BUILD_OBJ = $(BUILD_DIR)/kiwi.bin

CFLAGS += -g -pipe -MMD -DCOMPILE_HOST=STRINGIFY\($(COMPILE_HOST)\)

#CFLAGS_UNSAFE_OPT := -fcx-limited-range -funsafe-math-optimizations
CFLAGS_UNSAFE_OPT := -funsafe-math-optimizations

APT_GET_FORCE =

# devsys
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
    ifeq ($(XC),-DXC)
        # Headers+libraries from a KiwiSDR are needed for cross-compilation with clang
        # For this mount the KiwiSDR's root file system:
        #   sshfs -o ro,follow_symlinks,auto_cache,reconnect root@kiwisdr:/ ${HOME}/mnt
        KIWI_XC_REMOTE_FS ?= ${HOME}/mnt

        HAS_PROC := $(shell test -d $(KIWI_XC_REMOTE_FS)/proc/device-tree && echo true)
        ifeq ($(HAS_PROC),true)
            BBAI_64 := $(shell cat $(KIWI_XC_REMOTE_FS)/proc/device-tree/model | grep -q -s "BeagleBone AI-64" && echo true)
            BBAI := $(shell cat $(KIWI_XC_REMOTE_FS)/proc/device-tree/model | sed "s/\(.*\)./\1x/" | grep -q -s "BeagleBone AIx" && echo true)
            RPI := $(shell cat $(KIWI_XC_REMOTE_FS)/proc/device-tree/model | grep -q -s "Raspberry Pi" && echo true)
        else
            # nfs doesn't see special files in /proc, so have to distinguish BBAI, BBAI-64 manually
            BBAI_64 := 
            BBAI := 
            RPI := $(shell cat $(KIWI_XC_REMOTE_FS)/etc/os-release | grep -q -s "raspbian" && echo true)
        endif

        BBG_BBB := $(and $(if $(BBAI_64),,true), $(if $(BBAI),,true), $(if $(RPI),,true))
        DEBIAN_VERSION := $(shell cat $(KIWI_XC_REMOTE_FS)/etc/debian_version | sed "s/\([0-9]*\)\..*/\1/")
        DEBIAN_10_AND_LATER := $(shell [ $(DEBIAN_VERSION) -ge 10 ] && echo true)
        DEBIAN_11_AND_LATER := $(shell [ $(DEBIAN_VERSION) -ge 11 ] && echo true)
        DEBIAN_12_AND_LATER := $(shell [ $(DEBIAN_VERSION) -ge 12 ] && echo true)

        CC = clang
        CPP = clang++
        CPP_FLAGS += -std=gnu++11
        CFLAGS += --target=armv7a-linux-gnueabihf
        CFLAGS += --sysroot=$(KIWI_XC_REMOTE_FS)
        CFLAGS += -I$(KIWI_XC_REMOTE_FS)/usr/include/c++/4.9
        CFLAGS += -I$(KIWI_XC_REMOTE_FS)/usr/include/arm-linux-gnueabihf/c++/4.9
        CFLAGS += -DHOST
        VIS_OPT = -Ofast
        KIWI_XC_LD ?= lld
        LDFLAGS += -fuse-ld=$(KIWI_XC_LD)
        LDFLAGS += -v
        LDFLAGS += --sysroot=$(KIWI_XC_REMOTE_FS)
        LDFLAGS += --target=armv7a-linux-gnueabihf
        LDFLAGS += -L$(KIWI_XC_REMOTE_FS)/usr/lib/arm-linux-gnueabihf
        LDFLAGS += -L$(KIWI_XC_REMOTE_FS)/usr/local/lib/
    else
        # make the compiles fast on dev system (but don't do this with cross-compiling case above)
        DEVSYS_BUT_NOT_XC = true
        OPT = 0
        CFLAGS += -DDEVSYS
        
        # platform choice when building on development machine
        BBAI_64 := 
        BBAI := 
        RPI := 
        BBG_BBB := $(and $(if $(BBAI_64),,true), $(if $(BBAI),,true), $(if $(RPI),,true))
        PLATFORM = BBG_BBB

        # stops incorrect warning for GNU %Nms / %Nm[ automatic string alloc:
        # warning: 'sscanf' may overflow; destination buffer in argument 3 has size 8, but the corresponding specifier may require size 33 [-Wfortify-source]
        # for sscanf(s, "%32m", (char **) &alloced_str_ptr)
        CFLAGS += -Wno-fortify-source

        ifeq ($(UNAME),Darwin)
            CC = clang
            CPP = clang++
            CPP_FLAGS += -std=gnu++11
        else
            # try clang on your development system (if you have it) -- it's better
            #CC = clang
            #CPP = clang++
        
            CC = gcc
            CPP = g++
        endif
    endif
endif

# Debian target
ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
    CFLAGS += -DHOST

    BBAI_64 := $(shell test -e /proc/device-tree && cat /proc/device-tree/model | grep -q -s "BeagleBone AI-64" && echo true)
    BBAI := $(shell test -e /proc/device-tree && cat /proc/device-tree/model | sed "s/\(.*\)./\1x/" | grep -q -s "BeagleBone AIx" && echo true)
    RPI := $(shell test -e /proc/device-tree && cat /proc/device-tree/model | grep -q -s "Raspberry Pi" && echo true)
    BBG_BBB := $(and $(if $(BBAI_64),,true), $(if $(BBAI),,true), $(if $(RPI),,true))

    DEBIAN_VERSION := $(shell cat /etc/debian_version | sed "s/\([0-9]*\)\..*/\1/")
    DEBIAN_10_AND_LATER := $(shell [ $(DEBIAN_VERSION) -ge 10 ] && echo true)
    DEBIAN_11_AND_LATER := $(shell [ $(DEBIAN_VERSION) -ge 11 ] && echo true)
    DEBIAN_12_AND_LATER := $(shell [ $(DEBIAN_VERSION) -ge 12 ] && echo true)

    ifeq ($(BBAI_64),true)
        ifeq ($(DEBIAN_VERSION),11)
            CC = clang-11
            CPP = clang++-11
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            CMD_DEPS_DEBIAN := /usr/bin/clang-11
        else
            # Debian 12+
            CC = clang
            CPP = clang++
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            CPP_FLAGS += -Wno-fortify-source
            CMD_DEPS_DEBIAN := /usr/bin/clang
        endif
    else ifeq ($(BBAI),true)
        ifeq ($(DEBIAN_VERSION),10)
            CC = clang-7
            CPP = clang++-7
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            CMD_DEPS_DEBIAN := /usr/bin/clang-7
        else ifeq ($(DEBIAN_VERSION),9)
            CC = clang-7
            CPP = clang++-7
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            CMD_DEPS_DEBIAN := /usr/bin/clang-7
        else
            # Debian 12+
            CC = clang
            CPP = clang++
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            CPP_FLAGS += -Wno-fortify-source
            CMD_DEPS_DEBIAN := /usr/bin/clang
        endif
    else ifeq ($(RPI),true)
        CMD_DEPS_DEBIAN := /usr/bin/clang-11
        CC = clang-11
        CPP = clang++-11
        VIS_OPT = -Ofast
        # needed for iq_display.cpp et al
        CPP_FLAGS += -std=gnu++11
        APT_GET_FORCE = --no-install-recommends --force-yes
    else ifeq ($(BBG_BBB),true)
        ifeq ($(DEBIAN_VERSION),11)
            CC = clang-11
            CPP = clang++-11
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            CMD_DEPS_DEBIAN := /usr/bin/clang-11
        else ifeq ($(DEBIAN_VERSION),10)
            CC = clang-8
            CPP = clang++-8
            #CC = clang
            #CPP = clang++
            CMD_DEPS_DEBIAN := /usr/bin/clang-8
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
        else ifeq ($(DEBIAN_VERSION),8)
            CC = clang-6.0
            CPP = clang++-6.0
            #CC = clang
            #CPP = clang++
            CMD_DEPS_DEBIAN := /usr/bin/clang-6.0
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            APT_GET_FORCE = --force-yes
        else ifeq ($(DEBIAN_VERSION),7)
            # clang 3.0 available on Debian 7.9 doesn't work
            CC = gcc
            CPP = g++
            CFLAGS += -DKIWI_DEBIAN_7
            CFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
            VIS_OPT = -O3
            # needed for iq_display.cpp et al (-std=gnu++11 isn't available on Debian 7.9)
            CPP_FLAGS += -std=gnu++0x
            APT_GET_FORCE = --force-yes
        else
            # Debian 12+
            CC = clang
            CPP = clang++
            VIS_OPT = -Ofast
            # needed for iq_display.cpp et al
            CPP_FLAGS += -std=gnu++11
            CPP_FLAGS += -Wno-fortify-source
            CMD_DEPS_DEBIAN := /usr/bin/clang
        endif
    endif

    ifeq ($(DEBIAN_VERSION),7)
        DEB := D7
    else ifeq ($(DEBIAN_VERSION),8)
        DEB := D8
    else ifeq ($(DEBIAN_VERSION),9)
        DEB := D9
    else ifeq ($(DEBIAN_VERSION),10)
        DEB := D10
    else ifeq ($(DEBIAN_VERSION),11)
        DEB := D11
    else ifeq ($(DEBIAN_VERSION),12)
        DEB := D12
    else
        BAD_DEB_DETECT := true
    endif
endif


ifeq ($(BBAI_64),true)
    ARCH := jacinto
    CPU := TDA4VM
    PLATFORMS := beaglebone beaglebone_ai64
    PLAT := BBAI_64
    PLAT_BACKUP := bbai64
    BINARY_INSTALL := true
    CFLAGS += -DMULTI_CORE
    FP_FLAGS += -mtune=cortex-a72 -mcpu=cortex-a72
    #FP_FLAGS += -mfpu=neon-vfpv4 -mfloat-abi=hard
    ARCH_DIR = arm64
    LIB_ARCH = aarch64-linux-gnu
else ifeq ($(BBAI),true)
    ARCH := sitara
    CPU := AM5729
    PLATFORMS := beaglebone beaglebone_ai
    PLAT := BBAI
    PLAT_BACKUP := bbai
    BINARY_INSTALL := true
    CFLAGS += -DMULTI_CORE
    FP_FLAGS += -mtune=cortex-a15 -mcpu=cortex-a15
    FP_FLAGS += -mfpu=neon-vfpv4 -mfloat-abi=hard
    ARCH_DIR = arm32
    LIB_ARCH = arm-linux-gnueabihf
else ifeq ($(RPI),true)
    ARCH := omap
    CPU := BCM2837
    PLATFORMS := raspberrypi
    PLAT := RPI
    PLAT_BACKUP := rpi
    CFLAGS += -DMULTI_CORE
    FP_FLAGS += -mtune=cortex-a53 -mcpu=cortex-a53
    FP_FLAGS += -mfpu=neon -mfloat-abi=hard
    ARCH_DIR = arm32
    LIB_ARCH = arm-linux-gnueabihf
else ifeq ($(BBG_BBB),true)
    ARCH := sitara
    CPU := AM3359
    PLATFORMS := beaglebone beaglebone_black
    PLAT := BBG_BBB
    PLAT_BACKUP := beaglebone-black
    BINARY_INSTALL := true
    FP_FLAGS += -mtune=cortex-a8 -mcpu=cortex-a8
    ifeq ($(DEBIAN_VERSION),7)
        FP_FLAGS += -mfpu=neon -mfloat-abi=hard
    else
        FP_FLAGS += -mfpu=neon-vfpv3 -mfloat-abi=hard
    endif
    ARCH_DIR = arm32
    LIB_ARCH = arm-linux-gnueabihf
else
    BAD_DEV_DETECT := true
endif

ifneq ($(DEVSYS_BUT_NOT_XC),true)
    CFLAGS += $(FP_FLAGS)
endif


#PROFILING = true
PROFILING = false
ifeq ($(PROFILING),true)
    CC = gcc
    CPP = g++
    CFLAGS += -pg -fprofile-arcs -ftest-coverage
    LDFLAGS += -pg -lgcov --coverage
    VIS_OPT =
    BUILD_OBJ = ./kiwi.bin
endif


# To use clang address sanitizer build with "make ASAN=1 [OPT=0] [OPT=1]" on target.
# There are shell aliases "masan", "masan0" and "masan1" for these.
# Generally, masan/masan0 results in code that is too slow, but masan1 is not over-optimized
# to the point gdb variable inspection becomes useless. So try masan1 for best results.
# Use gdb "asan" alias to set breakpoint necessary to backtrace address sanitizer errors.
#
# The leak sanitizer cannot be used with gdb. Use alias "leaks" to enable, "qleaks" to query status.
# Use admin server restart button to force server exit and leak post-processing to run.
ifeq ($(ASAN),1)
    ifeq ($(OPT),0)
        VIS_OPT = -fsanitize=address
        VIS_UNOPT = -fsanitize=address
    else ifeq ($(OPT),1)
        VIS_OPT = -O1 -fsanitize=address
        VIS_UNOPT = -O1 -fsanitize=address
    else
        VIS_OPT += -fsanitize=address
        VIS_UNOPT += -fsanitize=address
    endif

    CFLAGS += -fno-omit-frame-pointer -fno-optimize-sibling-calls
    CPP_FLAGS += -DDEBUG

    LDFLAGS += -fsanitize=address,leak
    #LDFLAGS += -v -fsanitize=address,leak
    #LDFLAGS += -fsanitize=address
endif


# static analyzer (NB: different from address sanitizer "asan")
# build on devsys or target with "make SAN=1" using alias "msan"
# uses -O0 for speedup when used on target (clang on Debian seems to catch more errors than on MacOS)
ifeq ($(SAN),1)
    CPP_FLAGS += -O0 -Werror --analyze -DKIWI_STATIC_ANALYSIS
    OPT = 0
    VIS_OPT =
endif


ifeq ($(GDB),1)
    CPP_FLAGS += -O0
    OPT = 0
    VIS_OPT =
endif
