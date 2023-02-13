# generate calls to extension main routines
#
# these come from the root Makefile:
#	$(EXT_DIRS) contains a list of all extension directory names
#	$(EXTS) contains a list of all extension names

# NB: BUILD_DIR only goes up one level because this Makefile is directly included by ../Makefile
BUILD_DIR = ../build
GEN_DIR = $(BUILD_DIR)/gen

EXT_FILE = $(GEN_DIR)/ext_init.cpp
EXT_INIT = "\textern void $(ext_name)_main(); $(ext_name)_main();\n"

$(EXT_FILE): extensions/Makefile $(EXT_DIRS) extensions/ext_int.h extensions/ext.h
	@echo generating $(EXT_FILE)
	@echo EXT_DIRS = $(EXT_DIRS)
	@echo "// auto-generated file -- do not edit by hand" > $(EXT_FILE)
	@echo "void extint_init() {" >> $(EXT_FILE)
	@echo $(foreach ext_name,$(EXTS),$(EXT_INIT)) >> $(EXT_FILE)1
	@sort -bdf -f $(EXT_FILE)1 >> $(EXT_FILE)
	@rm -f $(EXT_FILE)1
	@echo "}" >> $(EXT_FILE)

clean_ext:
	(cd extensions/ALE_2G/standalone_test; make clean)
	(cd extensions/HFDL; make hfdl_clean)
	(cd extensions/FT8; make ft8_clean)
