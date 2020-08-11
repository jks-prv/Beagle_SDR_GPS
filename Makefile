VERSION_MAJ = 1
VERSION_MIN = 402

REPO_NAME = Beagle_SDR_GPS
DEBIAN_VER = 8.11

# Caution: software update mechanism depends on format of first two lines in this file

#
# Makefile for KiwiSDR project
#
# Copyright (c) 2014-2020 John Seamons, ZL/KF6VO
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

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

	# enough parallel make jobs to overcome core stalls from filesystem or nfs delays
	ifeq ($(BBAI),true)
		# currently even 2 parallel compiles can run out of memory on BBAI
		#MAKE_ARGS = -j 2
		MAKE_ARGS =
	else ifeq ($(RPI),true)
		# fixme: test, but presumably the 1GB (min) of DRAM on RPi 3B & 4 would support this many
		MAKE_ARGS = -j 4
	else
		ifeq ($(DEBIAN_7),true)
			# Debian 7 gcc runs out of memory compiling edata_always*.cpp in parallel
			MAKE_ARGS =
		else
			MAKE_ARGS =
		endif
	endif
else
	# choices when building on development machine
	ifeq ($(XC),-DXC)
		# BBAI, DEBIAN_7 are taken from the mounted KiwiSDR root file system
		MAKE_ARGS = -j 7
	else
		BBAI = true
		#RPI = true
		MAKE_ARGS = -j
	endif
endif

ifeq ($(BBAI),true)
	ARCH = sitara
	CPU = AM5729
	PLATFORMS = beaglebone beaglebone_ai
	CFLAGS += -DMULTI_CORE
else ifeq ($(RPI),true)
	ARCH = omap
	CPU = BCM2837
	PLATFORMS = raspberrypi
	CFLAGS += -DMULTI_CORE
else
	ARCH = sitara
	CPU = AM3359
	PLATFORMS = beaglebone beaglebone_black
endif

# uncomment when using debugger so variables are not always optimized away
ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
#	OPT = 0
endif

KIWI_XC_REMOTE_FS ?= ${HOME}/mnt
KIWI_XC_HOST ?= kiwisdr
KIWI_XC_HOST_PORT ?= 22


################################
# "all" target must be first
################################
.PHONY: all
all: c_ext_clang_conv
	@make $(MAKE_ARGS) build_makefile_inc
	@make $(MAKE_ARGS) c_ext_clang_conv_all

.PHONY: debug
debug: c_ext_clang_conv
	@make $(MAKE_ARGS) DEBUG=-DDEBUG build_makefile_inc
	@make $(MAKE_ARGS) DEBUG=-DDEBUG c_ext_clang_conv_all

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
xc: c_ext_clang_conv
	@echo KIWI_XC_HOST = $(KIWI_XC_HOST)
	@echo KIWI_XC_HOST_PORT = $(KIWI_XC_HOST_PORT)
	@echo KIWI_XC_REMOTE_FS = $(KIWI_XC_REMOTE_FS)
	@make $(MAKE_ARGS) build_makefile_inc
	@make $(MAKE_ARGS) c_ext_clang_conv_all
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

BUILD_DIR = ../build
OBJ_DIR = $(BUILD_DIR)/obj
OBJ_DIR_O3 = $(BUILD_DIR)/obj_O3
KEEP_DIR = $(BUILD_DIR)/obj_keep
GEN_DIR = $(BUILD_DIR)/gen
TOOLS_DIR = $(BUILD_DIR)/tools

ifeq ($(OPT),0)
	OBJ_DIR_DEFAULT = $(OBJ_DIR)
else
	OBJ_DIR_DEFAULT = $(OBJ_DIR_O3)
endif

PKGS = 
PKGS_O3 = pkgs/mongoose pkgs/jsmn pkgs/sha256 pkgs/TNT_JAMA

# Each (internal) extension can have an optional Makefile:
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
-include $(wildcard extensions/*/Makefile)

PVT_EXT_DIR = ../extensions
PVT_EXT_DIRS = $(sort $(dir $(wildcard $(PVT_EXT_DIR)/*/extensions/*/)))

INT_EXT_DIRS1 = $(sort $(dir $(wildcard extensions/*/)))
EXT_SKIP1 = $(addsuffix /,$(addprefix extensions/,$(EXT_SKIP)))
INT_EXT_DIRS = $(subst $(EXT_SKIP1),,$(INT_EXT_DIRS1))

EXT_DIRS = $(INT_EXT_DIRS) $(PVT_EXT_DIRS)

