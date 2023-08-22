set_property IOSTANDARD LVCMOS33 [get_ports ADC_CLKIN]
set_property IOSTANDARD LVCMOS33 [get_ports ADC_CLKEN]
set_property IOSTANDARD LVCMOS33 [get_ports ADC_STENL]
set_property IOSTANDARD LVCMOS33 [get_ports ADC_STSIG]
set_property IOSTANDARD LVCMOS33 [get_ports DA_DALE]
set_property IOSTANDARD LVCMOS33 [get_ports DA_DACLK]
set_property IOSTANDARD LVCMOS33 [get_ports DA_DADAT]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_TCXO]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_ISGN]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_IMAG]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_QSGN]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_QMAG]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_GSCS]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_GSCLK]
set_property IOSTANDARD LVCMOS33 [get_ports GPS_GSDAT]
set_property IOSTANDARD LVCMOS33 [get_ports BBB_SCLK]
set_property IOSTANDARD LVCMOS33 [get_ports {BBB_CS_N[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {BBB_CS_N[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports BBB_MISO]
set_property IOSTANDARD LVCMOS33 [get_ports BBB_MOSI]
set_property IOSTANDARD LVCMOS33 [get_ports ADC_OVFL]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[13]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[12]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[11]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[10]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[9]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[8]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[7]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[6]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[5]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[4]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ADC_DATA[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports P926]
set_property IOSTANDARD LVCMOS33 [get_ports SND_INTR]
set_property IOSTANDARD LVCMOS33 [get_ports CMD_READY]
set_property IOSTANDARD LVCMOS33 [get_ports P915]
set_property IOSTANDARD LVCMOS33 [get_ports P913]
set_property IOSTANDARD LVCMOS33 [get_ports P911]
set_property IOSTANDARD LVCMOS33 [get_ports P826]
set_property IOSTANDARD LVCMOS33 [get_ports P819]
set_property IOSTANDARD LVCMOS33 [get_ports P817]
set_property IOSTANDARD LVCMOS33 [get_ports P818]
set_property IOSTANDARD LVCMOS33 [get_ports P815]
set_property IOSTANDARD LVCMOS33 [get_ports P816]
set_property IOSTANDARD LVCMOS33 [get_ports P813]
set_property IOSTANDARD LVCMOS33 [get_ports P814]
set_property IOSTANDARD LVCMOS33 [get_ports P811]
set_property IOSTANDARD LVCMOS33 [get_ports P812]
set_property IOSTANDARD LVCMOS33 [get_ports EWP]

# outputs
set_property DRIVE 4 [all_outputs]
set_property OFFCHIP_TERM NONE [all_outputs]
#set_property SLEW FAST [get_ports BBB_MISO]

# in Vivado for power analysis only
set_load 6.000 [all_outputs]

# 48 MHz
create_clock -period 20.833 -name BBB_SCLK -waveform {0.000 10.416} [get_ports BBB_SCLK]

# 16.368 MHz
create_clock -period 61.095 -name GPS_TCXO -waveform {0.000 30.548} [get_ports GPS_TCXO]

# 66.666666 & 66.666600 MHz
create_clock -period 15.000 -name ADC_CLKIN -waveform {0.000 7.500} [get_ports ADC_CLKIN]

set_input_delay -clock [get_clocks ADC_CLKIN] -min -add_delay 1.400 [get_ports {ADC_DATA[*]}]
set_input_delay -clock [get_clocks ADC_CLKIN] -max -add_delay 5.400 [get_ports {ADC_DATA[*]}]
set_input_delay -clock [get_clocks ADC_CLKIN] -min -add_delay 1.400 [get_ports ADC_OVFL]
set_input_delay -clock [get_clocks ADC_CLKIN] -max -add_delay 5.400 [get_ports ADC_OVFL]
set_input_delay -clock [get_clocks BBB_SCLK] -min -add_delay 6.846 [get_ports BBB_MOSI]
set_input_delay -clock [get_clocks BBB_SCLK] -max -add_delay 17.263 [get_ports BBB_MOSI]
set_input_delay -clock [get_clocks GPS_TCXO] -min -add_delay 10.000 [get_ports GPS_ISGN]
set_input_delay -clock [get_clocks GPS_TCXO] -max -add_delay 54.095 [get_ports GPS_ISGN]	; # gives Tsetup=7ns
set_input_delay -clock [get_clocks GPS_TCXO] -min -add_delay 10.000 [get_ports GPS_IMAG]
set_input_delay -clock [get_clocks GPS_TCXO] -max -add_delay 54.095 [get_ports GPS_IMAG]
set_input_delay -clock [get_clocks GPS_TCXO] -min -add_delay 10.000 [get_ports GPS_QSGN]
set_input_delay -clock [get_clocks GPS_TCXO] -max -add_delay 54.095 [get_ports GPS_QSGN]
set_input_delay -clock [get_clocks GPS_TCXO] -min -add_delay 10.000 [get_ports GPS_QMAG]
set_input_delay -clock [get_clocks GPS_TCXO] -max -add_delay 54.095 [get_ports GPS_QMAG]

# FIXME: is MISO timing marginal?
set_output_delay -clock [get_clocks BBB_SCLK] -max -add_delay 2.290 [get_ports BBB_MISO]
set_output_delay -clock [get_clocks BBB_SCLK] -min -add_delay -4.700 [get_ports BBB_MISO]

# arbitrary amount to keep timing report from complaining
set_output_delay -clock [get_clocks GPS_TCXO] -max -add_delay 1.000 [get_ports ADC_CLKEN]
set_output_delay -clock [get_clocks GPS_TCXO] -min -add_delay -1.000 [get_ports ADC_CLKEN]
set_output_delay -clock [get_clocks GPS_TCXO] -max -add_delay 1.000 [get_ports SND_INTR]	; # CTRL_SND_INTR
set_output_delay -clock [get_clocks GPS_TCXO] -min -add_delay -1.000 [get_ports SND_INTR]
set_output_delay -clock [get_clocks GPS_TCXO] -max -add_delay 1.000 [get_ports EWP]
set_output_delay -clock [get_clocks GPS_TCXO] -min -add_delay -1.000 [get_ports EWP]

# SPI CS needs to have no setup/hold effect during SCLK
set_false_path -from [get_ports {BBB_CS_N[0] BBB_CS_N[1]}] -to [get_clocks BBB_SCLK]

# ignore ha_out3/d1 hold violations (on posedge of ha_clk) since d1 is always unchanged
set_false_path -rise_from [get_clocks BBB_SCLK] -to [get_ports BBB_MISO]

# define async clock domains
set_clock_groups -asynchronous -group [get_clocks GPS_TCXO] -group [get_clocks ADC_CLKIN]
set_clock_groups -asynchronous -group [get_clocks ADC_CLKIN] -group [get_clocks GPS_TCXO]
set_clock_groups -asynchronous -group [get_clocks GPS_TCXO] -group [get_clocks BBB_SCLK]
set_clock_groups -asynchronous -group [get_clocks BBB_SCLK] -group [get_clocks GPS_TCXO]

# config
#set_property CONFIG_VOLTAGE 3.3 [current_design]
#set_property CFGBVS VCCO [current_design]
#set_property CONFIG_MODE S_SERIAL [current_design]
#set_property BITSTREAM.CONFIG.USR_ACCESS 0x12345678 [current_design]

# power analyzer
set_operating_conditions -airflow 0
set_operating_conditions -board_layers 4to7
set_operating_conditions -board small
set_operating_conditions -heatsink none
set_switching_activity -default_toggle_rate 100.000
set_switching_activity -toggle_rate 100.000 -static_probability 0.500 [get_ports BBB_MISO]
