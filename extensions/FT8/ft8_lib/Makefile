BUILD_DIR = .build

FT8_SRC  = $(wildcard ft8/*.c)
FT8_OBJ  = $(patsubst %.c,$(BUILD_DIR)/%.o,$(FT8_SRC))

COMMON_SRC = $(wildcard common/*.c)
COMMON_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_SRC))

FFT_SRC  = $(wildcard fft/*.c)
FFT_OBJ  = $(patsubst %.c,$(BUILD_DIR)/%.o,$(FFT_SRC))

TARGETS  = gen_ft8 decode_ft8 test_ft8

CFLAGS   = -fsanitize=address -O3 -ggdb3
CPPFLAGS = -std=c11 -I.
LDFLAGS  = -fsanitize=address -lm

# Optionally, use Portaudio for live audio input
ifdef PORTAUDIO_PREFIX
CPPFLAGS += -DUSE_PORTAUDIO -I$(PORTAUDIO_PREFIX)/include
LDFLAGS  += -lportaudio -L$(PORTAUDIO_PREFIX)/lib
endif

.PHONY: all clean run_tests install

all: $(TARGETS)

clean:
	rm -rf $(BUILD_DIR) $(TARGETS) 

run_tests: test_ft8
	@./test_ft8

install:
	$(AR) rc libft8.a $(FT8_OBJ) $(COMMON_OBJ)
	install libft8.a /usr/lib/libft8.a

gen_ft8: $(BUILD_DIR)/demo/gen_ft8.o $(FT8_OBJ) $(COMMON_OBJ) $(FFT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

decode_ft8: $(BUILD_DIR)/demo/decode_ft8.o $(FT8_OBJ) $(COMMON_OBJ) $(FFT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

test_ft8: $(BUILD_DIR)/test/test.o $(FT8_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $^