PVT_EXTS = $(subst $(PVT_EXT_DIR)/,,$(wildcard $(PVT_EXT_DIR)/*))
INT_EXTS = $(subst /,,$(subst extensions/,,$(wildcard $(INT_EXT_DIRS))))
EXTS = $(INT_EXTS) $(PVT_EXTS)

GPS = gps gps/ka9q-fec gps/GNSS-SDRLIB
RX = rx rx/CuteSDR rx/Teensy rx/wdsp rx/csdr rx/kiwi rx/CMSIS
ifneq ($(RPI),true)
	_DIRS = pru $(PKGS)
endif
_DIR_PLATFORMS = $(addprefix platform/, ${PLATFORMS})
_DIRS_O3 += . $(PKGS_O3) platform/common $(_DIR_PLATFORMS) $(EXT_DIRS) $(EXT_SUBDIRS) \
	$(RX) $(GPS) ui init support net web arch/$(ARCH)

ifeq ($(OPT),0)
	DIRS = $(_DIRS) $(_DIRS_O3)
	DIRS_O3 =
else
	DIRS = $(_DIRS)
	DIRS_O3 = $(_DIRS_O3)
endif

VPATH = $(DIRS) $(DIRS_O3) $(EXT_SUBDIRS_KEEP)
I = -I$(GEN_DIR) $(addprefix -I,$(DIRS)) $(addprefix -I,$(DIRS_O3)) $(addprefix -I,$(EXT_SUBDIRS_KEEP)) -I/usr/local/include
H = $(wildcard $(addsuffix /*.h,$(DIRS))) $(wildcard $(addsuffix /*.h,$(DIRS_O3)))
CPP_F = $(wildcard $(addsuffix /*.cpp,$(DIRS)))
CPP_F_O3 = $(wildcard $(addsuffix /*.cpp,$(DIRS_O3)))
CFILES_KEEP = $(wildcard $(addsuffix /*.cpp,$(EXT_SUBDIRS_KEEP)))

# remove generated files
CFILES = $(subst web/web.cpp,,$(CPP_F))
CFILES_O3 = $(subst web/web.cpp,,$(CPP_F_O3))

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	ifeq ($(XC),-DXC)
		LIBS += -lfftw3f -lutil
		DIR_CFG = /root/kiwi.config
		CFG_PREFIX =
	else
		# development machine, compile simulation version
		LIBS += -L/usr/local/lib -lfftw3f
		LIBS_DEP += /usr/local/lib/libfftw3f.a
		CMD_DEPS =
		DIR_CFG = unix_env/kiwi.config
		CFG_PREFIX = dist.
	endif

else
	# host machine (BBB), only build the FPGA-using version
	LIBS += -lfftw3f -lutil
	LIBS_DEP += /usr/lib/arm-linux-gnueabihf/libfftw3f.a
	CMD_DEPS = $(CMD_DEPS_DEBIAN) /usr/sbin/avahi-autoipd /usr/bin/upnpc /usr/bin/dig /usr/bin/pnmtopng /sbin/ethtool /usr/bin/sshpass
	CMD_DEPS += /usr/bin/killall /usr/bin/dtc /usr/bin/curl /usr/bin/wget
	DIR_CFG = /root/kiwi.config
	CFG_PREFIX =

	ifeq ($(BBAI),true)
		CMD_DEPS += /usr/bin/cpufreq-info
	endif

	# -lrt required for clock_gettime() on Debian 7; see clock_gettime(2) man page
	# jq command isn't available on Debian 7
	ifeq ($(DEBIAN_7),true)
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

KEYRING = $(DIR_CFG)/.keyring.dep
$(KEYRING):
	@echo "KEYRING.."
ifeq ($(DEBIAN_7),true)
	cp /etc/apt/sources.list /etc/apt/sources.list.orig
	sed -e 's/ftp\.us/archive/' < /etc/apt/sources.list >/tmp/sources.list
	mv /tmp/sources.list /etc/apt/sources.list
endif
	-apt-get -y update
	-apt-get -y install debian-archive-keyring
	-apt-get -y update
	@mkdir -p $(DIR_CFG)
	touch $(KEYRING)

/usr/lib/arm-linux-gnueabihf/libfftw3f.a:
	apt-get -y install libfftw3-dev

# NB not a typo: "clang-6.0" vs "clang-7"

/usr/bin/clang-6.0:
	# only available recently?
	-apt-get -y update
	apt-get -y install clang-6.0

/usr/bin/clang-7:
	-apt-get -y update
	apt-get -y install clang-7

/usr/bin/curl:
	-apt-get -y install curl

/usr/bin/wget:
	-apt-get -y install wget

/usr/sbin/avahi-autoipd:
	-apt-get -y install avahi-daemon avahi-utils libnss-mdns avahi-autoipd

/usr/bin/upnpc:
	-apt-get -y install miniupnpc

/usr/bin/dig:
	-apt-get -y install dnsutils

/usr/bin/pnmtopng:
	-apt-get -y install pnmtopng

/sbin/ethtool:
	-apt-get -y install ethtool

/usr/bin/sshpass:
	-apt-get -y install sshpass

/usr/bin/killall:
	-apt-get -y install psmisc

/usr/bin/dtc:
	-apt-get -y install device-tree-compiler

ifeq ($(BBAI),true)
/usr/bin/cpufreq-info:
	-apt-get -y install cpufrequtils
endif

ifneq ($(DEBIAN_7),true)
/usr/bin/jq:
	-apt-get -y install jq
endif

endif


################################
# dependencies
################################

#SRC_DEPS = Makefile
SRC_DEPS = 
BIN_DEPS = KiwiSDR.rx4.wf4.bit KiwiSDR.rx8.wf2.bit KiwiSDR.rx3.wf3.bit KiwiSDR.rx14.wf0.bit
#BIN_DEPS = 
DEVEL_DEPS = $(OBJ_DIR_DEFAULT)/web_devel.o $(KEEP_DIR)/edata_always.o
EMBED_DEPS = $(OBJ_DIR_DEFAULT)/web_embed.o $(OBJ_DIR)/edata_embed.o $(KEEP_DIR)/edata_always.o
EXTS_DEPS = $(OBJ_DIR)/ext_init.o

# these MUST be run by single-threaded make before use of -j in sub makes
GEN_ASM = $(GEN_DIR)/kiwi.gen.h verilog/kiwi.gen.vh
OUT_ASM = $(GEN_DIR)/kiwi.aout
GEN_VERILOG = $(addprefix verilog/rx/,cic_rx1_12k.vh cic_rx1_20k.vh cic_rx2_12k.vh cic_rx2_20k.vh cic_rx3_12k.vh cic_rx3_20k.vh cic_wf1.vh cic_wf2.vh)
GEN_NOIP2 = $(GEN_DIR)/noip2
SUB_MAKE_DEPS = $(KEYRING) $(CMD_DEPS) $(LIBS_DEP) $(GEN_ASM) $(OUT_ASM) $(GEN_VERILOG) $(GEN_NOIP2)


################################
# flags
################################

MF_INC = $(GEN_DIR)/Makefile.inc
START_MF_INC = | tee $(MF_INC) 2>&1
APPEND_MF_INC = | tee -a $(MF_INC) 2>&1

VERSION = -DVERSION_MAJ=$(VERSION_MAJ) -DVERSION_MIN=$(VERSION_MIN)
VER = v$(VERSION_MAJ).$(VERSION_MIN)
V = -Dv$(VERSION_MAJ)_$(VERSION_MIN)

INT_FLAGS += $(VERSION) -DKIWI -DKIWISDR -DARCH_$(ARCH) -DCPU_$(CPU) -DARCH_CPU=$(CPU) $(addprefix -DPLATFORM_,$(PLATFORMS))
INT_FLAGS += -DDIR_CFG=STRINGIFY\($(DIR_CFG)\) -DCFG_PREFIX=STRINGIFY\($(CFG_PREFIX)\)
INT_FLAGS += -DBUILD_DIR=STRINGIFY\($(BUILD_DIR)\) -DREPO=STRINGIFY\($(REPO)\) -DREPO_NAME=STRINGIFY\($(REPO_NAME)\)


################################
# conversion to clang
################################

# NB: afterwards have to rerun make to pickup filename change!
ifneq ($(PVT_EXT_DIRS),)
	PVT_EXT_C_FILES = $(shell find $(PVT_EXT_DIRS) -name '*.c' -print)
endif

.PHONY: c_ext_clang_conv
c_ext_clang_conv: $(SUB_MAKE_DEPS)
ifeq ($(PVT_EXT_C_FILES),)
#	@echo SUB_MAKE_DEPS = $(SUB_MAKE_DEPS)
#	@echo no installed extensions with files needing conversion from .c to .cpp for clang compatibility
else
	@echo PVT_EXT_C_FILES = $(PVT_EXT_C_FILES)
	@echo convert installed extension .c files to .cpp for clang compatibility
	find $(PVT_EXT_DIRS) -name '*.c' -exec mv '{}' '{}'pp \;
endif

.PHONY: build_makefile_inc
build_makefile_inc:
# consolidate flags into indirect Makefile since Make will be re-invoked
	@echo "----------------"
	@echo "building" $(MF_INC)
	@echo $(VER)
	@echo PLATFORMS = $(PLATFORMS)
	@echo DEBUG = $(DEBUG)
	@echo XC = $(XC)
	@echo
#
	@echo $(I) $(START_MF_INC)
#
	@echo $(APPEND_MF_INC)
	@echo $(CFLAGS) $(APPEND_MF_INC)
#
	@echo $(APPEND_MF_INC)
	@echo $(CPP_FLAGS) $(APPEND_MF_INC)
#
	@echo $(APPEND_MF_INC)
	@echo $(EXT_DEFINES) $(APPEND_MF_INC)
#
	@echo $(APPEND_MF_INC)
	@echo $(INT_FLAGS) $(APPEND_MF_INC)
#
	@echo "----------------"

.PHONY: c_ext_clang_conv_all
c_ext_clang_conv_all: $(BUILD_DIR)/kiwi.bin


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
# Verilog generator
################################
$(GEN_VERILOG): $(GEN_DIR)/kiwi.gen.h verilog/rx/cic_gen.c
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	(cd verilog/rx; make)
endif


################################
# FPGA embedded CPU
################################
$(GEN_ASM): kiwi.config $(wildcard e_cpu/asm/*)
	(cd e_cpu; make)
$(OUT_ASM): $(wildcard e_cpu/*.asm)
	(cd e_cpu; make no_gen)


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
#	x ant_switch still works?
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
	$(CC) $(I) -g $(FILE_OPTIM_SRC) -o $@

-include $(wildcard web/*/Makefile)
-include $(wildcard web/extensions/*/Makefile)
-include web/Makefile

# NB: $(FILE_OPTIM) *MUST* be here so "make install" builds EDATA_EMBED properly when NFS_READ_ONLY == yes
#EDATA_DEP = web/kiwi/Makefile web/openwebrx/Makefile web/pkgs/Makefile web/extensions/Makefile $(wildcard extensions/*/Makefile) $(FILE_OPTIM)
EDATA_DEP = web/kiwi/Makefile web/openwebrx/Makefile web/pkgs/Makefile web/extensions/Makefile $(FILE_OPTIM)

.PHONY: foptim_gen foptim_list foptim_clean

# NEVER let target Kiwis contact external optimization site via foptim_gen.
# If customers are developing they need to do a "make install" on a development machine
# OR a "make fopt/foptim" on the Kiwi to explicitly build the optimized files (but only if not NFS_READ_ONLY)

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
foptim_gen: foptim_files_embed foptim_ext foptim_files_maps
	@echo
else
foptim_gen:
endif

ifeq ($(NFS_READ_ONLY),yes)
fopt foptim:
	@echo "can't do fopt/foptim because of NFS_READ_ONLY=$(NFS_READ_ONLY)"
	@echo "(assumed foptim_gen performed on development machine with a make install)"
else
fopt foptim: foptim_files_embed foptim_ext foptim_files_maps
endif

foptim_list: loptim_embed loptim_ext loptim_maps
foptim_clean: roptim_embed roptim_ext roptim_maps

FILES_EMBED_SORTED_NW = $(sort $(EMBED_NW) $(EXT_EMBED_NW) $(PKGS_MAPS_EMBED_NW))
FILES_ALWAYS_SORTED_NW = $(sort $(FILES_ALWAYS))
#FILES_ALWAYS2_SORTED_NW = $(sort $(FILES_ALWAYS2))

EDATA_EMBED = $(GEN_DIR)/edata_embed.cpp
EDATA_ALWAYS = $(GEN_DIR)/edata_always.cpp

$(EDATA_EMBED): $(EDATA_DEP) $(addprefix web/,$(FILES_EMBED_SORTED_NW))
	(cd web; perl mkdata.pl edata_embed $(FILES_EMBED_SORTED_NW) >../$(EDATA_EMBED))

$(EDATA_ALWAYS): $(EDATA_DEP) $(addprefix web/,$(FILES_ALWAYS_SORTED_NW))
	(cd web; perl mkdata.pl edata_always $(FILES_ALWAYS_SORTED_NW) >../$(EDATA_ALWAYS))


################################
# vars
################################
.PHONY: vars
vars: c_ext_clang_conv
	@make $(MAKE_ARGS) c_ext_clang_conv_vars

.PHONY: c_ext_clang_conv_vars
c_ext_clang_conv_vars:
	@echo version $(VER)
	@echo UNAME = $(UNAME)
	@echo DEBIAN_DEVSYS = $(DEBIAN_DEVSYS)
	@echo ARCH = $(ARCH)
	@echo CPU = $(CPU)
	@echo PLATFORMS = $(PLATFORMS)
	@echo BUILD_DIR = $(BUILD_DIR)
	@echo OBJ_DIR = $(OBJ_DIR)
	@echo OBJ_DIR_O3 = $(OBJ_DIR_O3)
	@echo OBJ_DIR_DEFAULT = $(OBJ_DIR_DEFAULT)
	@echo CMD_DEPS = $(CMD_DEPS)
	@echo OPT = $(OPT)
	@echo CFLAGS = $(CFLAGS)
	@echo CPP_FLAGS = $(CPP_FLAGS)
	@echo
	@echo DEPS = $(OBJECTS:.o=.d)
	@echo SRC_DEPS = $(SRC_DEPS)
	@echo BIN_DEPS = $(BIN_DEPS)
	@echo SUB_MAKE_DEPS = $(SUB_MAKE_DEPS)
	@echo GEN_ASM = $(GEN_ASM)
	@echo
	@echo FILES_EMBED = $(FILES_EMBED)
	@echo FILES_EXT = $(FILES_EXT)
	@echo FILES_ALWAYS = $(FILES_ALWAYS)
#	@echo FILES_ALWAYS2 = $(FILES_ALWAYS2)
	@echo
	@echo EXT_SKIP = $(EXT_SKIP)
	@echo EXT_SKIP1 = $(EXT_SKIP1)
	@echo INT_EXT_DIRS1 = $(INT_EXT_DIRS1)
	@echo INT_EXT_DIRS = $(INT_EXT_DIRS)
	@echo PVT_EXT_DIRS = $(PVT_EXT_DIRS)
	@echo EXT_SUBDIRS = $(EXT_SUBDIRS)
	@echo EXT_DIRS = $(EXT_DIRS)
	@echo PVT_EXTS = $(PVT_EXTS)
	@echo INT_EXTS = $(INT_EXTS)
	@echo EXTS = $(EXTS)
	@echo
	@echo DIRS = $(DIRS)
	@echo DIRS_O3 = $(DIRS_O3)
	@echo VPATH = $(VPATH)
	@echo CFILES = $(CFILES)
	@echo CFILES_O3 = $(CFILES_O3)
	@echo CFILES_KEEP = $(CFILES_KEEP)
	@echo OBJECTS = $(OBJECTS)
	@echo O3_OBJECTS = $(O3_OBJECTS)
	@echo KEEP_OBJECTS = $(KEEP_OBJECTS)
	@echo MF_FILES = $(MF_FILES)
	@echo MF_OBJ = $(MF_OBJ)
	@echo MF_O3 = $(MF_O3)
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

# extension init generator and extension-specific makefiles
-include extensions/Makefile

CSRC = $(notdir $(CFILES))
OBJECTS1 = $(CSRC:%.c=$(OBJ_DIR)/%.o)
OBJECTS = $(OBJECTS1:%.cpp=$(OBJ_DIR)/%.o)

CSRC_O3 = $(notdir $(CFILES_O3))
O3_OBJECTS1 = $(CSRC_O3:%.c=$(OBJ_DIR_O3)/%.o)
O3_OBJECTS = $(O3_OBJECTS1:%.cpp=$(OBJ_DIR_O3)/%.o)

CSRC_KEEP = $(notdir $(CFILES_KEEP))
KEEP_OBJECTS1 = $(CSRC_KEEP:%.c=$(KEEP_DIR)/%.o)
KEEP_OBJECTS = $(KEEP_OBJECTS1:%.cpp=$(KEEP_DIR)/%.o)


# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)
-include $(O3_OBJECTS:.o=.d)
-include $(KEEP_OBJECTS:.o=.d)
-include $(DEVEL_DEPS:.o=.d)
-include $(EMBED_DEPS:.o=.d)
-include $(EXTS_DEPS:.o=.d)

COMP_CTR = $(BUILD_DIR)/.comp_ctr
C_CTR_LINK = 9997
C_CTR_INSTALL = 9998
C_CTR_DONE = 9999

.PHONY: c_ctr_reset
c_ctr_reset:
	@echo 1 >$(COMP_CTR)

#
# IMPORTANT
#
# By not including foptim_gen in the dependency list for the development build target $(BUILD_DIR)/kiwi.bin
# the external optimization site won't be called all the time as incremental changes are made to
# the js/css/html/image files.
#
# But this means a "make install" must be performed on the *development machine* prior to installation
# on targets or before uploading as a software release.
# Previously doing a "make install" on the development machine made no sense and was flagged as an error.
#

$(BUILD_DIR)/kiwi.bin: c_ctr_reset $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(OBJECTS) $(O3_OBJECTS) $(KEEP_OBJECTS) $(BIN_DEPS) $(DEVEL_DEPS) $(EXTS_DEPS)
	@echo $(C_CTR_LINK) >$(COMP_CTR)
ifneq ($(SAN),1)
	$(CPP) $(LDFLAGS) $(OBJECTS) $(O3_OBJECTS) $(KEEP_OBJECTS) $(DEVEL_DEPS) $(EXTS_DEPS) $(LIBS) -o $(BUILD_OBJ)
else
	@echo loader skipped for static analysis
endif

$(BUILD_DIR)/kiwid.bin: c_ctr_reset foptim_gen $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(OBJECTS) $(O3_OBJECTS) $(KEEP_OBJECTS) $(BIN_DEPS) $(EMBED_DEPS) $(EXTS_DEPS)
	@echo $(C_CTR_LINK) >$(COMP_CTR)
ifneq ($(SAN),1)
	$(CPP) $(LDFLAGS) $(OBJECTS) $(O3_OBJECTS) $(KEEP_OBJECTS) $(EMBED_DEPS) $(EXTS_DEPS) $(LIBS) -o $@
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
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) -DEDATA_DEVEL -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_DEFAULT)/web_embed.o: web/web.cpp config.h
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) -DEDATA_EMBED -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/edata_embed.o: $(EDATA_EMBED)
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(KEEP_DIR)/edata_always.o: $(EDATA_ALWAYS)
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/ext_init.o: $(GEN_DIR)/ext_init.cpp
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
	$(POST_PROCESS_DEPS)


