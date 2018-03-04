#!/bin/bash -e

#cape-univ*-00A0.dts

#sed -i -e 's:_pin {:_pin { pinctrl-single,pins = <:g' src/arm/cape-univ*-00A0.dts
#sed -i -e 's:				pinctrl-single,pins = <0x0:				AM33XX_IOPAD(0x08:g' src/arm/cape-univ*-00A0.dts
#sed -i -e 's:				pinctrl-single,pins = <0x1:				AM33XX_IOPAD(0x09:g' src/arm/cape-univ*-00A0.dts

sed -i -e 's:  0x00>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE0) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x01>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE1) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x02>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE2) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x03>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE3) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x04>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE4) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x05>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE5) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x06>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE6) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x07>; };:, PIN_OUTPUT_PULLDOWN | MUX_MODE7) >; };:g' src/arm/cape-univ*-00A0.dts

sed -i -e 's:  0x20>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE0) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x21>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE1) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x22>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE2) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x23>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE3) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x24>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE4) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x25>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE5) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x26>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE6) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x27>; };:, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE7) >; };:g' src/arm/cape-univ*-00A0.dts

sed -i -e 's:  0x2F>; };:, PIN_OUTPUT | INPUT_EN | MUX_MODE7) >; };:g' src/arm/cape-univ*-00A0.dts

sed -i -e 's:  0x30>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE0) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x31>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE1) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x32>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE2) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x33>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE3) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x34>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE4) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x35>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE5) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x36>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE6) >; };:g' src/arm/cape-univ*-00A0.dts
sed -i -e 's:  0x37>; };:, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE7) >; };:g' src/arm/cape-univ*-00A0.dts

