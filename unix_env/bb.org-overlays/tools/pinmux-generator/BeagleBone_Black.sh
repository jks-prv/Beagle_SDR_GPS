#!/bin/bash

source $(dirname "$0")/lib/pinmux.sh

file="BeagleBone_Black"

generate_timer="enable"
generate_spi="enable"

echo "" > ${file}.dts
echo "" >${file}-pinmux.dts
echo "" >${file}-gpio.dts
echo "" >${file}_config-pin.txt

#BeagleBone Black
gpio_index="7"

#pcbpin="P9_42" ; ball="C18" ; find_ball
#exit 2

msg="/************************/" ; echo_both
msg="/* P8 Header */" ; echo_both
msg="/************************/" ; echo_both
msg="" ; echo_both

pcbpin="P8_01" ; label_pin="gnd" ; label_info="GND" ; echo_label
pcbpin="P8_02" ; label_pin="gnd" ; label_info="GND" ; echo_label

msg="" ; echo_both

gpio_index="2"
pcbpin="P8_03" ; ball="R9" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_04" ; ball="T9" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_05" ; ball="R8" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_06" ; ball="T8" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_07" ; ball="R7" ; default_mode="7" ; find_ball
pcbpin="P8_08" ; ball="T7" ; default_mode="7" ; find_ball
pcbpin="P8_09" ; ball="T6" ; default_mode="7" ; find_ball
pcbpin="P8_10" ; ball="U6" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P8_11" ; ball="R12" ; default_mode="7" ; find_ball
pcbpin="P8_12" ; ball="T12" ; default_mode="7" ; find_ball

gpio_index="6"
pcbpin="P8_13" ; ball="T10" ; default_mode="7" ; find_ball
pcbpin="P8_14" ; ball="T11" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P8_15" ; ball="U13" ; default_mode="7" ; find_ball
pcbpin="P8_16" ; ball="V13" ; default_mode="7" ; find_ball

gpio_index="6"
pcbpin="P8_17" ; ball="U12" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P8_18" ; ball="V12" ; default_mode="7" ; find_ball

gpio_index="6"
pcbpin="P8_19" ; ball="U10" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P8_20" ; ball="V9" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_21" ; ball="U9" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball

gpio_index="2"
pcbpin="P8_22" ; ball="V8" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_23" ; ball="U8" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_24" ; ball="V7" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
pcbpin="P8_25" ; ball="U7" ; default_mode="7" ; cp_default="emmc" ; use_name="emmc" ; cp_default="emmc" ; find_ball
gpio_index="7"

gpio_index="1"
pcbpin="P8_26" ; ball="V6" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P8_27" ; ball="U5" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_28" ; ball="V5" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_29" ; ball="R5" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_30" ; ball="R6" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_31" ; ball="V4" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball

gpio_index="6"
pcbpin="P8_32" ; ball="T5" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_33" ; ball="V3" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_34" ; ball="U4" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_35" ; ball="V2" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_36" ; ball="U3" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
gpio_index="7"

pcbpin="P8_37" ; ball="U1" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_38" ; ball="U2" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_39" ; ball="T3" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_40" ; ball="T4" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball

gpio_index="6"
pcbpin="P8_41" ; ball="T1" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_42" ; ball="T2" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_43" ; ball="R3" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_44" ; ball="R4" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_45" ; ball="R1" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
pcbpin="P8_46" ; ball="R2" ; default_mode="7" ; cp_default="hdmi" ; use_name="hdmi" ; default_name="hdmi" ; find_ball
gpio_index="7"

msg="/************************/" ; echo_both
msg="/* P9 Header */" ; echo_both
msg="/************************/" ; echo_both ; msg="" ; echo_both

pcbpin="P9_01" ; label_pin="gnd" ; label_info="GND" ; echo_label
pcbpin="P9_02" ; label_pin="gnd" ; label_info="GND" ; echo_label

pcbpin="P9_03" ; label_pin="power" ; label_info="3V3" ; echo_label
pcbpin="P9_04" ; label_pin="power" ; label_info="3V3" ; echo_label

pcbpin="P9_05" ; label_pin="power" ; label_info="VDD_5V" ; echo_label
pcbpin="P9_06" ; label_pin="power" ; label_info="VDD_5V" ; echo_label

pcbpin="P9_07" ; label_pin="power" ; label_info="SYS_5V" ; echo_label
pcbpin="P9_08" ; label_pin="power" ; label_info="SYS_5V" ; echo_label

pcbpin="P9_09" ; label_pin="system" ; label_info="PWR_BUT" ; echo_label
pcbpin="P9_10" ; label_pin="system" ; label_info="RSTn" ; echo_label

