#
# Makefile for wrx project
#

#
# This Makefile can be run on both a build machine (I use a MacBook Pro) and the
# BeagleBone Black target (Angstrom or Debian release).
# Which machine you're on is figured out by this:
#
#	DEBIAN_ANGSTROM_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
#	DEBIAN = 0
#	ANGSTROM = 1
#	DEVSYS = 2
#	ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
#		...
#	ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))
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
#		./configure --enable-single
#		make
#		(sudo) make install
#	BeagleBone Black, Angstrom:
#		./configure --enable-single --enable-neon "CFLAGS=-O3 -march=armv7-a -mfloat-abi=softfp -mfpu=neon -mtune=cortex-a8 -mthumb-interwork"
#			(--enable-single == --enable-float)
#		make
#		(sudo) make install
#	BeagleBone Black, Debian:
#		just install the fftw package using ? (fixme)
#

VERSION_MAJ = 0
VERSION_MIN = 1

ARCH = sitara
PLATFORM = beaglebone_black

OBJ_DIR = obj
OBJ_DIR_O3 = $(OBJ_DIR)_O3
KEEP_DIR = obj_keep
PKGS = pkgs/closure_compiler pkgs/mongoose
DIRS = . rx rx/CuteSDR gps pru web support arch arch/$(ARCH) $(PKGS)

# some are always compiled with -O3 optimization
DIRS_O3 = platform/$(PLATFORM) $(wildcard apps/*)

VPATH = $(DIRS) $(DIRS_O3)
I = $(addprefix -I,$(DIRS)) $(addprefix -I,$(DIRS_O3))
H1 = $(wildcard $(addsuffix /*.h,$(DIRS))) $(wildcard $(addsuffix /*.h,$(DIRS_O3)))
H = $(subst ./dx.h,,$(H1))
C = $(wildcard $(addsuffix /*.c,$(DIRS)))
C1 = $(subst web/web.c,,$(subst web/edata_embed.c,,$(subst web/edata_always.c,,$(C))))
CFILES = $(C1) $(wildcard $(addsuffix /*.cpp,$(DIRS)))
CFILES_O3 = $(wildcard $(addsuffix /*.c,$(DIRS_O3))) $(wildcard $(addsuffix /*.cpp,$(DIRS_O3)))

DEBIAN_ANGSTROM_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
DEBIAN = 0
ANGSTROM = 1
DEVSYS = 2

ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))
# development machine, compile simulation version
	CFLAGS = -g -MD -DDEBUG
else
# host machine (BBB), only build the FPGA-using version
#	CFLAGS = -mfloat-abi=softfp -mfpu=neon
	CFLAGS = -mfpu=neon
#	CFLAGS += -O3
	CFLAGS += -g -MD -DDEBUG
endif

#ALL_DEPS = pru/pru_realtime.bin
#SRC_DEPS = Makefile
SRC_DEPS = 
BIN_DEPS =
DEVEL_DEPS = $(OBJ_DIR)/web_devel.o $(KEEP_DIR)/edata_always.o
EMBED_DEPS = $(OBJ_DIR)/web_embed.o $(OBJ_DIR)/edata_embed.o $(KEEP_DIR)/edata_always.o
GEN_ASM = wrx.gen.h wrx_v/wrx.gen.vh
OUT_ASM = e_cpu/wrx.aout
GEN_VERILOG = wrx_v/rx/cic_rx1.vh wrx_v/rx/cic_rx2.vh
ALL_DEPS += $(GEN_ASM) $(OUT_ASM) $(GEN_VERILOG)

.PHONY: all
all: $(ALL_DEPS) wrx.bin

# dx list
rx/rx_server.cpp: dx.h
	@touch rx/rx_server.cpp

# PRU
PASM_INCLUDES = $(wildcard pru/pasm/*.h)
PASM_SOURCE = $(wildcard pru/pasm/*.c)
pas: $(PASM_INCLUDES) $(PASM_SOURCE) Makefile
	cc -Wall -D_UNIX_ -I./pru/pasm $(PASM_SOURCE) -o pas

pru/pru_realtime.bin: pas pru/pru_realtime.p pru/pru_realtime.h pru/pru_realtime.hp
	(cd pru; ../pas -V3 -b -L -l -D_PASM_ -D$(SETUP) pru_realtime.p)

# Verilog generator
$(GEN_VERILOG): wrx.gen.h wrx_v/rx/cic_gen.c
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))
	(cd wrx_v/rx; make)
endif

# FPGA embedded CPU
$(GEN_ASM): wrx.config e_cpu/asm/asm.c e_cpu/asm/cpu.h
	(cd e_cpu; make)

$(OUT_ASM): e_cpu/wrx.asm
	(cd e_cpu; make no_gen)

# web server content
WRX_KEY = .taranaki
#WRX_KEY =
FILES_EMBED = config.js wrx.js wrx-data-demod.js
FILES_ALWAYS = proto.jpg pcb.jpg

-include $(wildcard web/*/Makefile)

