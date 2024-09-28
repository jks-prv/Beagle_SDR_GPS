include ../../Makefile.comp.inc

BUILD_DIR = ../../../build
GEN_DIR = $(BUILD_DIR)/gen

#${GEN_DIR}/noip2: Makefile noip2.c
.PHONY: noip
noip:
	$(CC) -Wall -g -Dlinux noip2.c -o ${GEN_DIR}/noip2

clean: 
	rm -f ${GEN_DIR}/noip2*

clean_dist: clean
	rm -f NO-*
