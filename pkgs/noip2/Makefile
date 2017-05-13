TGT=noip2
CC=gcc

# these defines are for Linux
LIBS=
ARCH=linux

${TGT}: Makefile ${TGT}.c 
	${CC} -Wall -g -D${ARCH} ${TGT}.c -o ${TGT} ${LIBS}

clean: 
	rm -rf ${TGT} ${TGT}.dSYM

clean_dist: clean
	rm -f NO-*
