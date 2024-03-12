FILES_EMBED_JS += web/kiwi/kiwi.js web/kiwi/kiwi_ui.js web/kiwi/kiwi_util.js web/kiwi/w3_util.js
FILES_EMBED_JS += web/kiwi/monitor.js web/kiwi/kiwi_map.js
FILES_EMBED_JS += web/kiwi/waterfall.js web/kiwi/noise_blank.js web/kiwi/noise_filter.js web/kiwi/ant_switch.js

FILES_EMBED_JS_NOPKG += web/kiwi/admin.js web/kiwi/admin_sdr.js web/kiwi/mfg.js web/kiwi/kiwi_js_load.js

FILES_EMBED_HTML += web/kiwi/admin.html web/kiwi/mfg.html

FILES_EMBED_MISC += web/kiwi/ip_blacklist.default.cjson

FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/gfx/*.jpg))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/gfx/*.png))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/*.jpg))
FILES_ALWAYS2 += $(subst web/,,$(wildcard web/kiwi/*.png))

CLEAN_KIWI = admin_sdr admin kiwi kiwi_ui kiwi_util w3_util monitor
CLEAN_MIN_GZ += $(addprefix web/kiwi/,$(addsuffix .min.js, $(CLEAN_KIWI))) $(addprefix web/kiwi/,$(addsuffix .min.js.gz, $(CLEAN_KIWI)))
