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

module RECEIVER (
	input wire		   adc_clk,
	input wire signed [ADC_BITS-1:0] adc_data,

    output wire        rx_rd_C,
    output wire [15:0] rx_dout_C,

    output wire        wf_rd_C,
    output wire [15:0] wf_dout_C,

	input  wire		   cpu_clk,
    output wire        ser,
    input  wire [31:0] tos,
    input  wire [15:0] op,
    input  wire        rdBit2,
    input  wire        wrReg2,
    input  wire        wrEvt2,
    input  wire [15:0] ctrl
	);
	
	wire set_rx_chan_C =	wrReg2 & op[SET_RX_CHAN];
	wire set_rx_freq_C =	wrReg2 & op[SET_RX_FREQ];
	
	wire set_wf_chan_C =	wrReg2 & op[SET_WF_CHAN];
	wire set_wf_freq_C =	wrReg2 & op[SET_WF_FREQ];
	wire set_wf_decim_C =	wrReg2 & op[SET_WF_DECIM];

	// Need to latch before SYNC_WIRE is applied because freeze_tos will be sampled by a clock
	// derived from SYNC_PULSE, which will take longer to become valid than the SYNC_WIRE.
	// I.e. without the latch, freeze_tos might have become invalid by the time it was sampled
	// by the SYNC_PULSE clock.
	
	reg [31:0] latch_tos;

    always @ (posedge cpu_clk)
    	if (wrReg2)
			latch_tos <= tos;
	
	wire [31:0] freeze_tos;
	SYNC_WIRE sync_freeze_tos [31:0] (.in(latch_tos), .out_clk(adc_clk), .out(freeze_tos));

`ifdef USE_GEN
	localparam RX_IN_WIDTH = 18;

	wire use_gen_A;
	SYNC_WIRE use_gen_inst (.in(ctrl[CTRL_USE_GEN]), .out_clk(adc_clk), .out(use_gen_A));

	wire signed [35:0] gen_data;
	wire [17:0] rx_data = use_gen_A? { gen_data[35], gen_data[33 -:17] } : { adc_data, {18-ADC_BITS{1'b0}} };
	
	GEN gen_inst (
		.adc_clk	(adc_clk),
		.gen_data	(gen_data),

		.cpu_clk	(cpu_clk),
		.freeze_tos	(freeze_tos),
        .op			(op),        
        .wrReg2     (wrReg2)
	);
`else
	localparam RX_IN_WIDTH = ADC_BITS;
	
	wire [ADC_BITS-1:0] rx_data = adc_data;

`endif
	
    localparam L2RX = clog2(RX_CHANS) - 1;
    reg [L2RX:0] rx_channel_C;
	
	reg ser_sel;
    always @ (posedge cpu_clk)
    begin
    	if (wrEvt2) ser_sel = op[GET_RX_SRQ];
    end
    	
    wire ser_load = wrEvt2 & op[GET_RX_SRQ];
    wire ser_next = rdBit2 & ser_sel;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_rx_chan_C) rx_channel_C <= tos[L2RX:0];		// careful, _not_ freeze_tos
    end

	wire [RX_CHANS-1:0] rx_sel_C = 1 << rx_channel_C;

	wire [RX_CHANS-1:0]	   rxn_srq_C;
	wire [RX_CHANS*16-1:0] rxn_dout_C;
	
	wire flip_bufs_C =		wrEvt2 & op[RX_FLIP_BUFS];
	wire get_rx_samp_C =	wrEvt2 & op[GET_RX_SAMP];

	RX #(.IN_WIDTH(RX_IN_WIDTH)) rx_inst [RX_CHANS-1:0] (
		.adc_clk		(adc_clk),
		.adc_data		(rx_data),
		
		.rx_srq_C		(rxn_srq_C),
		.rx_sel_C		(rx_sel_C),
		.rx_dout_C		(rxn_dout_C),

		.cpu_clk		(cpu_clk),
		.freeze_tos		(freeze_tos),
		
		.set_rx_freq_C	(set_rx_freq_C),
		
		.flip_bufs_C	(flip_bufs_C),
		.get_rx_samp_C	(get_rx_samp_C)
	);
	
    reg [RX_CHANS-1:0] srq_noted, srq_shift;
    always @ (posedge cpu_clk)
    begin
        if (ser_load) srq_noted <= rxn_srq_C;
        else		  srq_noted <= rxn_srq_C | srq_noted;
        
        if      (ser_load) srq_shift <= srq_noted;
        else if (ser_next) srq_shift <= srq_shift << 1;
    end

	MUX #(.WIDTH(16), .SEL(RX_CHANS)) rx_mux(.in(rxn_dout_C), .sel(rx_channel_C), .out(rx_dout_C));

	assign ser = srq_shift[RX_CHANS-1];
	assign rx_rd_C = get_rx_samp_C;
    
    //////////////////////////////////////////////////////////////////////////
    // waterfall(s)
    
    localparam L2WF = clog2(WF_CHANS) - 1;
    reg [L2WF:0] wf_channel_C;
	wire [WF_CHANS-1:0] wf_sel_C = 1 << wf_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_wf_chan_C) wf_channel_C <= tos[L2WF:0];		// careful, _not_ freeze_tos
    end
    
	wire [WF_CHANS*16-1:0] wfn_dout_C;
	MUX #(.WIDTH(16), .SEL(WF_CHANS)) wf_mux(.in(wfn_dout_C), .sel(wf_channel_C), .out(wf_dout_C));

	wire rst_wf_sampler_C =	wrEvt2 & op[WF_SAMPLER_RST];
	wire get_wf_samp_i_C =	wrEvt2 & op[GET_WF_SAMP_I];
	wire get_wf_samp_q_C =	wrEvt2 & op[GET_WF_SAMP_Q];
	assign wf_rd_C =		get_wf_samp_i_C || get_wf_samp_q_C;

`ifdef USE_WF_1CIC
	WATERFALL_1CIC #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst [WF_CHANS-1:0] (
`else
	WATERFALL #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst (
`endif
		.adc_clk			(adc_clk),
		.adc_data			(rx_data),
		
		.wf_sel_C			(wf_sel_C),
		.wf_dout_C			(wfn_dout_C),

		.cpu_clk			(cpu_clk),
		.freeze_tos			(freeze_tos),

		.set_wf_freq_C		(set_wf_freq_C),
		.set_wf_decim_C		(set_wf_decim_C),
		.rst_wf_sampler_C	(rst_wf_sampler_C),
		.get_wf_samp_i_C	(get_wf_samp_i_C),
		.get_wf_samp_q_C	(get_wf_samp_q_C)
	);

endmodule
