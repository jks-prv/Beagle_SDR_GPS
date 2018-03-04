#!/bin/bash

#https://github.com/beagleboard/beaglebone-blue/blob/master/BeagleBone_Blue_sch.pdf

source $(dirname "$0")/lib/pinmux.sh

file="BeagleBone_Blue"

echo "" > ${file}.dts
echo "" >${file}-pinmux.dts
echo "" >${file}-gpio.dts
echo "" >${file}_config-pin.txt

disable_timer="enable"

#BeagleBone Blue
gpio_index="7"

#E1
#EQEP_0A
pcbpin="E1_3" ; ball="B12" ; default_mode="1" ; cp_default="qep" ; find_ball
#EQEP_0B
pcbpin="E1_4" ; ball="C13" ; default_mode="1" ; cp_default="qep" ; find_ball

#E2
#EQEP_1A V2
gpio_index="6"
pcbpin="E2_3" ; ball="V2" ; default_mode="2" ; cp_default="qep" ; find_ball
gpio_index="7"

#EQEP_1B V3
gpio_index="6"
pcbpin="E2_4" ; ball="V3" ; default_mode="2" ; cp_default="qep" ; find_ball
gpio_index="7"

#E3
#EQEP_1A T12
pcbpin="E3_3" ; ball="T12" ; default_mode="4" ; cp_default="qep" ; find_ball
#EQEP_1B R12
pcbpin="E3_4" ; ball="R12" ; default_mode="4" ; cp_default="qep" ; find_ball

#E4
#pruin V13
pcbpin="E4_3" ; ball="V13" ; default_mode="6" ; cp_default="pruin" ; find_ball
#pruin U13
pcbpin="E4_4" ; ball="U13" ; default_mode="6" ; cp_default="pruin" ; find_ball

#UT0
#UART0_RX
pcbpin="UT0_3" ; ball="E15" ; default_mode="0" ; cp_default="uart" ; find_ball
#UART0_TX
pcbpin="UT0_4" ; ball="E16" ; default_mode="0" ; cp_default="uart" ; find_ball

#UT1
#UART1_RX
gpio_index="6"
pcbpin="UT1_3" ; ball="D16" ; default_mode="0" ; cp_default="uart" ; find_ball
gpio_index="7"

#UART1_TX
gpio_index="6"
pcbpin="UT1_4" ; ball="D15" ; default_mode="0" ; cp_default="uart" ; find_ball
gpio_index="7"

#UT5
#UART5_RX
pcbpin="UT5_3" ; ball="U2" ; default_mode="4" ; cp_default="uart" ; find_ball
#UART5_TX
pcbpin="UT5_4" ; ball="U1" ; default_mode="4" ; cp_default="uart" ; find_ball

#CAN has to be on...

#DSM2
#UART4_RX
pcbpin="DSM2_3" ; ball="T17" ; default_mode="6" ; cp_default="uart" ; find_ball

#i2c has to be on...

#GP0
gpio_index="6"
pcbpin="GP0_3" ; ball="U16" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="GP0_4" ; ball="V14" ; default_mode="7" ; find_ball

gpio_index="6"
pcbpin="GP0_5" ; ball="D13" ; default_mode="7" ; find_ball
gpio_index="7"

pcbpin="GP0_6" ; ball="C12" ; default_mode="7" ; find_ball

#GP1
pcbpin="GP1_3" ; ball="J15" ; default_mode="7" ; find_ball

pcbpin="GP1_4" ; ball="H17" ; default_mode="7" ; find_ball

gpio_index="2"
pcbpin="GP1_5" ; ball="R7" ; default_mode="7" ; find_ball
gpio_index="7"

gpio_index="2"
pcbpin="GP1_6" ; ball="T7" ; default_mode="7" ; find_ball
gpio_index="7"

#ADC

#GPS
pcbpin="GPS_3" ; ball="A17" ; default_mode="1" ; cp_default="uart" ; find_ball
pcbpin="GPS_4" ; ball="B17" ; default_mode="1" ; cp_default="uart" ; find_ball

#SPI1
#SPI1_MOSI
gpio_index="6"
pcbpin="SPI1_3" ; ball="D12" ; default_mode="3" ; cp_default="spi" ; find_ball
gpio_index="7"

#SPI1_MISO
gpio_index="6"
pcbpin="SPI1_4" ; ball="B13" ; default_mode="3" ; cp_default="spi" ; find_ball
gpio_index="7"

#SPI1_SCK
gpio_index="6"
pcbpin="SPI1_5" ; ball="A13" ; default_mode="3" ; cp_default="spi_sclk" ; find_ball
gpio_index="7"

#SPI1_SS1
pcbpin="S1.1_6" ; ball="H18" ; default_mode="2" ; cp_default="spi_cs" ; find_ball
#SPI1_SS2
pcbpin="S1.2_6" ; ball="C18" ; default_mode="2" ; cp_default="spi_cs" ; find_ball

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
