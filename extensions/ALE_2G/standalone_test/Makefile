#
# m f=[0..8]
# md f=[0..8]
# m f=[0..8] d=[mcd]       // dsp_e: CALLS, MSGS, CMDS, DEBUG
#

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

FILES = $(wildcard ../*.cpp ../*.h *.h)
CFILES = $(wildcard ../*.cpp)

run: ale_2g
	./ale_2g -f $(f) $(dd) $(oo) $(rr)

debug: ale_2g
	./ale_2g -d -f $(f) $(dd) $(oo) $(rr)

ale_2g: $(FILES)
	g++ -Ofast -I. -I.. -I../../.. -I../../DRM/dream/resample -lm -o ale_2g $(CFILES)

clean:
	rm -f ./ale_2g
