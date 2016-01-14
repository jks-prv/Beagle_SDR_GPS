#
# Steps to creating manufacturing package:
#
#	openoffice: edit quotation.odt, create pdf
#	eeschema: create bom
#		print to plot/kiwi.schematic.pdf
#	make, run through octopart.com
#	openoffice: update kiwi.bom.ods by hand from kiwi.bom.odt.csv and octopart.com prices
#		save to kiwi.bom.xls
#	pcbnew: plot, drill, drill report
#	pcbnew: fab-out, .pos, mm, one-file, plot/kiwi-all.pos
#	pcbnew: print top & bot assembly diagram PDFs via print menu:
#		set ref text color via "visibles > render" side-menu button color
#		must be checked in "visibles layer" side-menu: f.silk dwgs.user eco1.user
#		sel print panel technical layers: f/b.mask (to get pads printed) f.silk dwgs.user eco1.user
#		sel print panel: single page, def pen 0.05mm, plot/kiwi.assembly.top/bot.pdf
#		bug: makes multiple identical pages for as many technical layers selected (4)
#	make package
#

PROJ = kiwi

all: pn
	./pn

run: pn
	./pn 1

pn: ../tools/pn.c
	cc -g -DPROJECT=$(PROJ) -I../../code ../tools/pn.c -o pn

BOM = $(PROJ).bom.ods $(PROJ).bom.xls
DOC = $(PROJ).assembly.top.pdf $(PROJ).assembly.bot.pdf $(PROJ).schematic.pdf
QUOTE = $(PROJ).quotation.pdf

.PHONY: package
package:
	mkdir -p plot
	cp ../../../docs/pcb.fab/quotation/kiwi.quotation.pdf plot
	cp $(BOM) plot
	(cd plot; tar cf $(PROJ).tar $(PROJ)*.g?? $(PROJ)*.drl $(PROJ)*.pos *.rpt $(BOM) $(DOC) $(QUOTE))
