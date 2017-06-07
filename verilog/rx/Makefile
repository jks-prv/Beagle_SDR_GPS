GEN_VERILOG = cic_rx1.vh cic_rx2.vh cic_wf1.vh cic_wf2.vh
FLAGS = -g -I. -I.. -I../.. -lm

all: $(GEN_VERILOG)

$(GEN_VERILOG): cic_gen
	time ./cic_gen

cic_gen: cic_gen.c Makefile ../../kiwi.gen.h
	cc $(FLAGS) -o $@ $<
	
debug: cic_gen_debug
	./cic_gen_debug

cic_gen_debug: cic_gen_debug.c Makefile ../../kiwi.gen.h
	cc $(FLAGS) -DDEBUG -o $@ $<

diff:
	-diff cic_gen.c cic_gen_debug.c

# leave cic*.vh generated files so distribution has a working default
clean:
	-rm -rf cic_gen cic_gen_debug *.dSYM
