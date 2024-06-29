VERSION_MAJ = 1
VERSION_MIN = 690

# Caution: software update mechanism depends on format of first two lines in this file

# use new binary distro mechanism
#BINARY_DISTRO := true

#
# Makefile for KiwiSDR project
#
# Copyright (c) 2014-2023 John Seamons, ZL4VO/KF6VO
#
# This Makefile can be run on both a build machine (I use a MacBook Pro) and the
# BeagleBone Black target (Debian release).
#

#
# installing FFTW:
#	to create /usr/local/lib/libfftw3f.a (the 'f' in '3f' means single precision)
#
#	Mac:
#		download the sources from fftw.org
#		./configure --enable-single
#		make
#		(sudo) make install
#
#	BeagleBone Black, Debian:
#		the Makefile automatically installs the package using apt-get
#


################################
# build environment detection
################################

include Makefile.comp.inc

# for any config-specific options/dependencies
-include ../kiwi.config/Makefile.kiwi.inc

REPO_USER := jks-prv
REPO_NAME := Beagle_SDR_GPS
REPO_GIT  := $(REPO_USER)/$(REPO_NAME).git
REPO := https://github.com/$(REPO_GIT)

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

	# enough parallel make jobs to overcome core stalls from filesystem or nfs delays
	ifeq ($(BBAI_64),true)
	    # 4 GB DRAM
		MAKE_ARGS := -j 2
	else ifeq ($(BBAI),true)
	    # 1 GB DRAM, but not all of it allocated to arm cpu by device tree
		# 2 parallel compiles can run out of memory on BBAI when kiwid also loaded
		#MAKE_ARGS := -j 2
		MAKE_ARGS :=
	else ifeq ($(RPI),true)
		# fixme: test, but presumably the 1GB (min) of DRAM on RPi 3B & 4 would support this many
		MAKE_ARGS := -j 4
	else
		ifeq ($(DEBIAN_VERSION),7)
			# Debian 7 gcc runs out of memory compiling edata_always*.cpp in parallel
			MAKE_ARGS :=
		else
			MAKE_ARGS :=
		endif
	endif
else
	# choices when building on development machine
	ifeq ($(XC),-DXC)
		# BBAI, DEBIAN_VERSION etc are taken from the mounted KiwiSDR root file system
		MAKE_ARGS := -j 7
	else
		MAKE_ARGS := -j
	endif
endif

# uncomment when using debugger so variables are not always optimized away
ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
#	OPT = 0
endif

KIWI_XC_REMOTE_FS ?= $(HOME)/mnt
KIWI_XC_HOST ?= kiwisdr
KIWI_XC_HOST_PORT ?= 22

################################
# "all" target must be first
################################
.PHONY: all
all: check_detect make_prereq
	@make $(MAKE_ARGS) build_makefile_inc
    ifeq ($(BINARY_DISTRO),true)
	    @make $(MAKE_ARGS) make_binary
    else
	    @make $(MAKE_ARGS) make_all
    endif

.PHONY: skip
skip: skip_cert_check check_detect make_prereq
	@make $(MAKE_ARGS) build_makefile_inc
	@make $(MAKE_ARGS) make_all

.PHONY: debug
debug: check_detect make_prereq
	@make $(MAKE_ARGS) DEBUG=-DDEBUG build_makefile_inc
	@make $(MAKE_ARGS) DEBUG=-DDEBUG make_all

.PHONY: check_detect
check_detect:
    ifeq ($(BAD_DEV_DETECT),true)
	    @echo "bad device detect"
	    @echo "BBAI_64 = $(BBAI_64)"
	    @echo "BBAI = $(BBAI)"
	    @echo "RPI = $(RPI)"
	    @echo "BBG_BBB = $(BBG_BBB)"
	    @echo "DEBIAN_VERSION = $(DEBIAN_VERSION)"
	    @exit -1
    endif
    ifeq ($(BAD_DEB_DETECT),true)
	    @echo "bad Debian version detect"
	    @echo "DEBIAN_VERSION = $(DEBIAN_VERSION)"
	    @exit -1
    endif

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
.PHONY: xc
ifeq ($(XC),)
    xc: 
	    @if [ ! -f $(KIWI_XC_REMOTE_FS)/ID.txt ] && \
	        [ ! -f $(KIWI_XC_REMOTE_FS)/boot/config.txt ]; then \
	        echo "ERROR: remote filesystem $(KIWI_XC_REMOTE_FS) not mounted?"; \
	        exit -1; \
	    fi
	    @make XC=-DXC $@
else
    xc: make_prereq
	    @echo KIWI_XC_HOST = $(KIWI_XC_HOST)
	    @echo KIWI_XC_HOST_PORT = $(KIWI_XC_HOST_PORT)
	    @echo KIWI_XC_REMOTE_FS = $(KIWI_XC_REMOTE_FS)
	    @echo KIWI_XC_COPY_SOURCES = $(KIWI_XC_COPY_SOURCES)
	    @make $(MAKE_ARGS) build_makefile_inc
	    @make $(MAKE_ARGS) make_all
    endif
endif


################################
# build files/directories
################################

# The built files (generated .cpp/.h files, binaries etc.) are placed outside of the source tree
# into the BUILD_DIR. This is so NFS can be used to share the sources between the
# development machine and the Kiwi Beagle. If the build files were also shared there would
# be file overwrites on one machine when you built on the other (and lots of unnecessary NFS copying
# back and forth. See the nfs and nfsup aliases in /root/.bashrc for NFS setup details.

BUILD_DIR := ../build
OBJ_DIR := $(BUILD_DIR)/obj
OBJ_DIR_O3 := $(BUILD_DIR)/obj_O3
KEEP_DIR := $(BUILD_DIR)/obj_keep
GEN_DIR := $(BUILD_DIR)/gen
TOOLS_DIR := $(BUILD_DIR)/tools
DIR_DATA = /tmp/kiwi.data

ifeq ($(OPT),0)
    OBJ_DIR_DEFAULT = $(OBJ_DIR)
else ifeq ($(OPT),1)
    OBJ_DIR_DEFAULT = $(OBJ_DIR)
else
    OBJ_DIR_DEFAULT = $(OBJ_DIR_O3)
endif

PKGS = 
PKGS_O3 = pkgs/utf8 pkgs/mongoose pkgs/jsmn pkgs/sha256 pkgs/TNT_JAMA pkgs/ant_switch

# Each extension can have an optional Makefile:
# The extension can opt-out of being included via EXT_SKIP (e.g. BBAI only, not Debian 7 etc.)
# EXT_SUBDIRS define any sub-dirs within the extension.
# EXT_DEFINES set any additional defines for the extension.
# Same for additional required libs via LIBS_DEP and LIBS.
# All of these should be appended to using "+="
EXT_SKIP =
EXT_SUBDIRS =
EXT_DEFINES =
LIBS_DEP =
LIBS =

PVT_EXT_DIR := ../extensions
PVT_EXT_DIRS_ := $(sort $(dir $(wildcard $(PVT_EXT_DIR)/*/extensions/*/)))
# remove ant_switch since we now host it internally
PVT_EXT_DIRS := $(subst ../extensions/ant_switch/extensions/ant_switch/,,$(PVT_EXT_DIRS_))
# included first to get value of OTHER_DIR before INT_EXT_DIRS definition
-include $(wildcard $(addsuffix Makefile,$(PVT_EXT_DIRS)))

