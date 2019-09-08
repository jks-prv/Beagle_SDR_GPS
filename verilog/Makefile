V_SRC_DIR = /media/sf_shared

all:
	@reset
	@echo "build project with Vivado via tcl"
	mkdir -p generated
	time vivado -mode batch -source make_proj.tcl
	
regen:
	@reset
	@echo "regenerate project with Vivado via tcl"
	rm -rf KiwiSDR generated/*
	mkdir -p generated
	time vivado -mode batch -source make_proj.tcl -tclargs regen
	
cv:
	@echo "you probably want to use \"make cv2\" here"

# command to "copy verilog" from KiwiSDR distribution into our build directory
# designed to complement the "make cv" command run in the KiwiSDR server build directory
EXCLUDE_RSYNC = ".DS_Store" "KiwiSDR/" "generated" ".Xil/" "vivado.*"
cv2:
	rsync -av --delete $(addprefix --exclude , $(EXCLUDE_RSYNC)) $(V_SRC_DIR)/KiwiSDR/ .
#	rsync -av --delete --exclude .DS_Store $(V_SRC_DIR)/KiwiSDR.Vivado.2014.4.ip/ ../import_ip
	rsync -av --delete --exclude .DS_Store $(V_SRC_DIR)/KiwiSDR.Vivado.2017.4.ip/ ../import_ip

cb:
	cp ../*.runs/impl_1/KiwiSDR.bit $(V_SRC_DIR)

ts:
	-tail -f KiwiSDR/KiwiSDR.runs/synth_1/runme.log
cs:
	cat KiwiSDR/KiwiSDR.runs/synth_1/runme.log

ti:
	-tail -f KiwiSDR/KiwiSDR.runs/impl_1/runme.log

ci:
	cat KiwiSDR/KiwiSDR.runs/impl_1/runme.log

RSYNC_REV_INC = $(addprefix --include=, "*.veo" "*.xci" "*.xml")
cip_rev:
	rsync -av --delete-excluded --prune-empty-dirs --include "*/"  $(RSYNC_REV_INC) --exclude="*" ../KiwiSDR.srcs/sources_1/ip/ ../import_ip/
	rsync -av --delete --exclude .DS_Store ../import_ip/ $(V_SRC_DIR)/KiwiSDR.ip

cm:
	cp kiwi_map.mrp kiwi_map_mrp.html $(V_SRC_DIR)
	cp KiwiSDR.syr kiwi_syr.html $(V_SRC_DIR)
	cp KiwiSDR.twr kiwi_twr.html $(V_SRC_DIR)
	cp KiwiSDR.twx $(V_SRC_DIR)

cp:
	cp KiwiSDR.pwr $(V_SRC_DIR)

clean:
	rm -f kiwi.coe
