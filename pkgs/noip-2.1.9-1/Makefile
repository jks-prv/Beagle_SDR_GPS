TGT=noip2
CC=gcc
PKG=noip-2.1.tgz

PREFIX=/usr/local
CONFDIR=${PREFIX}/etc
BINDIR=${PREFIX}/bin

# these defines are for Linux
LIBS=
ARCH=linux

# for Mac OS X and BSD systems that have getifaddr(), uncomment the next line
#ARCH=bsd_with_getifaddrs

# for early BSD systems without getifaddrs(), uncomment the next line
#ARCH=bsd


# for solaris, uncomment the next two lines
# LIBS=-lsocket -lnsl
# ARCH=sun

${TGT}: Makefile ${TGT}.c 
	${CC} -Wall -g -D${ARCH} -DPREFIX=\"${PREFIX}\" ${TGT}.c -o ${TGT} ${LIBS}

install: ${TGT} 
	if [ ! -d ${BINDIR} ]; then mkdir -p ${BINDIR};fi
	if [ ! -d ${CONFDIR} ]; then mkdir -p ${CONFDIR};fi
	cp ${TGT} ${BINDIR}/${TGT}
	${BINDIR}/${TGT} -C -c /tmp/no-ip2.conf
	mv /tmp/no-ip2.conf ${CONFDIR}/no-ip2.conf

package: ${TGT}
	rm  -f *.bak
	mv ${TGT} binaries/${TGT}-`uname -m`
	scp a-k:/local/bin/noip2 binaries/noip2-`ssh a-k uname -m`
	cd ..; tar zcvf /tmp/${PKG} noip-2.0/*
	scp /tmp/${PKG} a-k:/opt/www/${PKG}
	rm /tmp/${PKG}

clean: 
	rm -f *o
	rm -f binaries/*
	rm -f ${TGT}