pcbpin="P9_11" ; ball="T17" ; default_mode="7" ; find_ball
pcbpin="P9_12" ; ball="U18" ; default_mode="7" ; find_ball
pcbpin="P9_13" ; ball="U17" ; default_mode="7" ; find_ball
pcbpin="P9_14" ; ball="U14" ; default_mode="7" ; find_ball
pcbpin="P9_15" ; ball="R13" ; default_mode="7" ; find_ball
pcbpin="P9_16" ; ball="T14" ; default_mode="7" ; find_ball
pcbpin="P9_17" ; ball="A16" ; default_mode="7" ; find_ball
pcbpin="P9_18" ; ball="B16" ; default_mode="7" ; find_ball

pcbpin="P9_19" ; ball="D17" ; default_mode="3" ; cp_default="i2c" ; find_ball
pcbpin="P9_20" ; ball="D18" ; default_mode="3" ; cp_default="i2c" ; find_ball

pcbpin="P9_21" ; ball="B17" ; default_mode="7" ; find_ball
pcbpin="P9_22" ; ball="A17" ; default_mode="7" ; find_ball
pcbpin="P9_23" ; ball="V14" ; default_mode="7" ; find_ball

gpio_index="6"
pcbpin="P9_24" ; ball="D15" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P9_25" ; ball="A14" ; default_mode="7" ; cp_default="audio" ; use_name="audio" ; default_name="audio" ; find_ball

gpio_index="6"
pcbpin="P9_26" ; ball="D16" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P9_27" ; ball="C13" ; default_mode="7" ; find_ball
pcbpin="P9_28" ; ball="C12" ; default_mode="7" ; cp_default="audio" ; use_name="audio" ; default_name="audio" ; find_ball

gpio_index="6"
pcbpin="P9_29" ; ball="B13" ; default_mode="7" ; cp_default="audio" ; use_name="audio" ; default_name="audio" ; find_ball
pcbpin="P9_30" ; ball="D12" ; default_mode="7" ; find_ball
pcbpin="P9_31" ; ball="A13" ; default_mode="7" ; cp_default="audio" ; use_name="audio" ; default_name="audio" ; find_ball
gpio_index="7"

pcbpin="P9_32" ; label_pin="power" ; label_info="VADC" ; echo_label
pcbpin="P9_33" ; ball="C8" label_pin="adc" ; label_info="AIN4" ; echo_label_analog
pcbpin="P9_34" ; label_pin="gnd" ; label_info="AGND" ; echo_label
pcbpin="P9_35" ; ball="A8" label_pin="adc" ; label_info="AIN6" ; echo_label_analog
pcbpin="P9_36" ; ball="B8" label_pin="adc" ; label_info="AIN5" ; echo_label_analog
pcbpin="P9_37" ; ball="B7" label_pin="adc" ; label_info="AIN2" ; echo_label_analog
pcbpin="P9_38" ; ball="A7" label_pin="adc" ; label_info="AIN3" ; echo_label_analog
pcbpin="P9_39" ; ball="B6" label_pin="adc" ; label_info="AIN0" ; echo_label_analog
pcbpin="P9_40" ; ball="C7" label_pin="adc" ; label_info="AIN1" ; echo_label_analog

gpio_index="6"
pcbpin="P9_41" ; ball="D14" ; default_mode="7" ; find_ball
msg="/* P9_41.1 */" ; echo_both

pcbpin="P9_91" ; ball="D13" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P9_42" ; ball="C18" ; default_mode="7" ; find_ball
msg="/* P9_42.1 */" ; echo_both
pcbpin="P9_92" ; ball="B12" ; default_mode="7" ; find_ball

pcbpin="P9_43" ; label_pin="gnd" ; label_info="GND" ; echo_label
pcbpin="P9_44" ; label_pin="gnd" ; label_info="GND" ; echo_label
pcbpin="P9_45" ; label_pin="gnd" ; label_info="GND" ; echo_label
pcbpin="P9_46" ; label_pin="gnd" ; label_info="GND" ; echo_label

msg="" ; echo_both

cat ${file}-pinmux.dts >> ${file}.dts

echo "cape-universal {" >> ${file}.dts
echo "	compatible = \"gpio-of-helper\";" >> ${file}.dts
echo "	status = \"okay\";" >> ${file}.dts
echo "	pinctrl-names = \"default\";" >> ${file}.dts
echo "	pinctrl-0 = <>;" >> ${file}.dts

cat ${file}-gpio.dts >> ${file}.dts

echo "};" >> ${file}.dts

rm -rf ${file}-pinmux.dts || true
rm -rf ${file}-gpio.dts || true
