#!/bin/bash

source $(dirname "$0")/lib/pinmux.sh

file="PocketBeagle"

echo "" > ${file}.dts
echo "" >${file}-pinmux.dts
echo "" >${file}-gpio.dts
echo "" >${file}_config-pin.txt

disable_timer="enable"

#PocketBeagle
gpio_index="7"

msg="/************************/" ; echo_both
msg="/* P1 Header */" ; echo_both
msg="/************************/" ; echo_both
msg="" ; echo_both

pcbpin="P1_01" ; label_pin="power" ; label_info="VIN-AC" ; echo_label

pcbpin="P1_02" ; ball="R5" ; default_mode="7" ; cp_default="gpio_input" ; find_ball

pcbpin="P1_03" ; ball="F15" label_pin="system" ; label_info="usb1_vbus_out" ; echo_label_analog

pcbpin="P1_04" ; ball="R6" ; default_mode="7" ; find_ball

pcbpin="P1_05" ; ball="T18" label_pin="system" ; label_info="usb1_vbus_in" ; echo_label_analog

pcbpin="P1_06" ; ball="A16" ; default_mode="0" ; cp_default="spi_cs" ; find_ball

pcbpin="P1_07" ; label_pin="system" ; label_info="VIN-USB" ; echo_label

pcbpin="P1_08" ; ball="A17" ; default_mode="0" ; cp_default="spi_sclk" ; find_ball

pcbpin="P1_09" ; ball="R18" label_pin="system" ; label_info="USB1-DN" ; echo_label_analog

pcbpin="P1_10" ; ball="B17" ; default_mode="0" ; cp_default="spi" ; find_ball

pcbpin="P1_11" ; ball="R17" label_pin="system" ; label_info="USB1-DP" ; echo_label_analog

pcbpin="P1_12" ; ball="B16" ; default_mode="0" ; cp_default="spi" ; find_ball

pcbpin="P1_13" ; ball="P17" label_pin="system" ; label_info="USB1-ID" ; echo_label_analog

pcbpin="P1_14" ; label_pin="power" ; label_info="VOUT-3.3V" ; echo_label

pcbpin="P1_15" ; label_pin="gnd" ; label_info="GND" ; echo_label

pcbpin="P1_16" ; label_pin="gnd" ; label_info="GND" ; echo_label

pcbpin="P1_17" ; ball="A9" label_pin="power" ; label_info="VREFN" ; echo_label_analog

pcbpin="P1_18" ; ball="B9" label_pin="power" ; label_info="VREFP" ; echo_label_analog

pcbpin="P1_19" ; ball="B6" label_pin="adc" ; label_info="AIN0" ; echo_label_analog

gpio_index="6"
pcbpin="P1_20" ; ball="D14" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P1_21" ; ball="C7" label_pin="adc" ; label_info="AIN1" ; echo_label_analog

pcbpin="P1_22" ; label_pin="gnd" ; label_info="GND" ; echo_label

pcbpin="P1_23" ; ball="B7" label_pin="adc" ; label_info="AIN2" ; echo_label_analog

pcbpin="P1_24" ; label_pin="power" ; label_info="VOUT-5V" ; echo_label

pcbpin="P1_25" ; ball="A7" label_pin="adc" ; label_info="AIN3" ; echo_label_analog

pcbpin="P1_26" ; ball="D18" ; default_mode="3" ; cp_default="i2c" ; find_ball

pcbpin="P1_27" ; ball="C8" label_pin="adc" ; label_info="AIN4" ; echo_label_analog

pcbpin="P1_28" ; ball="D17" ; default_mode="3" ; cp_default="i2c" ; find_ball

pcbpin="P1_29" ; ball="A14" ; default_mode="6" ; cp_default="pruin" ; find_ball

pcbpin="P1_30" ; ball="E16" ; default_mode="0" ; cp_default="uart" ; find_ball

pcbpin="P1_31" ; ball="B12" ; default_mode="6" ; cp_default="pruin" ; find_ball

pcbpin="P1_32" ; ball="E15" ; default_mode="0" ; cp_default="uart" ; find_ball

gpio_index="6"
pcbpin="P1_33" ; ball="B13" ; default_mode="6" ; cp_default="pruin" ; find_ball
gpio_index="7"