# .c

#$(OBJ_DIR)/%.o: %.c $(SRC_DEPS)
#	$(CC) -x c $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
#	$(CC) $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
#	@expr `cat $(COMP_CTR)` + 1 >$(COMP_CTR)
#	$(POST_PROCESS_DEPS)

#$(OBJ_DIR_O3)/%.o: %.c $(SRC_DEPS)
#	$(CC) $(OPTS_VIS_OPT) @$(MF_INC) -c -o $@ $<
#	@expr `cat $(COMP_CTR)` + 1 >$(COMP_CTR)
#	$(POST_PROCESS_DEPS)


# .cpp $(CFLAGS_UNSAFE_OPT)

$(OBJ_DIR_O3)/search.o: search.cpp $(SRC_DEPS)
	$(CPP) $(OPTS_VIS_OPT) $(CFLAGS_UNSAFE_OPT) @$(MF_INC) -c -o $@ $<
#	@expr `cat $(COMP_CTR)` + 1 >$(COMP_CTR)
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_O3)/simd.o: simd.cpp $(SRC_DEPS)
	$(CPP) $(OPTS_VIS_OPT) $(CFLAGS_UNSAFE_OPT) @$(MF_INC) -c -o $@ $<
#	@expr `cat $(COMP_CTR)` + 1 >$(COMP_CTR)
	$(POST_PROCESS_DEPS)


