# generate calls to extension main routines
#
# these come from the root Makefile:
#	$(EXT_DIRS) contains a list of all extension directory names
#	$(EXTS) contains a list of all extension names

EXT_INIT = "\textern void $(ext_name)_main(); $(ext_name)_main();\n"

extensions/ext_init.cpp: extensions/Makefile $(EXT_DIRS) extensions/ext_int.h extensions/ext.h
	@echo generating extensions/ext_init.cpp
	@echo EXT_DIRS $(EXT_DIRS)
	@echo "// auto-generated file -- do not edit by hand" > extensions/ext_init.cpp
	@echo "void extint_init() {" >> extensions/ext_init.cpp
	@echo $(foreach ext_name,$(EXTS),$(EXT_INIT)) >> extensions/ext_init.cpp
	@echo "}" >> extensions/ext_init.cpp
