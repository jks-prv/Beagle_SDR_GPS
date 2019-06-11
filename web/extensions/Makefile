FILES_EMBED_JS += web/extensions/ext.js

# filter out previously optimized
EXT_ALL_W = $(patsubst %.gz,,$(patsubst %.min.css,,$(patsubst %.min.js,,$(addprefix web/,$(FILES_EXT)))))
EXT_EMBED_NW = $(subst web/,,$(shell $(FILE_OPTIM) -t8 -u $(EXT_ALL_W) 2> /dev/null))

.PHONY: foptim_ext
foptim_ext: $(FILE_OPTIM)
	@echo
	@echo '### foptim_ext'
	$(FILE_OPTIM) -t9 -zip $(EXT_ALL_W)

.PHONY: uoptim_ext
uoptim_ext: $(FILE_OPTIM)
	@echo 'EXT_EMBED_NW =' `$(FILE_OPTIM) -t10 -u $(EXT_ALL_W)`

.PHONY: loptim_ext
loptim_ext: $(FILE_OPTIM)
	-ls -la `$(FILE_OPTIM) -l $(EXT_ALL_W)`

.PHONY: roptim_ext
roptim_ext: $(FILE_OPTIM)
#	rm -f `$(FILE_OPTIM) -l $(EXT_ALL_W)`