gpio_index="6"
pcbpin="P1_34" ; ball="T11" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P1_35" ; ball="V5" ; default_mode="6" ; cp_default="pruin" ; find_ball

gpio_index="6"
pcbpin="P1_36" ; ball="A13" ; default_mode="1" ; cp_default="pwm" ; find_ball
gpio_index="7"

msg="" ; echo_both

msg="/************************/" ; echo_both
msg="/* P2 Header */" ; echo_both
msg="/************************/" ; echo_both
msg="" ; echo_both

pcbpin="P2_01" ; ball="U14" ; default_mode="6" ; cp_default="pwm" ; find_ball

default_mode="7" ; pcbpin="P2_02" ; ball="V17" ; find_ball

gpio_index="6"
pcbpin="P2_03" ; ball="T10" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P2_04" ; ball="T16" ; default_mode="7" ; find_ball

pcbpin="P2_05" ; ball="T17" ; default_mode="6" ; cp_default="uart" ; find_ball

gpio_index="6"
pcbpin="P2_06" ; ball="U16" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P2_07" ; ball="U17" ; default_mode="6" ; cp_default="uart" ; find_ball

default_mode="7" ; pcbpin="P2_08" ; ball="U18" ; find_ball

gpio_index="6"
pcbpin="P2_09" ; ball="D15" ; default_mode="3" ; cp_default="i2c" ; find_ball
gpio_index="7"

default_mode="7" ; pcbpin="P2_10" ; ball="R14" ; find_ball

gpio_index="6"
pcbpin="P2_11" ; ball="D16" ; default_mode="3" ; cp_default="i2c" ; find_ball
gpio_index="7"

pcbpin="P2_12" ; label_pin="system" ; label_info="POWER_BUTTON" ; echo_label

pcbpin="P2_13" ; label_pin="power" ; label_info="VOUT-5V" ; echo_label

pcbpin="P2_14" ; label_pin="power" ; label_info="BAT-VIN" ; echo_label

pcbpin="P2_15" ; label_pin="gnd" ; label_info="GND" ; echo_label

pcbpin="P2_16" ; label_pin="power" ; label_info="BAT-TEMP" ; echo_label

pcbpin="P2_17" ; ball="V12" ; default_mode="7" ; find_ball
pcbpin="P2_18" ; ball="U13" ; default_mode="7" ; find_ball

gpio_index="6"
pcbpin="P2_19" ; ball="U12" ; default_mode="7" ; find_ball
gpio_index="7"

gpio_index="6"
pcbpin="P2_20" ; ball="T13" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="P2_21" ; label_pin="gnd" ; label_info="GND" ; echo_label

pcbpin="P2_22" ; ball="V13" ; default_mode="7" ; find_ball

pcbpin="P2_23" ; label_pin="power" ; label_info="VOUT-3.3V" ; echo_label

pcbpin="P2_24" ; ball="T12" ; default_mode="7" ; find_ball

pcbpin="P2_25" ; ball="E17" ; default_mode="4" ; cp_default="spi" ; find_ball

pcbpin="P2_26" ; label_pin="system" ; label_info="RESET#" ; echo_label

pcbpin="P2_27" ; ball="E18" ; default_mode="4" ; cp_default="spi" ; find_ball

gpio_index="6"
pcbpin="P2_28" ; ball="D13" ; default_mode="6" ; cp_default="pruin" ; find_ball
gpio_index="7"

pcbpin="P2_29" ; ball="C18" ; default_mode="4" ; cp_default="spi_sclk" ; find_ball

pcbpin="P2_30" ; ball="C12" ; default_mode="6" ; cp_default="pruin" ; find_ball

gpio_index="6"
pcbpin="P2_31" ; ball="A15" ; default_mode="4" ; cp_default="spi_cs" ; find_ball
gpio_index="7"

gpio_index="6"
pcbpin="P2_32" ; ball="D12" ; default_mode="6" ; cp_default="pruin" ; find_ball
gpio_index="7"

pcbpin="P2_33" ; ball="R12" ; default_mode="7" ; find_ball
pcbpin="P2_34" ; ball="C13" ; default_mode="6" ; cp_default="pruin" ; find_ball
pcbpin="P2_35" ; ball="U5" ; default_mode="7" ; cp_default="gpio_input" ; find_ball

pcbpin="P2_36" ; ball="C9" label_pin="adc" ; label_info="AIN7" ; echo_label_analog

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