# .cpp

$(OBJ_DIR)/%.o: %.cpp $(SRC_DEPS)
	$(CPP) $(OPTS_VIS_UNOPT) @$(MF_INC) -c -o $@ $<
#	@expr `cat $(COMP_CTR)` + 1 >$(COMP_CTR)
	$(POST_PROCESS_DEPS)

$(KEEP_DIR)/%.o: %.cpp $(SRC_DEPS)
	$(CPP) $(OPTS_VIS_OPT) @$(MF_INC) -c -o $@ $<
#	@expr `cat $(COMP_CTR)` + 1 >$(COMP_CTR)
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_O3)/%.o: %.cpp $(SRC_DEPS)
	$(CPP) $(OPTS_VIS_OPT) @$(MF_INC) -c -o $@ $<
#	@expr `cat $(COMP_CTR)` + 1 >$(COMP_CTR)
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

REBOOT = $(DIR_CFG)/.reboot
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	DO_ONCE =
else
	DO_ONCE = $(DIR_CFG)/.do_once.dep
endif

$(DO_ONCE):
ifeq ($(BBAI),true)
	@echo "disable window system"
	-systemctl stop lightdm.service
	-systemctl disable lightdm.service
	make install_kiwi_device_tree
	@touch $(REBOOT)
endif
	@mkdir -p $(DIR_CFG)
	@touch $(DO_ONCE)