web/edata_embed.c: $(addprefix web/,$(FILES_EMBED))
	(cd web; perl mkdata.pl edata_embed $(FILES_EMBED) >edata_embed.c)

web/edata_always.c: $(addprefix web/,$(FILES_ALWAYS))
	(cd web; perl mkdata.pl edata_always $(FILES_ALWAYS) >edata_always.c)

comma := ,
empty :=
space := $(empty) $(empty)
#UI_LIST = $(subst $(space),$(comma),$(WRX_UI_LIST))
UI_LIST = $(subst $(space),,$(WRX_UI_LIST))

LIBS = -lfftw3f
VERSION = -DFIRMWARE_VER_MAJ=$(VERSION_MAJ) -DFIRMWARE_VER_MIN=$(VERSION_MIN)
FLAGS += $(I) $(VERSION) -DWRX -DARCH_$(ARCH) -DPLATFORM_$(PLATFORM) -DWRX_KEY=\"$(WRX_KEY)\"
FLAGS += -DWRX_UI_LIST=$(UI_LIST) -DLOGGING_HOST=\"$(LOGGING_HOST)\"
CSRC = $(notdir $(CFILES))
CSRC_O3 = $(notdir $(CFILES_O3))
OBJECTS1 = $(CSRC:%.c=$(OBJ_DIR)/%.o)
OBJECTS = $(OBJECTS1:%.cpp=$(OBJ_DIR)/%.o)
O3_OBJECTS1 = $(CSRC_O3:%.c=$(OBJ_DIR_O3)/%.o)
O3_OBJECTS = $(O3_OBJECTS1:%.cpp=$(OBJ_DIR_O3)/%.o)

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)
-include $(O3_OBJECTS:.o=.d)

wrx.bin: $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(OBJECTS) $(O3_OBJECTS) $(BIN_DEPS) $(DEVEL_DEPS)
	g++ $(OBJECTS) $(O3_OBJECTS) $(DEVEL_DEPS) $(LIBS) -o $@

wrxd.bin: $(OBJ_DIR) $(OBJ_DIR_O3) $(KEEP_DIR) $(OBJECTS) $(O3_OBJECTS) $(BIN_DEPS) $(EMBED_DEPS)
	g++ $(OBJECTS) $(O3_OBJECTS) $(EMBED_DEPS) $(LIBS) -o $@

debug:
	@echo DEPS = $(OBJECTS:.o=.d)
	@echo WRX_UI_LIST = $(UI_LIST)
	@echo DEBIAN_ANGSTROM_DEVSYS = $(DEBIAN_ANGSTROM_DEVSYS)
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

$(OBJ_DIR)/web_devel.o: web/web.c
	g++ $(CFLAGS) $(FLAGS) -DEDATA_DEVEL -c -o $@ $<
	$(POST_PROCESS_DEPS)

$(OBJ_DIR)/web_embed.o: web/web.c
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

DEV = wrx
CAPE = cape-bone-$(DEV)-00A0
PRU  = cape-bone-$(DEV)-P-00A0

# Only do a 'make install' on the target machine (not needed on the development machine).
# For the Beagle this installs the device tree files in the right place and gets NTP configured.
install: wrxd.bin
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))
	@echo only run \'make install\' on target
else
	cp wrxd.bin wrxd
	cp e_cpu/wrx.aout wrxd.aout
#	cp pru/pru_realtime.bin wrxd_realtime.bin
# don't strip symbol table while we're debugging daemon crashes
#	install -D -s -o root -g root wrxd /usr/local/bin/wrxd
	install -D -o root -g root wrxd /usr/local/bin/wrxd
	install -D -o root -g root wrxd.aout /usr/local/bin/wrxd.aout
