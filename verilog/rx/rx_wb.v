/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2008 Alex Shovkoplyas, VE3NEA
// Copyright (c) 2013 Phil Harman, VK6APH
// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO

module rx_wb (
	input  wire		   adc_clk,
	input  wire signed [IN_WIDTH-1:0] adc_data,
	input  wire		   rd_getI,
	input  wire		   rd_getQ,
	input  wire		   rd_getWB,
	output wire		   rx_avail_A,
	output reg		   rx_avail_wb_A,
	output wire [15:0] rx_dout_A,

    // debug
	input  wire [ 2:0] didx_i,
	input  wire [15:0] waddr_i,
	input  wire [15:0] count_i,

	input  wire		   cpu_clk,
    input  wire [31:0] freeze_tos_A,
	input  wire		   rx_sel_C,
    input  wire        set_rx_freqH_C,
    input  wire        set_rx_freqL_C
	);
	
`include "kiwi.gen.vh"

	parameter IN_WIDTH  = "required";

	reg signed [47:0] rx_phase_inc;
	wire set_phaseH, set_phaseL;

	SYNC_PULSE set_phaseH_inst (.in_clk(cpu_clk), .in(rx_sel_C && set_rx_freqH_C), .out_clk(adc_clk), .out(set_phaseH));
	SYNC_PULSE set_phaseL_inst (.in_clk(cpu_clk), .in(rx_sel_C && set_rx_freqL_C), .out_clk(adc_clk), .out(set_phaseL));

    always @ (posedge adc_clk)
    begin
        if (set_phaseH) rx_phase_inc[16 +:32] <= freeze_tos_A;
        if (set_phaseL) rx_phase_inc[ 0 +:16] <= freeze_tos_A;
    end

	wire signed [RX1_BITS-1:0] rx_mix_i, rx_mix_q;

	IQ_MIXER #(.IN_WIDTH(IN_WIDTH), .OUT_WIDTH(RX1_BITS))
		rx_mixer (
			.clk		(adc_clk),
			.phase_inc	(rx_phase_inc),
			.in_data	(adc_data),
			.out_i		(rx_mix_i),
			.out_q		(rx_mix_q)
		);
	
	wire rx_cic1_avail;
	wire signed [RX2_BITS-1:0] rx_cic1_out_i, rx_cic1_out_q;

	localparam RX1_GROWTH = RX1_STAGES * clog2(RX1_DECIM);

cic_prune_var #(.INC_FILE("rx1"), .STAGES(RX1_STAGES), .DECIM_TYPE(RX1_DECIM), .GROWTH(RX1_GROWTH), .IN_WIDTH(RX1_BITS), .OUT_WIDTH(RX2_BITS))
	rx_cic1_i(
		.clock			(adc_clk),
		.reset			(1'b0),
		.decimation		(18'b0),
		.in_strobe		(1'b1),
		.out_strobe		(rx_cic1_avail),
		.in_data		(rx_mix_i),
		.out_data		(rx_cic1_out_i)
    );

cic_prune_var #(.INC_FILE("rx1"), .STAGES(RX1_STAGES), .DECIM_TYPE(RX1_DECIM), .GROWTH(RX1_GROWTH), .IN_WIDTH(RX1_BITS), .OUT_WIDTH(RX2_BITS))
	rx_cic1_q(
		.clock			(adc_clk),
		.reset			(1'b0),
		.decimation		(18'b0),
		.in_strobe		(1'b1),
		.out_strobe		(),
		.in_data		(rx_mix_q),
		.out_data		(rx_cic1_out_q)
    );
    
    always @ (posedge adc_clk)
        rx_avail_wb_A <= rx_cic1_avail;     // delay 1 ADC clk to align with rx_cic2_avail/rx_avail_A

    wire rx_cic2_avail;

	localparam RX2_GROWTH = RX2_STAGES * clog2(RX2_DECIM);

	wire signed [RXO_BITS-1:0] rx_cic2_out_i, rx_cic2_out_q;

cic_prune_var #(.INC_FILE("rx2"), .STAGES(RX2_STAGES), .DECIM_TYPE(RX2_DECIM), .GROWTH(RX2_GROWTH), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
	rx_cic2_i(
		.clock			(adc_clk),
		.reset			(1'b0),
		.decimation		(18'b0),
		.in_strobe		(rx_cic1_avail),
		.out_strobe		(rx_cic2_avail),
		.in_data		(rx_cic1_out_i),
		.out_data		(rx_cic2_out_i)
    );

cic_prune_var #(.INC_FILE("rx2"), .STAGES(RX2_STAGES), .DECIM_TYPE(RX2_DECIM), .GROWTH(RX2_GROWTH), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
	rx_cic2_q(
		.clock			(adc_clk),
		.reset			(1'b0),
		.decimation		(18'b0),
		.in_strobe		(rx_cic1_avail),
		.out_strobe		(),
		.in_data		(rx_cic1_out_q),
		.out_data		(rx_cic2_out_q)
    );

	wire signed [RXO_BITS-1:0] rx_cic_out_i, rx_cic_out_q;

`ifdef USE_RX_CICF

    wire rx_cicf_avail;
	wire signed [RXO_BITS-1:0] rx_cicf_out_i, rx_cicf_out_q;

