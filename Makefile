#
# Makefile for KiwiSDR project
#

#
# This Makefile can be run on both a build machine (I use a MacBook Pro) and the
# BeagleBone Black target (Debian release).
# Which machine you're on is figured out by this:
#
#	DEBIAN_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
#	DEBIAN = 0
#	NOT_DEBIAN = 1
#	DEVSYS = 2
#	ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
#		...
#	ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
#		...
#
# The '/etc/dogtag' file is present on the Beagle and not on the dev machine.
# Grep returns 0 if "Debian" is found in /etc/dogtag, 1 if it isn't and 2 if /etc/dogtag doesn't exist.
# This same mechanism is used in the wrapper shell script because device tree files need to be
# loaded only on the Beagle.
#

#
# installing FFTW:
#
#	to create /usr/local/lib/libfftw3f.a (the 'f' in '3f' means single precision)
#	Mac:
#		download the sources from the right place
#		./configure --enable-single
#		make
#		(sudo) make install
#	BeagleBone Black, Debian:
#		just install the fftw package using apt-get or aptitude:
#			apt-get install libfftw3-dev
#			aptitude install libfftw3-dev
#
#
# installing libconfig:
#
#	Mac:
#		nothing to install -- dummy routines in code so it will compile
#	BeagleBone Black, Debian:
#			aptitude install libconfig-dev
#

VERSION_MAJ = 0
VERSION_MIN = 5

ARCH = sitara
PLATFORM = beaglebone_black

DEBIAN_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
DEBIAN = 0
NOT_DEBIAN = 1
DEVSYS = 2

# makes compiles fast on dev system
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	OPT = O0
endif

OBJ_DIR = obj
OBJ_DIR_O3 = $(OBJ_DIR)_O3
KEEP_DIR = obj_keep
PKGS = pkgs/closure_compiler pkgs/mongoose

ifeq ($(OPT),O0)
	DIRS = . pru $(PKGS) web platform/$(PLATFORM) $(wildcard apps/*) rx rx/CuteSDR rx/csdr gps ui support arch arch/$(ARCH)
else
	DIRS = . pru $(PKGS) web
endif

ifeq ($(OPT),O0)
	DIRS_O3 =
else
	DIRS_O3 = platform/$(PLATFORM) $(wildcard apps/*) rx rx/CuteSDR rx/csdr gps ui support arch arch/$(ARCH)
endif

VPATH = $(DIRS) $(DIRS_O3)
I = $(addprefix -I,$(DIRS)) $(addprefix -I,$(DIRS_O3))
H = $(wildcard $(addsuffix /*.h,$(DIRS))) $(wildcard $(addsuffix /*.h,$(DIRS_O3)))
C = $(wildcard $(addsuffix /*.c,$(DIRS)))
C1 = $(subst web/web.c,,$(subst web/edata_embed.c,,$(subst web/edata_always.c,,$(C))))
CFILES = $(C1) $(wildcard $(addsuffix /*.cpp,$(DIRS)))
CFILES_O3 = $(wildcard $(addsuffix /*.c,$(DIRS_O3))) $(wildcard $(addsuffix /*.cpp,$(DIRS_O3)))

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
# development machine, compile simulation version
	CFLAGS = -g -MD -DDEBUG -DDEVSYS
	LIBS = -lfftw3f
	LIBS_DEP = /usr/local/lib/libfftw3f.a
else
# host machine (BBB), only build the FPGA-using version
#	CFLAGS = -mfloat-abi=softfp -mfpu=neon
	CFLAGS = -mfpu=neon
#	CFLAGS += -O3
	CFLAGS += -g -MD -DDEBUG -DHOST
	LIBS = -lfftw3f -lconfig
	LIBS_DEP = /usr/lib/arm-linux-gnueabihf/libfftw3f.a /usr/lib/arm-linux-gnueabihf/libconfig.a
endif

#ALL_DEPS = pru/pru_realtime.bin
#SRC_DEPS = Makefile
SRC_DEPS = 
BIN_DEPS =
DEVEL_DEPS = $(OBJ_DIR)/web_devel.o $(KEEP_DIR)/edata_always.o
EMBED_DEPS = $(OBJ_DIR)/web_embed.o $(OBJ_DIR)/edata_embed.o $(KEEP_DIR)/edata_always.o
GEN_ASM = kiwi.gen.h verilog/kiwi.gen.vh
OUT_ASM = e_cpu/kiwi.aout
GEN_VERILOG = verilog/rx/cic_rx1.vh verilog/rx/cic_rx2.vh
ALL_DEPS += $(GEN_ASM) $(OUT_ASM) $(GEN_VERILOG)

.PHONY: all
all: $(LIBS_DEP) $(ALL_DEPS) kiwi.bin

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
/usr/lib/arm-linux-gnueabihf/libfftw3f.a:
	apt-get update
	apt-get -y install libfftw3-dev

/usr/lib/arm-linux-gnueabihf/libconfig.a:
	apt-get update
	apt-get -y install libconfig-dev
endif

# PRU
PASM_INCLUDES = $(wildcard pru/pasm/*.h)
PASM_SOURCE = $(wildcard pru/pasm/*.c)
pas: $(PASM_INCLUDES) $(PASM_SOURCE) Makefile
	cc -Wall -D_UNIX_ -I./pru/pasm $(PASM_SOURCE) -o pas

pru/pru_realtime.bin: pas pru/pru_realtime.p pru/pru_realtime.h pru/pru_realtime.hp
	(cd pru; ../pas -V3 -b -L -l -D_PASM_ -D$(SETUP) pru_realtime.p)

# Verilog generator
$(GEN_VERILOG): kiwi.gen.h verilog/rx/cic_gen.c
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	(cd verilog/rx; make)
endif

# FPGA embedded CPU
$(GEN_ASM): kiwi.config $(wildcard e_cpu/asm/*)
	(cd e_cpu; make)
$(OUT_ASM): e_cpu/kiwi.asm
	(cd e_cpu; make no_gen)

# web server content
KIWI_KEY = .taranaki
#KIWI_KEY =
FILES_EMBED = kiwi.js kiwi_util.js kiwi-data-demod.js
FILES_ALWAYS = proto.jpg pcb.jpg

-include $(wildcard web/*/Makefile)

