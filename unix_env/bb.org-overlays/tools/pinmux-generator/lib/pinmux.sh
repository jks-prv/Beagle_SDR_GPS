#!/bin/bash

#cat AM335x.json | jq '.devicePins'
#cat AM335x.json | jq '.useCases'
#cat AM335x.json | jq '.packages .ID_0 .packagePin[202] .devicePinID'

echo_both () {
#	echo "$msg"
	echo "$msg" >> ${file}.dts
	echo "$msg" >> ${file}-pinmux.dts
}

echo_label () {
	msg="/* ${pcbpin}                ${label_info} */" ; echo_both ; msg="" ; echo_both
	echo "${pcbpin}_PIN=\"${label_pin}\"" >>${file}_config-pin.txt
	echo "${pcbpin}_INFO=\"${label_info}\"" >>${file}_config-pin.txt
	echo "${pcbpin}_CAPE=\"\"" >>${file}_config-pin.txt
	echo "" >>${file}_config-pin.txt
}

echo_label_analog () {
	msg="/* ${pcbpin} (ZCZ ball ${ball})  ${label_info}         */" ; echo_both ; msg="" ; echo_both
	echo "${pcbpin}_PIN=\"${label_pin}\"" >>${file}_config-pin.txt
	echo "${pcbpin}_INFO=\"${label_info}\"" >>${file}_config-pin.txt
	echo "${pcbpin}_CAPE=\"\"" >>${file}_config-pin.txt
	echo "" >>${file}_config-pin.txt
}

