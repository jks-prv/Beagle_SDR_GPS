#DEF = audio
#DEF = integrate
#DEF = hog
#DEF = multiply
#DEF = ext64
#DEF = decimate
DEF = security

CMD =
ifeq ($(DEF),decimate)
    CMD = /Applications/baudline.app/Contents/Resources/baudline -quadrature -overlays 2 /Users/jks/new.dec2.au
endif

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

audio: audio.c
	g++ -o audio audio.c

integrate: integrate.c
	g++ -o integrate integrate.c

hog: hog.c
	g++ $(CFLAGS) $(I) -lrt -o hog hog.c

ext64: ext64.c
	g++ $(CFLAGS) $(I) -o ext64 ext64.c

decimate: decimate.c
	g++ $(CFLAGS) $(I) -o decimate decimate.c

multiply: multiply.c
	g++ $(CFLAGS) $(I) -o multiply multiply.c

security: security.c
	g++ $(CFLAGS) $(I) -o security security.c

run: $(DEF)
	./$(DEF)
	$(CMD)

clean:
	rm -f audio integrate hog multiply ext64 decimate security
