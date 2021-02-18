#
# When running Vivado on a machine/OS different from where the Kiwi sources are kept,
# and those two machines are connected via a shared folder, specify the shared folder directory
# with V_SRC_DIR and use the makefile targets "cv2" and "cb" to copy sources and results back and forth.
#
# When building in batch mode, as opposed to using the Vivado UI, V_SRC_DIR is passed to the tcl script
# to specify where to place the resulting .bit files.
#
# V_SRC_DIR can be set as a global environment variable else it will default to the value below.
#

ifeq ($(V_SRC_DIR)x,x)
    V_SRC_DIR = /media/sf_shared
endif

all:
	@reset
	@echo "build project with Vivado via tcl"
	time vivado -mode batch -source make_proj.tcl -tclargs --result_dir $(V_SRC_DIR)
	
rx4:
	@reset
	@echo "build rx4_wf4 project with Vivado via tcl"
	time vivado -mode batch -source make_proj.tcl -tclargs --result_dir $(V_SRC_DIR) --rx4_wf4
	
rx8:
	@reset
	@echo "build rx8_wf2 project with Vivado via tcl"
	time vivado -mode batch -source make_proj.tcl -tclargs --result_dir $(V_SRC_DIR) --rx8_wf2
	
rx3:
	@reset
	@echo "build rx3_wf3 project with Vivado via tcl"
	time vivado -mode batch -source make_proj.tcl -tclargs --result_dir $(V_SRC_DIR) --rx3_wf3
	
rx14:
	@reset
	@echo "build rx14_wf0 project with Vivado via tcl"
	time vivado -mode batch -source make_proj.tcl -tclargs --result_dir $(V_SRC_DIR) --rx14_wf0
	
rebuild:
	@reset
	@echo "rebuild project ip with Vivado via tcl"
	rm -rf KiwiSDR generated/*
	mkdir -p generated
	time vivado -mode batch -source make_proj.tcl -tclargs --result_dir $(V_SRC_DIR) --regen_ip
	
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
