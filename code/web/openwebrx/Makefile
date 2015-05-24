WRX_UI_LIST += "{ 8073, \"openwebrx\", 1, 30.0*MHz },"

FILES_EMBED += openwebrx/index.html openwebrx/openwebrx.css openwebrx/upgrade.html openwebrx/openwebrx.js openwebrx/audio.js
FILES_EMBED += $(subst web/,,$(wildcard web/openwebrx/icons/*.png))

FILES_ALWAYS += $(subst web/,,$(wildcard web/openwebrx/gfx/*.png)) $(subst web/,,$(wildcard web/openwebrx/gfx/*.jpg)) openwebrx/favicon.ico openwebrx/gfx/font-expletus-sans/ExpletusSans-Medium.ttf
#FILES_ALWAYS += openwebrx/openwebrx.ccomp.js

# don't attempt to run java/closure on Beagle
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))

OWRX = web/openwebrx
#CCOMP_OWRX = --compilation_level ADVANCED_OPTIMIZATIONS
#CCOMP_OWRX = --compilation_level SIMPLE_OPTIMIZATIONS
CCOMP_OWRX = --compilation_level WHITESPACE_ONLY
$(OWRX)/openwebrx.ccomp.js: $(OWRX)/openwebrx.js
	java -jar pkgs/closure_compiler/compiler.jar $(CCOMP_OWRX) --js_output_file=$(OWRX)/openwebrx.ccomp.js $(OWRX)/openwebrx.js

endif