ifeq ($(OTHER_DIR),)
    INT_EXT_DIRS1 = $(sort $(dir extensions/ $(wildcard extensions/*/)))
    EXT_SKIP1 = $(addsuffix /,$(addprefix extensions/,$(EXT_SKIP)))
    INT_EXT_DIRS = $(subst $(EXT_SKIP1),,$(INT_EXT_DIRS1))
else
    INT_EXT_DIRS1 = extensions/
    EXT_SKIP1 =
    INT_EXT_DIRS = extensions/
    OTHER_INC = rx/kiwi rx/CuteSDR rx/csdr rx/Teensy rx/wdsp gps \
        extensions/wspr extensions/noise_blank extensions/noise_filter
endif

# extension-specific makefiles and makefile for the extension init generator
# PVT listed before INT so they can set EXT_SKIP etc.
-include $(wildcard $(addsuffix Makefile,$(INT_EXT_DIRS)))
EXT_DIRS = $(PVT_EXT_DIRS) $(INT_EXT_DIRS)

PVT_EXTS_ := $(subst $(PVT_EXT_DIR)/,,$(wildcard $(PVT_EXT_DIR)/*))
# remove ant_switch since we now host it internally
PVT_EXTS := $(subst ant_switch,,$(PVT_EXTS_))
INT_EXTS = $(subst /,,$(subst extensions/,,$(wildcard $(INT_EXT_DIRS))))
EXTS = $(INT_EXTS) $(PVT_EXTS)

ifeq ($(OTHER_DIR),)
    GPS = gps gps/ka9q-fec gps/GNSS-SDRLIB
-include rx/Makefile
else
    GPS =
    RX = rx
endif

ifneq ($(RPI),true)
    _DIRS = pru $(PKGS)
endif
_DIR_PLATFORMS = $(addprefix platform/, $(PLATFORMS))
_DIRS_O3 += . $(PKGS_O3) platform/common $(_DIR_PLATFORMS) $(EXT_DIRS) $(EXT_SUBDIRS) \
    $(RX) $(GPS) dev ui cfg dx support net web arch/$(ARCH)

ifeq ($(OPT),0)
    DIRS = $(_DIRS) $(_DIRS_O3)
    DIRS_O3 =
else ifeq ($(OPT),1)
    DIRS = $(_DIRS) $(_DIRS_O3)
    DIRS_O3 =
else
    DIRS = $(_DIRS)
    DIRS_O3 = $(_DIRS_O3)
endif

VPATH = $(DIRS) $(DIRS_O3) $(EXT_SUBDIRS_KEEP)
I += -I$(GEN_DIR) $(addprefix -I,$(DIRS)) $(addprefix -I,$(DIRS_O3)) $(addprefix -I,$(OTHER_INC)) $(addprefix -I,$(EXT_SUBDIRS_KEEP)) -I/usr/local/include $(EXT_I)
H = $(wildcard $(addsuffix /*.h,$(DIRS))) $(wildcard $(addsuffix /*.h,$(DIRS_O3)))
CPP_F = $(wildcard $(addsuffix /*.cpp,$(DIRS))) $(wildcard $(addsuffix /*.c,$(DIRS)))
CPP_F_O3 = $(wildcard $(addsuffix /*.cpp,$(DIRS_O3))) $(wildcard $(addsuffix /*.c,$(DIRS_O3)))
CFILES_KEEP = $(wildcard $(addsuffix /*.cpp,$(EXT_SUBDIRS_KEEP)))

# remove generated files
CFILES = $(subst web/web.cpp,,$(CPP_F))
CFILES_O3 = $(subst web/web.cpp,,$(CPP_F_O3))

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	ifeq ($(XC),-DXC)
		LIBS += -lfftw3f -lutil
		DIR_CFG = /root/kiwi.config
		CFG_PREFIX =

        ifeq ($(DEBIAN_10_AND_LATER),true)
#           USE_SSL isn't compatible with gdb. So for now we revert to USE_CRYPT
            INT_FLAGS += -DUSE_CRYPT
            LIBS += -lcrypt
#            INT_FLAGS += -DUSE_SSL
#            LIBS += -lssl
        else
            INT_FLAGS += -DUSE_CRYPT
            LIBS += -lcrypt
        endif
	else
		# development machine, compile simulation version
		LIBS += -L/usr/local/lib -lfftw3f
		LIBS_DEP += /usr/local/lib/libfftw3f.a
		CMD_DEPS =
		DIR_CFG = unix_env/kiwi.config
		CFG_PREFIX = dist.
	endif

else
	# host machine, only build the FPGA-using version
	LIBS += -lfftw3f -lutil
	LIBS_DEP += /usr/lib/$(LIB_ARCH)/libfftw3f.a
	CMD_DEPS = $(CMD_DEPS_DEBIAN) /usr/sbin/avahi-autoipd /usr/bin/upnpc /usr/bin/dig /usr/bin/pgmtoppm /sbin/ethtool /usr/bin/sshpass
	CMD_DEPS += /usr/bin/killall /usr/bin/dtc /usr/bin/curl /usr/bin/wget /usr/bin/htop /usr/bin/colordiff /usr/bin/file
	DIR_CFG = /root/kiwi.config
	CFG_PREFIX =

	ifeq ($(DEBIAN_VERSION),10)
	    CMD_DEPS += /usr/bin/connmanctl
	endif

	ifeq ($(DEBIAN_11_AND_LATER),true)
	    CMD_DEPS += /usr/sbin/ipset
	else
	    CMD_DEPS += /sbin/ipset
	endif

# currently a bug where -lcrypt and -lssl can't be used together for some reason (crash at startup)
	ifeq ($(DEBIAN_10_AND_LATER),true)
#       USE_SSL isn't compatible with gdb. So for now we revert to USE_CRYPT
		INT_FLAGS += -DUSE_CRYPT
	    LIBS += -lcrypt
#		INT_FLAGS += -DUSE_SSL
#	    LIBS += -lssl
		CMD_DEPS += /usr/include/openssl/ssl.h
	else
		INT_FLAGS += -DUSE_CRYPT
	    LIBS += -lcrypt
	endif

	ifeq ($(BBAI_64),true)
		CMD_DEPS += /usr/bin/cpufreq-info
	endif

	ifeq ($(BBAI),true)
		CMD_DEPS += /usr/bin/cpufreq-info
	endif

	# -lrt required for clock_gettime() on Debian 7; see clock_gettime(2) man page
	# jq command isn't available on Debian 7
	ifeq ($(DEBIAN_VERSION),7)
		LIBS += -lrt
	else
		CMD_DEPS += /usr/bin/jq
	endif
endif


################################
# package install
################################

# install packages for needed libraries or commands
# some of these are prefixed with "-" to keep update from failing if there is damage to /var/lib/dpkg/info
ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

# runs only once per update of the .keyringN.dep filename
KEYRING := $(DIR_CFG)/.keyring4.dep
$(KEYRING):
	@echo "KEYRING.."
ifeq ($(DEBIAN_VERSION),7)
	@echo "switch to using Debian 7 (Wheezy) archive repo"
	-cp /etc/apt/sources.list /etc/apt/sources.list.orig
	-sed -e 's/ftp\.us/archive/' < /etc/apt/sources.list >/tmp/sources.list
	-mv /tmp/sources.list /etc/apt/sources.list
endif
ifeq ($(DEBIAN_VERSION),8)
	@echo "switch to using Debian 8 (Jessie) archive repo"
	-cp /etc/apt/sources.list /etc/apt/sources.list.orig
	-cp unix_env/sources.D8.new.list /etc/apt/sources.list
	-cp unix_env/sources.D8.new.list /etc/apt/
	-cp unix_env/sources.D8.upgrade.list /etc/apt/
endif
ifeq ($(DEBIAN_VERSION),9)
	@echo "switch to using Debian 9 (Stretch) archive repo"
	-cp /etc/apt/sources.list /etc/apt/sources.list.orig
	-cp unix_env/sources.D9.new.list /etc/apt/sources.list
endif
	-apt-get -y $(APT_GET_FORCE) update
	-apt-get -y $(APT_GET_FORCE) install debian-archive-keyring
	-apt-get -y $(APT_GET_FORCE) update
	@mkdir -p $(DIR_CFG)
	touch $(KEYRING)

# runs once-per-boot
INSTALL_CERTIFICATES := /tmp/.kiwi-ca-certs
$(INSTALL_CERTIFICATES):
	@echo "INSTALL_CERTIFICATES.."
	make $(KEYRING)
	-apt-get -y $(APT_GET_FORCE) install ca-certificates
	-apt-get -y $(APT_GET_FORCE) update
	touch $(INSTALL_CERTIFICATES)

.PHONY: skip_cert_check
skip_cert_check:
	touch $(INSTALL_CERTIFICATES)

/usr/lib/$(LIB_ARCH)/libfftw3f.a:
	apt-get -y $(APT_GET_FORCE) install libfftw3-dev

/usr/bin/clang:
	-apt-get -y $(APT_GET_FORCE) update
	apt-get -y $(APT_GET_FORCE) install clang

# NB not a typo: "clang-6.0" vs "clang-7"

/usr/bin/clang-6.0:
	# With D8 now archived, clang-6.0 must be obtained from the archived D9 backports.
	# But that repo makes use of an interactive script that requires noninteractive handling.
	-apt-get -y $(APT_GET_FORCE) update
	(UCF_FORCE_CONFOLD=1 DEBIAN_FRONTEND=noninteractive apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" -y $(APT_GET_FORCE) install clang-6.0)

/usr/bin/clang-7:
	-apt-get -y $(APT_GET_FORCE) update
	apt-get -y $(APT_GET_FORCE) install clang-7

/usr/bin/clang-8:
	-apt-get -y $(APT_GET_FORCE) update
	apt-get -y $(APT_GET_FORCE) install clang-8

/usr/bin/clang-11:
	-apt-get -y $(APT_GET_FORCE) update
	apt-get -y $(APT_GET_FORCE) install clang-11

/usr/bin/curl:
	-apt-get -y $(APT_GET_FORCE) install curl

/usr/bin/wget:
	-apt-get -y $(APT_GET_FORCE) install wget

/usr/bin/htop:
	-apt-get -y $(APT_GET_FORCE) install htop

/usr/bin/colordiff:
	-apt-get -y $(APT_GET_FORCE) install colordiff

/usr/sbin/avahi-autoipd:
	-apt-get -y $(APT_GET_FORCE) install avahi-daemon avahi-utils libnss-mdns avahi-autoipd

/usr/bin/upnpc:
	-apt-get -y $(APT_GET_FORCE) install miniupnpc

/usr/bin/dig:
	-apt-get -y $(APT_GET_FORCE) install dnsutils

/usr/bin/pgmtoppm:
	-apt-get -y $(APT_GET_FORCE) install netpbm

/sbin/ethtool:
	-apt-get -y $(APT_GET_FORCE) install ethtool

/usr/bin/sshpass:
	-apt-get -y $(APT_GET_FORCE) install sshpass

/usr/bin/killall:
	-apt-get -y $(APT_GET_FORCE) install psmisc

/usr/bin/dtc:
	-apt-get -y $(APT_GET_FORCE) install device-tree-compiler

/usr/bin/file:
	-apt-get -y $(APT_GET_FORCE) install file

ifeq ($(DEBIAN_VERSION),10)
    /usr/bin/connmanctl:
	    -apt-get -y $(APT_GET_FORCE) install connman
endif

ifeq ($(DEBIAN_10_AND_LATER),true)
    /usr/include/openssl/ssl.h:
	    -apt-get -y install openssl libssl1.1 libssl-dev
endif

ifeq ($(DEBIAN_11_AND_LATER),true)
    /usr/sbin/ipset:
	    -apt-get -y install ipset
else
    /sbin/ipset:
	    -apt-get -y $(APT_GET_FORCE) install ipset
endif

ifeq ($(BBAI_64),true)
    /usr/bin/cpufreq-info:
	    -apt-get -y install cpufrequtils
endif

ifeq ($(BBAI),true)
    /usr/bin/cpufreq-info:
	    -apt-get -y install cpufrequtils
endif

ifneq ($(DEBIAN_VERSION),7)
    /usr/bin/jq:
	    -apt-get -y $(APT_GET_FORCE) install jq
endif

endif


################################
# flags
################################

VERSION := -DVERSION_MAJ=$(VERSION_MAJ) -DVERSION_MIN=$(VERSION_MIN)
VER := v$(VERSION_MAJ).$(VERSION_MIN)
V := -Dv$(VERSION_MAJ)_$(VERSION_MIN)

INT_FLAGS += $(VERSION) -DKIWI -DKIWISDR
INT_FLAGS += -DARCH_$(ARCH) -DCPU_$(CPU) -DARCH_CPU=$(CPU) -DARCH_CPU_S=STRINGIFY\($(CPU)\) $(addprefix -DPLATFORM_,$(PLATFORMS))
INT_FLAGS += -DARCH_DIR=STRINGIFY\($(ARCH_DIR)\)
INT_FLAGS += -DDIR_CFG=STRINGIFY\($(DIR_CFG)\) -DDIR_DATA=STRINGIFY\($(DIR_DATA)\) -DCFG_PREFIX=STRINGIFY\($(CFG_PREFIX)\)
INT_FLAGS += -DBUILD_DIR=STRINGIFY\($(BUILD_DIR)\) -DREPO_NAME=STRINGIFY\($(REPO_NAME)\)  -DREPO_GIT=STRINGIFY\($(REPO_GIT)\)


################################
# dependencies
################################

#SRC_DEPS = Makefile
SRC_DEPS = 
BIN_DEPS = KiwiSDR.rx4.wf4.bit KiwiSDR.rx8.wf2.bit KiwiSDR.rx3.wf3.bit KiwiSDR.rx14.wf0.bit
#BIN_DEPS = 
DEVEL_DEPS = $(OBJ_DIR_DEFAULT)/web_devel.o $(KEEP_DIR)/edata_always.o $(KEEP_DIR)/edata_always2.o
EMBED_DEPS = $(OBJ_DIR_DEFAULT)/web_embed.o $(OBJ_DIR)/edata_embed.o $(KEEP_DIR)/edata_always.o $(KEEP_DIR)/edata_always2.o
EXTS_DEPS = $(OBJ_DIR)/ext_init.o

# these MUST be run by single-threaded make before use of -j in sub makes
GEN_ASM = $(GEN_DIR)/kiwi.gen.h verilog/kiwi.gen.vh
GEN_OTHER_ASM = $(GEN_DIR)/other.gen.h verilog/other.gen.vh
OUT_ASM = $(GEN_DIR)/kiwi.aout
GEN_VERILOG = $(addprefix verilog/rx/,cic_rx1_12k.vh cic_rx1_20k.vh cic_rx2_12k.vh cic_rx2_20k.vh cic_rx3_12k.vh cic_rx3_20k.vh cic_wf1.vh cic_wf2.vh)
GEN_NOIP2 = $(GEN_DIR)/noip2
SUB_MAKE_DEPS = $(INSTALL_CERTIFICATES) $(GEN_DIR) $(CMD_DEPS) $(LIBS_DEP) $(GEN_ASM) $(GEN_OTHER_ASM) $(GEN_OTHER) $(OUT_ASM) $(GEN_VERILOG) $(GEN_NOIP2)

.PHONY: make_prereq
make_prereq: DISABLE_WS $(SUB_MAKE_DEPS)
	@echo "make_prereq DONE"

.PHONY: make_all
make_all: $(BUILD_DIR)/kiwi.bin
	@echo "make_all DONE"

PLAT_KIWI_BIN := bin/kiwi_$(VER)_$(PLAT).bin
HAS_KIWI_BIN := $(shell test -x $(PLAT_KIWI_BIN) && echo true)

.PHONY: make_binary
make_binary:
    ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	    @make make_all
    else
        ifeq ($(BINARY_INSTALL),true)
	        @echo "================"
	        @echo "make_binary: $(PLAT_KIWI_BIN)"
            ifeq ($(HAS_KIWI_BIN),true)
	            @echo "   => exists: not compiling from sources"
	            @echo "   => cp $(PLAT_KIWI_BIN) $(BUILD_DIR)/kiwi.bin"
	            @cp $(PLAT_KIWI_BIN) $(BUILD_DIR)/kiwi.bin
	            @echo "================"
            else
	            @echo "   => doesn't exist: compiling from sources"
	            @echo "================"
	            @make $(MAKE_ARGS) make_all
	            @echo "================"
	            @echo "   => cp $(BUILD_DIR)/kiwi.bin $(PLAT_KIWI_BIN)"
	            @echo "================"
	            @cp $(BUILD_DIR)/kiwi.bin $(PLAT_KIWI_BIN)
            endif
        else
	        @make $(MAKE_ARGS) make_all
        endif
    endif
	@echo "make_binary DONE"

.PHONY: force
force: make_prereq
	rm -f $(PLAT_KIWI_BIN) $(PLAT_KIWID_BIN)
	@make $(MAKE_ARGS) build_makefile_inc
	@echo "================"
	@echo "make force"
	@make $(MAKE_ARGS) make_binary


################################
# build included makefile
################################

MF_INC := $(GEN_DIR)/Makefile.inc
START_MF_INC = | tee $(MF_INC) 2>&1
APPEND_MF_INC = | tee -a $(MF_INC) 2>&1

.PHONY: build_makefile_inc
build_makefile_inc:
# consolidate flags into indirect Makefile since Make will be re-invoked
	@echo "----------------"
	@echo "building" $(MF_INC)
	@echo $(VER)
    ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
	    @echo "Debian $(DEBIAN_VERSION)"
    endif
	@echo PROJECT = $(PROJECT)
	@echo ARCH = $(ARCH)
	@echo CPU = $(CPU)
	@echo PLAT = $(PLAT)
	@echo PLATFORMS = $(PLATFORMS)
	@echo DEBUG = $(DEBUG)
	@echo GDB = $(GDB)
	@echo XC = $(XC)
	@echo
#
	@echo $(I) $(START_MF_INC)
#
	@echo $(APPEND_MF_INC)
	@echo $(CFLAGS) $(APPEND_MF_INC)
#
	@echo $(APPEND_MF_INC)
	@echo $(EXT_DEFINES) $(APPEND_MF_INC)
#
	@echo $(APPEND_MF_INC)
	@echo $(INT_FLAGS) $(APPEND_MF_INC)
#
	@echo "----------------"


################################
# Makefile dependencies
################################

# dependence on VERSION_{MAJ,MIN}
MAKEFILE_DEPS = main.cpp
MF_FILES = $(addsuffix .o,$(basename $(notdir $(MAKEFILE_DEPS))))
MF_OBJ = $(addprefix $(OBJ_DIR)/,$(MF_FILES))
MF_O3 = $(wildcard $(addprefix $(OBJ_DIR_O3)/,$(MF_FILES)))
$(MF_OBJ) $(MF_O3): Makefile


################################
# PRU (not currently used)
################################
PASM_INCLUDES = $(wildcard pru/pasm/*.h)
PASM_SOURCE = $(wildcard pru/pasm/*.c)
pas: $(PASM_INCLUDES) $(PASM_SOURCE) Makefile
	$(CC) -Wall -D_UNIX_ -I./pru/pasm $(PASM_SOURCE) -o pas

pru/pru_realtime.bin: pas pru/pru_realtime.p pru/pru_realtime.h pru/pru_realtime.hp
	(cd pru; ../pas -V3 -b -L -l -D_PASM_ -D$(SETUP) pru_realtime.p)


################################
# FPGA embedded CPU
################################

# OTHER_DIR set in Makefile of an extension wanting USE_OTHER FPGA framework
ifeq ($(OTHER_DIR),)
    PROJECT = "KiwiSDR"
else
    OTHER_DIR2 = -x $(OTHER_DIR)
    OTHER_CONFIG = $(subst ../../,../,$(OTHER_DIR)/other.config)
endif

$(GEN_ASM): kiwi.config verilog/kiwi.inline.vh $(wildcard e_cpu/asm/*)
	(cd e_cpu; make OTHER_DIR="$(OTHER_DIR2)")
$(GEN_OTHER_ASM): $(OTHER_CONFIG) e_cpu/other.config $(wildcard e_cpu/asm/*)
	(cd e_cpu; make gen_other OTHER_DIR="$(OTHER_DIR2)")
$(OUT_ASM): $(wildcard e_cpu/*.asm)
	(cd e_cpu; make no_gen OTHER_DIR="$(OTHER_DIR2)")
asm_binary:
	(cd e_cpu; make binary OTHER_DIR="$(OTHER_DIR2)")
asm_debug:
	(cd e_cpu; make debug OTHER_DIR="$(OTHER_DIR2)")
asm_stat:
	(cd e_cpu; make stat OTHER_DIR="$(OTHER_DIR2)")



################################
# noip.com DDNS DUC
################################
$(GEN_NOIP2): pkgs/noip2/noip2.c
	(cd pkgs/noip2; make)


################################
# web server content
################################

#
# How the file optimization strategy works:
#
# FIXME
#	talk about: optim website used, NFS_READ_ONLY, EDATA_DEVEL bypass mode (including -fopt argument)
#	x should not be embedding version # in min file, but adding on demand like plain file
#	what to do about version number in .gz files though?
#	no "-zip" for html files because of possible %[] substitution (replace %[] substitutions)
#
# TODO
#	x concat embed
#	EDATA_ALWAYS/2
#	x jpg/png crush in file_optim
#	x clean target that removes .min .gz
#
# CHECK
#	does google analytics html head insert still work?
#	x update from v1.2 still works?
#	mobile works okay?
#
#
# 1) file_optim program is used to create minified and possibly gzipped versions of web server
#	content files (e.g. .js .css .html .jpg .png) Because the optimized versions of individual
#	files are included in the distro the minimization process, which uses an external website and
#	is very time consuming, is not performed by individual Kiwis as part of the update process.
#	But file_optim is run as part of the standard Makefile rules to account for any changes a user
#	may make as part of their own development (i.e. so a make install will continue to work for them)
#
# 2) Web server code has been modified to lookup optimized alternatives first, i.e. a reference to
#	xxx.js will cause lookups in order to: xxx.min.js.gz, xxx.min.js then finally xxx.js
#
# 3) Packaging of files. All files are left available individually to support EDATA_DEVEL development.
#	But for EDATA_EMBED production the minified versions of many files (typically .js and .css) are
#	concatenated together and gzipped as a single package file.
#
# 4) There are special cases. All the mapping-related files are packaged together so they can be
#	loaded on-demand by the TDoA extension and admin GPS tab. Extension files are not packaged so they
#	can be loaded on-demand.
#
#
# constituents of edata_embed:
#	extensions/*/*.min.{js,css}[.gz]
#	kiwi/{admin,mfg}.min.html
#	kiwi/{admin,admin_sdr,mfg}.min.js[.gz]	don't package: conflict with each other
#	kiwi/kiwi_js_load.min.js				don't package: used by dynamic loader
#	kiwisdr.min.{js,css}.gz					generated package: font-awesome, text-security, w3, w3-ext, openwebrx, kiwi
#	openwebrx/{index}.min.html
#	openwebrx/robots.txt
#	pkgs/js/*.min.js[.gz]
#	pkgs/xdLocalStorage/*
#	pkgs_maps/pkgs_maps.min.{js,css}[.gz]	generated package
#
#
# DANGER: old $(TOOLS_DIR)/files_optim in v1.289 prints "panic: argc" when given the args presented
# from the v1.290 Makefile when invoked by the Makefile inline $(shell ...)
# So in v1.290 change name of utility to "file_optim" (NB: "file" not "files") so old files_optim will not be invoked.
# This is an issue because a download & make clean of a new version doesn't remove the old $(TOOLS_DIR)
#

# files optimizer
FILE_OPTIM = $(TOOLS_DIR)/file_optim
FILE_OPTIM_SRC = tools/file_optim.cpp 

$(FILE_OPTIM): $(FILE_OPTIM_SRC)
#	use $(I) here, not full $(MF_INC)
	@echo "$(CC) ... -g $(FILE_OPTIM_SRC) -o $@"
	@$(CC) $(I) -g $(FILE_OPTIM_SRC) -o $@

-include $(wildcard web/*/Makefile)
-include $(wildcard web/extensions/*/Makefile)
-include web/Makefile

# NB: $(FILE_OPTIM) *MUST* be here so "make install" builds EDATA_EMBED properly when NFS_READ_ONLY == yes
#EDATA_DEP = web/kiwi/Makefile web/openwebrx/Makefile web/pkgs/Makefile web/extensions/Makefile $(wildcard extensions/*/Makefile) $(FILE_OPTIM)
EDATA_DEP = web/kiwi/Makefile web/openwebrx/Makefile web/pkgs/Makefile web/extensions/Makefile $(FILE_OPTIM)

.PHONY: foptim_gen foptim_list

# NEVER let target Kiwis contact external optimization site via foptim_gen.
# If customers are developing they need to do a "make install" on a development machine
# OR a "make fopt/foptim" on the Kiwi to explicitly build the optimized files (but only if not NFS_READ_ONLY)

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
    FOPTIM_DEP := foptim_gen
    foptim_gen: foptim_files_embed foptim_ext foptim_files_maps
	    @echo
else
    FOPTIM_DEP :=
endif

ifeq ($(NFS_READ_ONLY),yes)
    fopt foptim:
	    @echo "can't do fopt/foptim because of NFS_READ_ONLY=$(NFS_READ_ONLY)"
	    @echo "(assumed foptim_gen performed on development machine with a make install)"
else
    fopt foptim: foptim_files_embed foptim_ext foptim_files_maps
endif

foptim_list: loptim_embed loptim_ext loptim_maps

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
    CLEAN_MIN_GZ_2 = $(wildcard $(CLEAN_MIN_GZ))
.PHONY: foptim_clean clean_min clean_gz
    
    ifeq ($(CLEAN_MIN_GZ_2),)
        foptim_clean clean_min clean_gz: $(TOOLS_DIR) roptim_embed roptim_ext roptim_maps
	        @echo "nothing to clean"
    else
        foptim_clean clean_min clean_gz: $(TOOLS_DIR) roptim_embed roptim_ext roptim_maps
	        @echo "removing:"
	        @-ls -la $(CLEAN_MIN_GZ_2)
	        @-rm $(CLEAN_MIN_GZ_2)
    endif
endif

FILES_EMBED_SORTED_NW = $(sort $(EMBED_NW) $(EXT_EMBED_NW) $(PKGS_MAPS_EMBED_NW))
FILES_ALWAYS_SORTED_NW = $(sort $(FILES_ALWAYS))
FILES_ALWAYS2_SORTED_NW = $(sort $(FILES_ALWAYS2))

EDATA_EMBED = $(GEN_DIR)/edata_embed.cpp
EDATA_ALWAYS = $(GEN_DIR)/edata_always.cpp
EDATA_ALWAYS2 = $(GEN_DIR)/edata_always2.cpp

$(EDATA_EMBED): $(EDATA_DEP) $(addprefix web/,$(FILES_EMBED_SORTED_NW))
	(cd web; perl mkdata.pl edata_embed $(FILES_EMBED_SORTED_NW) >../$(EDATA_EMBED))

$(EDATA_ALWAYS): $(EDATA_DEP) $(addprefix web/,$(FILES_ALWAYS_SORTED_NW))
	(cd web; perl mkdata.pl edata_always $(FILES_ALWAYS_SORTED_NW) >../$(EDATA_ALWAYS))

$(EDATA_ALWAYS2): $(EDATA_DEP) $(addprefix web/,$(FILES_ALWAYS2_SORTED_NW))
	(cd web; perl mkdata.pl edata_always2 $(FILES_ALWAYS2_SORTED_NW) >../$(EDATA_ALWAYS2))


################################
# vars
################################
.PHONY: vars
vars: make_prereq
	make $(MAKE_ARGS) make_vars

.PHONY: make_vars
make_vars: check_detect
	@echo version $(VER)
	@echo
	@echo UNAME = $(UNAME)
	@echo DEBIAN_DEVSYS = $(DEBIAN_DEVSYS)
	@echo DEBIAN_VERSION = $(DEBIAN_VERSION)
	@echo DEBIAN_10_AND_LATER = $(DEBIAN_10_AND_LATER)
	@echo DEBIAN_11_AND_LATER = $(DEBIAN_11_AND_LATER)
	@echo DEBIAN_12_AND_LATER = $(DEBIAN_12_AND_LATER)
	@echo
	@echo BBAI_64 = $(BBAI_64)
	@echo BBAI = $(BBAI)
	@echo RPI = $(RPI)
	@echo BBG_BBB = $(BBG_BBB)
	@echo
	@echo PROJECT = $(PROJECT)
	@echo ARCH = $(ARCH)
	@echo CPU = $(CPU)
	@echo PLAT = $(PLAT)
	@echo PLATFORMS = $(PLATFORMS)
	@echo PLATS_BIN_RELEASE = $(PLATS_BIN_RELEASE)
	@echo PLAT_BACKUP = $(PLAT_BACKUP)
	@echo
	@echo REPO_USER = $(REPO_USER)
	@echo REPO_GIT = $(REPO_GIT)
	@echo REPO = $(REPO)
	@echo
	@echo BUILD_DIR = $(BUILD_DIR)
	@echo OTHER_DIR = $(OTHER_DIR)
	@echo EXISTS_OTHER_BITFILE = $(shell test -f $(V_DIR)/KiwiSDR.other.bit && echo true)
	@echo OBJ_DIR = $(OBJ_DIR)
	@echo OBJ_DIR_O3 = $(OBJ_DIR_O3)
	@echo OBJ_DIR_DEFAULT = $(OBJ_DIR_DEFAULT)
	@echo CMD_DEPS = $(CMD_DEPS)
	@echo OPT = $(OPT)
	@echo CFLAGS = $(CFLAGS)
	@echo CPP_FLAGS = $(CPP_FLAGS)
	@echo
	@echo DEPS = $(OBJECTS:.o=.d)
	@echo
	@echo SRC_DEPS = $(SRC_DEPS)
	@echo
	@echo BIN_DEPS = $(BIN_DEPS)
	@echo
	@echo LIBS_DEP = $(LIBS_DEP)
	@echo
	@echo SUB_MAKE_DEPS = $(SUB_MAKE_DEPS)
	@echo
	@echo DTS_DEP_SRC = $(DTS_DEP_SRC)
	@echo DTS_DEP_SRC2 = $(DTS_DEP_SRC2)
	@echo DTS_DEP_DST = $(DTS_DEP_DST)
	@echo DTS_DEP_DST2 = $(DTS_DEP_DST2)
	@echo
	@echo GEN_ASM = $(GEN_ASM)
	@echo
	@echo GEN_OTHER_ASM = $(GEN_OTHER_ASM)
	@echo
	@echo FILES_EMBED = $(FILES_EMBED)
	@echo
	@echo FILES_EXT = $(FILES_EXT)
	@echo
	@echo FILES_ALWAYS = $(FILES_ALWAYS)
	@echo
	@echo FILES_ALWAYS2 = $(FILES_ALWAYS2)
	@echo
	@echo EXT_SKIP = $(EXT_SKIP)
	@echo
	@echo EXT_SKIP1 = $(EXT_SKIP1)
	@echo
	@echo INT_EXT_DIRS1 = $(INT_EXT_DIRS1)
	@echo
	@echo INT_EXT_DIRS = $(INT_EXT_DIRS)
	@echo
	@echo PVT_EXT_DIRS = $(PVT_EXT_DIRS)
	@echo
	@echo EXT_SUBDIRS = $(EXT_SUBDIRS)
	@echo
	@echo EXT_DIRS = $(EXT_DIRS)
	@echo
	@echo PVT_EXTS = $(PVT_EXTS)
	@echo
	@echo INT_EXTS = $(INT_EXTS)
	@echo
	@echo EXTS = $(EXTS)
	@echo
	@echo DIRS = $(DIRS)
	@echo
	@echo DIRS_O3 = $(DIRS_O3)
	@echo
	@echo VPATH = $(VPATH)
	@echo
	@echo I = $(I)
	@echo
	@echo CFILES = $(CFILES)
	@echo
	@echo CFILES_O3 = $(CFILES_O3)
	@echo
	@echo CFILES_KEEP = $(CFILES_KEEP)
	@echo
	@echo OBJECTS = $(OBJECTS)
	@echo
	@echo O3_OBJECTS = $(O3_OBJECTS)
	@echo
	@echo KEEP_OBJECTS = $(KEEP_OBJECTS)
	@echo
	@echo EXT_OBJECTS = $(EXT_OBJECTS)
	@echo
	@echo MF_FILES = $(MF_FILES)
	@echo
	@echo MF_OBJ = $(MF_OBJ)
	@echo
	@echo MF_O3 = $(MF_O3)
	@echo
	@echo PKGS = $(PKGS)

.PHONY: makefiles
makefiles:
	@echo
	@echo $(MF_INC)
	@cat $(MF_INC)

.PHONY: build_log blog
build_log blog:
	@-tail -n 500 -f /root/build.log


################################
# general build rules
################################

CSRC = $(notdir $(CFILES))
OBJECTS1 = $(CSRC:%.c=$(OBJ_DIR)/%.o)
OBJECTS = $(OBJECTS1:%.cpp=$(OBJ_DIR)/%.o)

CSRC_O3 = $(notdir $(CFILES_O3))
O3_OBJECTS1 = $(CSRC_O3:%.c=$(OBJ_DIR_O3)/%.o)
O3_OBJECTS = $(O3_OBJECTS1:%.cpp=$(OBJ_DIR_O3)/%.o)

CSRC_KEEP = $(notdir $(CFILES_KEEP))
KEEP_OBJECTS1 = $(CSRC_KEEP:%.c=$(KEEP_DIR)/%.o)
KEEP_OBJECTS = $(KEEP_OBJECTS1:%.cpp=$(KEEP_DIR)/%.o)

ALL_OBJECTS = $(OBJECTS) $(O3_OBJECTS) $(KEEP_OBJECTS)


# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)
-include $(O3_OBJECTS:.o=.d)
-include $(KEEP_OBJECTS:.o=.d)
-include $(DEVEL_DEPS:.o=.d)
-include $(EMBED_DEPS:.o=.d)
-include $(EXTS_DEPS:.o=.d)

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

    # FIXME: isn't there a better way to do this in GNU make?

    EXISTS_RX4_WF4 := $(shell test -f KiwiSDR.rx4.wf4.bit && echo true)
    ifeq ($(EXISTS_RX4_WF4),true)
    else
        KiwiSDR.rx4.wf4.bit:
    endif

    EXISTS_RX8_WF2 := $(shell test -f KiwiSDR.rx8.wf2.bit && echo true)
    ifeq ($(EXISTS_RX8_WF2),true)
    else
        KiwiSDR.rx8.wf2.bit:
    endif

    EXISTS_RX3_WF3 := $(shell test -f KiwiSDR.rx3.wf3.bit && echo true)
    ifeq ($(EXISTS_RX3_WF3),true)
    else
        KiwiSDR.rx3.wf3.bit:
    endif

    EXISTS_RX14_WF0 := $(shell test -f KiwiSDR.rx14.wf0.bit && echo true)
    ifeq ($(EXISTS_RX14_WF0),true)
    else
        KiwiSDR.rx14.wf0.bit:
    endif

    EXISTS_OTHER := $(shell test -f KiwiSDR.other.bit && echo true)
    ifeq ($(EXISTS_OTHER),true)
        OTHER_DEP = KiwiSDR.other.bit
    else
        OTHER_DEP =
    endif
endif

#
# IMPORTANT
#
# By not including $(FOPTIM_DEP) in the dependency list for the development build target $(BUILD_DIR)/kiwi.bin
# the external optimization site won't be called all the time as incremental changes are made to
# the js/css/html/image files.
#
# But this means a "make install" must be performed on the *development machine* prior to installation
# on targets or before uploading as a software release.
# Previously doing a "make install" on the development machine made no sense and was flagged as an error.
#

$(BUILD_DIR)/kiwi.bin: $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(ALL_OBJECTS) $(BIN_DEPS) $(DEVEL_DEPS) $(EXTS_DEPS)
ifneq ($(SAN),1)
    ifeq ($(KIWI_SKIP_LINK),true)
	    @echo "DEVSYS: SKIP OF kiwi.bin LINK STEP"
	    touch $@
    else
	    @echo $(CPP) $(LDFLAGS) "..." $(DEVEL_DEPS) $(EXTS_DEPS) $(LIBS) -o $(BUILD_OBJ)
	    @$(CPP) $(LDFLAGS) $(ALL_OBJECTS) $(DEVEL_DEPS) $(EXTS_DEPS) $(LIBS) -o $(BUILD_OBJ)
    endif
else
	@echo loader skipped for static analysis
endif

$(BUILD_DIR)/kiwid.bin: $(FOPTIM_DEP) $(OTHER_DEP) $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(ALL_OBJECTS) $(BIN_DEPS) $(EMBED_DEPS) $(EXTS_DEPS)
ifneq ($(SAN),1)
    ifeq ($(KIWI_SKIP_LINK),true)
	    @echo "DEVSYS: SKIP OF KIWID.BIN LINK STEP"
	    touch $@
    else
	    @echo $(CPP) $(LDFLAGS) "..." $(EMBED_DEPS) $(EXTS_DEPS) $(LIBS) -o $@
	    @$(CPP) $(LDFLAGS) $(ALL_OBJECTS) $(EMBED_DEPS) $(EXTS_DEPS) $(LIBS) -o $@
    endif
else
	@echo loader skipped for static analysis
endif

# auto generation of dependency info, see:
#	http://scottmcpeak.com/autodepend/autodepend.html
#	http://make.mad-scientist.net/papers/advanced-auto-dependency-generation
df = $(basename $@)
POST_PROCESS_DEPS = \
	@mv -f $(df).d $(df).d.tmp; \
	sed -e 's|.*:|$(df).o:|' < $(df).d.tmp > $(df).d; \
	sed -e 's/.*://' -e 's/\\$$//' < $(df).d.tmp | fmt -1 | \
	sed -e 's/^ *//' -e 's/$$/:/' >> $(df).d; \
	rm -f $(df).d.tmp


# special
OPTS_VIS_UNOPT = $(strip $(V) $(DEBUG) $(XC) $(VIS_UNOPT))
OPTS_VIS_OPT = $(strip $(V) $(DEBUG) $(XC) $(VIS_OPT))

$(OBJ_DIR_DEFAULT)/web_devel.o: web/web.cpp config.h
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) $(CPP_FLAGS) -DEDATA_DEVEL -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_DEFAULT)/web_embed.o: web/web.cpp config.h
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) $(CPP_FLAGS) -DEDATA_EMBED -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/edata_embed.o: $(EDATA_EMBED)
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(KEEP_DIR)/edata_always.o: $(EDATA_ALWAYS)
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(KEEP_DIR)/edata_always2.o: $(EDATA_ALWAYS2)
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/ext_init.o: $(GEN_DIR)/ext_init.cpp
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)