fir_iq #(.WIDTH(RXO_BITS))
    cicf(
		.adc_clk        (adc_clk),
		.reset			(1'b0),
		.in_strobe		(rx_cic2_avail),
		.out_strobe		(rx_cicf_avail),
		.in_data_i		(rx_cic2_out_i),
		.in_data_q		(rx_cic2_out_q),
		.out_data_i		(rx_cicf_out_i),
		.out_data_q		(rx_cicf_out_q)
    );

    assign rx_avail_A   = rx_cicf_avail;
    assign rx_cic_out_i = rx_cicf_out_i;
    assign rx_cic_out_q = rx_cicf_out_q;
`else
    assign rx_avail_A   = rx_cic2_avail;
    assign rx_cic_out_i = rx_cic2_out_i;
    assign rx_cic_out_q = rx_cic2_out_q;
`endif

	reg [15:0] rx_dout;
	
	always @*
	    if (rd_getWB)
	    begin
//`define WB_PATTERN
`ifdef WB_PATTERN
            case (didx_i)
                0: rx_dout = rd_getI? count_i : (rd_getQ? 16'h0def : 16'h0a0b);
                1: rx_dout = rd_getI? count_i : (rd_getQ? 16'h1def : 16'h1a1b);
                2: rx_dout = rd_getI? count_i : (rd_getQ?  waddr_i : 16'h2a2b);
                3: rx_dout = rd_getI? count_i : (rd_getQ? 16'h3def : 16'h3a3b);
                4: rx_dout = rd_getI? count_i : (rd_getQ? 16'h4def : 16'h4a4b);
                5: rx_dout = rd_getI? count_i : (rd_getQ? 16'h5def : 16'h5a5b);
                6: rx_dout = rd_getI? count_i : (rd_getQ? 16'h6def : 16'h6a6b);
                7: rx_dout = rd_getI? count_i : (rd_getQ? 16'h7def : 16'h7a7b);
            endcase
`else
            // sign extend
            // for RX2_BITS = 18
            // 2222 1111 111111
            // 3210 9876 54321098 76543210
            // eeee eeSd dddddddd dddddddd
            //        12 345678
            //                 12 34567890
            
		    rx_dout = rd_getI? rx_cic1_out_i[15:0] : ( rd_getQ? rx_cic1_out_q[15:0] :
		        { {{6{rx_cic1_out_i[17]}}, rx_cic1_out_i[17:16]}, {{6{rx_cic1_out_q[17]}}, rx_cic1_out_q[17:16]} } );
`endif
	    end else
	    begin
		    rx_dout = rd_getI? rx_cic_out_i[15:0] : ( rd_getQ? rx_cic_out_q[15:0] : {rx_cic_out_i[RXO_BITS-1 -:8], rx_cic_out_q[RXO_BITS-1 -:8]} );
        end
        
	assign rx_dout_A = rx_dout;

endmodule
