//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////


// Copyright (c) 2014 John Seamons, ZL/KF6VO

`default_nettype none

`include "kiwi.vh"

module KiwiSDR (

    input  wire	signed [ADC_BITS-1:0] ADC_DATA,
    input  wire	ADC_OVFL,
    input  wire	ADC_CLKIN,
    output wire	ADC_CLKEN,

    input  wire IF_SGN,
    input  wire IF_MAG,
    input  wire GPS_TCXO,

    input  wire	BBB_SCLK,
    input  wire [1:0] BBB_CS_N,
    input  wire BBB_MOSI,
    output wire BBB_MISO,

    output wire P911,
    output wire P913,
    input  wire P915,
    output wire CMD_READY,  // ctrl[CTRL_CMD_READY]
    output wire SND_INTR,   // ctrl[CTRL_SND_INTR]
    output wire P926,		// inside pin row
    
    output wire P826,		// outside pin row
    output wire P819,
    output wire P817,
    output wire P818,		// outside pin row
    output wire P815,
    output wire P816,		// outside pin row
    output wire P813,
    output wire P814,		// outside pin row
    output wire P811,
    output wire P812,		// outside pin row

    output wire EWP
    );
    
    
    //////////////////////////////////////////////////////////////////////////
    // debug
    
    // P9: 25 23 21 19 17 15 13 11 09 07 05 03 01
    //                    b2 b1 b0
    //
    // P9: 26 24 22 20 18 16 14 12 10 08 06 04 02
    //     b3
    
    wire [3:0] P9;
    
    assign P926 = P9[3];    // P9-26
    assign P915 = P9[2];    // P9-15
    assign P913 = P9[1];    // P9-13
    assign P911 = P9[0];    // P9-11

    // P8: 25 23 21 19 17 15 13 11 09 07 05 03 01
    //              b8 b7 b6 b5 b4
    //
    // P8: 26 24 22 20 18 16 14 12 10 08 06 04 02
    //     b9          b3 b2 b1 b0
    
    wire [9:0] P8;
    
    assign P826 = P8[9];
    assign P819 = P8[8];
    assign P817 = P8[7];
    assign P815 = P8[6];
    assign P813 = P8[5];
    assign P811 = P8[4];
    assign P818 = P8[3];
    assign P816 = P8[2];
    assign P814 = P8[1];
    assign P812 = P8[0];

    
    //////////////////////////////////////////////////////////////////////////
    // clocks

    wire adc_clk, gps_clk, cpu_clk;
    
    IBUFG vcxo_ibufg(.I(ADC_CLKIN), .O(adc_clk));
	assign ADC_CLKEN = ctrl[CTRL_OSC_EN];

    IBUFG tcxo_ibufg(.I(GPS_TCXO), .O(gps_clk));		// 16.368 MHz TCXO
    assign cpu_clk = gps_clk;

	reg signed [ADC_BITS-1:0] reg_adc_data;
    always @ (posedge adc_clk)
    begin
    	reg_adc_data <= ADC_DATA;
    end
    
    wire  [2:1] rst;

	wire [15:0] op;
    wire [31:0] nos, tos;
    reg  [15:0] par;
    wire [1:0]  ser;
    wire        rdBit, rdBit2, rdReg, rdReg2, wrReg, wrReg2, wrEvt, wrEvt2;

    wire        boot_done, host_srq, mem_rd, gps_rd;
    wire [15:0] host_dout, mem_dout, gps_dout;
    
    
    //////////////////////////////////////////////////////////////////////////
    // global control & status registers

    reg [15:0] ctrl;
    
    always @ (posedge cpu_clk)
    begin
        if (wrReg & op[SET_CTRL]) ctrl <= tos[15:0];
    end

	assign EWP = ctrl[CTRL_EEPROM_WP];
	assign CMD_READY = ctrl[CTRL_CMD_READY];
	assign SND_INTR = ctrl[CTRL_SND_INTR];
	
	// keep Vivado from complaining about unused inputs and outputs
	assign P9[0] = ctrl[CTRL_UNUSED_OUT];
	assign P9[1] = ctrl[CTRL_UNUSED_OUT];
	assign P9[3] = ctrl[CTRL_UNUSED_OUT];

	assign P8[0] = ctrl[CTRL_UNUSED_OUT];
	assign P8[1] = ctrl[CTRL_UNUSED_OUT];
	assign P8[2] = ctrl[CTRL_UNUSED_OUT];
	assign P8[3] = ctrl[CTRL_UNUSED_OUT];
	assign P8[4] = ctrl[CTRL_UNUSED_OUT];
	assign P8[5] = ctrl[CTRL_UNUSED_OUT];
	assign P8[6] = ctrl[CTRL_UNUSED_OUT];
	assign P8[7] = ctrl[CTRL_UNUSED_OUT];
	assign P8[8] = ctrl[CTRL_UNUSED_OUT];
	assign P8[9] = ctrl[CTRL_UNUSED_OUT];
    
	wire unused_inputs = IF_MAG | P9[2];

    wire [15:0] status;
    wire [3:0] stat_user = { 3'b0, dna_data };
    // when the firmware returns status it replaces stat_replaced with FW_ID
    wire [2:0] stat_replaced = { 2'b0, unused_inputs };
    wire [3:0] fpga_id = { FPGA_ID };
    assign status[15:0] = { rx_overflow_C, stat_replaced, FPGA_VER, stat_user, fpga_id };


    //////////////////////////////////////////////////////////////////////////
    // device DNA
    
    wire dna_data;
    DNA_PORT dna(.CLK(ctrl[CTRL_DNA_CLK]), .READ(ctrl[CTRL_DNA_READ]), .SHIFT(ctrl[CTRL_DNA_SHIFT]), .DIN(1'b1), .DOUT(dna_data));


    //////////////////////////////////////////////////////////////////////////
    // receiver

	wire rx_rd, wf_rd;
	wire [15:0] rx_dout, wf_dout;
	
	wire [47:0] ticks_A;
	
    RECEIVER receiver (
    	.adc_clk	(adc_clk),
    	.adc_data	(reg_adc_data),

		// these are all on the cpu_clk
        .rx_rd_C	(rx_rd),
        .rx_dout_C	(rx_dout),

        .wf_rd_C	(wf_rd),
        .wf_dout_C	(wf_dout),

        .ticks_A	(ticks_A),
        
		.cpu_clk	(cpu_clk),
        .ser		(ser[1]),        
        .tos		(tos),
        .op			(op),        
        .rdReg      (rdReg),
        .rdBit2     (rdBit2),
        .wrReg2     (wrReg2),
        .wrEvt2     (wrEvt2),
        
        .ctrl       (ctrl)
    	);

	wire rx_ovfl_C, rx_orst;
	reg rx_overflow_C;
    always @ (posedge cpu_clk)
    begin
    	if (rx_orst) rx_overflow_C <= rx_ovfl_C; else
    	rx_overflow_C <= rx_overflow_C | rx_ovfl_C;
    end

//`define ADC_OVFL_ON_ONE_SAMPLE
`ifdef ADC_OVFL_ON_ONE_SAMPLE
	SYNC_PULSE sync_adc_ovfl (.in_clk(adc_clk), .in(ADC_OVFL), .out_clk(cpu_clk), .out(rx_ovfl_C));
`else
    // signal overflow only if a variable value of 64k consecutive samples have ADC overflow asserted
    localparam ADC_OVFL_CTR_BITS = 16;
	reg [ADC_OVFL_CTR_BITS-1:0] adc_ovfl_ctr, adc_ovfl_cnt, cnt_mask;
    reg adc_ovfl;

    always @ (posedge cpu_clk)
        if (wrReg2 & op[SET_CNT_MASK]) cnt_mask <= tos[ADC_OVFL_CTR_BITS-1:0];

    always @ (posedge adc_clk)
    begin
        if (adc_ovfl_ctr == ((1 << ADC_OVFL_CTR_BITS) - 1))
        begin
            adc_ovfl <= ((adc_ovfl_cnt & cnt_mask) != 0)? 1:0;
            adc_ovfl_cnt <= 0;
            adc_ovfl_ctr <= 0;
        end else
        begin
            adc_ovfl = 0;
            adc_ovfl_cnt <= adc_ovfl_cnt + ADC_OVFL;
            adc_ovfl_ctr <= adc_ovfl_ctr + 1;
        end
    end

	SYNC_PULSE sync_adc_ovfl (.in_clk(adc_clk), .in(adc_ovfl), .out_clk(cpu_clk), .out(rx_ovfl_C));
`endif


    //////////////////////////////////////////////////////////////////////////
    // CPU parallel port input mux
	
wire [31:0] wcnt;

    always @*
`ifdef USE_CPU_CTR
		if (rdReg & op[GET_CPU_CTR0]) par = { cpu_ctr[1][ 7 -:8], cpu_ctr[0][ 7 -:8] }; else
		if (rdReg & op[GET_CPU_CTR1]) par = { cpu_ctr[1][15 -:8], cpu_ctr[0][15 -:8] }; else
		if (rdReg & op[GET_CPU_CTR2]) par = { cpu_ctr[1][23 -:8], cpu_ctr[0][23 -:8] }; else
		if (rdReg & op[GET_CPU_CTR3]) par = { cpu_ctr[1][31 -:8], cpu_ctr[0][31 -:8] }; else
`endif

		if (rdReg & op[GET_STATUS]) par = status; else
		par = host_dout;
	

    //////////////////////////////////////////////////////////////////////////
    // host SPI interface

    HOST host (
        .hb_clk		(gps_clk),
        .rst		(rst),
        .spi_sclk   (BBB_SCLK),
        .spi_cs		(~BBB_CS_N),
        .spi_mosi   (BBB_MOSI),
        .spi_miso   (BBB_MISO),
        .host_srq   (host_srq),
        
        .gps_rd 	(gps_rd),
        .gps_dout	(gps_dout),

        .rx_rd		(rx_rd),
        .rx_dout	(rx_dout),

        .wf_rd		(wf_rd),
        .wf_dout	(wf_dout),

        .hb_ovfl	(rx_overflow_C),
        .hb_orst	(rx_orst),          // NB: hb_clk = gps_clk = cpu_clk

        .host_dout  (host_dout),
        .mem_rd     (mem_rd),
        .mem_dout   (mem_dout),
        .boot_done  (boot_done),

        .tos        (tos),
        .op         (op),
        .rdReg      (rdReg),
        .wrReg      (wrReg),
        .wrEvt      (wrEvt)
        );


    //////////////////////////////////////////////////////////////////////////
    // CPU cycle counters

`ifdef USE_CPU_CTR
    reg cpu_ctr_ena;
    wire [31:0] cpu_ctr[1:0];
    wire sclr = wrEvt2 & op[CPU_CTR_CLR];
    
	ip_acc_u32b cpu_ctr0 (.clk(cpu_clk), .sclr(sclr), .b(1), .q(cpu_ctr[0]));
	ip_acc_u32b cpu_ctr1 (.clk(cpu_clk), .sclr(sclr), .b({{31{1'b0}}, cpu_ctr_ena}), .q(cpu_ctr[1]));

	always @ (posedge cpu_clk)
	begin
		if (wrEvt2 & op[CPU_CTR_ENA]) cpu_ctr_ena <= 1;
		if (wrEvt2 & op[CPU_CTR_DIS]) cpu_ctr_ena <= 0;
	end
`endif

    CPU cpu (
        .clk        (cpu_clk),
        .rst        (rst),
        
        .par        (par),
        .ser        (ser),
        .mem_rd     (mem_rd),
        .mem_dout   (mem_dout),
        .boot_done  (boot_done),
        
        .tos        (tos),
        .op         (op),
        .rdBit      (rdBit),
        .rdBit2     (rdBit2),
        .rdReg      (rdReg),
        .rdReg2     (rdReg2),
        .wrReg      (wrReg),
        .wrReg2     (wrReg2),
        .wrEvt      (wrEvt),
        .wrEvt2     (wrEvt2)
        );


`ifdef DEF_GPS_CHANS
    GPS gps (
        .clk        (gps_clk),
        .adc_clk	(adc_clk),
        .host_srq   (host_srq),

        .I_data		(IF_SGN),
        .gps_rd 	(gps_rd),
        .gps_dout	(gps_dout),
        
        .ticks_A	(ticks_A),
        
        .ser		(ser[0]),        
        .tos		(tos),			// fixme: cpu_clk?
        .op			(op),        
        .rdBit      (rdBit),
        .rdReg      (rdReg),
        .wrReg      (wrReg),
        .wrEvt      (wrEvt)
        );
`else

	// if no GPS configured, still need to capture host_srq in the same way as GPS.
	assign ser[0] = srq_out;
	reg srq_noted, srq_out;
	
    always @ (posedge cpu_clk)
    begin
        if (rdReg & op[GET_SRQ]) srq_noted <= host_srq;
        else				      srq_noted <= host_srq | srq_noted;
        if (rdReg & op[GET_SRQ]) srq_out <= srq_noted;
    end
`endif

endmodule
