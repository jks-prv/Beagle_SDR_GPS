FT8_SKIP ?= false

ifeq ($(DEBIAN_VERSION),7)
    FT8_SKIP = true
endif

THIS_FT8 = extensions/FT8
$(GEN_DIR)/ext_init.cpp: $(THIS_FT8)/Makefile

EXT_EXCLUDE_RSYNC += $(THIS_FT8)/ft8_lib/.build

ifeq ($(FT8_SKIP),true)
    EXT_SKIP += FT8
else
    EXT_SUBDIRS += extensions/FT8/ft8_lib extensions/FT8/ft8_lib/common extensions/FT8/ft8_lib/ft8

    EXT_DEFINES += -DFT8 -DFT8_KIWI

    .PHONY: ft8_clean
    ft8_clean:
	    (cd ft8_lib; make clean)
endif