ifeq ($(BBAI),true)
DIR_DTB = dtb-$(SYS_MAJ).$(SYS_MIN)-ti
install_kiwi_device_tree:
	@echo "install Kiwi device tree to configure GPIO pins"
	@echo $(SYS_MAJ).$(SYS_MIN) $(SYS)
	cp platform/beaglebone_AI/am5729-beagleboneai-kiwisdr-cape.dts /opt/source/$(DIR_DTB)/src/arm
	(cd /opt/source/$(DIR_DTB); make)
	cp /opt/source/$(DIR_DTB)/src/arm/am5729-beagleboneai-kiwisdr-cape.dtb /boot/dtbs/$(SYS)
	cp /opt/source/$(DIR_DTB)/src/arm/am5729-beagleboneai-kiwisdr-cape.dtb /boot/dtbs/$(SYS)/am5729-beagleboneai.dtb
endif

V_DIR = ~/shared/shared

ifeq ($(XC),) ## do not copy bit streams from ~/shared/shared when cross-compiling
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))

KiwiSDR.rx4.wf4.bit: $(V_DIR)/KiwiSDR.rx4.wf4.bit
	rsync -av $(V_DIR)/KiwiSDR.rx4.wf4.bit .

KiwiSDR.rx8.wf2.bit: $(V_DIR)/KiwiSDR.rx8.wf2.bit
	rsync -av $(V_DIR)/KiwiSDR.rx8.wf2.bit .

KiwiSDR.rx3.wf3.bit: $(V_DIR)/KiwiSDR.rx3.wf3.bit
	rsync -av $(V_DIR)/KiwiSDR.rx3.wf3.bit .

KiwiSDR.rx14.wf0.bit: $(V_DIR)/KiwiSDR.rx14.wf0.bit
	rsync -av $(V_DIR)/KiwiSDR.rx14.wf0.bit .

endif
endif

DEV = kiwi
CAPE = cape-bone-$(DEV)-00A0
SPI  = cape-bone-$(DEV)-S-00A0
PRU  = cape-bone-$(DEV)-P-00A0

DIR_CFG_SRC = unix_env/kiwi.config

EXISTS_BASHRC_LOCAL = $(shell test -f ~root/.bashrc.local && echo true)

CFG_KIWI = kiwi.json
EXISTS_KIWI = $(shell test -f $(DIR_CFG)/$(CFG_KIWI) && echo true)

CFG_ADMIN = admin.json
EXISTS_ADMIN = $(shell test -f $(DIR_CFG)/$(CFG_ADMIN) && echo true)

CFG_CONFIG = config.js
EXISTS_CONFIG = $(shell test -f $(DIR_CFG)/$(CFG_CONFIG) && echo true)

CFG_DX = dx.json
EXISTS_DX = $(shell test -f $(DIR_CFG)/$(CFG_DX) && echo true)

CFG_DX_MIN = dx.min.json
EXISTS_DX_MIN = $(shell test -f $(DIR_CFG)/$(CFG_DX_MIN) && echo true)

ETC_HOSTS_HAS_KIWI = $(shell grep -qi kiwisdr /etc/hosts && echo true)

SSH_KEYS = /root/.ssh/authorized_keys
EXISTS_SSH_KEYS = $(shell test -f $(SSH_KEYS) && echo true)

# Doing a 'make install' on the development machine is only used to build the optimized files.
# For the Beagle this installs the device tree files in the right place and other misc stuff.
# DANGER: do not use $(MAKE_ARGS) here! The targets for building $(EMBED_DEPS) must be run sequentially

.PHONY: install
install: c_ext_clang_conv
	@# don't use MAKE_ARGS here!
	@make c_ext_clang_conv_install

# copy binaries to Kiwi named $(KIWI_XC_HOST)
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
RSYNC_XC = rsync -av -e "ssh -p $(KIWI_XC_HOST_PORT) -l root"

.PHONY: install_xc
ifeq ($(XC),)
install_xc:
	@make XC=-DXC $@
else
install_xc: c_ext_clang_conv
	# NB: --relative option use of "/./" to delimit creation dir path
	$(RSYNC_XC) --relative .././build/kiwi.bin root@$(KIWI_XC_HOST):~root/
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

.PHONY: c_ext_clang_conv_install
c_ext_clang_conv_install: $(DO_ONCE) $(BUILD_DIR)/kiwid.bin
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	# remainder of "make install" only makes sense to run on target
else
	@echo $(C_CTR_INSTALL) >$(COMP_CTR)
# don't strip symbol table while we're debugging daemon crashes
#	install -D -s -o root -g root $(BUILD_DIR)/kiwid.bin /usr/local/bin/kiwid
	install -D -o root -g root $(BUILD_DIR)/kiwid.bin /usr/local/bin/kiwid
	install -D -o root -g root $(GEN_DIR)/kiwi.aout /usr/local/bin/kiwid.aout
#	install -D -o root -g root $(GEN_DIR)/kiwi_realtime.bin /usr/local/bin/kiwid_realtime.bin
	install -D -o root -g root KiwiSDR.rx4.wf4.bit /usr/local/bin/KiwiSDR.rx4.wf4.bit
	install -D -o root -g root KiwiSDR.rx8.wf2.bit /usr/local/bin/KiwiSDR.rx8.wf2.bit
	install -D -o root -g root KiwiSDR.rx3.wf3.bit /usr/local/bin/KiwiSDR.rx3.wf3.bit
	install -D -o root -g root KiwiSDR.rx14.wf0.bit /usr/local/bin/KiwiSDR.rx14.wf0.bit
#
	install -o root -g root unix_env/kiwid /etc/init.d
	install -o root -g root -m 0644 unix_env/kiwid.service /etc/systemd/system
	install -D -o root -g root -m 0644 unix_env/$(CAPE).dts /lib/firmware/$(CAPE).dts
	install -D -o root -g root -m 0644 unix_env/$(SPI).dts /lib/firmware/$(SPI).dts
	install -D -o root -g root -m 0644 unix_env/$(PRU).dts /lib/firmware/$(PRU).dts
#
	install -D -o root -g root $(GEN_DIR)/noip2 /usr/local/bin/noip2
#
	install -D -o root -g root -m 0644 $(DIR_CFG_SRC)/frpc.template.ini $(DIR_CFG)/frpc.template.ini
	install -D -o root -g root pkgs/frp/frpc /usr/local/bin/frpc