# .c

$(OBJ_DIR)/%.o: %.c $(SRC_DEPS)
    ifeq ($(ASM_OUT),1)
	    $(CC) $(OPTS_VIS_OPT) @$(MF_INC) -S -o $(subst .o,.S,$@) $<
    endif
#	$(CC) -x c $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
	$(CC) $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_O3)/%.o: %.c $(SRC_DEPS)
    ifeq ($(ASM_OUT),1)
	    $(CC) $(OPTS_VIS_OPT) @$(MF_INC) -S -o $(subst .o,.S,$@) $<
    endif
	$(CC) $(OPTS_VIS_OPT) @$(MF_INC) -c -o $@ $<
	$(POST_PROCESS_DEPS)


# .cpp $(CFLAGS_UNSAFE_OPT)

$(OBJ_DIR_O3)/search.o: search.cpp $(SRC_DEPS)
    ifeq ($(ASM_OUT),1)
	    $(CPP) $(OPTS_VIS_OPT) $(CFLAGS_UNSAFE_OPT) @$(MF_INC) $(CPP_FLAGS) -S -o $(subst .o,.S,$@) $<
    endif
	$(CPP) $(OPTS_VIS_OPT) $(CFLAGS_UNSAFE_OPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_O3)/simd.o: simd.cpp $(SRC_DEPS)
    ifeq ($(ASM_OUT),1)
	    $(CPP) $(OPTS_VIS_OPT) $(CFLAGS_UNSAFE_OPT) @$(MF_INC) $(CPP_FLAGS) -S -o $(subst .o,.S,$@) $<
    endif
	$(CPP) $(OPTS_VIS_OPT) $(CFLAGS_UNSAFE_OPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)


