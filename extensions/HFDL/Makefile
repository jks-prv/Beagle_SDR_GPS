ifeq ($(DEBIAN_7),true)
    HFDL_SKIP = true
else
    HFDL_SKIP = false
endif

THIS_HFDL = extensions/HFDL
$(GEN_DIR)/ext_init.cpp: $(THIS_HFDL)/Makefile

EXT_EXCLUDE_RSYNC += $(THIS_HFDL)/dumphfdl/build $(THIS_HFDL)/libacars-2.1.3/build $(THIS_HFDL)/liquid-dsp/sandbox

ifeq ($(HFDL_SKIP),true)
    EXT_SKIP += HFDL
else
    EXT_SUBDIRS += extensions/HFDL/dumphfdl/src extensions/HFDL/dumphfdl/src/libfec

    EXT_DEFINES += -DHFDL -DHFDL_KIWI -DDUMPHFDL_VERSION=STRINGIFY\(1.1.0\)

    ifeq ($(DEBIAN_DEVSYS),$(DEBIAN))
        EXT_I += -I/usr/local/include/libacars-2 -I/usr/local/include/liquid
        EXT_I += -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include -I/usr/include/glib-2.0

        LIBS += -Wl,-rpath -Wl,/usr/lib/arm-linux-gnueabihf

        LIBS_DEP += /usr/local/lib/libacars-2.so /usr/local/lib/libliquid.so
        /usr/local/lib/libacars-2.so /usr/local/lib/libliquid.so:
	        rsync -av extensions/HFDL/include/ /usr/local/include
	        rsync -av extensions/HFDL/lib/$(CPU)/ /usr/local/lib
	        rsync -av extensions/HFDL/lib/pkgconfig/ /usr/local/lib/pkgconfig

        LIBS_DEP += /usr/bin/cmake
        /usr/bin/cmake:
	        -apt-get -y install cmake

        LIBS_DEP += /usr/lib/arm-linux-gnueabihf/libglib-2.0.so
        /usr/lib/arm-linux-gnueabihf/libglib-2.0.so:
	        -apt-get -y install libglib2.0-dev

        LIBS_DEP += /usr/lib/arm-linux-gnueabihf/libconfig++.so
        /usr/lib/arm-linux-gnueabihf/libconfig++.so:
	        -apt-get -y install libconfig++-dev

        LIBS_DEP += /usr/lib/arm-linux-gnueabihf/libsqlite3.so
        /usr/lib/arm-linux-gnueabihf/libsqlite3.so:
	        -apt-get -y install libsqlite3-dev
    else
        EXT_I += -I/opt/local/include/glib-2.0 -I/usr/local/include/libacars-2
        EXT_I += -I/usr/local/Cellar/glib/2.70.0_1/lib/glib-2.0/include

        LIBS += -L/usr/local/lib -L/opt/local/lib -L /usr/local/MacGPG2/lib
    endif

    LIBS += -lacars-2 -lliquid -lglib-2.0 -lconfig -lsqlite3

    .PHONY: hfdl_clean
    hfdl_clean:
	    rm -rf dumphfdl/build
endif