#
	install -D -o root -g root -m 0644 unix_env/bashrc ~root/.bashrc
	install -D -o root -g root -m 0644 unix_env/profile ~root/.profile
#
	install -D -o root -g root -m 0644 unix_env/gdbinit ~root/.gdbinit
	install -D -o root -g root -m 0644 unix_env/gdb_break ~root/.gdb_break
	install -D -o root -g root -m 0644 unix_env/gdb_valgrind ~root/.gdb_valgrind
#
	install -D -o root -g root -m 0644 $(DIR_CFG_SRC)/v.sed $(DIR_CFG)/v.sed
#
	rsync -av --delete $(DIR_CFG_SRC)/samples/ $(DIR_CFG)/samples

# only install post-customized config files if they've never existed before
ifneq ($(EXISTS_BASHRC_LOCAL),true)
	@echo installing .bashrc.local
	cp unix_env/bashrc.local ~root/.bashrc.local
endif

ifneq ($(EXISTS_KIWI),true)
	@echo installing $(DIR_CFG)/$(CFG_KIWI)
	@mkdir -p $(DIR_CFG)
	cp $(DIR_CFG_SRC)/dist.$(CFG_KIWI) $(DIR_CFG)/$(CFG_KIWI)

# don't prevent admin.json transition process
ifneq ($(EXISTS_ADMIN),true)
	@echo installing $(DIR_CFG)/$(CFG_ADMIN)
	@mkdir -p $(DIR_CFG)
	cp $(DIR_CFG_SRC)/dist.$(CFG_ADMIN) $(DIR_CFG)/$(CFG_ADMIN)
endif
endif

ifneq ($(EXISTS_DX),true)
	@echo installing $(DIR_CFG)/$(CFG_DX)
	@mkdir -p $(DIR_CFG)
	cp $(DIR_CFG_SRC)/dist.$(CFG_DX) $(DIR_CFG)/$(CFG_DX)
endif

ifneq ($(EXISTS_DX_MIN),true)
	@echo installing $(DIR_CFG)/$(CFG_DX_MIN)
	@mkdir -p $(DIR_CFG)
	cp $(DIR_CFG_SRC)/dist.$(CFG_DX_MIN) $(DIR_CFG)/$(CFG_DX_MIN)
endif

ifneq ($(EXISTS_CONFIG),true)
	@echo installing $(DIR_CFG)/$(CFG_CONFIG)
	@mkdir -p $(DIR_CFG)
	cp $(DIR_CFG_SRC)/dist.$(CFG_CONFIG) $(DIR_CFG)/$(CFG_CONFIG)
endif

ifneq ($(ETC_HOSTS_HAS_KIWI),true)
	@echo appending kiwisdr to /etc/hosts
	@echo '127.0.0.1       kiwisdr' >>/etc/hosts
endif

	systemctl enable kiwid.service
	@echo $(C_CTR_DONE) >$(COMP_CTR)

# remove public keys leftover from development
ifeq ($(EXISTS_SSH_KEYS),true)
	@-sed -i -e '/.*jks-/d' $(SSH_KEYS)
endif

# must be last obviously
	@if [ -f $(REBOOT) ]; then rm $(REBOOT); echo "MUST REBOOT FOR CHANGES TO TAKE EFFECT"; echo -n "Press \"return\" key to reboot else control-C: "; read in; reboot; fi;

endif


################################
# operation
################################
ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

enable disable start stop restart status:
	-systemctl --full --lines=100 $@ kiwid.service || true

# SIGUSR1 == SIG_DUMP
reload dump:
	-killall -q -s USR1 kiwid
	-killall -q -s USR1 kiwi.bin

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
	-@$(LOGS) | grep kiwid
	@rm -f /tmp/kiwi.log

slog:
	-@cat /var/log/user.log | grep kiwid

tlog:
	-@cat /var/log/user.log | grep kiwid | tail -500

syslog:
	tail -n 1000 -f /var/log/syslog

flog:
	tail -n 100 -f /var/log/frpc.log

LOCAL_IP = grep -vi 192.168.1
LEAVING = grep -i leaving | grep -vi kf6vo | $(LOCAL_IP)
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
OPT_GIT_NO_HTTPS = $(shell test -f /root/kiwi.config/opt.git_no_https && echo true)
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
	git pull -v $(GIT_PROTO)://github.com/jks-prv/Beagle_SDR_GPS.git

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
	@echo KiwiSDR cape EEPROM:
ifeq ($(DEBIAN_7),true)
	hexdump -C /sys/bus/i2c/devices/1-0054/eeprom
else
	ifeq ($(BBAI),true)
		hexdump -C /sys/bus/i2c/devices/3-0054/eeprom
	else
		ifeq ($(RPI),true)
			hexdump -C /sys/bus/i2c/devices/1-0054/eeprom
		else
			hexdump -C /sys/bus/i2c/devices/3-0054/eeprom
		endif
	endif
endif
	@echo
	@echo BeagleBone EEPROM:
	hexdump -C /sys/bus/i2c/devices/0-0050/eeprom

REPO = https://github.com/jks-prv/$(REPO_NAME).git

# selectively transfer files to the target so everything isn't compiled each time
EXCLUDE_RSYNC = ".DS_Store" ".git" "/obj" "/obj_O3" "/obj_keep" "*.dSYM" "*.bin" "*.aout" "e_cpu/a" "*.aout.h" "kiwi.gen.h" \
	"verilog/kiwi.gen.vh" "web/edata*" "node_modules" "morse-pro-compiled.js"
