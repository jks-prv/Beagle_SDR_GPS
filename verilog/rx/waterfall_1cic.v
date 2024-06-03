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

// Copyright (c) 2014 John Seamons, ZL4VO/KF6VO

module WATERFALL_1CIC (
	input  wire		   adc_clk,
	input  wire signed [IN_WIDTH-1:0] adc_data,

	input  wire		   wf_sel_C,
	output wire [15:0] wf_dout_C,

    input  wire        cpu_clk,
    input  wire [31:0] freeze_tos_A,

    input  wire        samp_wf_rd_rst_C,
    input  wire        samp_wf_sync_C,
    input  wire        set_wf_freqH_C,
    input  wire        set_wf_freqL_C,
    input  wire        set_wf_decim_C,
    input  wire        rst_wf_sampler_C,
    input  wire        get_wf_samp_i_C,
    input  wire        get_wf_samp_q_C
	);

`include "kiwi.gen.vh"

	parameter IN_WIDTH  = "required";

	wire rst_wf_sampler_A;
	SYNC_PULSE reset_inst (.in_clk(cpu_clk), .in(wf_sel_C && rst_wf_sampler_C), .out_clk(adc_clk), .out(rst_wf_sampler_A));
	
	wire rst_wf_samp_wr_A = rst_wf_sampler_A && freeze_tos_A[WF_SAMP_WR_RST];
	wire rst_wf_samp_rd_C = rst_wf_sampler_C && samp_wf_rd_rst_C; // qualified with wf_sel_C below

	reg wf_continuous_A;
    always @ (posedge adc_clk)
    begin
    	if (rst_wf_sampler_A) wf_continuous_A <= freeze_tos_A[WF_SAMP_CONTIN];
    end

	wire wf_samp_sync_C = rst_wf_sampler_C && samp_wf_sync_C;	// qualified with wf_sel_C below

	reg signed [47:0] wf_phase_inc;
	wire set_wf_freqH_A, set_wf_freqL_A;

	SYNC_PULSE set_freqH_inst (.in_clk(cpu_clk), .in(wf_sel_C && set_wf_freqH_C), .out_clk(adc_clk), .out(set_wf_freqH_A));
	SYNC_PULSE set_freqL_inst (.in_clk(cpu_clk), .in(wf_sel_C && set_wf_freqL_C), .out_clk(adc_clk), .out(set_wf_freqL_A));

    always @ (posedge adc_clk)
    begin
        if (set_wf_freqH_A) wf_phase_inc[16 +:32] <= freeze_tos_A;
        if (set_wf_freqL_A) wf_phase_inc[ 0 +:16] <= freeze_tos_A;
    end

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

	SYNC_PULSE set_decim_inst (.in_clk(cpu_clk), .in(wf_sel_C && set_wf_decim_C), .out_clk(adc_clk), .out(set_wf_decim_A));

	localparam MD = max(1, clog2(WF_1CIC_MAXD));    // +1 because need to represent WF_1CIC_MAXD, not WF_1CIC_MAXD-1
	// see freeze_tos_A[] below
	// assert_cond(WF_1CIC_MAXD <= 32768);
	// assert_cond(MD <= 16);
	//wire [MD-1:0] md = 0; how_big(.p(md));

	reg [MD-1:0] decim;
	
    always @ (posedge adc_clk)
        if (set_wf_decim_A)
        	decim <= freeze_tos_A[0 +:MD];

    wire wf_cic_avail;
    wire [WFO_BITS-1:0] wf_cic_out_i, wf_cic_out_q;
    
    // NB: for N=pow2, M=1: N * log2(R) == log2(pow(R*M, N)), N=#stages, R=decim
    localparam WF1_GROWTH = WF1_STAGES * clog2(WF_1CIC_MAXD);
    //wire [WF1_GROWTH-1:0] wf1_growth = 0; how_big(.p(wf1_growth));
    
    // decim = 1 .. WF_1CIC_MAXD

    cic_prune_var #(.INC_FILE("wf1"), .STAGES(WF1_STAGES), .DECIM_TYPE(-WF_1CIC_MAXD), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WFO_BITS))
    wf_cic_i(
        .clock			(adc_clk),
        .reset			(rst_wf_samp_wr_A),
        .decimation		(decim),
        .in_strobe		(1'b1),
        .out_strobe		(wf_cic_avail),
        .in_data		(wf_mix_i),
        .out_data		(wf_cic_out_i)
    );

    cic_prune_var #(.INC_FILE("wf1"), .STAGES(WF1_STAGES), .DECIM_TYPE(-WF_1CIC_MAXD), .GROWTH(WF1_GROWTH), .IN_WIDTH(WF1_BITS), .OUT_WIDTH(WFO_BITS))
    wf_cic_q(
        .clock			(adc_clk),
        .reset			(rst_wf_samp_wr_A),
        .decimation		(decim),
        .in_strobe		(1'b1),
        .out_strobe		(),
        .in_data		(wf_mix_q),
        .out_data		(wf_cic_out_q)
    );

	IQ_SAMPLER_8K_32B wf_samp(
		.wr_clk			(adc_clk),
		.wr_rst			(rst_wf_samp_wr_A),
		.wr_continuous	(wf_continuous_A),
		.wr				(wf_cic_avail),
		.wr_i			(wf_cic_out_i),
		.wr_q			(wf_cic_out_q),
		
		.rd_clk			(cpu_clk),
		.rd_rst			(wf_sel_C && rst_wf_samp_rd_C),
		.rd_sync		(wf_sel_C && wf_samp_sync_C),
		.rd_i			(wf_sel_C && get_wf_samp_i_C),
		.rd_q			(wf_sel_C && get_wf_samp_q_C),
		.rd_iq			(wf_dout_C)
	);

endmodule