# .cpp

$(OBJ_DIR)/%.o: %.cpp $(SRC_DEPS)
    ifeq ($(ASM_OUT),1)
	    $(CPP) $(OPTS_VIS_OPT) @$(MF_INC) $(CPP_FLAGS) -S -o $(subst .o,.S,$@) $<
    endif
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(KEEP_DIR)/%.o: %.cpp $(SRC_DEPS)
	$(CPP) $(OPTS_VIS_OPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_O3)/%.o: %.cpp $(SRC_DEPS)
    ifeq ($(ASM_OUT),1)
	    $(CPP) $(OPTS_VIS_OPT) @$(MF_INC) $(CPP_FLAGS) -S -o $(subst .o,.S,$@) $<
    endif
	$(CPP) $(OPTS_VIS_OPT) @$(MF_INC) $(CPP_FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)


$(KEEP_DIR):
	@mkdir -p $(KEEP_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR_O3):
	@mkdir -p $(OBJ_DIR_O3)

$(GEN_DIR):
	@mkdir -p $(GEN_DIR)

$(TOOLS_DIR):
	@mkdir -p $(TOOLS_DIR)


################################
# installation
################################

# incremental update/upgrade can cause the window system to be re-enabled
CHECK_DISABLE_SVCS=false
ifeq ($(BBAI_64),true)
    CHECK_DISABLE_SVCS=true
