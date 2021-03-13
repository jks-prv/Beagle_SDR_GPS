FILES_EMBED_JS += web/kiwi/kiwi.js web/kiwi/kiwi_ui.js web/kiwi/kiwi_util.js web/kiwi/w3_util.js web/kiwi/monitor.js
FILES_EMBED_JS_NOPKG += web/kiwi/admin.js web/kiwi/admin_sdr.js web/kiwi/mfg.js web/kiwi/kiwi_js_load.js

FILES_EMBED_CSS += web/kiwi/kiwi.css web/kiwi/w3_ext.css

FILES_EMBED_HTML += web/kiwi/admin.html web/kiwi/mfg.html

FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/gfx/*.jpg))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/gfx/*.png))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/*.jpg))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/*.png))

CLEAN_KIWI = admin_sdr admin kiwi kiwi_util w3_util monitor
CLEAN_MIN_GZ += $(addprefix web/kiwi/,$(addsuffix .min.js, $(CLEAN_KIWI))) $(addprefix web/kiwi/,$(addsuffix .min.js.gz, $(CLEAN_KIWI)))