RSYNC_ARGS = -av --delete $(addprefix --exclude , $(EXCLUDE_RSYNC)) . root@$(HOST):~root/$(REPO_NAME)
RSYNC = rsync $(RSYNC_ARGS)
RSYNC_PORT = rsync -e "ssh -p $(PORT) -l root" $(RSYNC_ARGS)

rsync_su:
	sudo $(RSYNC)
rsync_port:
	sudo $(RSYNC_PORT)
rsync_bit:
	rsync -av $(V_DIR)/KiwiSDR.rx4.wf4.bit $(V_DIR)/KiwiSDR.rx8.wf2.bit $(V_DIR)/KiwiSDR.rx3.wf3.bit $(V_DIR)/KiwiSDR.rx14.wf0.bit .
	sudo $(RSYNC)
rsync_bit_port:
	rsync -av $(V_DIR)/KiwiSDR.rx4.wf4.bit $(V_DIR)/KiwiSDR.rx8.wf2.bit $(V_DIR)/KiwiSDR.rx3.wf3.bit $(V_DIR)/KiwiSDR.rx14.wf0.bit .
	sudo $(RSYNC_PORT)

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))

# generate the files needed to build the Verilog code
verilog: $(GEN_VERILOG)
	@echo verilog/ directory should now contain all necessary generated files:
	@echo verilog/kiwi.gen.vh, verilog/rx/cic_*.vh

# command to "copy verilog" from KiwiSDR distribution to the Vivado build location
# designed to complement the "make cv2" command run on the Vivado build machine
EXCLUDE_CV = ".DS_Store" "rx/cic_gen" "rx/*.dSYM"
cv: $(GEN_VERILOG)
	rsync -av --delete $(addprefix --exclude , $(EXCLUDE_CV)) verilog/ $(V_DIR)/KiwiSDR
	rsync -av --delete $(addprefix --exclude , $(EXCLUDE_CV)) verilog.Vivado.2014.4.ip/ $(V_DIR)/KiwiSDR.Vivado.2014.4.ip
	rsync -av --delete $(addprefix --exclude , $(EXCLUDE_CV)) verilog.Vivado.2017.4.ip/ $(V_DIR)/KiwiSDR.Vivado.2017.4.ip

cv2:
	@echo "you probably want to use \"make cv\" here"

endif


# files that have moved to $(BUILD_DIR) that are present in earlier versions (e.g. v1.2)
clean_deprecated:
	-rm -rf obj obj_O3 obj_keep kiwi.bin kiwid.bin *.dSYM web/edata*
	-rm -rf *.dSYM pas extensions/ext_init.cpp kiwi.gen.h kiwid kiwid.aout kiwid_realtime.bin .comp_ctr

clean: clean_deprecated
	(cd e_cpu; make clean)
	(cd verilog; make clean)
	(cd verilog/rx; make clean)
	(cd tools; make clean)
	(cd pkgs/noip2; make clean)
	-rm -rf $(addprefix pru/pru_realtime.,bin lst txt) $(TOOLS_DIR)/file_optim
	# but not $(KEEP_DIR)
	-rm -rf $(LOG_FILE) $(BUILD_DIR)/kiwi* $(GEN_DIR) $(OBJ_DIR) $(OBJ_DIR_O3)
	-rm -f Makefile.1

clean_dist: clean
	-rm -rf $(BUILD_DIR)


# The following support the development process
# and are not used for ordinary software builds.

clone:
	git clone $(REPO)

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))

# used by scgit alias
copy_to_git:
	@(echo 'current dir is:'; pwd)
	@echo
	@(cd $(GITAPP)/$(REPO_NAME); echo 'repo branch set to:'; pwd; git branch)
	@echo '################################'
	@echo -n 'did you make install to rebuild the optimized files? '
	@read not_used
	make clean_dist
	rsync -av --delete --exclude .git --exclude .DS_Store . $(GITAPP)/$(REPO_NAME)

copy_from_git:
	@(echo 'current dir is:'; pwd)
	@echo
	@(cd $(GITAPP)/$(REPO_NAME); echo 'repo branch set to:'; pwd; git branch)
	@echo -n 'are you sure? '
	@read not_used
	make clean_dist
	rsync -av --delete --exclude .git --exclude .DS_Store $(GITAPP)/$(REPO_NAME)/. .

# used by gdiff alias
gitdiff:
	colordiff -br --exclude=.DS_Store --exclude=.git "--exclude=*.min.*" $(GITAPP)/$(REPO_NAME) . || true
gitdiff_brief:
	colordiff -br --brief --exclude=.DS_Store --exclude=.git $(GITAPP)/$(REPO_NAME) . || true

endif

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

/usr/bin/xz: $(KEYRING)
	apt-get -y install xz-utils

#
# DANGER: "count=1600M" below (i.e. 1.6 GB) must be larger than the partition size (currently ~1.4 GB)
# computed by the tools/kiwiSDR-make-microSD-flasher-from-eMMC.sh script.
# Otherwise the image file will have strange effects like /boot/uEnv.txt being the correct size but
# filled with zeroed bytes (which of course is a disaster).
#
create_img_from_sd: /usr/bin/xz
	@echo "--- this takes about an hour"
	@echo "--- KiwiSDR server will be stopped to maximize write speed"
	make stop
	dd if=/dev/mmcblk1 bs=1M iflag=count_bytes count=1600M | xz --verbose > ~/KiwiSDR_$(VER)_BBB_Debian_$(DEBIAN_VER).img.xz
	sha256sum ~/KiwiSDR_$(VER)_BBB_Debian_$(DEBIAN_VER).img.xz

endif
