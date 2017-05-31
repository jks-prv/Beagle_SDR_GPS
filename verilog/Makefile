V_SRC_DIR = /media/sf_shared
V_DST_DIR = .

IS_VIVADO_ISE = $(shell grep -q -s Vivado IS_VIVADO; echo $$?)
VIVADO = 0
ISE = 2

all:
	@echo "no default"
	
cv:
	@echo "you probably want to use \"make cv2\" here"
	
cv2:
	rsync -av --delete --exclude .DS_Store $(V_SRC_DIR)/KiwiSDR/ $(V_DST_DIR)

cb:
ifeq ($(IS_VIVADO_ISE),$(VIVADO))
	cp ~/verilog/KiwiSDR/runs/KiwiSDR.bit $(V_SRC_DIR)
else
	cp KiwiSDR.bit $(V_SRC_DIR)
endif

cm:
	cp kiwi_map.mrp kiwi_map_mrp.html $(V_SRC_DIR)
	cp KiwiSDR.syr kiwi_syr.html $(V_SRC_DIR)
	cp KiwiSDR.twr kiwi_twr.html $(V_SRC_DIR)
	cp KiwiSDR.twx $(V_SRC_DIR)

cp:
	cp KiwiSDR.pwr $(V_SRC_DIR)

cc:
	cp KiwiSDR.xdc ../../../constrs_1/new

cc2:
	cp ../../../constrs_1/new/KiwiSDR.xdc KiwiSDR.new.xdc
	cp KiwiSDR.new.xdc $(V_SRC_DIR)
	
clean:
	-rm -rf *.coe
