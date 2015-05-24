all: pn gerbers
	./pn

run: pn
	./pn 1

pn: pn.c
	cc -g -o pn pn.c

.PHONY: gerbers
gerbers:
	(cd plot; tar cf wrx.gerbers.tar *.g?? *.drl *.pos)
