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

`include "kiwi.vh"

module WATERFALL (
	input  wire		   adc_clk,
	input  wire signed [IN_WIDTH-1:0] adc_data,

	//fixme: not yet converted to new multi-w/f interface (see waterfall_1cic.v)
	output wire		   wf_rd_C,
	output wire [15:0] wf_dout_C,

    input  wire        cpu_clk,
    input  wire [31:0] freeze_tos,
    input  wire [15:0] op,
    input  wire        wrEvt2,

    input  wire        set_wf_freq,
    input  wire        set_wf_decim
	);

	parameter IN_WIDTH  = "required";

	wire rst_wf_sampler_A;
	SYNC_PULSE reset_inst (.in_clk(cpu_clk), .in(wf_sel_C && rst_wf_sampler_C), .out_clk(adc_clk), .out(rst_wf_sampler_A));

	reg signed [31:0] wf_phase_inc;
	wire set_wf_freq_A;

	SYNC_PULSE set_freq_inst (.in_clk(cpu_clk), .in(set_wf_freq), .out_clk(adc_clk), .out(set_wf_freq_A));

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

	SYNC_PULSE set_decim_inst (.in_clk(cpu_clk), .in(set_wf_decim), .out_clk(adc_clk), .out(set_wf_decim_A));

	localparam MD = clog2(WF_2CIC_MAXD) + 2;		// fixme: why is +2 needed to make this work?
initial begin
$display("WF_2CIC_MAXD=%s MD=%s", WF_2CIC_MAXD, MD);
//$finish;
end

	reg signed [MD-1:0] decim1, decim2;	// [5:0] because has to hold 32, not 31 (fixme: consider changing?)

    always @ (posedge adc_clk)
        if (set_wf_decim_A)
        begin
        	decim1 <= freeze_tos[0 +:MD];
    		decim2 <= freeze_tos[8 +:MD];
    	end

wire wf_cic1_avail;
wire [WF2_BITS-1:0] wf_cic1_out_i, wf_cic1_out_q;

localparam WF1_GROWTH = WF1_STAGES * clog2(WF_2CIC_MAXD);

// decim1 = 1, 2, 4, 8, 16, 32

`ifdef USE_WF_PRUNE
cic_prune #(.INCLUDE("cic_wf1.vh"), .DECIMATION(-32), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WF2_BITS))
`else
cic #(.STAGES(WF1_STAGES), .DECIMATION(-32), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WF2_BITS))
`endif
  wf_cic1_i(
    .clock			(adc_clk),
    .reset			(rst_wf_sampler_A),
    .decimation		(decim1),
    .in_strobe		(1'b1),
    .out_strobe		(wf_cic1_avail),
    .in_data		(wf_mix_i),
    .out_data		(wf_cic1_out_i)
    );

`ifdef USE_WF_PRUNE
cic_prune #(.INCLUDE("cic_wf1.vh"), .DECIMATION(-32), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WF2_BITS))
`else
cic #(.STAGES(WF1_STAGES), .DECIMATION(-32), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WF2_BITS))
`endif
  wf_cic1_q(
    .clock			(adc_clk),
    .reset			(rst_wf_sampler_A),
    .decimation		(decim1),
    .in_strobe		(1'b1),
    .out_strobe		(),
    .in_data		(wf_mix_q),
    .out_data		(wf_cic1_out_q)
    );

wire wf_cic2_avail;
wire [WFO_BITS-1:0] wf_cic2_out_i, wf_cic2_out_q;

localparam WF2_GROWTH = WF2_STAGES * clog2(WF_2CIC_MAXD);

// decim2 = 1, 2, 4, 8, 16, 32

`ifdef USE_WF_PRUNE
cic_prune #(.INCLUDE("cic_wf2.vh"), .DECIMATION(-32), .GROWTH(WF2_GROWTH), .IN_WIDTH(WF2_BITS), .OUT_WIDTH(WFO_BITS))
`else
cic #(.STAGES(WF2_STAGES), .DECIMATION(-32), .GROWTH(WF2_GROWTH), .IN_WIDTH(WF2_BITS), .OUT_WIDTH(WFO_BITS))
`endif
  wf_cic2_i(
    .clock			(adc_clk),
    .reset			(rst_wf_sampler_A),
    .decimation		(decim2),
    .in_strobe		(wf_cic1_avail),
    .out_strobe		(wf_cic2_avail),
    .in_data		(wf_cic1_out_i),
    .out_data		(wf_cic2_out_i)
    );

`ifdef USE_WF_PRUNE
cic_prune #(.INCLUDE("cic_wf2.vh"), .DECIMATION(-32), .GROWTH(WF2_GROWTH), .IN_WIDTH(WF2_BITS), .OUT_WIDTH(WFO_BITS))
`else
cic #(.STAGES(WF2_STAGES), .DECIMATION(-32), .GROWTH(WF2_GROWTH), .IN_WIDTH(WF2_BITS), .OUT_WIDTH(WFO_BITS))
`endif
  wf_cic2_q(
    .clock			(adc_clk),
    .reset			(rst_wf_sampler_A),
    .decimation		(decim2),
    .in_strobe		(wf_cic1_avail),
    .out_strobe		(),
    .in_data		(wf_cic1_out_q),
    .out_data		(wf_cic2_out_q)
    );

	//wire rst_wf_sampler =	wrEvt2 [NOW IT's wrReg2] && op[WF_SAMPLER_RST];	
	wire get_wf_samp_i =	wrEvt2 && op[GET_WF_SAMP_I];
	wire get_wf_samp_q =	wrEvt2 && op[GET_WF_SAMP_Q];

`ifdef USE_WF_MEM24
	IQ_SAMPLER_8K_24B wf_samp(
		.wr_clk		(adc_clk),
		.wr_rst		(rst_wf_sampler_A),
		.wr			(wf_cic2_avail),
		.wr_i_h16	(wf_cic2_out_i[23 -:16]),
		.wr_i_l8	(wf_cic2_out_i[7 -:8]),
		.wr_q_h16	(wf_cic2_out_q[23 -:16]),
		.wr_q_l8	(wf_cic2_out_q[7 -:8]),
		
		.rd_clk		(cpu_clk),
		.rd_rst		(rst_wf_sampler),
		.rd_i		(get_wf_samp_i),
		.rd_q		(get_wf_samp_q),
		.rd_3		(get_wf_samp_3),
		.rd_iq		(wf_dout_C)
	  );

	wire get_wf_samp_3 = wrEvt2 && op[GET_WF_SAMP_3];
	assign wf_rd_C = get_wf_samp_i || get_wf_samp_q || get_wf_samp_3;
`else

	IQ_SAMPLER_8K_16B wf_samp(
		.wr_clk		(adc_clk),
		.wr_rst		(rst_wf_sampler_A),
		.wr			(wf_cic2_avail),
		.wr_i		(wf_cic2_out_i),
		.wr_q		(wf_cic2_out_q),
		
		.rd_clk		(cpu_clk),
		.rd_rst		(rst_wf_sampler),
		.rd_i		(get_wf_samp_i),
		.rd_q		(get_wf_samp_q),
		.rd_iq		(wf_dout_C)
	  );

	assign wf_rd_C = get_wf_samp_i || get_wf_samp_q;
`endif

endmodule
