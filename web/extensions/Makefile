FILES_EMBED_JS += web/extensions/ext.js

# filter out previously optimized
EXT_ALL_W = $(patsubst %.gz,,$(patsubst %.min.css,,$(patsubst %.min.js,,$(addprefix web/,$(FILES_EXT)))))
EXT_EMBED_NW = $(subst web/,,$(shell $(FILES_OPTIM) -u $(EXT_ALL_W)))

.PHONY: foptim_ext
foptim_ext: $(FILES_OPTIM)
	@echo
	@echo '### foptim_ext'
	$(FILES_OPTIM) -zip $(EXT_ALL_W)

.PHONY: uoptim_ext
uoptim_ext: $(FILES_OPTIM)
	@echo 'EXT_EMBED_NW =' `$(FILES_OPTIM) -u $(EXT_ALL_W)`

.PHONY: loptim_ext
loptim_ext: $(FILES_OPTIM)
	-ls -la `$(FILES_OPTIM) -l $(EXT_ALL_W)`

.PHONY: roptim_ext
roptim_ext: $(FILES_OPTIM)
#	rm -f `$(FILES_OPTIM) -l $(EXT_ALL_W)`
