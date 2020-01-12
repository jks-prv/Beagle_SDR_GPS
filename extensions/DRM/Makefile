ifeq ($(DEBIAN_7),true)
    DRM_SKIP = true
else
    DRM_SKIP = false
endif

THIS_DRM = extensions/DRM
$(GEN_DIR)/ext_init.cpp: $(THIS_DRM)/Makefile

ifeq ($(DRM_SKIP),true)
    EXT_SKIP += DRM
else
    DRM_SUBDIR = . linux sourcedecoders sound chanest datadecoding datadecoding/journaline drmchannel \
        FAC interleaver matlib MDI MLC MSC OFDMcellmapping resample SDC sync tables util FDK-AAC
    
    # this code is so slow to compile that we cache it across updates
    EXT_SUBDIRS_KEEP += $(addprefix extensions/DRM/dream/,$(DRM_SUBDIR))
    
    EXT_DEFINES += -DDRM -DHAVE_DLFCN_H -DHAVE_MEMORY_H -DHAVE_STDINT_H -DHAVE_STDLIB_H -DHAVE_STRINGS_H \
        -DHAVE_STRING_H -DSTDC_HEADERS -DHAVE_INTTYPES_H -DHAVE_STDINT_H -DHAVE_SYS_STAT_H \
        -DHAVE_SYS_TYPES_H -DHAVE_UNISTD_H -DHAVE_LIBZ -DHAVE_LIBSNDFILE \
        -DUSE_CONSOLEIO -DUSE_KIWI -DHAVE_LIBFDK_AAC -DHAVE_USAC

    ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
        
        LIBS_DEP += /usr/local/lib/libfdk-aac.so
        LIBS += -Wl,-rpath -Wl,/usr/local/lib -lfdk-aac
        
	    LIBS_DEP += /usr/lib/arm-linux-gnueabihf/libsndfile.so  /usr/lib/arm-linux-gnueabihf/libz.so
        LIBS += -ldl

        /usr/local/lib/libfdk-aac.so:
	        rsync -av extensions/DRM/FDK-AAC/lib/ /usr/local/lib
        
        /usr/lib/arm-linux-gnueabihf/libsndfile.so:
	        -apt-get -y install libsndfile1-dev
        
        /usr/lib/arm-linux-gnueabihf/libz.so:
	        -apt-get -y install zlib1g-dev

    else
        LIBS += -lfdk-aac

        # install needed libs by hand and adjust the following path
        LIBS += -L/opt/local/lib
    endif

    LIBS += -lz -lsndfile
endif