web/edata_embed.c: $(addprefix web/,$(FILES_EMBED))
	(cd web; perl mkdata.pl edata_embed $(FILES_EMBED) >edata_embed.c)

web/edata_always.c: $(addprefix web/,$(FILES_ALWAYS))
	(cd web; perl mkdata.pl edata_always $(FILES_ALWAYS) >edata_always.c)

comma := ,
empty :=
space := $(empty) $(empty)
#UI_LIST = $(subst $(space),$(comma),$(KIWI_UI_LIST))
UI_LIST = $(subst $(space),,$(KIWI_UI_LIST))

VERSION = -DVERSION_MAJ=$(VERSION_MAJ) -DVERSION_MIN=$(VERSION_MIN)
FLAGS += $(I) $(VERSION) -DKIWI -DARCH_$(ARCH) -DPLATFORM_$(PLATFORM) -DKIWI_KEY=\"$(KIWI_KEY)\"
FLAGS += -DKIWI_UI_LIST=$(UI_LIST) -DDIR_CFG=\"$(DIR_CFG)\" -DREPO=\"$(REPO)\"
CSRC = $(notdir $(CFILES))
CSRC_O3 = $(notdir $(CFILES_O3))
OBJECTS1 = $(CSRC:%.c=$(OBJ_DIR)/%.o)
OBJECTS = $(OBJECTS1:%.cpp=$(OBJ_DIR)/%.o)
O3_OBJECTS1 = $(CSRC_O3:%.c=$(OBJ_DIR_O3)/%.o)
O3_OBJECTS = $(O3_OBJECTS1:%.cpp=$(OBJ_DIR_O3)/%.o)

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)
-include $(O3_OBJECTS:.o=.d)

kiwi.bin: $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(OBJECTS) $(O3_OBJECTS) $(BIN_DEPS) $(DEVEL_DEPS)
	g++ $(OBJECTS) $(O3_OBJECTS) $(DEVEL_DEPS) $(LIBS) -o $@

kiwid.bin: $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(OBJECTS) $(O3_OBJECTS) $(BIN_DEPS) $(EMBED_DEPS)
	g++ $(OBJECTS) $(O3_OBJECTS) $(EMBED_DEPS) $(LIBS) -o $@

debug:
	@echo version $(VERSION_MAJ).$(VERSION_MIN)
	@echo DEPS = $(OBJECTS:.o=.d)
	@echo KIWI_UI_LIST = $(UI_LIST)
	@echo DEBIAN_DEVSYS = $(DEBIAN_DEVSYS)
	@echo SRC_DEPS: $(SRC_DEPS)
	@echo BIN_DEPS: $(BIN_DEPS)
	@echo ALL_DEPS: $(ALL_DEPS)
	@echo GEN_ASM: $(GEN_ASM)
	@echo FILES_EMBED: $(FILES_EMBED)
	@echo FILES_ALWAYS $(FILES_ALWAYS)
	@echo VPATH: $(VPATH)
	@echo CFILES: $(CFILES)
	@echo CFILES_O3: $(CFILES_O3)
	@echo OBJECTS: $(OBJECTS)
	@echo O3_OBJECTS: $(O3_OBJECTS)