endif
ifeq ($(BBAI),true)
    CHECK_DISABLE_SVCS=true
endif
ifeq ($(DEBIAN_10_AND_LATER),true)
    CHECK_DISABLE_SVCS=true
endif

.PHONY: DISABLE_WS
DISABLE_WS:
    ifeq ($(CHECK_DISABLE_SVCS),true)
        ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
        else
	        @echo "disable unwanted services"
	        -@systemctl stop nodered.service >/dev/null 2>&1 || true
	        -@systemctl disable nodered.service >/dev/null 2>&1 || true
	        -@systemctl stop lightdm.service >/dev/null 2>&1 || true
	        -@systemctl disable lightdm.service >/dev/null 2>&1 || true
	        -@systemctl stop nginx.service >/dev/null 2>&1 || true
	        -@systemctl disable nginx.service >/dev/null 2>&1 || true
            ifeq ($(BBAI_64),true)
	            -@systemctl stop bb-code-server.service >/dev/null 2>&1 || true
	            -@systemctl disable bb-code-server.service >/dev/null 2>&1 || true
            endif
            ifeq ($(BBAI),true)
	            -@systemctl stop bonescript-autorun.service >/dev/null 2>&1 || true
	            -@systemctl disable bonescript-autorun.service >/dev/null 2>&1 || true
            endif
        endif
    endif

ASK_REBOOT = $(DIR_CFG)/.ask_reboot
FORCE_REBOOT = /tmp/.force_reboot
DO_ONCE =
DTS_DEP_DST =
DTS_DEP_DST2 =

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
    DO_ONCE = $(DIR_CFG)/.do_once.dep

    DTS_DEP_SRC  = $(addprefix $(DIR_DTS)/,$(DTS))
    DTS_DEP_SRC2 = $(addprefix $(DIR_DTS)/,$(DTS2))
    ifneq ($(DIR_DTB),)
        DTS_DEP_DST  = $(addprefix $(DIR_DTB)/,$(DTS))
        DTS_DEP_DST2 = $(addprefix $(DIR_DTB)/,$(DTS2))
    endif

    $(DO_ONCE):
	    @mkdir -p $(DIR_CFG)
	    @touch $(DO_ONCE)
	    make install_kiwi_device_tree
	    @touch $(FORCE_REBOOT)

    ifeq ($(BBAI_64),true)
        DTS = k3-j721e-beagleboneai64.dts k3-j721e-beagleboneai64-bone-buses.dtsi
        DTS2 = 
        DIR_DTS  = platform/beaglebone_AI64/$(DEB)
        DIR_DTB_BASE = $(wildcard /opt/source/dtb-$(SYS_MAJ).$(SYS_MIN)-*)
        DIR_DTB  = $(DIR_DTB_BASE)/src/arm64
        DIR_DTB2 = $(DIR_DTB_BASE)/src/arm64/overlays
        ifeq ($(DEBIAN_11_AND_LATER),true)
            #DTS2 = overlays/kiwisdr-cape.dts
            EXTLINUX := /boot/firmware/extlinux/extlinux.conf
            EXISTS_EXTLINUX := $(shell test -e $(EXTLINUX) && echo true)
            ifeq ($(EXISTS_EXTLINUX),true)
                DEB := "D11.9+"
            endif
        endif

        # re-install device tree if changes made to *.dts source file
#        $(DTS_DEP_DST) $(DTS_DEP_DST2): $(DTS_DEP_SRC) $(DTS_DEP_SRC2)
#	        @echo "BBAI-64: re-install Kiwi device tree to configure GPIO pins"
#	        make install_kiwi_device_tree
#	        touch $(FORCE_REBOOT)

        install_kiwi_device_tree:
	        @echo "BBAI-64: install Kiwi device tree to configure GPIO pins"
	        @echo $(SYS_MAJ).$(SYS_MIN) $(SYS)
            ifeq ($(DEBIAN_11_AND_LATER),true)
	            @cp -v $(DTS_DEP_SRC) $(DIR_DTB)
#	            @cp -v $(DTS_DEP_SRC2) $(DIR_DTB2)
	            (cd $(DIR_DTB_BASE); make)
	            (cd $(DIR_DTB_BASE); make install_arm64)
	            # including this breaks /dev/spidev9 since D11.9 / r113?
                #ifeq ($(EXISTS_EXTLINUX),true)
	            #   -sed -i -e 's:#fdtoverlays /overlays/<file>.dtbo:fdtoverlays /overlays/BONE-SPI0_0.dtbo:' $(EXTLINUX)
                #endif
            else
	            @cp -v $(DTS_DEP_SRC) $(DIR_DTB)
	            (cd $(DIR_DTB_BASE); make all)
	            (cd $(DIR_DTB_BASE); make install)
            endif
    endif

    ifeq ($(BBAI),true)
        DTS = am5729-beagleboneai.dts am5729-beagleboneai-kiwisdr-cape.dts
        DTS2 = 
        DIR_DTS = platform/beaglebone_AI/$(DEB)
        DIR_DTB_BASE = $(wildcard /opt/source/dtb-$(SYS_MAJ).$(SYS_MIN)-*)
        DIR_DTB = $(DIR_DTB_BASE)/src/arm
        DIR_DTB2 = $(DIR_DTB_BASE)/src/arm/overlays
        ifeq ($(DEBIAN_12_AND_LATER),true)
            #DTS2 = overlays/kiwisdr-cape.dts
        endif

        DTB_KIWI = am5729-beagleboneai-kiwisdr-cape.dtb
        DTB_DEB_NEW = am5729-beagleboneai-custom.dtb
        UENV_HAS_DTB_NEW := $(shell grep -qi '^dtb=$(DTB_DEB_NEW)' /boot/uEnv.txt && echo true)

        # re-install device tree if changes made to *.dts source file
#        $(DTS_DEP_DST) $(DTS_DEP_DST2): $(DTS_DEP_SRC) $(DTS_DEP_SRC2)
#	        @echo "BBAI: re-install Kiwi device tree to configure GPIO pins"
#	        make install_kiwi_device_tree
#	        touch $(FORCE_REBOOT)

        install_kiwi_device_tree:
	        @echo "BBAI: install Kiwi device tree to configure GPIO/SPI pins"
	        @echo $(SYS_MAJ).$(SYS_MIN) $(SYS)
            ifeq ($(DEBIAN_12_AND_LATER),true)
	            @cp -v $(DTS_DEP_SRC) $(DIR_DTB)
#	            @cp -v $(DTS_DEP_SRC2) $(DIR_DTB2)
	            (cd $(DIR_DTB_BASE); make)
	            (cd $(DIR_DTB_BASE); make install_arm)
            else
	            @cp -v $(DTS_DEP_SRC) $(DIR_DTB)
	            (cd $(DIR_DTB_BASE); make)
	            # intentionally don't do "make install"
	            # instead only copy single DTB_KIWI .dtb to am5729-beagleboneai.dtb (name change)
	            @cp -v $(DIR_DTB)/$(DTB_KIWI) /boot
	            @cp -v $(DIR_DTB)/$(DTB_KIWI) /boot/dtbs/$(SYS)
	            @cp -v $(DIR_DTB)/$(DTB_KIWI) /boot/dtbs/$(SYS)/am5729-beagleboneai.dtb
	            @echo "UENV_HAS_DTB_NEW = $(UENV_HAS_DTB_NEW)"
                ifeq ($(UENV_HAS_DTB_NEW),true)
	                -@cp -v --backup=numbered /boot/uEnv.txt /boot/uEnv.txt.save
	                -sed -i -e 's/^dtb=$(DTB_DEB_NEW)/dtb=$(DTB_KIWI)/' /boot/uEnv.txt
                endif
            endif
    endif

    ifeq ($(BBG_BBB),true)
        DTS = cape-bone-kiwi-00A0.dts
        DIR_DTS = platform/beaglebone_black
        ifeq ($(DEBIAN_12_AND_LATER),true)
            #DIR_DTB_BASE = $(wildcard /opt/source/dtb-$(SYS_MAJ).$(SYS_MIN)-*)
            #DIR_DTB = $(DIR_DTB_BASE)/src/arm
        else
            DIR_DTB = /lib/firmware
        endif

        # re-install device tree if changes made to *.dts source file
#        $(DTS_DEP_DST): $(DTS_DEP_SRC)
#	        @echo "BBG_BBB: re-install Kiwi device tree to configure GPIO pins"
#	        make install_kiwi_device_tree
#	        touch $(FORCE_REBOOT)

        install_kiwi_device_tree:
            ifeq ($(DEBIAN_12_AND_LATER),true)
	            @echo "BBG_BBB: D12+, SPI at boottime via uEnv.txt"
	            -sed -i -e 's:^#uboot_overlay_addr4=<file4>.dtbo:uboot_overlay_addr4=BB-SPIDEV0-00A0.dtbo:' /boot/uEnv.txt
	            grep uboot_overlay_addr4 /boot/uEnv.txt || true
            else ifeq ($(DEBIAN_10_AND_LATER),true)
	            @echo "BBG_BBB: D10-11, check uEnv.txt for BB-SPIDEV0 and cape-bone-kiwi"
                # For SPI uEnv.txt needs added by hand: cape_enable=bone_capemgr.enable_partno=BB-SPIDEV0
	            grep BB-SPIDEV0 /boot/uEnv.txt || true
	            grep uboot_overlay_addr4 /boot/uEnv.txt || true
