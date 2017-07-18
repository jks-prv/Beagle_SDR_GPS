UTIL = wspr
UTILS = audio integrate hog multiply ext64 decimate security wspr

CMD =
ifeq ($(UTIL),decimate)
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

all: $(UTIL)

$(UTIL): $(UTIL).c
	g++ $(CFLAGS) $(I) -o $@ $<

run: $(UTIL)
	./$(UTIL)
	$(CMD)

clean:
	rm -f $(UTILS) *.dat
