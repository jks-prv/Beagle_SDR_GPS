#
# The '/etc/dogtag' file is present on the Beagle and not on the dev machine.
# Grep returns 0 if "Debian" is found in /etc/dogtag, 1 if it isn't and 2 if /etc/dogtag doesn't exist.
# This same mechanism is used in the wrapper shell script (./k) because device tree files need to be
# loaded only on the Beagle.
#

DEBIAN_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
DEBIAN = 0
NOT_DEBIAN = 1
DEVSYS = 2

UNAME = $(shell uname)
SYS = $(shell uname -r)
SYS_MAJ = $(shell uname -r | awk '{print $1}' | cut -d. -f1)
SYS_MIN = $(shell uname -r | awk '{print $1}' | cut -d. -f2)

################################
# compiler/option selection
################################

O_OPT =
O_UNOPT =

BUILD_OBJ = $(BUILD_DIR)/kiwi.bin

# devsys
ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	ifeq ($(UNAME),Darwin)
		CC = clang
		CPP = clang++
		CPP_FLAGS += -std=gnu++11
	else
		# try clang on your development system (if you have it) -- it's better
		#CC = clang
		#CPP = clang++
	
		CC = gcc
		CPP = g++
	endif

	CFLAGS += -DDEVSYS
endif

# Debian target
ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
	BBAI = $(shell cat /proc/device-tree/model | grep -q -s "BeagleBone AI" && echo true)
	DEBIAN_7 = $(shell cat /etc/debian_version | grep -q -s "7\." && echo true)

	ifeq ($(DEBIAN_7),true)
		# clang 3.0 available on Debian 7.9 doesn't work
		CC = gcc
		CPP = g++
		CFLAGS += -DKIWI_DEBIAN7
		O_OPT = -O3
		# needed for iq_display.cpp et al (-std=gnu++11 isn't available on Debian 7.9)
		CPP_FLAGS += -std=gnu++0x
	else ifeq ($(BBAI),true)
	    #PROFILING = true
	    PROFILING = false
	    ifeq ($(PROFILING),true)
            CC = gcc
            CPP = g++
            CFLAGS += -pg -fprofile-arcs -ftest-coverage
            LDFLAGS += -pg -lgcov --coverage
		    O_OPT =
		    BUILD_OBJ = ./kiwi.bin
		else
            CC = clang-7
            CPP = clang++-7
		    O_OPT = -Ofast
        endif
		# needed for iq_display.cpp et al
		CPP_FLAGS += -std=gnu++11
		# clang still used by other things (e_cpu etc)
		CMD_DEPS_DEBIAN = /usr/bin/clang-7
	else
		CMD_DEPS_DEBIAN = /usr/bin/clang-6.0
		CC = clang-6.0
		CPP = clang++-6.0
		#CC = clang
		#CPP = clang++
        O_OPT = -Ofast
		# needed for iq_display.cpp et al
		CPP_FLAGS += -std=gnu++11
	endif

    # To use clang address sanitizer build with "make ASAN=1 [OPT=0]" on target.
    # There are shell aliases "masan" and "masan0" for these.
    # Use gdb "asan" alias to set breakpoint necessary to backtrace address errors.
    ifeq ($(ASAN),1)
        CFLAGS += -fsanitize=address -fno-omit-frame-pointer
        #LDFLAGS += -v -fsanitize=address
        LDFLAGS += -fsanitize=address
        ifeq ($(OPT),0)
            O_OPT =
        endif
    endif
endif
