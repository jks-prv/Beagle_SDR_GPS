BUILD_DIR = ../../../build
TOOLS_DIR = $(BUILD_DIR)/tools
GEN_DIR = $(BUILD_DIR)/gen

CIC_GEN = $(TOOLS_DIR)/cic_gen
CIC_GEN_DEBUG = $(TOOLS_DIR)/cic_gen_debug

GEN_VERILOG = cic_rx1.vh cic_rx2.vh cic_wf1.vh cic_wf2.vh
FLAGS = -g -I. -I.. -I$(GEN_DIR) -lm

all: $(GEN_VERILOG)

$(TOOLS_DIR):
	@mkdir -p $(TOOLS_DIR)

$(GEN_DIR):
	@mkdir -p $(GEN_DIR)

$(GEN_VERILOG): $(CIC_GEN)
	time ./$(CIC_GEN)

$(CIC_GEN): cic_gen.c $(TOOLS_DIR) Makefile $(GEN_DIR)/kiwi.gen.h
	clang $(FLAGS) -o $@ $<
	
debug: $(CIC_GEN_DEBUG)
	./$(CIC_GEN_DEBUG)

$(CIC_GEN_DEBUG): cic_gen_debug.c $(TOOLS_DIR) Makefile $(GEN_DIR)/kiwi.gen.h
	clang $(FLAGS) -DDEBUG -o $@ $<

diff:
	-diff cic_gen.c cic_gen_debug.c

# leave cic*.vh generated files so distribution has a working default
clean:
	-rm -rf $(TOOLS_DIR)/cic* $(GEN_DIR)