// auto generation of dependency info, see:
//	http://scottmcpeak.com/autodepend/autodepend.html
//	http://make.mad-scientist.net/papers/advanced-auto-dependency-generation
df = $(basename $@)
POST_PROCESS_DEPS = \
	@mv -f $(df).d $(df).d.tmp; \
	sed -e 's|.*:|$(df).o:|' < $(df).d.tmp > $(df).d; \
	sed -e 's/.*://' -e 's/\\$$//' < $(df).d.tmp | fmt -1 | \
	sed -e 's/^ *//' -e 's/$$/:/' >> $(df).d; \
	rm -f $(df).d.tmp

$(OBJ_DIR)/web_devel.o: web/web.c config.h
	g++ $(CFLAGS) $(FLAGS) -DEDATA_DEVEL -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/web_embed.o: web/web.c config.h
	g++ $(CFLAGS) $(FLAGS) -DEDATA_EMBED -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/edata_embed.o: web/edata_embed.c
	g++ $(CFLAGS) $(FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(KEEP_DIR)/edata_always.o: web/edata_always.c
	g++ $(CFLAGS) $(FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(KEEP_DIR):
	@mkdir -p $(KEEP_DIR)

$(OBJ_DIR)/%.o: %.c $(SRC_DEPS)
#	g++ -x c $(CFLAGS) $(FLAGS) -c -o $@ $<
	g++ $(CFLAGS) $(FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_O3)/%.o: %.c $(SRC_DEPS)
	g++ -O3 $(CFLAGS) $(FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/%.o: %.cpp $(SRC_DEPS)
	g++ $(CFLAGS) $(FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR_O3)/%.o: %.cpp $(SRC_DEPS)
	g++ -O3 $(CFLAGS) $(FLAGS) -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR_O3):
	@mkdir -p $(OBJ_DIR_O3)

DEV = kiwi
CAPE = cape-bone-$(DEV)-00A0
SPI  = cape-bone-$(DEV)-S-00A0
PRU  = cape-bone-$(DEV)-P-00A0

DIR_CFG = /root/kiwi.config
KIWI_CFG = $(shell test -f $(DIR_CFG)/kiwi.cfg; echo $$?)
CONFIG_JS = $(shell test -f $(DIR_CFG)/config.js; echo $$?)
DX_CFG = $(shell test -f $(DIR_CFG)/dx.cfg; echo $$?)

# Only do a 'make install' on the target machine (not needed on the development machine).
# For the Beagle this installs the device tree files in the right place and other misc stuff.
install: $(LIBS_DEP) $(ALL_DEPS) kiwid.bin
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	@echo only run \'make install\' on target
else
	cp kiwid.bin kiwid
	cp e_cpu/kiwi.aout kiwid.aout
#	cp pru/pru_realtime.bin kiwid_realtime.bin
	cp KiwiSDR.bit KiwiSDRd.bit
# don't strip symbol table while we're debugging daemon crashes
#	install -D -s -o root -g root kiwid /usr/local/bin/kiwid
	install -D -o root -g root kiwid /usr/local/bin/kiwid
	install -D -o root -g root kiwid.aout /usr/local/bin/kiwid.aout
#	install -D -o root -g root kiwid_realtime.bin /usr/local/bin/kiwid_realtime.bin
	install -D -o root -g root KiwiSDR.bit /usr/local/bin/KiwiSDRd.bit
	rm -f kiwid kiwid.aout kiwid_realtime.bin KiwiSDRd.bit
#
	install -o root -g root unix_env/kiwid /etc/init.d
	install -o root -g root unix_env/kiwid.service /etc/systemd/system
	install -D -o root -g root -m 0644 unix_env/$(CAPE).dts /lib/firmware/$(CAPE).dts
	install -D -o root -g root -m 0644 unix_env/$(SPI).dts /lib/firmware/$(SPI).dts
	install -D -o root -g root -m 0644 unix_env/$(PRU).dts /lib/firmware/$(PRU).dts
#
	install -D -o root -g root -m 0644 unix_env/bashrc ~root/.bashrc
	install -D -o root -g root -m 0644 unix_env/bashrc.local ~root/.bashrc.local
	install -D -o root -g root -m 0644 unix_env/profile ~root/.profile

# only install config files if they've never existed before
ifeq ($(KIWI_CFG),1)
	@echo installing $(DIR_CFG)/kiwi.cfg
	@mkdir -p $(DIR_CFG)
	cp kiwi.dist.cfg $(DIR_CFG)/kiwi.cfg
endif

ifeq ($(CONFIG_JS),1)
	@echo installing $(DIR_CFG)/config.js
	@mkdir -p $(DIR_CFG)
	cp web/config.dist.js $(DIR_CFG)/config.js
endif

ifeq ($(DX_CFG),1)
	@echo installing $(DIR_CFG)/dx.cfg
	@mkdir -p $(DIR_CFG)
	cp dx.dist.cfg $(DIR_CFG)/dx.cfg
endif

	systemctl enable kiwid.service

# disable display manager (on HDMI port) to improve real-time response and decrease boot time
	update-rc.d lightdm disable
endif

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
enable disable start stop restart status:
	systemctl --lines=100 $@ kiwid.service

reload:
	killall -s USR1 kiwid
	killall -s USR1 kiwi.bin

dump:
	killall -s USR2 kiwid
	killall -s USR2 kiwi.bin
endif

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
log2:
	grep kiwid /var/log/syslog
endif

ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))

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

