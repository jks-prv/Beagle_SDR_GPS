#
# assembler for the embedded processor
#

include ../Makefile.comp.inc

BUILD_DIR = ../../build
TOOLS_DIR = $(BUILD_DIR)/tools
GEN_DIR = $(BUILD_DIR)/gen

ASM = $(TOOLS_DIR)/asm

all: $(ASM)
	$(ASM) -o $(GEN_DIR)

$(TOOLS_DIR):
	@mkdir -p $(TOOLS_DIR)

$(GEN_DIR):
	@mkdir -p $(GEN_DIR)

ASM_INCLUDES = $(wildcard asm/*.h)
ASM_SOURCE = $(wildcard asm/*.cpp)

$(ASM): $(TOOLS_DIR) $(GEN_DIR) $(ASM_INCLUDES) $(ASM_SOURCE) Makefile
	$(CPP) -I. -I./asm -D_GNU_SOURCE -g $(ASM_SOURCE) -o $@

# don't regenerate the .h/.vh files
no_gen: $(ASM)
	$(ASM) -n -o $(GEN_DIR)

debug: $(ASM)
	$(ASM) -n -d -o $(GEN_DIR)

binary: $(ASM)
	$(ASM) -n -b -o $(GEN_DIR)

stat: $(ASM)
	$(ASM) -n -s -o $(GEN_DIR)

clean:
	-rm -rf $(TOOLS_DIR)/asm* $(GEN_DIR)