#	            @echo "BBG_BBB: D10-11, GPIO at runtime via capemgr, SPI at boottime via uEnv.txt"
	            -@cp -v --backup=numbered /boot/uEnv.txt /boot/uEnv.txt.save
                # Debian 10
	            -sed -i -e 's:^#uboot_overlay_addr4=/lib/firmware/<file4>.dtbo:uboot_overlay_addr4=/lib/firmware/cape-bone-kiwi-00A0.dtbo:' /boot/uEnv.txt
                # Debian 11
	            -sed -i -e 's:^#uboot_overlay_addr4=<file4>.dtbo:uboot_overlay_addr4=/lib/firmware/cape-bone-kiwi-00A0.dtbo:' /boot/uEnv.txt
            else
	            @echo "BBG_BBB: D8, GPIO at runtime via capemgr, SPI at boottime via uEnv.txt"
                # ./k and init:kiwid load cape-bone-kiwi-00A0 via capemgr and run dtc if necessary
                # So we only have to place cape-bone-kiwi-00A0.dts in /lib/firmware
	            @cp -v $(DTS_DEP_SRC) $(DIR_DTB)
                # For SPI uEnv.txt needs added by hand: cape_enable=bone_capemgr.enable_partno=BB-SPIDEV0
            endif
    endif
endif

DIR_CFG_SRC = unix_env/kiwi.config

EXISTS_BASHRC_LOCAL := $(shell test -f ~root/.bashrc.local && echo true)

CFG_KIWI = kiwi.json
EXISTS_KIWI := $(shell test -f $(DIR_CFG)/$(CFG_KIWI) && echo true)

CFG_ADMIN = admin.json
EXISTS_ADMIN := $(shell test -f $(DIR_CFG)/$(CFG_ADMIN) && echo true)

CFG_CONFIG = config.js
EXISTS_CONFIG := $(shell test -f $(DIR_CFG)/$(CFG_CONFIG) && echo true)

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
    DX = dx.json
    EXISTS_DX := $(shell test -f $(DIR_CFG)/$(DX) && echo true)
    DX_CFG = dx_config.json
    DX_SHA256 := $(shell test -f $(DIR_CFG)/$(DX) && sha256sum $(DIR_CFG)/$(DX) | cut -c1-8)
    DX_NEEDS_UPDATE := $(findstring $(DX_SHA256),612d92da 574fb11d f607e7c7 668dec9e)

    # If missing install DIR_CFG copies (initial versions).
    # If Internet connection exists subsequent updates will occur.
    DX_COMM = dx_community.json
    EXISTS_DX_COMM := $(shell test -f $(DIR_CFG)/$(DX_COMM) && echo true)
    DX_COMM_CFG = dx_community_config.json
    EXISTS_DX_COMM_CFG := $(shell test -f $(DIR_CFG)/$(DX_COMM_CFG) && echo true)

    ETC_HOSTNAME_IS_BB := $(shell grep -qi beaglebone /etc/hostname && echo true)
    ETC_HOSTS_HAS_KIWI := $(shell grep -qi kiwisdr /etc/hosts && echo true)

    SSH_KEYS = /root/.ssh/authorized_keys
    EXISTS_SSH_KEYS := $(shell test -f $(SSH_KEYS) && echo true)

    # because different Debian distros install iptables in different dirs do detection this way
    MISSING_IPTABLES := $(shell which iptables || echo true)
endif

# Doing a 'make install' on the development machine is only used to build the optimized files.
# For the Beagle this installs the device tree files in the right place and other misc stuff.
# DANGER: do not use $(MAKE_ARGS) here! The targets for building $(EMBED_DEPS) must be run sequentially

.PHONY: install
install: make_prereq
	@# don't use MAKE_ARGS here!
    ifeq ($(BINARY_DISTRO),true)
	    make make_install_binary
    else
	    make make_install
    endif

# copy binaries to Kiwi named $(KIWI_XC_HOST)
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
RSYNC_XC = rsync -av -e "ssh -p $(KIWI_XC_HOST_PORT) -l root"

.PHONY: install_xc
ifeq ($(XC),)
    install_xc:
	    @make XC=-DXC install_xc
else
    install_xc: make_prereq $(BUILD_DIR)/kiwid.bin
	    # must use rsync instead of install because $(KIWI_XC_REMOTE_FS) may be read-only
	    # NB: --relative option use of "/./" to delimit creation dir path
	    $(RSYNC_XC) --relative .././build/kiwi.bin root@$(KIWI_XC_HOST):~root/
	    $(RSYNC_XC) --relative .././build/gen/kiwi.aout root@$(KIWI_XC_HOST):~root/
	    $(RSYNC_XC) $(BUILD_DIR)/kiwid.bin root@$(KIWI_XC_HOST):/usr/local/bin/kiwid
	    $(RSYNC_XC) $(GEN_DIR)/kiwi.aout root@$(KIWI_XC_HOST):/usr/local/bin/kiwid.aout
	    # don't copy dependency files (*.d) here because include paths are slightly different
	    # e.g. development machine: /usr/local/include/fftw3.h
	    # versus Beagle: /usr/include/fftw3.h
	    $(RSYNC_XC) --relative .././build/obj/*.o root@$(KIWI_XC_HOST):~root/
	    $(RSYNC_XC) --relative .././build/obj_O3/*.o root@$(KIWI_XC_HOST):~root/
	    $(RSYNC_XC) --relative .././build/obj_keep/*.o root@$(KIWI_XC_HOST):~root/
        ifeq ($(KIWI_XC_COPY_SOURCES),true)
	        $(RSYNC_XC) --delete $(addprefix --exclude , $(EXCLUDE_RSYNC)) . root@$(KIWI_XC_HOST):~root/$(REPO_NAME)
        endif
endif
endif

PLAT_KIWID_BIN := bin/kiwid_$(VER)_$(PLAT).bin
HAS_KIWID_BIN := $(shell test -x $(PLAT_KIWID_BIN) && echo true)

.PHONY: make_install_binary
make_install_binary:
    ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	    @make make_install
    else
        ifeq ($(BINARY_INSTALL),true)
	        @echo "================"
	        @echo "make_install_binary: $(PLAT_KIWID_BIN)"
            ifeq ($(HAS_KIWID_BIN),true)
	            @echo "   => exists: not installing from sources"
	            @echo "   => cp $(PLAT_KIWID_BIN) $(BUILD_DIR)/kiwid.bin"
	            @cp $(PLAT_KIWID_BIN) $(BUILD_DIR)/kiwid.bin
	            touch $(BUILD_DIR)/kiwid.bin
	            @echo "================"
	            @make make_install
            else
	            @echo "   => doesn't exist: installing from sources"
	            @echo "================"
	            @# don't use MAKE_ARGS here!
	            @make make_install
	            @echo "================"
	            @echo "   => cp $(BUILD_DIR)/kiwid.bin $(PLAT_KIWID_BIN)"
	            @echo "================"
	            @cp $(BUILD_DIR)/kiwid.bin $(PLAT_KIWID_BIN)
            endif
        else
	        @make make_install
        endif
    endif
	@echo "make_install_binary DONE"

.PHONY: make_install
make_install: $(DO_ONCE) $(DTS_DEP_DST) $(BUILD_DIR)/kiwid.bin
    ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	    @echo
	    @echo "############################################"
	    @echo "# CHECK FOR MINIMIZATION FAILURE"
	    @echo "# kiwi_js_load.min.js and xd-utils.min.js are okay to be in this list"
	    find . -name "*.min.js" -size -1k -ls
#	    @echo "# DANGER: REMOVING FILES ###############################################################"
#	    find . -name "*.min.js" -size -1k -exec rm "{}" \;
	    # report when something has gone wrong with the minimization process
	    find . -name "*.min.js" -exec grep -q "no minifier" "{}" \; -ls
	    find . -name "*.min.html" -exec grep -q "no minifier" "{}" \; -ls
	    find . -name "*.min.css" -exec grep -q "no minifier" "{}" \; -ls
	    @echo "############################################"
	    @echo

	    # remainder of "make install" only makes sense to run on target
    else
        ifneq ($(DTS_DEP_DST),)
	        -@ls -la $(DTS_DEP_DST)
	        -@ls -la $(DTS_DEP_SRC)
        endif

        # don't strip symbol table while we're debugging server crashes
	    install -D -o root -g root $(BUILD_DIR)/kiwid.bin /usr/local/bin/kiwid
	    install -D -o root -g root $(GEN_DIR)/kiwi.aout /usr/local/bin/kiwid.aout

        ifeq ($(EXISTS_RX4_WF4),true)
	        install -D -o root -g root KiwiSDR.rx4.wf4.bit /usr/local/bin/KiwiSDR.rx4.wf4.bit
        endif

        ifeq ($(EXISTS_RX8_WF2),true)
	        install -D -o root -g root KiwiSDR.rx8.wf2.bit /usr/local/bin/KiwiSDR.rx8.wf2.bit
        endif

        ifeq ($(EXISTS_RX3_WF3),true)
	        install -D -o root -g root KiwiSDR.rx3.wf3.bit /usr/local/bin/KiwiSDR.rx3.wf3.bit
        endif

        ifeq ($(EXISTS_RX14_WF0),true)
	        install -D -o root -g root KiwiSDR.rx14.wf0.bit /usr/local/bin/KiwiSDR.rx14.wf0.bit
        endif

        ifeq ($(EXISTS_OTHER),true)
	        install -D -o root -g root KiwiSDR.other.bit /usr/local/bin/KiwiSDR.other.bit
        endif

	    install -o root -g root unix_env/kiwid /etc/init.d
	    install -o root -g root -m 0644 unix_env/kiwid.service /etc/systemd/system

	    install -D -o root -g root $(GEN_DIR)/noip2 /usr/local/bin/noip2

	    install -D -o root -g root -m 0644 $(DIR_CFG_SRC)/frpc.template.ini $(DIR_CFG)/frpc.template.ini
	    install -D -o root -g root pkgs/frp/$(ARCH_DIR)/frpc /usr/local/bin/frpc

	    install -D -o root -g root -m 0644 unix_env/bashrc ~root/.bashrc
	    install -D -o root -g root -m 0644 unix_env/profile ~root/.profile

	    install -D -o root -g root -m 0644 unix_env/gdbinit ~root/.gdbinit
	    install -D -o root -g root -m 0644 unix_env/gdb_break ~root/.gdb_break
	    install -D -o root -g root -m 0644 unix_env/gdb_valgrind ~root/.gdb_valgrind

	    install -D -o root -g root -m 0644 $(DIR_CFG_SRC)/v.sed $(DIR_CFG)/v.sed
	    install -D -o root -g root -m 0644 $(DIR_CFG_SRC)/rsyslog.sed $(DIR_CFG)/rsyslog.sed

	    rsync -av --delete $(DIR_CFG_SRC)/samples/ $(DIR_CFG)/samples

        # only install post-customized config files if they've never existed before
        ifneq ($(EXISTS_BASHRC_LOCAL),true)
	        @echo "\nINSTALLING .bashrc.local"
	        cp unix_env/bashrc.local ~root/.bashrc.local
        endif

        ifneq ($(EXISTS_KIWI),true)
	        @echo "\nINSTALLING $(DIR_CFG)/$(CFG_KIWI)"
	        @mkdir -p $(DIR_CFG)
	        cp $(DIR_CFG_SRC)/dist.$(CFG_KIWI) $(DIR_CFG)/$(CFG_KIWI)
        endif

        ifneq ($(EXISTS_ADMIN),true)
	        @echo "\nINSTALLING $(DIR_CFG)/$(CFG_ADMIN)"
	        @mkdir -p $(DIR_CFG)
	        cp $(DIR_CFG_SRC)/dist.$(CFG_ADMIN) $(DIR_CFG)/$(CFG_ADMIN)
        endif

        ifneq ($(EXISTS_DX),true)
	        @echo "\nINSTALLING $(DIR_CFG)/$(DX)"
	        @mkdir -p $(DIR_CFG)
	        cp $(DIR_CFG_SRC)/dist.$(DX) $(DIR_CFG)/$(DX)
	        cp $(DIR_CFG_SRC)/dist.$(DX_CFG) $(DIR_CFG)/$(DX_CFG)
        endif

	    @echo "\nDX_SHA256=$(DX_SHA256) DX_NEEDS_UPDATE=$(DX_NEEDS_UPDATE)"

        ifneq ($(DX_NEEDS_UPDATE),)
	        @echo "\nUPDATING $(DIR_CFG)/$(DX)"
	        cp --backup=numbered $(DIR_CFG)/$(DX) $(DIR_CFG)/$(DX).save
	        cp $(DIR_CFG_SRC)/dist.$(DX) $(DIR_CFG)/$(DX)
            # if dx.json hasn't changed then presume a conversion from config.js isn't necessary
	        @echo "\nINSTALLING $(DIR_CFG)/$(DX_CFG)"
	        cp $(DIR_CFG_SRC)/dist.$(DX_CFG) $(DIR_CFG)/$(DX_CFG)
        endif

        ifneq ($(EXISTS_DX_COMM),true)
	        @echo "\nINSTALLING $(DIR_CFG)/$(DX_COMM)"
	        @mkdir -p $(DIR_CFG)
	        cp $(DIR_CFG_SRC)/dist.$(DX_COMM) $(DIR_CFG)/$(DX_COMM)
        endif

        ifneq ($(EXISTS_DX_COMM_CFG),true)
	        @echo "\nINSTALLING $(DIR_CFG)/$(DX_COMM_CFG)"
	        @mkdir -p $(DIR_CFG)
	        cp $(DIR_CFG_SRC)/dist.$(DX_COMM_CFG) $(DIR_CFG)/$(DX_COMM_CFG)
        endif

        ifneq ($(EXISTS_CONFIG),true)
	        @echo "\nINSTALLING $(DIR_CFG)/$(CFG_CONFIG)"
	        @mkdir -p $(DIR_CFG)
	        cp $(DIR_CFG_SRC)/dist.$(CFG_CONFIG) $(DIR_CFG)/$(CFG_CONFIG)
        endif

        ifeq ($(ETC_HOSTNAME_IS_BB),true)
	        @echo "CHANGING /etc/hostname to kiwisdr"
	        @echo 'kiwisdr' >/etc/hostname
        endif

        ifneq ($(ETC_HOSTS_HAS_KIWI),true)
	        @echo "\nAPPENDING kiwisdr to /etc/hosts"
	        @echo '127.0.0.1       kiwisdr' >>/etc/hosts
        endif

	    @echo
	    systemctl enable kiwid.service

        # remove public keys leftover from development
        ifeq ($(EXISTS_SSH_KEYS),true)
	        @-sed -i -e '/.*jks-/d' $(SSH_KEYS)
        endif

        ifeq ($(MISSING_IPTABLES),true)
	        -apt-get -y $(APT_GET_FORCE) install iptables
        endif

        # must be last obviously
	    @if [ -f $(ASK_REBOOT) ]; then rm $(ASK_REBOOT); echo "\nMUST REBOOT FOR CHANGES TO TAKE EFFECT"; echo -n "Press \"return\" key to reboot else control-C: "; read in; reboot; fi;
	    @if [ -f $(FORCE_REBOOT) ]; then rm $(FORCE_REBOOT); echo "\nMUST REBOOT FOR CHANGES TO TAKE EFFECT. REBOOTING..."; reboot; fi;

    endif

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
    restore_dx:
	    cp unix_env/kiwi.config/dist.dx.json ../kiwi.config/dx.json
	    cp unix_env/kiwi.config/dist.dx_config.json ../kiwi.config/dx_config.json
endif


################################
# operation
################################
ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

enable disable start stop restart status:
	-systemctl --full --lines=250 $@ kiwid.service || true

stop_disable:
	-systemctl --full --lines=250 stop kiwid.service || true
	-systemctl --full --lines=250 disable kiwid.service || true

enable_start:
	-systemctl --full --lines=250 enable kiwid.service || true
	-systemctl --full --lines=250 start kiwid.service || true

avahi-enable avahi-disable avahi-start avahi-stop avahi-status:
	-systemctl --full --lines=250 $(subst avahi-,,$@) avahi-daemon.service || true
	-systemctl --full --lines=250 $(subst avahi-,,$@) avahi-daemon.socket || true

# SIGUSR1 == SIG_DUMP
reload dump:
	-killall -q -s USR1 kiwid
	-killall -q -s USR1 kiwi.bin

# to generate kiwi.config/info.json
hup:
	-killall -q -s HUP kiwid
	-killall -q -s HUP kiwi.bin

log2:
	grep kiwid /var/log/syslog

gdb:
	gdb /usr/local/bin/kiwid /tmp/core-kiwid*

LOGS = \
	zcat /var/log/user.log.4.gz > /tmp/kiwi.log; \
	zcat /var/log/user.log.3.gz >> /tmp/kiwi.log; \
	zcat /var/log/user.log.2.gz >> /tmp/kiwi.log; \
	cat /var/log/user.log.1 >> /tmp/kiwi.log; \
	cat /var/log/user.log >> /tmp/kiwi.log; \
	cat /tmp/kiwi.log

log:
	-@$(LOGS) | grep -a kiwid
	@rm -f /tmp/kiwi.log

LOGS_SHORT = \
	cat /var/log/user.log.1 > /tmp/kiwi.log; \
	cat /var/log/user.log >> /tmp/kiwi.log; \
	cat /tmp/kiwi.log

slog:
	-@$(LOGS_SHORT) | grep -a kiwid
	@rm -f /tmp/kiwi.log

tlog:
	-@cat /var/log/user.log | grep -a kiwid | tail -500

syslog:
	tail -n 1000 -f /var/log/syslog

flog:
	tail -n 100 -f /var/log/frpc.log

LOCAL_IP = grep -vi 192.168.1
LEAVING = grep -ai leaving | grep -vi kf6vo | $(LOCAL_IP)
users:
	-@$(LOGS) | $(LEAVING)
	@rm -f /tmp/kiwi.log

USERS = sed -e 's/[^"]* "/"/' | sed -e 's/(LEAVING.*//'
users2:
	-@$(LOGS) | $(LEAVING) | $(USERS)
	@rm -f /tmp/kiwi.log

