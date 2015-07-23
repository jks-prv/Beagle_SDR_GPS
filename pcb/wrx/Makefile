#
# Steps to creating manufacturing package:
#
#	oo: edit quotation.odt, create pdf
#	eeschema: create bom
#		print to plot/wrx.schematic.pdf
#	make, run through octopart
#	oo: update wrx.bom.ods by hand from wrx.bom.odt.csv and octo prices
#		save to wrx.bom.xls
#	pcbnew: plot, drill, drill report
#	pcbnew: fab-out, .pos, mm, one-file, plot/wrx-all.pos
#	pcbnew: sel in layers toolbar, print, f.silk dwgs.user eco1.user, single page, plot/wrx.assembly.pdf
#	make package
#

PROJ = wrx

all: pn
	./pn

run: pn
	./pn 1

pn: ../tools/pn.c
	cc -g -DPROJECT=$(PROJ) -I../../code ../tools/pn.c -o pn

BOM = $(PROJ).bom.ods $(PROJ).bom.xls
DOC = $(PROJ).assembly.pdf $(PROJ).schematic.pdf
QUOTE = $(PROJ).quotation.pdf

.PHONY: package
package:
	mkdir -p plot
	cp ../../../docs/pcb.fab/quotation/wrx.quotation.pdf plot
	cp $(BOM) plot
	(cd plot; tar cf $(PROJ).tar $(PROJ)*.g?? $(PROJ)*.drl $(PROJ)*.pos *.rpt $(BOM) $(DOC) $(QUOTE))
