# filter out previously optimized
FILES_EMBED_JS_F = $(filter-out %.gz,$(filter-out %.min.js,$(FILES_EMBED_JS)))
FILES_EMBED_JS_F2 = $(shell $(FILE_OPTIM) -t1 -u -js $(FILES_EMBED_JS_F) 2> /dev/null)
FILES_EMBED_JS_PKG = $(FILES_EMBED_JS_F)

FILES_EMBED_JS_NOPKG_F = $(filter-out %.gz,$(filter-out %.min.js,$(FILES_EMBED_JS_NOPKG)))
FILES_EMBED_JS_NOPKG2 = $(shell $(FILE_OPTIM) -t2 -u -js $(FILES_EMBED_JS_NOPKG_F) 2> /dev/null)

#FILES_EMBED_CSS_F = $(filter-out %.gz,$(filter-out %.min.js,$(FILES_EMBED_CSS)))
# correct css override order
FILES_EMBED_CSS_KEEP_MIN  = web/pkgs/font-awesome-4.6.3/css/font-awesome.css
FILES_EMBED_CSS_F  = web/pkgs/text-security/text-security-disc.css
FILES_EMBED_CSS_F += web/pkgs/w3.css
FILES_EMBED_CSS_F += web/kiwi/w3_ext.css
FILES_EMBED_CSS_F += web/openwebrx/openwebrx.css
FILES_EMBED_CSS_F += web/kiwi/kiwi.css
FILES_EMBED_CSS_PKG = $(FILES_EMBED_CSS_F) $(FILES_EMBED_CSS_KEEP_MIN)

FILES_EMBED_HTML_F = $(filter-out %.gz,$(filter-out %.min.js,$(FILES_EMBED_HTML)))
FILES_EMBED_HTML_F2 = $(subst web/,,$(shell $(FILE_OPTIM) -t3 -u -html $(FILES_EMBED_HTML_F) 2> /dev/null))

EMBED_FOPT_W = $(FILES_EMBED_JS_F) $(FILES_EMBED_JS_NOPKG_F) $(FILES_EMBED_CSS_F) $(FILES_EMBED_HTML_F)
EMBED_NW = $(subst web/,,$(FILES_EMBED_MISC)) $(FILES_EMBED_HTML_F2) $(subst web/,,$(FILES_EMBED_JS_NOPKG2)) kiwisdr.min.js.gz kiwisdr.min.css.gz

.PHONY: foptim_files_embed
foptim_files_embed: foptim_embed web/kiwisdr.min.js web/kiwisdr.min.css

.PHONY: foptim_embed
foptim_embed: $(FILE_OPTIM) $(FILES_EMBED_JS_F) $(FILES_EMBED_CSS_F)  $(FILES_EMBED_HTML_F)
	@echo
	@echo '### foptim_embed'
# NB: "-zip" not used here so resulting .min.js files can be packaged
	$(FILE_OPTIM) -t4 $(FILES_EMBED_JS_F) $(FILES_EMBED_CSS_F)
	$(FILE_OPTIM) -t5 -zip $(FILES_EMBED_JS_NOPKG_F)
# no "-zip" for html files because of possible %[] substitution
# FIXME: eliminate %[] for this reason (and others) -- do dynamically in js
	$(FILE_OPTIM) -t6 $(FILES_EMBED_HTML_F)
# FIXME: try zipped pngs
#	$(FILE_OPTIM) -zip $(FILES_EMBED_PNG_F)
	@echo
	@echo FILES_EMBED_JS_PKG = $(FILES_EMBED_JS_PKG)
	@echo FILES_EMBED_CSS_PKG = $(FILES_EMBED_CSS_PKG)

web/kiwisdr.min.js: $(FILES_EMBED_JS_PKG)
	cat $(FILES_EMBED_JS_F2) >web/kiwisdr.min.js
	gzip --fast --keep --force web/kiwisdr.min.js

web/kiwisdr.min.css: $(FILES_EMBED_CSS_PKG)
	cat $(FILES_EMBED_CSS_KEEP_MIN) `$(FILE_OPTIM) -t7 -u -css $(FILES_EMBED_CSS_F)` >web/kiwisdr.min.css
	gzip --fast --keep --force web/kiwisdr.min.css

.PHONY: loptim_embed
loptim_embed: $(FILE_OPTIM)
	-ls -la `$(FILE_OPTIM) -l $(EMBED_FOPT_W)` web/kiwisdr.min.js web/kiwisdr.min.css

.PHONY: roptim_embed
roptim_embed: $(FILE_OPTIM)
#	rm -f `$(FILE_OPTIM) -l $(EMBED_FOPT_W)` web/kiwisdr.min.js web/kiwisdr.min.css
