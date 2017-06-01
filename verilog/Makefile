V_SRC_DIR = /media/sf_shared
V_DST_DIR = .

all:
	@echo "no default"
	
cv:
	@echo "you probably want to use \"make cv2\" here"
	
cv2:
	rsync -av --delete --exclude .DS_Store $(V_SRC_DIR)/KiwiSDR/ $(V_DST_DIR)

cb:
	cp ~/verilog/KiwiSDR/KiwiSDR.runs/impl_1/KiwiSDR.bit $(V_SRC_DIR)

cm:
	cp kiwi_map.mrp kiwi_map_mrp.html $(V_SRC_DIR)
	cp KiwiSDR.syr kiwi_syr.html $(V_SRC_DIR)
	cp KiwiSDR.twr kiwi_twr.html $(V_SRC_DIR)
	cp KiwiSDR.twx $(V_SRC_DIR)

cp:
	cp KiwiSDR.pwr $(V_SRC_DIR)

clean:
