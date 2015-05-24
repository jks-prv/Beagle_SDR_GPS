GEN_VERILOG = cic_rx1.vh cic_rx2.vh
FLAGS = -g -I. -I.. -I../.. -lm

all: $(GEN_VERILOG)

$(GEN_VERILOG): cic_gen
	time ./cic_gen

cic_gen: cic_gen.c Makefile ../../wrx.gen.h
	cc $(FLAGS) -o $@ $<
	
clean:
	-rm -rf cic_gen *.dSYM $(GEN_VERILOG)
