HFDL_SKIP ?= false

ifeq ($(DEBIAN_VERSION),7)
    HFDL_SKIP = true
endif

THIS_HFDL = extensions/HFDL
$(GEN_DIR)/ext_init.cpp: $(THIS_HFDL)/Makefile

EXT_EXCLUDE_RSYNC += $(THIS_HFDL)/dumphfdl/build $(THIS_HFDL)/libacars-2.1.3/build $(THIS_HFDL)/liquid-dsp/sandbox

ifeq ($(HFDL_SKIP),true)
    EXT_SKIP += HFDL
else
    EXT_SUBDIRS += extensions/HFDL/dumphfdl/src extensions/HFDL/dumphfdl/src/libfec

    EXT_DEFINES += -DHFDL -DHFDL_KIWI -DDUMPHFDL_VERSION=STRINGIFY\(1.1.0\)

    ifeq ($(XC),-DXC)
        EXT_I += -I$(KIWI_XC_REMOTE_FS)/usr/local/include/libacars-2 -I$(KIWI_XC_REMOTE_FS)/usr/local/include/liquid
        EXT_I += -I$(KIWI_XC_REMOTE_FS)/usr/lib/$(LIB_ARCH)/glib-2.0/include -I$(KIWI_XC_REMOTE_FS)/usr/include/glib-2.0

        LIBS += -Wl,-rpath -Wl,$(KIWI_XC_REMOTE_FS)/usr/lib/$(LIB_ARCH)

        LIBS_DEP += $(KIWI_XC_REMOTE_FS)/usr/local/lib/libacars-2.so $(KIWI_XC_REMOTE_FS)/usr/local/lib/libliquid.so

        LIBS_DEP += $(KIWI_XC_REMOTE_FS)/usr/bin/cmake

        LIBS_DEP += $(KIWI_XC_REMOTE_FS)/lib/$(LIB_ARCH)/libglib-2.0.so.0

        LIBS_DEP += $(KIWI_XC_REMOTE_FS)/usr/lib/$(LIB_ARCH)/libconfig++.so

        LIBS_DEP += $(KIWI_XC_REMOTE_FS)/usr/lib/$(LIB_ARCH)/libsqlite3.so
    else
    
        ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
            EXT_I += -I/usr/local/include/libacars-2 -I/usr/local/include/liquid
            EXT_I += -I/usr/lib/$(LIB_ARCH)/glib-2.0/include -I/usr/include/glib-2.0

            LIBS += -Wl,-rpath -Wl,/usr/lib/$(LIB_ARCH)

            LIBS_DEP += /usr/local/lib/libacars-2.so /usr/local/lib/libliquid.so
            /usr/local/lib/libacars-2.so /usr/local/lib/libliquid.so:
	            rsync -av extensions/HFDL/include/ /usr/local/include
	            rsync -av extensions/HFDL/lib/$(CPU)/ /usr/local/lib
	            rsync -av extensions/HFDL/lib/pkgconfig/ /usr/local/lib/pkgconfig

            LIBS_DEP += /usr/bin/cmake
            /usr/bin/cmake:
	            -apt-get -y $(APT_GET_FORCE) install cmake

            LIBS_DEP += /usr/lib/$(LIB_ARCH)/libglib-2.0.so
            /usr/lib/$(LIB_ARCH)/libglib-2.0.so:
	            -apt-get -y $(APT_GET_FORCE) install libglib2.0-dev

            LIBS_DEP += /usr/lib/$(LIB_ARCH)/libconfig++.so
            /usr/lib/$(LIB_ARCH)/libconfig++.so:
	            -apt-get -y $(APT_GET_FORCE) install libconfig++-dev

            LIBS_DEP += /usr/lib/$(LIB_ARCH)/libsqlite3.so
            /usr/lib/$(LIB_ARCH)/libsqlite3.so:
	            -apt-get -y $(APT_GET_FORCE) install libsqlite3-dev
        else
            EXT_I += -I/opt/local/include/glib-2.0 -I/usr/local/include/libacars-2
            EXT_I += -I/usr/local/lib/glib-2.0/include

            LIBS += -L/usr/local/lib -L/opt/local/lib -L /usr/local/MacGPG2/lib
        endif
    endif

    LIBS += -lacars-2 -lliquid -lglib-2.0 -lconfig -lsqlite3

    .PHONY: hfdl_clean
    hfdl_clean:
	    rm -rf dumphfdl/build
endif