echo_pinmux () {
	if [ "x${cp_default}" = "x" ] ; then
		echo "/* ${pcbpin} (ZCZ ball ${found_ball}) */" >> ${file}-pinmux.dts
	else
		echo "/* ${pcbpin} (ZCZ ball ${found_ball}) ${cp_default} */" >> ${file}-pinmux.dts
		unset default_name
	fi
	echo "${pcbpin}_pinmux {" >> ${file}-pinmux.dts
	echo "	compatible = \"bone-pinmux-helper\";" >> ${file}-pinmux.dts
	echo "	status = \"okay\";" >> ${file}-pinmux.dts
	list="\"default\", \"gpio\", \"gpio_pu\", \"gpio_pd\", \"gpio_input\""
	cp_pinmux="default gpio gpio_pu gpio_pd gpio_input"
	if [ "x${cp_info_default}" = "x" ] ; then
		cp_info="${gpio_name} default ${gpio_name} ${gpio_name} ${gpio_name} ${gpio_name}"
	else
		cp_info="${cp_info_default} default ${gpio_name} ${gpio_name} ${gpio_name} ${gpio_name}"
		unset cp_info_default
	fi
	if [ "x${got_spi_pin}" = "xenable" ] ; then
		list="${list}, \"spi\""
		cp_pinmux="${cp_pinmux} spi"
		cp_info="${cp_info} ${spi_name}"
	fi
	if [ "x${got_spi_cs_pin}" = "xenable" ] ; then
		list="${list}, \"spi_cs\""
		cp_pinmux="${cp_pinmux} spi_cs"
		cp_info="${cp_info} ${spi_cs_name}"
	fi
	if [ "x${got_spi_sclk_pin}" = "xenable" ] ; then
		list="${list}, \"spi_sclk\""
		cp_pinmux="${cp_pinmux} spi_sclk"
		cp_info="${cp_info} ${spi_sclk_name}"
	fi
	if [ "x${got_uart_pin}" = "xenable" ] ; then
		list="${list}, \"uart\""
		cp_pinmux="${cp_pinmux} uart"
		cp_info="${cp_info} ${uart_name}"
	fi
	if [ "x${got_can_pin}" = "xenable" ] ; then
		list="${list}, \"can\""
		cp_pinmux="${cp_pinmux} can"
		cp_info="${cp_info} ${can_name}"
	fi
	if [ "x${got_i2c_pin}" = "xenable" ] ; then
		list="${list}, \"i2c\""
		cp_pinmux="${cp_pinmux} i2c"
		cp_info="${cp_info} ${i2c_name}"
	fi
	if [ "x${got_eqep_pin}" = "xenable" ] ; then
		list="${list}, \"qep\""
		cp_pinmux="${cp_pinmux} qep"
		cp_info="${cp_info} ${eqep_name}"
	fi
	if [ "x${got_pwm_pin}" = "xenable" ] ; then
		list="${list}, \"pwm\""
		cp_pinmux="${cp_pinmux} pwm"
		cp_info="${cp_info} ${pwm_name}"
	fi
	if [ "x${got_pru_uart_pin}" = "xenable" ] ; then
		list="${list}, \"pru_uart\""
		cp_pinmux="${cp_pinmux} pru_uart"
		cp_info="${cp_info} pru_uart"
	fi
	if [ "x${got_timer_pin}" = "xenable" ] ; then
		list="${list}, \"timer\""
		cp_pinmux="${cp_pinmux} timer"
		cp_info="${cp_info} ${timer_name}"
	fi
	if [ "x${got_pru_ecap_pin}" = "xenable" ] ; then
		list="${list}, \"pru_ecap\""
		cp_pinmux="${cp_pinmux} pru_ecap"
		cp_info="${cp_info} pru_ecap"
	fi
	if [ "x${got_pruout_pin}" = "xenable" ] ; then
		list="${list}, \"pruout\""
		cp_pinmux="${cp_pinmux} pruout"
		cp_info="${cp_info} ${pruout_name}"
	fi
	if [ "x${got_pruin_pin}" = "xenable" ] ; then
		list="${list}, \"pruin\""
		cp_pinmux="${cp_pinmux} pruin"
		cp_info="${cp_info} ${pruin_name}"
	fi

	echo "${pcbpin}_PRU=\"${cp_pru_gpio_number}\"" >> ${file}_config-pin.txt
	echo "${pcbpin}_GPIO=\"${cp_gpio_number}\"" >> ${file}_config-pin.txt

	if [ "x${cp_default}" = "x" ] ; then
		echo "${pcbpin}_PIN=\"gpio\"" >> ${file}_config-pin.txt
	else
		echo "${pcbpin}_PIN=\"${cp_default}\"" >> ${file}_config-pin.txt
		unset cp_default
	fi

	echo "${pcbpin}_PINMUX=\"${cp_pinmux}\"" >> ${file}_config-pin.txt
	echo "${pcbpin}_INFO=\"${cp_info}\"" >> ${file}_config-pin.txt
	echo "${pcbpin}_CAPE=\"\"" >> ${file}_config-pin.txt
	echo "" >> ${file}_config-pin.txt

	echo "	pinctrl-names = ${list};" >> ${file}-pinmux.dts
	echo "	pinctrl-0 = <&${pcbpin}_default_pin>;" >> ${file}-pinmux.dts
	echo "	pinctrl-1 = <&${pcbpin}_gpio_pin>;" >> ${file}-pinmux.dts
	echo "	pinctrl-2 = <&${pcbpin}_gpio_pu_pin>;" >> ${file}-pinmux.dts
	echo "	pinctrl-3 = <&${pcbpin}_gpio_pd_pin>;" >> ${file}-pinmux.dts
	echo "	pinctrl-4 = <&${pcbpin}_gpio_input_pin>;" >> ${file}-pinmux.dts
	index=5
	if [ "x${got_spi_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_spi_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_spi_cs_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_spi_cs_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_spi_sclk_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_spi_sclk_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_uart_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_uart_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_can_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_can_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_i2c_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_i2c_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_eqep_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_qep_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_pwm_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_pwm_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_pru_uart_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_pru_uart_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_pru_ecap_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_pru_ecap_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_timer_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_timer_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_pruout_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_pruout_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi
	if [ "x${got_pruin_pin}" = "xenable" ] ; then
		echo "	pinctrl-${index} = <&${pcbpin}_pruin_pin>;" >> ${file}-pinmux.dts
		index=$((index + 1))
	fi

	echo "};" >> ${file}-pinmux.dts
	echo "" >> ${file}-pinmux.dts
}

echo_gpio () {
	echo "	${pcbpin} {" >> ${file}-gpio.dts
	echo "		gpio-name = \"${pcbpin}\";" >> ${file}-gpio.dts
	echo "		gpio = <&${gpio_pinmux} 0>;" >> ${file}-gpio.dts
	echo "		input;" >> ${file}-gpio.dts
	echo "		dir-changeable;" >> ${file}-gpio.dts
	echo "	};" >> ${file}-gpio.dts
	echo "">> ${file}-gpio.dts
}

get_json_pkg () {
	pinmux_version="4.0.1511"
	if [ -d ./tmp/ ] ; then
		rm -rf ./tmp/ || true
	fi
	wget -c http://software-dl.ti.com/ccs/esd/pinmux/pinmux-${pinmux_version}-setup.run
	chmod +x pinmux-${pinmux_version}-setup.run
	mkdir tmp
	./pinmux-${pinmux_version}-setup.run --unattendedmodeui none --mode unattended --prefix ./tmp
	cp -v tmp/pinmux/deviceData/AM335x/AM335x.json ./
	rm -rf ./tmp/ || true
	rm -rf pinmux-${pinmux_version}-setup.run || true
	rm -rf pinmux_${pinmux_version}.log || true
}

get_name_mode () {
	#cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .signalName' | sed 's/\"//g' | sed 's/\[/_/g' | sed 's/\]//g' || true

	name=$(cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .signalName' | sed 's/\"//g' | sed 's/\[/_/g' | sed 's/\]//g' | awk '{print tolower($0)}' || true)
	mode=$(cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .mode' | sed 's/\"//g' | awk '{print tolower($0)}' || true)
	ioDir=$(cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .ioDir' | sed 's/\"//g' || true)
}

find_ball () {
	echo "##################"
	echo "${pcbpin}"
	#Use "ball" to get devicePinID & powerDomainID

	#cat AM335x.json | jq '.packages .ID_0 .packagePin[0]'
	#{
	#  "devicePinID": "ID_1690",
	#  "ball": "R13",
	#  "powerDomainID": "ID_1691"
	#}

	for number in {0..202}
	do
		compare=$(cat AM335x.json | jq '.packages .ID_0 .packagePin['$number'] .ball' | sed 's/\"//g' || true)
		if [ "x${compare}" = "x${ball}" ] ; then
			#echo "debug-${ball}-----------------------------------------"
			#cat AM335x.json | jq '.packages .ID_0 .packagePin['$number']'
			#echo "debug-${ball}-----------------------------------------"
			found_devicePinID=$(cat AM335x.json | jq '.packages .ID_0 .packagePin['$number'] .devicePinID' | sed 's/\"//g' || true)
			found_ball=$(cat AM335x.json | jq '.packages .ID_0 .packagePin['$number'] .ball' | sed 's/\"//g' || true)
			found_powerDomainID=$(cat AM335x.json | jq '.packages .ID_0 .packagePin['$number'] .powerDomainID' | sed 's/\"//g' || true)
			echo "devicePinID=${found_devicePinID},ball=${found_ball},powerDomainID=${found_powerDomainID}"
			break;
		fi
	done

	#Using devicePinID find name

	#cat AM335x.json | jq '.devicePins .ID_1985'
	#{
	#  "type": "DevicePin",
	#  "name": "lcd_hsync",
	#  "description": "",
	#  "devicePinType": "Default",
	#  "id": "ID_1985"
	#}

	#echo "debug-${ball}-----------------------------------------"
	#cat AM335x.json | jq '.devicePins .'${found_devicePinID}''
	#echo "debug-${ball}-----------------------------------------"
	PinID=$(cat AM335x.json | jq '.devicePins .'${found_devicePinID}' .name' | sed 's/\"//g' || true)
	echo "name=${PinID}"

	#Using devicePinID find controlRegisterOffset

	#cat AM335x.json | jq '.pinCommonInfos .ID_1985'
	#{
	#  "type": "PinCommonInfo",
	#  "devicePinID": "ID_1985",
	#  "controlRegisterOffset": "0x08e4",
	#  "pupdStateDuringHHV": "OFF",
	#  "pupdStateAfterHHV": "PD",

	#echo "debug-${ball}-----------------------------------------"
	#cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}''
	#echo "debug-${ball}-----------------------------------------"
	cro=$(cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .controlRegisterOffset' | sed 's/\"//g' || true)
	echo "controlRegisterOffset=${cro}"

	#  "pinModeInfo": [
	#    {
	#      "peripheralPinID": "ID_1986",
	#      "mode": "0",
	#      "interfaceName": "LCDC",
	#      "signalName": "lcd_hsync",
	#      "ioDir": "O"
	#    },
	#    {
	#      "peripheralPinID": "ID_1775",
	#      "mode": "1",
	#      "interfaceName": "GPMC",
	#      "signalName": "gpmc_a9",
	#      "ioDir": "O"
	#    },
	#    {
	#      "peripheralPinID": "ID_1715",
	#      "mode": "2",
	#      "interfaceName": "GPMC",
	#      "signalName": "gpmc_a2",
	#      "ioDir": "O"
	#    },
	#    {
	#      "peripheralPinID": "ID_1987",
	#      "mode": "3",
	#      "interfaceName": "ECAT_PRUSS1",
	#      "signalName": "pr1_edio_data_in3",
	#      "ioDir": "I"
	#    },
	#    {
	#      "peripheralPinID": "ID_1988",
	#      "mode": "4",
	#      "interfaceName": "ECAT_PRUSS1",
	#      "signalName": "pr1_edio_data_out3",
	#      "ioDir": "O"
	#    },
	#    {
	#      "peripheralPinID": "ID_1989",
	#      "mode": "5",
	#      "interfaceName": "PRU1_PRUSS1",
	#      "signalName": "pr1_pru1_pru_r30[9]",
	#      "ioDir": "O"
	#    },
	#    {
	#      "peripheralPinID": "ID_1990",
	#      "mode": "6",
	#      "interfaceName": "PRU1_PRUSS1",
	#      "signalName": "pr1_pru1_pru_r31[9]",
	#      "ioDir": "I"
	#    },
	#    {
	#      "peripheralPinID": "ID_1991",
	#      "mode": "7",
	#      "interfaceName": "GPIO2",
	#      "signalName": "gpio2[23]",
	#      "ioDir": "IO"
	#    }
	#  ]
	#}

	for number in {0..7}
	do
		cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .interfaceName'
		compare=$(cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .interfaceName' | sed 's/\"//g' || true)
		get_name_mode
		echo ${pcbpin}:${ball}:${name}:${mode}:${ioDir}:${number}

#		if [ ! "x${name}" = "xnull" ] ; then
#			echo "/* ${pcbpin}:${ball}:${name}:${mode}:${ioDir} */" >> ${file}.dts
#		fi

		if [ "x${mode}" = "x${default_mode}" ] ; then
			echo default_mode=index=${number}
			default_index=${number}
		fi
	done

	unset cp_info_default
	number=${default_index}
	get_name_mode
	echo ${pcbpin}:${ball}:${name}:${mode}:${ioDir}:${number}

	case "${cp_default}" in
	pruin)
		pruin_tmp=$(echo ${name} | sed 's/pr1_//g' | sed 's/pru_r31_/in/g')
		name=${pruin_tmp}
		;;
	esac

	if [ ! "x${use_name}" = "x" ] ; then
		echo "/* ${pcbpin} (ZCZ ball ${found_ball}) ${use_name} */" >> ${file}.dts
		unset use_name
	else
		echo "/* ${pcbpin} (ZCZ ball ${found_ball}) ${name} */" >> ${file}.dts
	fi
	cp_info_default=${name}

	dtabs=1
	case "${cp_default}" in
	gpio_input|pruin)
		dtabs=3
		pinsetting="PIN_INPUT"
		;;
	pwm)
		pinsetting="PIN_OUTPUT_PULLDOWN | INPUT_EN"
		;;
	uart|i2c|spi|spi_cs|spi_sclk)
		pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
		;;
	*)
		pinsetting="PIN_OUTPUT_PULLDOWN | INPUT_EN"
		;;
	esac

	echo "${pcbpin}_default_pin: pinmux_${pcbpin}_default_pin { pinctrl-single,pins = <" >> ${file}.dts
	if [ "x${dtabs}" = "x1" ] ; then
		echo "	AM33XX_IOPAD(${cro}, ${pinsetting} | MUX_MODE${mode}) >; };	/* ${PinID}.${name} */" >> ${file}.dts
	fi
	if [ "x${dtabs}" = "x2" ] ; then
		echo "	AM33XX_IOPAD(${cro}, ${pinsetting} | MUX_MODE${mode}) >; };		/* ${PinID}.${name} */" >> ${file}.dts
	fi
	if [ "x${dtabs}" = "x3" ] ; then
		echo "	AM33XX_IOPAD(${cro}, ${pinsetting} | MUX_MODE${mode}) >; };			/* ${PinID}.${name} */" >> ${file}.dts
	fi

	number=${gpio_index}
	get_name_mode

	echo "${pcbpin}_gpio_pin: pinmux_${pcbpin}_gpio_pin { pinctrl-single,pins = <" >> ${file}.dts
	echo "	AM33XX_IOPAD(${cro}, PIN_OUTPUT | INPUT_EN | MUX_MODE${mode}) >; };		/* ${PinID}.${name} */" >> ${file}.dts
	echo "${pcbpin}_gpio_pu_pin: pinmux_${pcbpin}_gpio_pu_pin { pinctrl-single,pins = <" >> ${file}.dts
	echo "	AM33XX_IOPAD(${cro}, PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE${mode}) >; };	/* ${PinID}.${name} */" >> ${file}.dts
	echo "${pcbpin}_gpio_pd_pin: pinmux_${pcbpin}_gpio_pd_pin { pinctrl-single,pins = <" >> ${file}.dts
	echo "	AM33XX_IOPAD(${cro}, PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE${mode}) >; };	/* ${PinID}.${name} */" >> ${file}.dts
	echo "${pcbpin}_gpio_input_pin: pinmux_${pcbpin}_gpio_input_pin { pinctrl-single,pins = <" >> ${file}.dts
	echo "	AM33XX_IOPAD(${cro}, PIN_INPUT | MUX_MODE${mode}) >; };			/* ${PinID}.${name} */" >> ${file}.dts

	gpio_mul=$(echo ${name} | awk -F'_' '{print $1}' | awk -F'gpio' '{print $2}' || true)
	gpio_add=$(echo ${name} | awk -F'_' '{print $2}' || true)
	cp_gpio_number=$(echo "${gpio_mul} * 32" | bc)
	cp_gpio_number=$(echo "${cp_gpio_number} + ${gpio_add}" | bc)
	cp_pru_gpio_number=$(echo "${cp_gpio_number} + 32" | bc)

	gpio_name=${name}
	gpio_pinmux=$(echo ${gpio_name} | sed 's/_/ /g')

	unset got_can_pin
	unset got_eqep_pin
	unset got_pwm_pin
	unset got_i2c_pin
	unset got_pru_ecap_pin
	unset got_pru_uart_pin
	unset got_pruout_pin
	unset got_pruin_pin
	unset got_spi_pin
	unset got_spi_cs_pin
	unset got_spi_sclk_pin
	unset got_timer_pin
	unset got_uart_pin

	for number in {0..7}
	do
		cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .interfaceName'
		compare=$(cat AM335x.json | jq '.pinCommonInfos .'${found_devicePinID}' .pinModeInfo['$number'] .interfaceName' | sed 's/\"//g' || true)
		get_name_mode
		if [ ! "x${name}" = "xnull" ] ; then
			echo ${name}

			unset valid_pin_mode

			tabs=1
			case "${name}" in 
			dcan*_rx)
				can_name=${name}
				valid_pin_mode="can"
				pinsetting="PIN_INPUT_PULLUP"
				got_can_pin="enable"
				tabs=2
				;;
			dcan*_tx)
				can_name=${name}
				valid_pin_mode="can"
				pinsetting="PIN_OUTPUT_PULLUP"
				got_can_pin="enable"
				tabs=2
				;;
			eqep*)
				valid_pin_mode="qep"
				eqep_name=${name}
				pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
				got_eqep_pin="enable"
				;;
			ehrpwm*|ecap0_in_pwm0_out)
				valid_pin_mode="pwm"
				pwm_name=${name}
				pinsetting="PIN_OUTPUT_PULLDOWN | INPUT_EN"
				got_pwm_pin="enable"
				;;
			i2c*_sda|i2c*_scl)
				valid_pin_mode="i2c"
				i2c_name=${name}
				pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
				got_i2c_pin="enable"
				;;
			pr1_ecap0*)
				valid_pin_mode="pru_ecap"
				pinsetting="PIN_OUTPUT_PULLDOWN | INPUT_EN"
				got_pru_ecap_pin="enable"
				;;
			pr1_uart0*)
				valid_pin_mode="pru_uart"
				pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
				got_pru_uart_pin="enable"
				;;
			pr1_pru*_pru_r30*)
				valid_pin_mode="pruout"
				pruout_name=$(echo ${name} | sed 's/pr1_//g' | sed 's/pru_r30_/out/g')
				name=${pruout_name}
				pinsetting="PIN_OUTPUT_PULLDOWN | INPUT_EN"
				got_pruout_pin="enable"
				;;
			pr1_pru*_pru_r31*)
				valid_pin_mode="pruin"
				pruin_name=$(echo ${name} | sed 's/pr1_//g' | sed 's/pru_r31_/in/g')
				name=${pruin_name}
				pinsetting="PIN_INPUT"
				got_pruin_pin="enable"
				tabs=3
				;;
			spi0_d*|spi1_d*)
				valid_pin_mode="spi"
				spi_name=${name}
				pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
				got_spi_pin="enable"
				;;
			spi0_cs*|spi1_cs*)
				valid_pin_mode="spi_cs"
				spi_cs_name=${name}
				pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
				got_spi_cs_pin="enable"
				;;
			spi*_sclk)
				valid_pin_mode="spi_sclk"
				spi_sclk_name=${name}
				pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
				got_spi_sclk_pin="enable"
				;;
			timer*)
				if [ "x${disable_timer}" = "x" ] ; then
					valid_pin_mode="timer"
					timer_name=${name}
					pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
					got_timer_pin="enable"
				fi
				;;
			uart*_rxd|uart*_txd)
				valid_pin_mode="uart"
				uart_name=${name}
				pinsetting="PIN_OUTPUT_PULLUP | INPUT_EN"
				got_uart_pin="enable"
				;;
			esac

			if [ ! "x${valid_pin_mode}" = "x" ] ; then
				echo "${pcbpin}_${valid_pin_mode}_pin: pinmux_${pcbpin}_${valid_pin_mode}_pin { pinctrl-single,pins = <" >> ${file}.dts
				if [ "x${tabs}" = "x1" ] ; then
					echo "	AM33XX_IOPAD(${cro}, ${pinsetting} | MUX_MODE${mode}) >; };	/* ${PinID}.${name} */" >> ${file}.dts
				fi
				if [ "x${tabs}" = "x2" ] ; then
					echo "	AM33XX_IOPAD(${cro}, ${pinsetting} | MUX_MODE${mode}) >; };		/* ${PinID}.${name} */" >> ${file}.dts
				fi
				if [ "x${tabs}" = "x3" ] ; then
					echo "	AM33XX_IOPAD(${cro}, ${pinsetting} | MUX_MODE${mode}) >; };			/* ${PinID}.${name} */" >> ${file}.dts
				fi
			fi
		fi
	done

