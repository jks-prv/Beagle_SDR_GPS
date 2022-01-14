# After much confusion figured out how downloading from eibispace.de will break the file UTF-8 encoding.
# Underlying cause is not understood. In Safari DO NOT use right-click "download linked file" function.
# Instead, use the wget/iconv combination shown. Firefox has the same issue.
# wget -O eibi.in http://eibispace.de/dx/sked-b21.csv && iconv -f ISO-8859-1 -t UTF-8 eibi.in > eibi.csv
# Check this entry to make sure correct accented chars: 690;0000-2400;;B;Rádio Clube do Pará;P;B;be;1;;

# Added ALE back manually from A21 using BBedit:
# Append file ALE-a21.kiwi.csv
# Then to sort combined lists by freq:
# 	Text > Apply text transform > Sort lines
# 		> Configure (numbers match by value; sort using pattern; "[0-9.]+;" entire match)
# 		> THEN SELECT THE SINGLE FILE WITH: Apply > Choose
# 			*** otherwise it will apply to the file last time sort was done!
# 		> Put back top header line (the one beginning with "kHz:")
# 			It gets incorrectly sorted a few dozen lines down the list

include ../../Makefile.comp.inc

PROG = eibi_proc
MORE = 
CMD =

ARCH = sitara
PLATFORM = beaglebone_black
PKGS = pkgs/mongoose pkgs/jsmn pkgs/parson pkgs/sha256
GPS = gps gps/ka9q-fec gps/GNSS-SDRLIB
DIRS = . pru $(PKGS) web extensions
DIRS += platform/common platform/beaglebone platform/$(PLATFORM) $(EXT_DIRS) rx rx/CuteSDR rx/csdr $(GPS) init net ui support arch arch/$(ARCH)
DIRS += ../build/gen

VPATH = $(addprefix ../../,$(DIRS))
I = $(addprefix -I../../,$(DIRS)) -I/usr/local/include

all: $(PROG)

$(PROG): $(PROG).o $(MORE)
	$(CPP) $(CFLAGS) $(I) -o $@ $?

%.o: %.cpp
	$(CPP) $(CFLAGS) $(I) -c $<

# always reads from sked-current.csv
# always writes file ../../init/EiBi.h
run: $(PROG)
	./$(PROG) $(ARGS)
	$(CMD)
	rm -f $(PROG) *.o *.d

clean:
	rm -f $(PROG) *.o *.d
