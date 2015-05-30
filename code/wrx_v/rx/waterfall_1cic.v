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

// Copyright (c) 2014 John Seamons, ZL/KF6VO

`include "wrx.vh"

module WATERFALL_1CIC (
	input  wire		   adc_clk,
	input  wire signed [IN_WIDTH-1:0] adc_data,

	input  wire		   wf_sel_C,
	output wire [15:0] wf_dout_C,

    input  wire        cpu_clk,
    input  wire [31:0] freeze_tos,

    input  wire        set_wf_freq_C,
    input  wire        set_wf_decim_C,
    input  wire        rst_wf_sampler_C,
    input  wire        get_wf_samp_i_C,
    input  wire        get_wf_samp_q_C
	);

	parameter IN_WIDTH  = "required";

	wire reset;
	SYNC_PULSE reset_inst (.in_clk(cpu_clk), .in(rst_wf_sampler_C), .out_clk(adc_clk), .out(reset));

	reg signed [31:0] wf_phase_inc;
	wire set_wf_freq_A;

	SYNC_PULSE set_freq_inst (.in_clk(cpu_clk), .in(wf_sel_C & set_wf_freq_C), .out_clk(adc_clk), .out(set_wf_freq_A));

    always @ (posedge adc_clk)
        if (set_wf_freq_A) wf_phase_inc <= freeze_tos;

	wire signed [WF1_BITS-1:0] wf_mix_i, wf_mix_q;

IQ_MIXER #(.IN_WIDTH(IN_WIDTH), .OUT_WIDTH(WF1_BITS))
	wf_mixer (
		.clk		(adc_clk),
		.phase_inc	(wf_phase_inc),
		.in_data	(adc_data),
		.out_i		(wf_mix_i),
		.out_q		(wf_mix_q)
	);
	
	wire set_wf_decim_A;

	SYNC_PULSE set_decim_inst (.in_clk(cpu_clk), .in(wf_sel_C & set_wf_decim_C), .out_clk(adc_clk), .out(set_wf_decim_A));

	localparam MD = clog2(WF_1CIC_MAXD) + 2;		// fixme: why is +2 needed to make this work?
initial begin
$display("WF_1CIC_MAXD=%s MD=%s", WF_1CIC_MAXD, MD);
//$finish;
end

	reg signed [MD-1:0] decim;
	
    always @ (posedge adc_clk)
        if (set_wf_decim_A)
        	decim <= freeze_tos[0 +:MD];

wire wf_cic_avail;
wire [WFO_BITS-1:0] wf_cic_out_i, wf_cic_out_q;

localparam WF1_GROWTH = WF1_STAGES * clog2(WF_1CIC_MAXD);

// decim = 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048

`ifdef USE_WF_PRUNE
cic_prune_var #(.INCLUDE("cic_wf1.vh"), .STAGES(WF1_STAGES), .DECIMATION(-2048), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WFO_BITS))
`else
cic #(.STAGES(WF1_STAGES), .DECIMATION(-1024), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WFO_BITS))
`endif
  wf_cic_i(
    .clock			(adc_clk),
    .reset			(reset),
    .decimation		(decim),
    .in_strobe		(1'b1),
    .out_strobe		(wf_cic_avail),
    .in_data		(wf_mix_i),
    .out_data		(wf_cic_out_i)
    );

`ifdef USE_WF_PRUNE
cic_prune_var #(.INCLUDE("cic_wf1.vh"), .STAGES(WF1_STAGES), .DECIMATION(-2048), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WFO_BITS))
`else
cic #(.STAGES(WF1_STAGES), .DECIMATION(-1024), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WFO_BITS))
`endif
  wf_cic_q(
    .clock			(adc_clk),
    .reset			(reset),
    .decimation		(decim),
    .in_strobe		(1'b1),
    .out_strobe		(),
    .in_data		(wf_mix_q),
    .out_data		(wf_cic_out_q)
    );

	IQ_SAMPLER_8K_16B wf_samp(
		.wr_clk		(adc_clk),
		.wr_rst		(reset),
		.wr			(wf_cic_avail),
		.wr_i		(wf_cic_out_i),
		.wr_q		(wf_cic_out_q),
		
		.rd_clk		(cpu_clk),
		.rd_rst		(wf_sel_C && rst_wf_sampler_C),
		.rd_i		(wf_sel_C && get_wf_samp_i_C),
		.rd_q		(wf_sel_C && get_wf_samp_q_C),
		.rd_iq		(wf_dout_C)
	  );

endmodule
