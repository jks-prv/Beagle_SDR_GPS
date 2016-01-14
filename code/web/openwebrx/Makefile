KIWI_UI_LIST += "{ 8073, \"openwebrx\", 1, 30.0*MHz },"

FILES_EMBED += openwebrx/index.html openwebrx/admin.html openwebrx/openwebrx.css openwebrx/openwebrx.js openwebrx/admin.js openwebrx/audio.js
FILES_EMBED += $(subst web/,,$(wildcard web/openwebrx/icons/*.png))

FILES_ALWAYS += $(subst web/,,$(wildcard web/openwebrx/gfx/*.png)) $(subst web/,,$(wildcard web/openwebrx/gfx/*.jpg)) openwebrx/favicon.ico openwebrx/gfx/font-expletus-sans/ExpletusSans-Medium.ttf
#FILES_ALWAYS += openwebrx/openwebrx.ccomp.js

# don't attempt to run java/closure on Beagle
ifeq ($(DEBIAN_ANGSTROM_DEVSYS),$(DEVSYS))

OPENWEBRX = web/openwebrx
#CCOMP_OPENWEBRX = --compilation_level ADVANCED_OPTIMIZATIONS
#CCOMP_OPENWEBRX = --compilation_level SIMPLE_OPTIMIZATIONS
CCOMP_OPENWEBRX = --compilation_level WHITESPACE_ONLY
$(OPENWEBRX)/openwebrx.ccomp.js: $(OPENWEBRX)/openwebrx.js
	java -jar pkgs/closure_compiler/compiler.jar $(CCOMP_OPENWEBRX) --js_output_file=$(OPENWEBRX)/openwebrx.ccomp.js $(OPENWEBRX)/openwebrx.js

endif
