KIWI_UI_LIST += "{ \"openwebrx\", 1 },"

FILES_EMBED_JS += web/openwebrx/openwebrx.js web/openwebrx/audio.js web/openwebrx/ima_adpcm.js

FILES_EMBED_HTML += web/openwebrx/index.html web/openwebrx/robots.txt

########

FILES_ALWAYS2 += $(subst web/,,$(wildcard web/openwebrx/gfx/*.jpg))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/openwebrx/gfx/*.png))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/openwebrx/icons/*.png))
FILES_ALWAYS2 += openwebrx/favicon.ico
FILES_ALWAYS2 += openwebrx/gfx/font-expletus-sans/ExpletusSans-Medium.ttf
#FILES_ALWAYS2 += openwebrx/openwebrx.ccomp.js

CLEAN_OWRX = audio openwebrx
CLEAN_MIN_GZ += $(addprefix web/openwebrx/,$(addsuffix .min.js, $(CLEAN_OWRX)))
