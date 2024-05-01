
# See HOWTO.update.database.txt

include ../../Makefile.comp.inc

PROG = eibi_proc
MORE = 
CMD =

ARCH = sitara
PLATFORM = beaglebone_black
PKGS = pkgs/mongoose pkgs/jsmn pkgs/parson pkgs/sha256
GPS = gps gps/ka9q-fec gps/GNSS-SDRLIB
DIRS = . pru $(PKGS) web extensions
DIRS += platform/common platform/beaglebone platform/$(PLATFORM) $(EXT_DIRS) rx rx/CuteSDR rx/csdr $(GPS) cfg dx net ui support arch arch/$(ARCH)
DIRS += ../build/gen

VPATH = $(addprefix ../../,$(DIRS))
I = $(addprefix -I../../,$(DIRS)) -I/usr/local/include

all: $(PROG)

$(PROG): $(PROG).o $(MORE)
	$(CPP) $(CFLAGS) $(I) -o $@ $?

%.o: %.cpp
	$(CPP) $(CFLAGS) $(I) -c $<

# always reads from sked-current.csv
# always writes file ../../dx/EiBi.h
run: $(PROG)
	./$(PROG) $(ARGS)
	$(CMD)
	rm -f $(PROG) *.o *.d eibi.in

clean:
	rm -f $(PROG) *.o *.d eibi.in
