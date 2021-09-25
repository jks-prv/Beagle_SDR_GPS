
ifeq ($(f),)
    f = 8
endif

ifneq ($(d),)
    dd = -$(d)
endif

ifneq ($(o),)
    oo = -o $(o)
endif

ifneq ($(r),)
    rr = -r $(r)
endif

ifneq ($(m),)
    mm = -m
endif

ifneq ($(y),)
    yy = -y
endif

FILES = $(wildcard ../*.cpp ../*.h *.h)
CFILES = $(wildcard ../*.cpp)

run: ale_2g
	./ale_2g -f $(f) $(dd) $(oo) $(rr) $(mm) $(yy)

debug: ale_2g
	./ale_2g -f -1 -c $(oo) $(rr) $(yy)

help:
	@echo "m f=[0..8] o=OFFSET r=REPEAT d=[xcad]  // DX, CMDS, ALL, DEBUG"

ale_2g: $(FILES)
	g++ -Ofast -I. -I.. -I../../.. -I../../DRM/dream/resample -lm -lfftw3f -o ale_2g $(CFILES)

clean:
	rm -f ./ale_2g
