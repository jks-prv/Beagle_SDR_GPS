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

// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO

`default_nettype none

// for compatibility with antenna switch extension
// i.e. let Beagle drive these, not FPGA
`define P8_ARE_INPUTS

module KiwiSDR (

    input  wire	signed [ADC_BITS-1:0] ADC_DATA,
    input  wire	ADC_OVFL,
    input  wire	ADC_CLKIN,
    output wire	ADC_CLKEN,
    output wire	ADC_STENL,
    output wire	ADC_STSIG,

    output wire	DA_DALE,
    output wire	DA_DACLK,
    output wire	DA_DADAT,

    input  wire GPS_TCXO,
    input  wire GPS_ISGN,
    input  wire GPS_IMAG,
    input  wire GPS_QSGN,
    input  wire GPS_QMAG,
    output wire GPS_GSCS,
    output wire GPS_GSCLK,
    output wire GPS_GSDAT,

    input  wire	BBB_SCLK,       // P922
    input  wire [1:0] BBB_CS_N, // 1=P916 0=P917
    input  wire BBB_MOSI,       // P918
    output wire BBB_MISO,       // P921

    output wire P911,       // P911, GPIO 0_30, unused debug out
    output wire P913,       // P913, GPIO 0_31, unused debug out

    input  wire P915,       // P915, GPIO 1_0-2_0, unused debug in
    output wire CMD_READY,  // P923, GPIO 1_17, ctrl[CTRL_CMD_READY]
    output wire SND_INTR,   // P924, GPIO 0_15, ctrl[CTRL_SND_INTR]
    output wire P926,		// P926, GPIO 0_14, unused debug out

`ifdef P8_ARE_INPUTS
    input  wire P826,		// outside pin row
    input  wire P819,
    input  wire P817,
    input  wire P818,		// outside pin row
    input  wire P815,
    input  wire P816,		// outside pin row
    input  wire P813,
    input  wire P814,		// outside pin row
    input  wire P811,
    input  wire P812,		// outside pin row
`else
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
`endif

    output wire EWP
    );
    
`include "kiwi.gen.vh"
`include "other.gen.vh"

    //////////////////////////////////////////////////////////////////////////
    // debug
    //////////////////////////////////////////////////////////////////////////
    
    // P9: 25 23 21 19 17 15 13 11 09 07 05 03 01   pcb top, outside row
    //                    b2 b1 b0
    //
    // P9: 26 24 22 20 18 16 14 12 10 08 06 04 02   pcb top, inside row
    //     b3
    
    wire [2:0] P9;
    
`ifdef USE_WB
    wire awb_debug, rx_avail_wb_A, rx_avail_A;
    assign P911 = awb_debug;
    assign P913 = rx_avail_wb_A;
    assign P926 = rx_avail_A;
`else
    assign P911 = P9[0];    // P911
    assign P913 = P9[1];    // P913
    assign P926 = P9[2];    // P926
`endif

    // P8: 25 23 21 19 17 15 13 11 09 07 05 03 01   pcb top, inside row
    //              b8 b7 b6 b5 b4
    //
    // P8: 26 24 22 20 18 16 14 12 10 08 06 04 02   pcb top, outside row
    //     b9          b3 b2 b1 b0
    
`ifdef P8_ARE_INPUTS
`else
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
`endif

    
    //////////////////////////////////////////////////////////////////////////
    // clocks
    //////////////////////////////////////////////////////////////////////////

`ifdef USE_SDR
    wire adc_clk;
    IBUF xo_ibuf(.I(ADC_CLKIN), .O(adc_clk));

    wire gps_tcxo_buf;
    wire cpu_clk = gps_tcxo_buf;
    wire gps_clk = gps_tcxo_buf;
    
    // KiwiSDR 2
    //
    // GPS_TCXO from MAX2769B (GCLK) does not start reliably at power-up.
    // Workaround by using ADC_CLKIN/4 = 16.66665 MHz initially and then
    // switch to GPS_TCXO after MAX2769B has been programmed via serial interface.

    wire adc_div_4;
    
    BUFR #(.BUFR_DIVIDE("4")) BUFR_inst (
        .CLR    (1'b0),
        .I      (adc_clk),
        .CE     (1'b1),
        .O      (adc_div_4)
    );
    
    BUFGMUX_CTRL BUFGMUX_CTRL_inst (
        .I0     (adc_div_4),
        .I1     (GPS_TCXO),
        .S      (ctrl[CTRL_GPS_CLK_EN]),
        .O      (gps_tcxo_buf)
    );

    //IBUF tcxo_ibuf(.I(GPS_TCXO), .O(gps_tcxo_buf));     // 16.368 MHz TCXO
`endif

	assign ADC_CLKEN = !ctrl[CTRL_OSC_DIS];

`ifdef USE_SDR
	reg signed [ADC_BITS-1:0] reg_adc_data, reg2_adc_data;
    always @ (posedge adc_clk)
    begin
    	reg2_adc_data <= ADC_DATA;
    	reg_adc_data  <= reg2_adc_data;
    end
`endif
    
    wire  [2:1] rst;

	wire [15:0] op;
    wire [31:0] nos, tos;
    reg  [15:0] par;
    wire [2:0]  ser;
    wire        rdBit0, rdBit1, rdBit2, rdReg, rdReg2, wrReg, wrReg2, wrEvt, wrEvt2;

    wire        boot_done, host_srq, mem_rd, gps_rd, ext_rd;
    wire [15:0] host_dout, mem_dout, gps_dout, ext_dout;
    
    
    //////////////////////////////////////////////////////////////////////////
    // global control & status registers
    //////////////////////////////////////////////////////////////////////////

    reg [13:0] ctrl;
    
    always @ (posedge cpu_clk)
    begin
        if (wrReg & op[SET_CTRL]) ctrl <= tos[15:0];
    end

	assign ADC_STENL = !ctrl[CTRL_STEN];
    wire [1:0] ser_sel = ctrl[1:0];

    wire ser_attn = (ser_sel == CTRL_SER_ATTN);
	assign DA_DALE = ser_attn && ctrl[CTRL_SER_LE_CSN];
	assign DA_DACLK = ser_attn && ctrl[CTRL_SER_CLK];
	assign DA_DADAT = ser_attn && ctrl[CTRL_SER_DATA];

    wire ser_gps = (ser_sel == CTRL_SER_GPS);
	assign GPS_GSCS = ser_gps && ctrl[CTRL_SER_LE_CSN];
	assign GPS_GSCLK = ser_gps && ctrl[CTRL_SER_CLK];
	assign GPS_GSDAT = ser_gps && ctrl[CTRL_SER_DATA];
	
    wire ser_dna = (ser_sel == CTRL_SER_DNA);
	wire dna_read = ser_dna && ctrl[CTRL_SER_LE_CSN];
	wire dna_clk = ser_dna && ctrl[CTRL_SER_CLK];
	wire dna_shift = ser_dna && ctrl[CTRL_SER_DATA];

	assign EWP = ctrl[CTRL_EEPROM_WP];
	assign CMD_READY = ctrl[CTRL_CMD_READY];

`ifdef USE_OTHER
    wire dev_intr;
`else
    wire dev_intr = 1'b0;
`endif
	assign SND_INTR = ctrl[CTRL_SND_INTR] | dev_intr;
    
	// keep Vivado from complaining about unused inputs and outputs
	assign P9[0] = ctrl[CTRL_UNUSED_OUT];
	assign P9[1] = ctrl[CTRL_UNUSED_OUT];
	assign P9[2] = ctrl[CTRL_UNUSED_OUT];

`ifdef P8_ARE_INPUTS
`else
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
`endif
    
	wire unused_inputs = P915
`ifdef USE_OTHER
        | unused_inputs_other
`else
`ifdef P8_ARE_INPUTS
        | P811 | P812 | P813 | P814 | P815 | P816 | P817 | P818 | P819 | P826
`endif
`ifdef USE_GPS
        | unused_inputs_gps
`else
        | GPS_ISGN | GPS_IMAG | GPS_QSGN | GPS_QMAG
`endif
`endif
        ;

`ifdef USE_OTHER
    wire [2:0] other_flags;
`else
    wire [2:0] other_flags = 3'b0;
`endif
    wire [3:0] stat_user = { other_flags, dna_data };

    // when the eCPU firmware returns status it replaces stat_replaced with FW_ID
    wire [2:0] stat_replaced = { 2'b0, unused_inputs };
    wire [3:0] fpga_id = { FPGA_ID };
    wire [15:0] status = { rx_overflow_C, stat_replaced, FPGA_VER, stat_user, fpga_id };


    //////////////////////////////////////////////////////////////////////////
    // device DNA
    //////////////////////////////////////////////////////////////////////////
    
    // level to single pulse
    localparam RISE=2'b01;
    reg [1:0] _dna_rd, _dna_sf;
    wire dna_rd = (_dna_rd == RISE);
    wire dna_sf = (_dna_sf == RISE);
    always @ (posedge cpu_clk)
        begin
            _dna_rd <= {_dna_rd[0], dna_read && dna_clk};
            _dna_sf <= {_dna_sf[0], dna_shift && dna_clk};
        end

    wire dna_data;
    DNA_PORT dna(.CLK(cpu_clk), .READ(dna_rd), .SHIFT(dna_sf), .DIN(1'b1), .DOUT(dna_data));


    //////////////////////////////////////////////////////////////////////////
    // receiver
    //////////////////////////////////////////////////////////////////////////

	wire rx_rd, wf_rd;
	wire [15:0] rx_dout, wf_dout;
	wire [47:0] ticks_A;
	
`ifdef USE_SDR
	wire use_gen_C = ctrl[CTRL_USE_GEN];
	
	wire self_test;
	assign ADC_STSIG = self_test;

`ifdef USE_WB
    receiver_wb receiver_inst (
`else
    receiver receiver_inst (
`endif
    	.adc_clk	    (adc_clk),
    	.adc_data	    (reg_adc_data),
    	.adc_ovfl       (ADC_OVFL),

		// these are all on the cpu_clk
        .rx_rd_C	    (rx_rd),
        .rx_dout_C	    (rx_dout),

        .wf_rd_C	    (wf_rd),
        .wf_dout_C	    (wf_dout),

        .ticks_A	    (ticks_A),
        .adc_ovfl_C     (rx_ovfl_C),
        .adc_count_C    (adc_count),
        
		.cpu_clk	    (cpu_clk),
        .ser		    (ser[1]),        
        .tos		    (tos),
        .op_11          (op[10:0]),        
        .rdReg          (rdReg),
        .wrReg          (wrReg),
        .wrReg2         (wrReg2),
        .wrEvt2         (wrEvt2),
        
        .use_gen_C      (use_gen_C),
        
`ifdef USE_WB
        // debug
        .rx_avail_wb_A  (rx_avail_wb_A),
        .rx_avail_A     (rx_avail_A),
        .awb_debug      (awb_debug),
`endif
        
        .self_test_en_C (ctrl[CTRL_STEN]),
        .self_test      (self_test)
    	);

	wire rx_ovfl_C, rx_orst;
	reg rx_overflow_C;
    always @ (posedge cpu_clk)
    begin
    	if (rx_orst) rx_overflow_C <= rx_ovfl_C; else
    	rx_overflow_C <= rx_overflow_C | rx_ovfl_C;
    end
    
    wire [31:0] adc_count;

`else
    assign rx_rd = 0;
    assign rx_dout = 0;
    assign wf_rd = 0;
    assign wf_dout = 0;
	wire rx_orst;
	wire rx_overflow_C = 0;

`ifdef USE_OTHER
`else
	assign ser[1] = 1'b0;
	assign ser[2] = 1'b0;
`endif
`endif


    //////////////////////////////////////////////////////////////////////////
    // CPU parallel port input mux
    //////////////////////////////////////////////////////////////////////////
	
    always @*
    begin
`ifdef USE_CPU_CTR
		if (rdReg & op[GET_CPU_CTR0]) par = { cpu_ctr[1][ 7 -:8], cpu_ctr[0][ 7 -:8] }; else
		if (rdReg & op[GET_CPU_CTR1]) par = { cpu_ctr[1][15 -:8], cpu_ctr[0][15 -:8] }; else
		if (rdReg & op[GET_CPU_CTR2]) par = { cpu_ctr[1][23 -:8], cpu_ctr[0][23 -:8] }; else
		if (rdReg & op[GET_CPU_CTR3]) par = { cpu_ctr[1][31 -:8], cpu_ctr[0][31 -:8] }; else
`endif

`ifdef USE_SDR
		if (rdReg2 & op[GET_ADC_CTR0]) par = { adc_count[15: 0] }; else
		if (rdReg2 & op[GET_ADC_CTR1]) par = { adc_count[31:16] }; else
`endif

		if (rdReg & op[GET_STATUS]) par = status; else
		par = host_dout;
	end
	

    //////////////////////////////////////////////////////////////////////////
    // host SPI interface
    //////////////////////////////////////////////////////////////////////////

    HOST host (
        .hb_clk		(cpu_clk),
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

        .ext_rd 	(ext_rd),
        .ext_dout	(ext_dout),

        // NB: hb_clk = gps_clk = cpu_clk
        .hb_ovfl	(rx_overflow_C),
        .hb_orst	(rx_orst),

        .host_dout  (host_dout),
        .mem_rd     (mem_rd),
        .mem_dout   (mem_dout),
        .boot_done  (boot_done),

        .tos        (tos[15:0]),
        .op_4   (op[3:0]),
        .rdReg      (rdReg),
        .wrReg      (wrReg),
        .wrEvt      (wrEvt)
        );


    //////////////////////////////////////////////////////////////////////////
    // CPU cycle counters
    //////////////////////////////////////////////////////////////////////////

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
        .rdBit0     (rdBit0),
        .rdBit1     (rdBit1),
        .rdBit2     (rdBit2),
        .rdReg      (rdReg),
        .rdReg2     (rdReg2),
        .wrReg      (wrReg),
        .wrReg2     (wrReg2),
        .wrEvt      (wrEvt),
        .wrEvt2     (wrEvt2)
        );


`ifdef USE_GPS
    wire unused_inputs_gps;

    GPS gps (
        .clk            (gps_clk),
        .adc_clk	    (adc_clk),
        .host_srq       (host_srq),

        .I_sign		    (GPS_ISGN),
        .I_mag		    (GPS_IMAG),
        .Q_sign		    (GPS_QSGN),
        .Q_mag		    (GPS_QMAG),
        .gps_rd 	    (gps_rd),
        .gps_dout	    (gps_dout),
        
        .ticks_A	    (ticks_A),
        
        .ser		    (ser[0]),        
        .tos		    (tos),      // no clk domain crossing because gps_clk = cpu_clk
        .op_8           (op[7:0]),        
        .rdBit          (rdBit0),
        .rdReg          (rdReg),
        .wrReg          (wrReg),
        .wrEvt          (wrEvt),
        
        // o
        .unused_inputs  (unused_inputs_gps)
        );
`else

	// if no GPS configured, still need to capture host_srq in the same way as GPS does.
	assign ser[0] = srq_out;
	reg srq_noted, srq_out;
	
    always @ (posedge cpu_clk)
    begin
        if (rdReg & op[GET_SRQ]) srq_noted <= host_srq;
        else				     srq_noted <= host_srq | srq_noted;
        if (rdReg & op[GET_SRQ]) srq_out   <= srq_noted;
    end
    
    assign gps_rd = 0;
    assign gps_dout = 0;
`endif

`ifdef USE_OTHER
    wire cpu_clk, unused_inputs_other;
    
    other other (
        .ADC_CLKIN      (ADC_CLKIN),
        .GPS_TCXO       (GPS_TCXO),
        // o
        .cpu_clk        (cpu_clk),
        
        .ADC_DATA       (ADC_DATA),
        .ADC_OVFL       (ADC_OVFL),
        
        .I_sign         (GPS_ISGN),
        .I_mag          (GPS_IMAG),
        .Q_sign         (GPS_QSGN),
        .Q_mag          (GPS_QMAG),

        .tos		    (tos[31:0]),
        .op             (op),        
        .wrReg          (wrReg),
        .wrEvt          (wrEvt),
        .rdBit_diag     (rdBit1),
        // o
        .diag_sout      (ser[1]),
        .host_sout      (ser[2]),
        .dev_intr       (dev_intr),
        .rd             (ext_rd),
        .dout           (ext_dout),

        .ctrl           (ctrl),
        // o
        .flags          (other_flags),
        
        // o
        .unused_inputs  (unused_inputs_other)
        );
`else
	assign ser[2] = 1'b0;
    assign ext_rd = 0;
    assign ext_dout = 0;
`endif

endmodule
