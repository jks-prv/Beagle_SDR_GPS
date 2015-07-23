PROJ = ant

all: pn
	./pn

run: pn
	./pn 1

pn: ../tools/pn.c
	cc -g -DPROJECT=$(PROJ) -I../../code ../tools/pn.c -o pn

BOM_PROTO = $(PROJ).bom.proto.ods $(PROJ).bom.proto.xls

.PHONY: gerb
gerb:
	mkdir -p plot
	cp $(BOM_PROTO) plot
	(cd plot; tar cf $(PROJ).gerbers.BOM.tar *.g?? *.drl *.pos $(BOM_PROTO))
