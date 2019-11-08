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
// Copyright (c) 2014 John Seamons, ZL/KF6VO

`include "kiwi.vh"

module RX (
	input  wire		   adc_clk,
	input  wire signed [IN_WIDTH-1:0] adc_data,

	input  wire		   rx_sel_C,

	input  wire		   rd_i,
	input  wire		   rd_q,
	output wire [OUT_WIDTH-1:0] rx_dout_i_A, rx_dout_q_A,
	output wire		   rx_avail_A,

	input  wire		   cpu_clk,
    input  wire [31:0] freeze_tos,
    
    input  wire        set_rx_freqH_C,
    input  wire        set_rx_freqL_C
	);
	
	parameter IN_WIDTH  = "required";
	parameter OUT_WIDTH  = "required";

	reg signed [47:0] rx_phase_inc;
	wire set_phaseH, set_phaseL;

	SYNC_PULSE set_phaseH_inst (.in_clk(cpu_clk), .in(rx_sel_C && set_rx_freqH_C), .out_clk(adc_clk), .out(set_phaseH));
	SYNC_PULSE set_phaseL_inst (.in_clk(cpu_clk), .in(rx_sel_C && set_rx_freqL_C), .out_clk(adc_clk), .out(set_phaseL));

    always @ (posedge adc_clk)
    begin
        if (set_phaseH) rx_phase_inc[16 +:32] <= freeze_tos;
        if (set_phaseL) rx_phase_inc[ 0 +:16] <= freeze_tos;
    end

	wire signed [RX1_BITS-1:0] rx_mix_i, rx_mix_q;

	IQ_MIXER #(.PHASE_WIDTH(48), .IN_WIDTH(IN_WIDTH), .OUT_WIDTH(RX1_BITS))
		rx_mixer (
			.clk		(adc_clk),
			.phase_inc	(rx_phase_inc),
			.in_data	(adc_data),
			.out_i		(rx_mix_i),
			.out_q		(rx_mix_q)
		);
	
	wire rx_cic1_avail;
	wire signed [RX2_BITS-1:0] rx_cic1_out_i, rx_cic1_out_q;

cic_prune_var #(.INCLUDE("rx1"), .STAGES(RX1_STAGES), .DECIMATION(RX1_DECIM), .GROWTH(RX1_GROWTH), .IN_WIDTH(RX1_BITS), .OUT_WIDTH(RX2_BITS))
	rx_cic1_i(
		.clock			(adc_clk),
		.reset			(1'b0),
		.decimation		(18'b0),
		.in_strobe		(1'b1),
		.out_strobe		(rx_cic1_avail),
		.in_data		(rx_mix_i),
		.out_data		(rx_cic1_out_i)
    );

cic_prune_var #(.INCLUDE("rx1"), .STAGES(RX1_STAGES), .DECIMATION(RX1_DECIM), .GROWTH(RX1_GROWTH), .IN_WIDTH(RX1_BITS), .OUT_WIDTH(RX2_BITS))
	rx_cic1_q(
		.clock			(adc_clk),
		.reset			(1'b0),
		.decimation		(18'b0),
		.in_strobe		(1'b1),
		.out_strobe		(),
		.in_data		(rx_mix_q),
		.out_data		(rx_cic1_out_q)
    );

`ifdef USE_RX_SEQ_MUX

    wire rx_cic2_strobe_i, rx_cic2_strobe_q;
	wire signed [OUT_WIDTH-1:0] rx_cic2_out_i, rx_cic2_out_q;

    cic_seq_integ_iq_prune #(.INCLUDE("rx3"), .STAGES(RX2_STAGES), .DECIMATION(RX2_DECIM), .GROWTH(RX2_GROWTH), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(OUT_WIDTH))
        rx_cic2 (
            .clock			(adc_clk),
            .reset			(1'b0),
    
            .in_strobe		(rx_cic1_avail),
            .in_data_i		(rx_cic1_out_i),
            .in_data_q		(rx_cic1_out_q),
    
            .out_strobe_i   (rx_cic2_strobe_i),
            .out_strobe_q   (rx_cic2_strobe_q),
            .out_data_i     (rx_cic2_out_i),
            .out_data_q     (rx_cic2_out_q)
        );

    assign rx_avail_A  = rx_cic2_strobe_i;
	assign rx_dout_i_A = rx_cic2_out_i;
	assign rx_dout_q_A = rx_cic2_out_q;

`elsif USE_RX_SEQ_SEP

    wire rx_cic2_strobe_i, rx_cic2_strobe_q;
	wire signed [RXO_BITS-1:0] rx_cic2_out;
	reg  signed [RXO_BITS-1:0] rx_cic2_out_i, rx_cic2_out_q;

    cic_seq_sep_iq_prune #(.INCLUDE("rx3"), .STAGES(RX2_STAGES), .DECIMATION(RX2_DECIM), .GROWTH(RX2_GROWTH), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
        rx_cic2 (
            .clock			(adc_clk),
            .reset			(1'b0),
    
            .in_strobe		(rx_cic1_avail),
            .in_data_i		(rx_cic1_out_i),
            .in_data_q		(rx_cic1_out_q),
    
            .out_strobe_i   (rx_cic2_strobe_i),
            .out_strobe_q   (rx_cic2_strobe_q),
            .out_data_iq    (rx_cic2_out)
        );

    always @ (posedge adc_clk)
    begin
        if (rx_cic2_strobe_i) rx_cic2_out_i <= rx_cic2_out;
        if (rx_cic2_strobe_q) rx_cic2_out_q <= rx_cic2_out;
    end

    assign rx_avail_A = rx_cic2_strobe_q;   // okay that this is one clock early due to delays in rx audio sample mem state machine

	reg [OUT_WIDTH-1:0] rx_dout;
	always @*
		rx_dout = rd_i? rx_cic2_out_i[OUT_WIDTH-1:0] : ( rd_q? rx_cic2_out_q[OUT_WIDTH-1:0] : {rx_cic2_out_i[RXO_BITS-1 -:8], rx_cic2_out_q[RXO_BITS-1 -:8]} );

	assign rx_dout_i_A = rx_dout;
	assign rx_dout_q_A = {OUT_WIDTH{1'b0}};

`else

    wire rx_cic2_strobe;
	wire signed [RXO_BITS-1:0] rx_cic2_out_i, rx_cic2_out_q;

    cic_prune_var #(.INCLUDE("rx2"), .STAGES(RX2_STAGES), .DECIMATION(RX2_DECIM), .GROWTH(RX2_GROWTH), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
        rx_cic2_i(
            .clock			(adc_clk),
            .reset			(1'b0),
            .decimation		(18'b0),
    
            .in_strobe		(rx_cic1_avail),
            .in_data		(rx_cic1_out_i),
    
            .out_strobe		(rx_cic2_strobe),
            .out_data		(rx_cic2_out_i)
        );
    
    cic_prune_var #(.INCLUDE("rx2"), .STAGES(RX2_STAGES), .DECIMATION(RX2_DECIM), .GROWTH(RX2_GROWTH), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
        rx_cic2_q(
            .clock			(adc_clk),
            .reset			(1'b0),
            .decimation		(18'b0),
    
            .in_strobe		(rx_cic1_avail),
            .in_data		(rx_cic1_out_q),
    
            .out_strobe		(),
            .out_data		(rx_cic2_out_q)
        );
    
    assign rx_avail_A = rx_cic2_strobe;

	reg [OUT_WIDTH-1:0] rx_dout;
	always @*
		rx_dout = rd_i? rx_cic2_out_i[OUT_WIDTH-1:0] : ( rd_q? rx_cic2_out_q[OUT_WIDTH-1:0] : {rx_cic2_out_i[RXO_BITS-1 -:8], rx_cic2_out_q[RXO_BITS-1 -:8]} );

	assign rx_dout_i_A = rx_dout;
	//assign rx_dout_q_A = {OUT_WIDTH{1'b0}};
	assign rx_dout_q_A = 0;

`endif

endmodule
