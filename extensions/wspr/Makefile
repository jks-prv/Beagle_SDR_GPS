# old Makefile for standalone WSPR build

#CC = gcc
CC = g++

ARCH = sitara
PLATFORM = beaglebone_black
PKGS = pkgs/mongoose
DIRS = . $(PKGS) web platform/$(PLATFORM) rx rx/CuteSDR gps support arch arch/$(ARCH)
I = $(addprefix -I../../,$(DIRS))

CFLAGS = -I. $I -I/usr/include -Wall -O2 -DEXT_WSPR
LDFLAGS = -L/usr/lib 
LIBS = -lsndfile -lfftw3 -lm

DEPS = fano.h wspr.h
OBJ = wspr.o fano.o tab.o file.o wspr_util.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

k9an-wsprd: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)

run: k9an-wsprd
	k9an-wsprd 140729_0354.c2
	k9an-wsprd 140729_0206.c2
	k9an-wsprd -f 14.0956 140729_0206.wav

clean:
	rm -f *.o k9an-wsprd
