# filter out previously optimized
FILES_EMBED_JS_F = $(filter-out %.gz,$(filter-out %.min.js,$(FILES_EMBED_JS)))
FILES_EMBED_JS_F2 = $(shell $(FILES_OPTIM) -u -js $(FILES_EMBED_JS_F))
FILES_EMBED_JS_NOPKG2 = $(shell $(FILES_OPTIM) -u -js $(FILES_EMBED_JS_NOPKG))

#FILES_EMBED_CSS_F = $(filter-out %.gz,$(filter-out %.min.js,$(FILES_EMBED_CSS)))
# correct css override order
FILES_EMBED_CSS_KEEP_MIN  = web/pkgs/font-awesome-4.6.3/css/font-awesome.css
FILES_EMBED_CSS_KEEP_MIN += web/pkgs/text-security/text-security-disc.min.css
FILES_EMBED_CSS_F  = web/pkgs/w3.css
FILES_EMBED_CSS_F += web/kiwi/w3_ext.css
FILES_EMBED_CSS_F += web/openwebrx/openwebrx.css
FILES_EMBED_CSS_F += web/kiwi/kiwi.css

FILES_EMBED_HTML_F = $(filter-out %.gz,$(filter-out %.min.js,$(FILES_EMBED_HTML)))
FILES_EMBED_HTML_F2 = $(subst web/,,$(shell $(FILES_OPTIM) -u -html $(FILES_EMBED_HTML_F)))

EMBED_FOPT_W = $(FILES_EMBED_JS_F) $(FILES_EMBED_JS_NOPKG) $(FILES_EMBED_CSS_F) $(FILES_EMBED_HTML_F)
EMBED_NW = $(subst web/,,$(FILES_EMBED_MISC)) $(FILES_EMBED_HTML_F2) $(subst web/,,$(FILES_EMBED_JS_NOPKG2)) kiwisdr.min.js.gz kiwisdr.min.css.gz

.PHONY: foptim_embed
foptim_embed: $(FILES_OPTIM) $(FILES_EMBED_JS_F) $(FILES_EMBED_CSS_F)  $(FILES_EMBED_HTML_F)
	@echo
	@echo '### foptim_embed'
# NB: "-zip" not used here so resulting .min.js files can be packaged by uoptim_embed target
	$(FILES_OPTIM) $(FILES_EMBED_JS_F) $(FILES_EMBED_CSS_F)
	$(FILES_OPTIM) -zip $(FILES_EMBED_JS_NOPKG)
# no "-zip" for html files because of possible %[] substitution
# FIXME: eliminate %[] for this reason (and others) -- do dynamically in js
	$(FILES_OPTIM) $(FILES_EMBED_HTML_F)
# FIXME: try zipped pngs
#	$(FILES_OPTIM) -zip $(FILES_EMBED_PNG_F)

.PHONY: uoptim_embed
uoptim_embed: $(FILES_OPTIM)
	cat $(FILES_EMBED_JS_F2) >web/kiwisdr.min.js
	gzip --fast --keep --force web/kiwisdr.min.js
	cat $(FILES_EMBED_CSS_KEEP_MIN) `$(FILES_OPTIM) -u -css $(FILES_EMBED_CSS_F)` >web/kiwisdr.min.css
	gzip --fast --keep --force web/kiwisdr.min.css

.PHONY: loptim_embed
loptim_embed: $(FILES_OPTIM)
	-ls -la `$(FILES_OPTIM) -l $(EMBED_FOPT_W)` web/kiwisdr.min.js web/kiwisdr.min.css

.PHONY: roptim_embed
roptim_embed: $(FILES_OPTIM)
#	rm -f `$(FILES_OPTIM) -l $(EMBED_FOPT_W)` web/kiwisdr.min.js web/kiwisdr.min.css
