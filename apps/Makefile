# generate calls to app init routines
# $(APPS) comes from the root Makefile, containing a list of all app directory names

APP_INIT = "\textern void $(app)_init(); $(app)_init();"

apps/apps_init.c:
	@echo generating apps/apps_init.c
	@echo "// auto-generated file -- do not edit by hand" > apps/apps_init.c
	@echo "void apps_init() {" >> apps/apps_init.c
	@echo $(foreach app,$(APPS),$(APP_INIT)) >> apps/apps_init.c
	@echo "}" >> apps/apps_init.c