unique:
	-@$(LOGS) | $(LEAVING) | $(USERS) | sort -u
	@rm -f /tmp/kiwi.log

endif

v ver version:
	@echo "you are running version" $(VER)

# workaround for sites having problems with git using https (even when curl with https works fine)
OPT_GIT_NO_HTTPS := $(shell test -f /root/kiwi.config/opt.git_no_https && echo true)
ifeq ($(OPT_GIT_NO_HTTPS),true)
	GIT_PROTO = git
else
	GIT_PROTO = https
endif

# invoked by update process -- alter with care!
git:
	@# remove local changes from development activities before the pull
	git clean -fd
	git checkout .
	git pull -v $(GIT_PROTO)://github.com/$(REPO_GIT)

GITHUB_COM_IP = "52.64.108.95"
git_using_ip:
	@# remove local changes from development activities before the pull
	git clean -fd
	git checkout .
	git pull -v $(GIT_PROTO)://$(GITHUB_COM_IP)/$(REPO_GIT)

update_check:
	git fetch origin
	git show origin:Makefile > Makefile.1
	diff Makefile Makefile.1 | head

force_update:
	touch $(MAKEFILE_DEPS)
	make


################################
# development
################################

dump_eeprom:
	@echo "KiwiSDR cape EEPROM:"
    ifeq ($(DEBIAN_VERSION),7)
	    -hexdump -C /sys/bus/i2c/devices/1-0054/eeprom
    else
    ifeq ($(BBAI_64),true)
	    -hexdump -C /sys/bus/i2c/devices/5-0054/eeprom
    else
    ifeq ($(BBAI),true)
	    -hexdump -C /sys/bus/i2c/devices/3-0054/eeprom
    else
    ifeq ($(RPI),true)
	    -hexdump -C /sys/bus/i2c/devices/1-0050/eeprom
    else
	    -hexdump -C /sys/bus/i2c/devices/2-0054/eeprom
    endif
    endif
    endif
    endif

dump_eeprom_all: dump_eeprom
	@echo
	@echo "BeagleBone EEPROM:"
    ifeq ($(BBAI_64),true)
	    -hexdump -C /sys/bus/i2c/devices/2-0050/eeprom
    else
    ifeq ($(BBAI),true)
	    @echo "(none on BBAI)"
    else
	    -hexdump -C /sys/bus/i2c/devices/0-0050/eeprom
    endif
    endif

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))

    # selectively transfer files to the target so everything isn't compiled each time
    GET_TOOLS_EXCLUDE_RSYNC := true
    -include tools/Makefile
    EXCLUDE_RSYNC = ".DS_Store" ".git" "/obj" "/obj_O3" "/obj_keep" "*.dSYM" "*.bin" "*.aout" "e_cpu/a" "*.aout.h" "kiwi.gen.h" \
        "verilog/kiwi.gen.vh" "web/edata*" "node_modules" "morse-pro-compiled.js"
    RSYNC_ARGS = -av --delete $(addprefix --exclude , $(EXCLUDE_RSYNC)) \
        $(addprefix --exclude , $(EXT_EXCLUDE_RSYNC)) $(addprefix --exclude , $(TOOLS_EXCLUDE_RSYNC)) \
        $(RSYNC_SRC) $(RSYNC_USER)@$(HOST):$(RSYNC_DST)
    RSYNC_ARGS_DRYRUN = -n $(RSYNC_ARGS)

    RSYNC_USER ?= root
    RSYNC_DIR ?= /root
    RSYNC_REPO ?= $(RSYNC_DIR)/$(REPO_NAME)
    RSYNC_SRC ?= .
    RSYNC_DST ?= $(RSYNC_REPO)
    PORT ?= 22

    ifeq ($(PORT),22)
        RSYNC = rsync
    else
        RSYNC = rsync -e "ssh -p $(PORT) -l $(RSYNC_USER)"
    endif

    rsync_su:
	    $(RSYNC) $(RSYNC_ARGS)

    rsync_dryrun:
	    $(RSYNC) $(RSYNC_ARGS_DRYRUN)
endif


################################
# Verilog
################################

