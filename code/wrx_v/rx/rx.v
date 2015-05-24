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

`include "wrx.vh"

module RX (
	input wire 		   adc_clk,
	input wire signed [IN_WIDTH-1:0] adc_data,

	output wire		   rx_srq_C,
	input  wire		   rx_sel_C,
	output wire [15:0] rx_dout_C,

	input  wire		   cpu_clk,
    input  wire [31:0] freeze_tos,
    
    input  wire        set_rx_freq_C,

    input  wire        flip_bufs_C,
    input  wire        get_rx_samp_C
	);
	
	parameter IN_WIDTH  = "required";

	reg signed [31:0] rx_phase_inc;
	wire set_phase;

	SYNC_PULSE set_phase_inst (.in_clk(cpu_clk), .in(rx_sel_C & set_rx_freq_C), .out_clk(adc_clk), .out(set_phase));

    always @ (posedge adc_clk)
        if (set_phase) rx_phase_inc <= freeze_tos;

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

cic_prune #(.INCLUDE("cic_rx1.vh"), .DECIMATION(RX1_DECIM), .IN_WIDTH(RX1_BITS), .OUT_WIDTH(RX2_BITS))
	rx_cic1_i(
		.clock			(adc_clk),
		.decimation		(13'b0),
		.in_strobe		(1'b1),
		.out_strobe		(rx_cic1_avail),
		.in_data		(rx_mix_i),
		.out_data		(rx_cic1_out_i)
    );

cic_prune #(.INCLUDE("cic_rx1.vh"), .DECIMATION(RX1_DECIM), .IN_WIDTH(RX1_BITS), .OUT_WIDTH(RX2_BITS))
	rx_cic1_q(
		.clock			(adc_clk),
		.decimation		(13'b0),
		.in_strobe		(1'b1),
		.out_strobe		(),
		.in_data		(rx_mix_q),
		.out_data		(rx_cic1_out_q)
    );

localparam clog2_31 = clog2(31);
initial begin
$display("clog2(31)=%s", clog2_31);
//$finish;
end

    wire rx_cic2_strobe_i, rx_cic2_strobe_q;

`ifdef USE_RX_SEQ

	wire [RXO_BITS-1:0] rx_cic2_out;

// fixme: why is cic_prune_seq broken when RX2_DECIM = 31?
//cic_prune_seq #(.INCLUDE("cic_rx3.vh"), .STAGES(RX2_STAGES), .DECIMATION(RX2_DECIM), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
cic_iq_seq #(.STAGES(RX2_STAGES), .DECIMATION(RX2_DECIM), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
	rx_cic2 (
		.clock			(adc_clk),
		.in_strobe		(rx_cic1_avail),
		.out_strobe_i	(rx_cic2_strobe_i),
		.out_strobe_q	(rx_cic2_strobe_q),
		.in_data_i		(rx_cic1_out_i),
		.in_data_q		(rx_cic1_out_q),
		.out_data		(rx_cic2_out)
    );

	reg write_3;
	wire wr = rx_cic2_strobe_i | rx_cic2_strobe_q | write_3;
	reg [RXO_BITS-1-16:0] rx_cic2_out_buf;

	always @ (posedge adc_clk)
	begin
		rx_cic2_out_buf <= rx_cic2_strobe_i? rx_cic2_out[RXO_BITS-1:16] : rx_cic2_out_buf;
		write_3 <= rx_cic2_strobe_q;
	end
	
	always @*
		rx_din = write_3? {rx_cic2_out_buf, rx_cic2_out[RXO_BITS-1:16]} : rx_cic2_out[15:0];

`else

	wire signed [RXO_BITS-1:0] rx_cic2_out_i, rx_cic2_out_q;

cic_prune #(.INCLUDE("cic_rx2.vh"), .DECIMATION(RX2_DECIM), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
	rx_cic2_i(
		.clock			(adc_clk),
		.decimation		(13'b0),
		.in_strobe		(rx_cic1_avail),
		.out_strobe		(rx_cic2_strobe_i),
		.in_data		(rx_cic1_out_i),
		.out_data		(rx_cic2_out_i)
    );

cic_prune #(.INCLUDE("cic_rx2.vh"), .DECIMATION(RX2_DECIM), .IN_WIDTH(RX2_BITS), .OUT_WIDTH(RXO_BITS))
	rx_cic2_q(
		.clock			(adc_clk),
		.decimation		(13'b0),
		.in_strobe		(rx_cic1_avail),
		.out_strobe		(rx_cic2_strobe_q),
		.in_data		(rx_cic1_out_q),
		.out_data		(rx_cic2_out_q)
    );

	reg write_i, write_q, write_3;
	wire wr = write_i || write_q || write_3;

	always @ (posedge adc_clk)
	begin
		write_i <= rx_cic2_strobe_i;
		write_q <= write_i;
		write_3 <= write_q;
	end
	
	always @*
		rx_din = write_q? rx_cic2_out_q[15:0] : ( write_3? {rx_cic2_out_i[RXO_BITS-1 -:8], rx_cic2_out_q[RXO_BITS-1 -:8]} : rx_cic2_out_i[15:0] );
	
`endif

	reg [15:0] rx_din;
	
	reg bank_C;
	wire bank_A;
	reg [8:0] waddr, raddr;
	
	SYNC_WIRE bank_inst (.in(bank_C), .out_clk(adc_clk), .out(bank_A));

	SYNC_PULSE srq_inst (.in_clk(adc_clk), .in(rx_cic2_strobe_q), .out_clk(cpu_clk), .out(rx_srq_C));

	wire flip_A;
	SYNC_PULSE flip_inst (.in_clk(cpu_clk), .in(flip_C), .out_clk(adc_clk), .out(flip_A));

	always @ (posedge adc_clk)
	begin
		if (flip_A)
		begin
			waddr <= 0;
		end else
			waddr <= waddr + wr;
	end
	
	wire flip_C = rx_sel_C & flip_bufs_C;

	always @ (posedge cpu_clk)
	begin
		if (flip_C)
		begin
			raddr <= 0;
			bank_C <= ~bank_C;
		end
		else raddr <= raddr + rd;
	end

	wire rd = rx_sel_C & get_rx_samp_C;
	
	ipcore_bram_1024_16b rx_buf (
		.clka	(adc_clk),				.clkb	(cpu_clk),
		.addra	({bank_A, waddr}),		.addrb	({~bank_C, raddr + rd}),
		.dina	(rx_din),				.doutb	(rx_dout_C),
		.wea	(wr)
	);

endmodule