echo "" >> ${file}.dts
echo_pinmux
echo_gpio

#0x05 = 0000 0101
#0x26 = 0010 0110
#0x27 = 0010 0111
#0x2F = 0010 1111
#0x30 = 0011 0000
#0x32 = 0011 0010
#0x37 = 0011 0111

#0x05 : PIN_OUTPUT_PULLDOWN | MUX_MODE5
#0x24 : PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE4
#0x26 : PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE6
#0x27 : PIN_OUTPUT_PULLDOWN | INPUT_EN | MUX_MODE7
#0x2F : PIN_OUTPUT | INPUT_EN | MUX_MODE7
#0x30 : PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE0
#0x32 : PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE2
#0x37 : PIN_OUTPUT_PULLUP | INPUT_EN | MUX_MODE7

#			P9_17_default_pin: pinmux_P9_17_default_pin {
#				pinctrl-single,pins = <0x15c  0x37>; };	/* Mode 7, Pull-Up, RxActive */
#			P9_17_gpio_pin: pinmux_P9_17_gpio_pin {
#				pinctrl-single,pins = <0x15c  0x2F>; };	/* Mode 7, RxActive */
#			P9_17_gpio_pu_pin: pinmux_P9_17_gpio_pu_pin {
#				pinctrl-single,pins = <0x15c  0x37>; };	/* Mode 7, Pull-Up, RxActive */
#			P9_17_gpio_pd_pin: pinmux_P9_17_gpio_pd_pin {
#				pinctrl-single,pins = <0x15c  0x27>; };	/* Mode 7, Pull-Down, RxActive */
#			P9_17_spi_pin: pinmux_P9_17_spi_pin {
#				pinctrl-single,pins = <0x15c  0x30>; };	/* Mode 0, Pull-Up, RxActive */
#			P9_17_i2c_pin: pinmux_P9_17_i2c_pin {
#				pinctrl-single,pins = <0x15c  0x32>; };	/* Mode 2, Pull-Up, RxActive */
#			P9_17_pwm_pin: pinmux_P9_17_pwm_pin {
#				pinctrl-single,pins = <0x15c  0x33>; };	/* Mode 3, Pull-Up, RxActive */
#			P9_17_pru_uart_pin: pinmux_P9_17_pru_uart_pin {
#				pinctrl-single,pins = <0x15c  0x34>; };	/* Mode 4, Pull-Up, RxActive */

	echo "##################"
}

if [ ! -f AM335x.json ] ; then
	get_json_pkg
fi
