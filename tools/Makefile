#DEF = a
#DEF = i
#DEF = hog
#DEF = multiply
DEF = ext64

DEBIAN_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
DEBIAN = 0
NOT_DEBIAN = 1
DEVSYS = 2

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	CFLAGS += -DDEVSYS
else
endif

ARCH = sitara
PLATFORM = beaglebone_black
PKGS = pkgs/mongoose pkgs/jsmn pkgs/parson

#DIRS = . .. ../rx/CuteSDR ../web ../pkgs/mongoose ../support ../platform/beaglebone_black ../arch/sitara
DIRS = . pru $(PKGS) web extensions
DIRS += platform/$(PLATFORM) $(EXT_DIRS) rx rx/CuteSDR rx/csdr gps ui support arch arch/$(ARCH)
I = $(addprefix -I../,$(DIRS)) -I/usr/local/include

all: $(DEF)

a: audio.c
	g++ -o a audio.c

i: integrate.c
	g++ -o i integrate.c

hog: hog.c
	g++ $(CFLAGS) $(I) -lrt -o hog hog.c

ext64: ext64.c
	g++ $(CFLAGS) $(I) -o ext64 ext64.c

multiply: multiply.c
	g++ $(CFLAGS) $(I) -o multiply multiply.c

run: $(DEF)
	./$(DEF)

clean:
	rm -f a i hog multiply ext64
