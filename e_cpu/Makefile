#
# assembler for the embedded processor
#

FLAGS =
DEP = gps44.h

all: a
	./a

ASM_INCLUDES = $(wildcard asm/*.h)
ASM_SOURCE = $(wildcard asm/*.c)
a: $(ASM_INCLUDES) $(ASM_SOURCE) Makefile
	cc -I. -I./asm -g $(ASM_SOURCE) -o $@

# don't regenerate the .h/.vh files
no_gen: a
	./a -n

debug: a
	./a -n -d

binary: a
	./a -n -b

stat: a
	./a -n -s

clean:
	-rm -rf a *.dSYM *.aout *.aout.h
