# generate calls to extension main routines
# $(EXTS) comes from the root Makefile, containing a list of all extension directory names

EXT_DIRS = $(sort $(dir $(wildcard extensions/*/*)))
EXT_INIT = "\textern void $(ext_name)_main(); $(ext_name)_main();\n"

extensions/ext_init.c:	extensions/Makefile $(EXT_DIRS) extensions/ext_int.h extensions/ext.h
	@echo generating extensions/ext_init.c
	@echo EXT_DIRS $(EXT_DIRS)
	@echo "// auto-generated file -- do not edit by hand" > extensions/ext_init.c
	@echo "void extint_init() {" >> extensions/ext_init.c
	@echo $(foreach ext_name,$(EXTS),$(EXT_INIT)) >> extensions/ext_init.c
	@echo "}" >> extensions/ext_init.c