-include verilog/Makefile

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))

    ifeq ($(XC),) ## do not copy bit streams from $(V_DIR) when cross-compiling

        # FIXME: isn't there a better way to do this in GNU make?

        EXISTS_V_DIR_RX4_WF4 := $(shell test -f $(V_DIR)/KiwiSDR.rx4.wf4.bit && echo true)
        ifeq ($(EXISTS_V_DIR_RX4_WF4),true)
            KiwiSDR.rx4.wf4.bit: $(V_DIR)/KiwiSDR.rx4.wf4.bit
	            rsync -av $(V_DIR)/KiwiSDR.rx4.wf4.bit .
        else
            KiwiSDR.rx4.wf4.bit:
        endif

        EXISTS_V_DIR_RX8_WF2 := $(shell test -f $(V_DIR)/KiwiSDR.rx8.wf2.bit && echo true)
        ifeq ($(EXISTS_V_DIR_RX8_WF2),true)
            KiwiSDR.rx8.wf2.bit: $(V_DIR)/KiwiSDR.rx8.wf2.bit
	            rsync -av $(V_DIR)/KiwiSDR.rx8.wf2.bit .
        else
            KiwiSDR.rx8.wf2.bit:
        endif

        EXISTS_V_DIR_RX3_WF3 := $(shell test -f $(V_DIR)/KiwiSDR.rx3.wf3.bit && echo true)
        ifeq ($(EXISTS_V_DIR_RX3_WF3),true)
            KiwiSDR.rx3.wf3.bit: $(V_DIR)/KiwiSDR.rx3.wf3.bit
	            rsync -av $(V_DIR)/KiwiSDR.rx3.wf3.bit .
        else
            KiwiSDR.rx3.wf3.bit:
        endif

        EXISTS_V_DIR_RX14_WF0 := $(shell test -f $(V_DIR)/KiwiSDR.rx14.wf0.bit && echo true)
        ifeq ($(EXISTS_V_DIR_RX14_WF0),true)
            KiwiSDR.rx14.wf0.bit: $(V_DIR)/KiwiSDR.rx14.wf0.bit
	            rsync -av $(V_DIR)/KiwiSDR.rx14.wf0.bit .
        else
            KiwiSDR.rx14.wf0.bit:
        endif

        EXISTS_OTHER_BITFILE := $(shell test -f $(V_DIR)/KiwiSDR.other.bit && echo true)
        ifeq ($(OTHER_DIR),)
            KiwiSDR.other.bit:
        else
            ifeq ($(EXISTS_OTHER_BITFILE),true)
                KiwiSDR.other.bit: $(V_DIR)/KiwiSDR.other.bit
	                rsync -av $(V_DIR)/KiwiSDR.other.bit .
            else
                KiwiSDR.other.bit:
            endif
        endif
    else
        KiwiSDR.other.bit:
    endif


    # other_rsync is a rule that can be defined in an extension Makefile (i.e. CFG = other) to do additional source installation
    other_rsync:
    # other_post_rsync is invoked after the rsync, e.g. for augmenting server sources
    other_post_rsync:

    rsync_bit: $(BIN_DEPS) other_rsync
	    $(RSYNC) $(RSYNC_ARGS)
        ifneq ($(OTHER_DIR),)
	        make other_post_rsync
        endif

endif


################################
# development
################################

# files that have moved to $(BUILD_DIR) that are present in earlier versions (e.g. v1.2)
clean_deprecated:
	-rm -rf obj obj_O3 obj_keep kiwi.bin kiwid.bin *.dSYM web/edata*
	-rm -rf *.dSYM pas extensions/ext_init.cpp kiwi.gen.h kiwid kiwid.aout kiwid_realtime.bin .comp_ctr

clean: clean_ext clean_deprecated
	(cd e_cpu; make clean)
	(cd verilog/rx; make clean)
	(cd tools; make clean)
	(cd pkgs/noip2; make clean)
	(cd pkgs/EiBi; make clean)
	-rm -rf $(addprefix pru/pru_realtime.,bin lst txt) $(TOOLS_DIR)/file_optim
	# but not $(KEEP_DIR)
	-rm -rf $(LOG_FILE) $(BUILD_DIR)/kiwi* $(GEN_DIR) $(OBJ_DIR) $(OBJ_DIR_O3)
	-rm -f Makefile.1

clean_dist: clean
	-rm -rf $(BUILD_DIR)

clean_logs:
	-journalctl --disk-usage
	-journalctl --vacuum-size=10M
	-journalctl --disk-usage


# The following support the development process
# and are not used for ordinary software builds.

clone:
	git clone $(REPO)

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))

BIN_PLATS := BBAI_64 BBAI BBG_BBB
BIN_EXISTS := $(foreach plat,$(BIN_PLATS),$(shell test -f bin/kiwi_$(VER)_$(plat).bin && echo true || echo false))

    # used by scgit alias
    copy_to_git:
	    @(echo 'current dir is:'; pwd)
	    @(cd $(GITAPP)/$(REPO_NAME); echo 'repo branch set to:'; pwd; git --no-pager branch)
        ifeq ($(BINARY_DISTRO),true)
	        @echo "checking for release bin files: $(BIN_PLATS)"
            ifeq ($(findstring false,$(BIN_EXISTS)),false)
	            @echo "ERROR release file missing:"
	            @ls -la bin
	            @exit -1;
            endif
        endif
	    @echo '################################'
	    @echo -n 'did you make install to rebuild the optimized files? '
	    @read not_used
	    @echo '################################'
	    make clean_dist
	    rsync -av --delete --exclude .git --exclude .DS_Store . $(GITAPP)/$(REPO_NAME)

    copy_from_git:
	    @(echo 'current dir is:'; pwd)
	    @echo
	    @(cd $(GITAPP)/$(REPO_NAME); echo 'repo branch set to:'; pwd; git --no-pager branch)
	    @echo -n 'are you sure? '
	    @read not_used
	    @echo -n 'are you REALLY sure? '
	    @read not_used
	    make clean_dist
	    rsync -av --delete --exclude .git --exclude .DS_Store $(GITAPP)/$(REPO_NAME)/. .

    # used by gdiff et al aliases
    GITDIFF_EXCLUDE := --exclude=.DS_Store --exclude=.git \
        --exclude=k --exclude=d --exclude=g --exclude=n --exclude=ng
    GITDIFF_EXCLUDE2 := $(GITDIFF_EXCLUDE) --exclude="*.min.*"

    gitdiff:
	    colordiff -br $(GITDIFF_EXCLUDE2) $(GITAPP)/$(REPO_NAME) . || true
    gitdiff_context:
	    colordiff -br -C 10 $(GITDIFF_EXCLUDE2) $(GITAPP)/$(REPO_NAME) . || true
    gitdiff_brief:
	    colordiff -br --brief $(GITDIFF_EXCLUDE) $(GITAPP)/$(REPO_NAME) . || true
    gitdiff_no_big:
	    colordiff -br $(GITDIFF_EXCLUDE2) --exclude="*.json" --exclude=EiBi.h --exclude=sked-current.csv $(GITAPP)/$(REPO_NAME) . || true
    gitdiff2:
	    colordiff -br $(GITDIFF_EXCLUDE2) ../../../sdr/KiwiSDR/$(REPO_NAME) . || true
endif

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
    prep_distro: clean_logs
	    -systemctl --full --lines=250 stop kiwid.service || true
	    -systemctl --full --lines=250 enable kiwid.service || true
	    (cd $(DIR_CFG); jq '.onetime_password_check = false | .rev_auto = false | .rev_auto_user = "" | .rev_auto_host = ""' admin.json > /tmp/jq && mv /tmp/jq admin.json)
	    (cd $(DIR_CFG); rm -f .do_once.dep .keyring4.dep frpc.ini seq_serno)
	    -rm -f /tmp/.kiwi* /root/.ssh/auth* /root/.ssh/known*
	    -rm -f build.log
	    -touch unix_env/reflash_delay_update
	    -cp unix_env/shadow /etc/shadow
	    sum *.bit

    /usr/bin/xz:
	    apt-get -y $(APT_GET_FORCE) install xz-utils


    ifeq ($(BBAI_64),true)
        SD_CARD_MMC_COPY := 1
        SD_CARD_MMC_PART := p2
        DISTRO_DEBIAN_VER := 11.9
        DD_SIZE := 6000M
    else ifeq ($(BBAI),true)
        SD_CARD_MMC_COPY := 0
        SD_CARD_MMC_PART := p1
        DISTRO_DEBIAN_VER := 11.9
        DD_SIZE := 3500M
    else ifeq ($(BBG_BBB),true)
        SD_CARD_MMC_COPY := 0
        SD_CARD_MMC_PART := p1
        DISTRO_DEBIAN_VER := 11.9
        DD_SIZE := 3000M
    endif

    # zero sd card first so .img.xz file is more compact on subsequent compression
    backup_zero:
	    lsblk
	    -dd if=/dev/zero of=/dev/mmcblk$(SD_CARD_MMC_COPY) bs=1M iflag=count_bytes count=$(DD_SIZE) status=progress
	    make backup
	    fsck /dev/mmcblk$(SD_CARD_MMC_COPY)$(SD_CARD_MMC_PART)

    # makefile version of admin backup tab
    backup:
        ifeq ($(DEBIAN_11_AND_LATER),true)
	        cp /etc/beagle-flasher/$(PLAT_BACKUP)-emmc-to-microsd /etc/default/beagle-flasher
	        (cd /root/$(REPO_NAME)/tools; bash ./$(PLAT_BACKUP)-flasher.sh)
        else
	        (cd /root/$(REPO_NAME)/tools; bash ./kiwiSDR-make-microSD-flasher-from-eMMC.sh)
        endif

    # Use "make backup_zero" above to make filesystem holes zeros for better compression.
    #
    # DANGER: DD_SIZE must be larger than the partition "used" size computed by the "d.mb" command alias.
    # Otherwise the image file will have strange effects like /boot/uEnv.txt being the correct size but
    # filled with zeroed bytes (which of course is a disaster).
    #
    TO_IMG = ~/KiwiSDR_$(VER)_$(PLAT)_Debian_$(DISTRO_DEBIAN_VER).img.xz

    create_img_from_sd: /usr/bin/xz
	    @echo "--- this takes about an hour"
	    @echo "--- KiwiSDR server will be stopped to maximize write speed"
	    lsblk
	    @echo "CAUTION: SD_CARD_MMC_COPY = $(SD_CARD_MMC_COPY)"
	    @echo "CAUTION: VERIFY FROM THE LIST ABOVE THAT THE SD CARD IS THE MMC NUMBER SHOWN"
	    @echo -n 'ARE YOU SURE? '
	    @read not_used
	    make stop
	    date
	    dd if=/dev/mmcblk$(SD_CARD_MMC_COPY) bs=1M iflag=count_bytes count=$(DD_SIZE) | xz --verbose > $(TO_IMG)
	    sha256sum $(TO_IMG)
	    date


    ifeq ($(BBAI_64),true)
        FROM_IMG = ~/bbai64-emmc-flasher-debian-11.7-minimal-arm64-2023-09-02-6gb.img.xz
    else ifeq ($(BBAI),true)
        FROM_IMG = ~/am57xx-eMMC-flasher-debian-11.7-minimal-armhf-2023-09-02-2gb.img.xz
    else ifeq ($(BBG_BBB),true)
        FROM_IMG = ~/am335x-eMMC-flasher-debian-11.8-minimal-armhf-2023-10-07-2gb.img.xz
    endif

    create_sd_from_img: /usr/bin/xz
	    lsblk
	    @echo "CAUTION: SD_CARD_MMC_COPY = $(SD_CARD_MMC_COPY)"
	    @echo "CAUTION: VERIFY FROM THE LIST ABOVE THAT THE SD CARD IS THE MMC NUMBER SHOWN"
	    @echo -n 'ARE YOU SURE? '
	    @read not_used
	    @echo -n 'ARE YOU *REALLY* SURE? /dev/mmcblk$(SD_CARD_MMC_COPY) WILL BE OVERWRITTEN! '
	    @read not_used
	    date
	    xzcat -v $(FROM_IMG) | dd of=/dev/mmcblk$(SD_CARD_MMC_COPY) bs=1M
	    blockdev --flushbufs /dev/mmcblk$(SD_CARD_MMC_COPY)
	    date
endif
