DRM_SKIP ?= false

ifeq ($(DEBIAN_VERSION),7)
    DRM_SKIP = true
endif

THIS_DRM = extensions/DRM
$(GEN_DIR)/ext_init.cpp: $(THIS_DRM)/Makefile

ifeq ($(DRM_SKIP),true)
    EXT_SKIP += DRM
    
    # resampler used by other code, so include subdir where it is located for DRM_SKIP case
    DRM_SUBDIR = resample
    EXT_SUBDIRS += $(addprefix extensions/DRM/dream/,$(DRM_SUBDIR))
    EXT_DEFINES += -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DHAVE_STDLIB_H -DHAVE_STRINGS_H -DSTDC_HEADERS
else
    DRM_SUBDIR = . linux sourcedecoders sound chanest datadecoding datadecoding/journaline drmchannel \
        FAC interleaver matlib MDI MLC MSC OFDMcellmapping resample SDC sync tables util FDK-AAC
    
    # this code is so slow to compile that we cache it across updates
    EXT_SUBDIRS_KEEP += $(addprefix extensions/DRM/dream/,$(DRM_SUBDIR))
    
    EXT_DEFINES += -DDRM -DHAVE_DLFCN_H -DHAVE_MEMORY_H -DHAVE_STDINT_H -DHAVE_STDLIB_H -DHAVE_STRINGS_H \
        -DHAVE_STRING_H -DSTDC_HEADERS -DHAVE_INTTYPES_H -DHAVE_SYS_STAT_H \
        -DHAVE_SYS_TYPES_H -DHAVE_UNISTD_H -DHAVE_LIBZ -DHAVE_LIBSNDFILE \
        -DUSE_CONSOLEIO -DUSE_KIWI -DHAVE_LIBFDK_AAC -DHAVE_USAC

    ifeq ($(XC),-DXC)
            LIBS_DEP += $(KIWI_XC_REMOTE_FS)/usr/local/lib/libfdk-aac.so
            LIBS += -Wl,-rpath -Wl,$(KIWI_XC_REMOTE_FS)/usr/local/lib -lfdk-aac
        
            LIBS_DEP += $(KIWI_XC_REMOTE_FS)/usr/lib/$(LIB_ARCH)/libsndfile.so
            LIBS_DEP += $(KIWI_XC_REMOTE_FS)/lib/$(LIB_ARCH)/libz.so.1
            LIBS += -ldl
    else        
        ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
            LIBS += -Wl,-rpath -Wl,/usr/local/lib
        
            LIBS_DEP += /usr/lib/$(LIB_ARCH)/libsndfile.so  /usr/lib/$(LIB_ARCH)/libz.so
            LIBS += -ldl

            ifeq ($(BBAI_64),true)
                LIBS_DEP += /usr/local/lib/libfdk-aac.so.2
                LIBS +=  -lfdk-aac
                
                /usr/local/lib/libfdk-aac.so.2:
	                rsync -av extensions/DRM/FDK-AAC/lib/$(ARCH_DIR)/ /usr/local/lib
            else
                LIBS_DEP += /usr/local/lib/libfdk-aac.so
                LIBS += -lfdk-aac
                
                /usr/local/lib/libfdk-aac.so:
	                rsync -av extensions/DRM/FDK-AAC/lib/$(ARCH_DIR)/ /usr/local/lib
            endif
        
            /usr/lib/$(LIB_ARCH)/libsndfile.so:
	            -apt-get -y $(APT_GET_FORCE) install libsndfile1-dev
        
            /usr/lib/$(LIB_ARCH)/libz.so:
	            -apt-get -y $(APT_GET_FORCE) install zlib1g-dev
        else
            LIBS += -lfdk-aac

            # install needed libs by hand and adjust the following path
            LIBS += -L/opt/local/lib
        endif
    endif

    LIBS += -lz -lsndfile
    #LIBS += -lsamplerate
endif