#	install -D -o root -g root wrxd_realtime.bin /usr/local/bin/wrxd_realtime.bin
	rm -f wrxd wrxd.aout wrxd_realtime.bin
	install -o root -g root unix_env/wrxd /etc/init.d
	install -o root -g root unix_env/wrxd.service /etc/systemd/system
	install -D -o root -g root -m 0644 unix_env/$(CAPE).dts /lib/firmware/$(CAPE).dts
	install -D -o root -g root -m 0644 unix_env/$(PRU).dts /lib/firmware/$(PRU).dts
	install -D -o root -g root -m 0644 unix_env/.bashrc ~root/.bashrc
	install -D -o root -g root -m 0644 unix_env/.profile ~root/.profile

# disable display manager (on HDMI port) to improve real-time response and decrease boot time
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
	systemctl enable wrxd.service
#	update-rc.d wrxd defaults
#	update-rc.d wrxd enable
	update-rc.d lightdm disable
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
	systemctl enable wrxd
	systemctl disable gdm
	/usr/lib/connman/test/set-global-timeservers pool.ntp.org
endif
endif

ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
gdb:
	gdb /usr/local/bin/wrxd /tmp/core-wrxd*
endif

ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
enable disable start stop restart status:
	systemctl --lines=100 $@ wrxd.service
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
enable disable start stop restart status:
	systemctl --lines=100 $@ wrxd
endif

log:
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
	journalctl -b | grep wrxd
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
	grep wrxd /var/log/syslog
endif

LOGS = \
	zcat /var/log/user.log.4.gz > /tmp/wrx.log; \
	zcat /var/log/user.log.3.gz >> /tmp/wrx.log; \
	zcat /var/log/user.log.2.gz >> /tmp/wrx.log; \
	cat /var/log/user.log.1 >> /tmp/wrx.log; \
	cat /var/log/user.log >> /tmp/wrx.log; \
	cat /tmp/wrx.log

LOCAL_IP = grep -vi 192.168.1
FIJI = grep -vi 103.1.181.74
LEAVING = grep -i leaving | grep -vi tauranga | grep -vi kf6vo | $(LOCAL_IP) | $(FIJI)
leave:
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
	-@$(LOGS) | $(LEAVING)
endif

USERS = sed -e 's/[^"]* "/"/' | sed -e 's/(LEAVING.*//'
users:
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
	-@$(LOGS) | $(LEAVING) | $(USERS)
endif

unique:
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(ANGSTROM))
endif
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEBIAN))
	-@$(LOGS) | $(LEAVING) | $(USERS) | sort -u
endif

v ver version:
	@echo "you are running version" $(VERSION_MAJ)"."$(VERSION_MIN)

JKS_DOTCOM_outside = www.jks.com
JKS_DOTCOM_inside = www
#JKS_DOTCOM = JKS_DOTCOM_outside
JKS_DOTCOM = JKS_DOTCOM_inside

LOGGING_HOST = $(JKS_DOTCOM)
DIST = wrx

#ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))

tar:
	make clean
	tar cfz ../$(DIST).tgz ../$(DIST)

scp: tar
	scp ../$(DIST).tgz root@wrx:~root

# selectively transfer files to the target so everything isn't compiled each time
EXCLUDE = "/obj" "/obj_O3" "/obj_keep" "*.dSYM" "*.bin" "*.aout" "e_cpu/a" "*.aout.h" "wrx.gen.h" "wrx_v/wrx.gen.vh" "web/edata*.c"
rsync:
	rsync -av --delete $(addprefix --exclude , $(EXCLUDE)) . root@$(WRX_SERVER):~root/$(DIST)
su_rsync:
	sudo rsync -av --delete $(addprefix --exclude , $(EXCLUDE)) . root@$(WRX_SERVER):~root/$(DIST)

V_SRC_DIR = wrx_v/
V_DST_DIR = ~/shared/shared/wrx_v

cv: $(GEN_VERILOG)
	rsync -av --delete $(V_SRC_DIR) $(V_DST_DIR)

cv_old: $(GEN_VERILOG)
	cp -r $(V_SRC_DIR) $(V_DST_DIR)

#endif

clean:
	(cd e_cpu; make clean)
	(cd wrx_v/rx; make clean)
	-rm -rf $(OBJ_DIR) $(OBJ_DIR_O3) $(DIST).bin $(DIST)d.bin *.dSYM ../$(DIST).tgz pas $(addprefix pru/pru_realtime.,bin lst txt) web/edata_embed.c $(GEN_ASM) $(GEN_VERILOG) $(DIST)d $(DIST)d.aout $(DIST)d_realtime.bin

clean_keep:
	-rm -rf $(KEEP_DIR) web/edata_always.c

clean_dist:
	make clean
	make clean_keep