slog:
	-@cat /var/log/user.log | grep kiwid

LOCAL_IP = grep -vi 192.168.1
FIJI = grep -vi 103.1.181.74
LEAVING = grep -i leaving | grep -vi tauranga | grep -vi kf6vo | $(LOCAL_IP) | $(FIJI)
leave:
	-@$(LOGS) | $(LEAVING)

USERS = sed -e 's/[^"]* "/"/' | sed -e 's/(LEAVING.*//'
users:
	-@$(LOGS) | $(LEAVING) | $(USERS)

unique:
	-@$(LOGS) | $(LEAVING) | $(USERS) | sort -u

endif

v ver version:
	@echo "you are running version" $(VERSION_MAJ)"."$(VERSION_MIN)

DIST = kiwi
REPO = https://github.com/jks-prv/Beagle_SDR_GPS.git

#ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))

clone:
	git clone $(REPO)

pull:
	git pull -v

tar:
	make clean_dist
	tar cfz ../../$(DIST).tgz .

scp: tar
	scp ../../$(DIST).tgz root@kiwi:~root/kiwi

V_DIR = ~/shared/shared

# selectively transfer files to the target so everything isn't compiled each time
EXCLUDE = "/obj" "/obj_O3" "/obj_keep" "*.dSYM" "*.bin" "*.aout" "e_cpu/a" "*.aout.h" "kiwi.gen.h" "verilog/kiwi.gen.vh" "web/edata*.c"
RSYNC = rsync -av $(PORT) --delete $(addprefix --exclude , $(EXCLUDE)) . root@$(HOST):~root/$(DIST)
rsync:
	$(RSYNC)
rsync_su:
	sudo $(RSYNC)
rsync_bit:
	rsync -av $(V_DIR)/KiwiSDR.bit .
	sudo $(RSYNC)

.PHONY: release
release:
	echo $(VERSION_MAJ).$(VERSION_MIN) >RELEASE
	rsync -av $(V_DIR)/KiwiSDR.bit .
	rsync -av $(PORT) --delete $(addprefix --exclude , $(EXCLUDE)) . root@www:~kiwi_rsync/$(DIST)

V_SRC_DIR = verilog/
V_DST_DIR = $(V_DIR)/KiwiSDR

cv: $(GEN_VERILOG)
	rsync -av --delete $(V_SRC_DIR) $(V_DST_DIR)

cv_old: $(GEN_VERILOG)
	cp -r $(V_SRC_DIR) $(V_DST_DIR)

#endif

clean:
	(cd e_cpu; make clean)
	(cd verilog; make clean)
	(cd verilog/rx; make clean)
	-rm -rf $(OBJ_DIR) $(OBJ_DIR_O3) $(DIST).bin $(DIST)d.bin *.dSYM ../$(DIST).tgz pas $(addprefix pru/pru_realtime.,bin lst txt) web/edata_embed.c $(GEN_ASM) $(GEN_VERILOG) $(DIST)d $(DIST)d.aout $(DIST)d_realtime.bin

clean_keep:
	-rm -rf $(KEEP_DIR) web/edata_always.c *.bit

clean_dist:
	make clean
	make clean_keep
