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

module rx (
	input  wire		   adc_clk,
	input  wire signed [IN_WIDTH-1:0] adc_data,
	input  wire		   rd_getI,
	input  wire		   rd_getQ,
	output wire		   rx_avail_A,
	output wire [15:0] rx_dout_A,

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
		rx_dout = rd_getI? rx_cic_out_i[15:0] : ( rd_getQ? rx_cic_out_q[15:0] : {rx_cic_out_i[RXO_BITS-1 -:8], rx_cic_out_q[RXO_BITS-1 -:8]} );

	assign rx_dout_A = rx_dout;

endmodule
