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
    input  wire        rdReg,
    input  wire        rdBit2,
    input  wire        wrReg2,
    input  wire        wrEvt2,
    
    input  wire [15:0] ctrl
	);
	
	wire set_rx_chan_C =	wrReg2 && op[SET_RX_CHAN];
	wire set_rx_freq_C =	wrReg2 && op[SET_RX_FREQ];
	wire set_rx_nsamps_C =	wrReg2 && op[SET_RX_NSAMPS];
	
	wire set_wf_chan_C =	wrReg2 && op[SET_WF_CHAN];
	wire set_wf_freq_C =	wrReg2 && op[SET_WF_FREQ];
	wire set_wf_decim_C =	wrReg2 && op[SET_WF_DECIM];

	// The FREEZE_TOS event starts the process of latching and synchronizing of the ecpu TOS data
	// from the cpu_clk to the adc_clk domain. This is needed by subsequent wrReg instructions
	// that want to load TOS values into registers clocked in the adc_clk domain.
	//
	// e.g. an ecpu code sequence like this:
	//		rdReg HOST_RX; wrEvt2 FREEZE_TOS; nop; nop; wrReg2 SET_WF_FREQ

`ifdef LATCH_TOS_A
	reg [31:0] latch_tos_C;
	reg cross_clk_domains_C;

    always @ (posedge cpu_clk)
    	if (wrEvt2 && op[FREEZE_TOS])
    	begin
			latch_tos_C <= tos;
			cross_clk_domains_C = 1'b1;		// ensure single cpu_clk period pulse so SYNC_PULSE works
		end
		else
			cross_clk_domains_C = 1'b0;
	
	wire [31:0] latch_tos_A;
	SYNC_WIRE sync_latch_tos [31:0] (.in(latch_tos_C), .out_clk(adc_clk), .out(latch_tos_A));

	wire cross_clk_domains_A;
	SYNC_PULSE sync_cross_clk_domains (.in_clk(cpu_clk), .in(cross_clk_domains_C), .out_clk(adc_clk), .out(cross_clk_domains_A));

	reg [31:0] freeze_tos_A;

    always @ (posedge adc_clk)
    	if (cross_clk_domains_A)
			freeze_tos_A <= latch_tos_A;
`else
	reg [31:0] latch_tos_C;

    always @ (posedge cpu_clk)
    	if (wrEvt2 && op[FREEZE_TOS]) latch_tos_C <= tos;

	wire [31:0] freeze_tos_A;
	SYNC_WIRE sync_latch_tos [31:0] (.in(latch_tos_C), .out_clk(adc_clk), .out(freeze_tos_A));
`endif
	
`ifdef USE_GEN
	localparam RX_IN_WIDTH = 18;

	wire use_gen_A;
	SYNC_WIRE use_gen_inst (.in(ctrl[CTRL_USE_GEN]), .out_clk(adc_clk), .out(use_gen_A));

	wire signed [17:0] gen_data;
	wire [17:0] rx_data = use_gen_A? gen_data : { adc_data, {18-ADC_BITS{1'b0}} };

	GEN gen_inst (
		.adc_clk	(adc_clk),
		.gen_data	(gen_data),

		.cpu_clk	(cpu_clk),
		.freeze_tos	(freeze_tos_A),
        .op			(op),        
        .wrReg2     (wrReg2)
	);
`else
	localparam RX_IN_WIDTH = ADC_BITS;
	
	wire [ADC_BITS-1:0] rx_data = adc_data;

`endif
	
    localparam L2RX = clog2(RX_CHANS) - 1;
    reg [L2RX:0] rx_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_rx_chan_C) rx_channel_C <= tos[L2RX:0];
    end

	wire [RX_CHANS-1:0] rxn_sel_C = 1 << rx_channel_C;

	wire [RX_CHANS-1:0] rxn_avail_A;
	wire [RX_CHANS*16-1:0] rxn_dout_A;
	
	wire get_rx_samp_C =	wrEvt2 && op[GET_RX_SAMP];
	
	// Verilog note: if rd_i & rd_q are not declared before use in arrayed module RX below
	// then automatic fanout of single-bit signal to all RX instances doesn't occur and
	// an "undriven" error for rd_* results.
	reg rd_i, rd_q;

	RX #(.IN_WIDTH(RX_IN_WIDTH)) rx_inst [RX_CHANS-1:0] (
		.adc_clk		(adc_clk),
		.adc_data		(rx_data),
		
		.rx_sel_C		(rxn_sel_C),

		.rd_i			(rd_i),
		.rd_q			(rd_q),
		.rx_dout_A		(rxn_dout_A),
		.rx_avail_A		(rxn_avail_A),

		.cpu_clk		(cpu_clk),
		.freeze_tos		(freeze_tos_A),
		
		.set_rx_freq_C	(set_rx_freq_C)
	);

	
	// shared rx audio sample memory
	// when the DDC samples are available, all the receiver outputs are interleaved into a common buffer
	
	wire rx_avail_A = rxn_avail_A[0];		// all DDCs should signal available at the same time since decimation is the same
	
    reg [L2RX+1:0] rxn;		// careful: needs to hold RX_CHANS for the "rxn == RX_CHANS" test, not RX_CHANS-1
    reg [L2RX:0] rxn_d;
    reg [6:0] count;
	reg flip_A, wr;
	reg transfer;
	reg [1:0] move;
	
	wire set_nsamps;
	SYNC_PULSE set_nsamps_inst (.in_clk(cpu_clk), .in(set_rx_nsamps_C), .out_clk(adc_clk), .out(set_nsamps));
    reg [6:0] nrx_samps;
    always @ (posedge adc_clk)
        if (set_nsamps) nrx_samps <= freeze_tos_A;

	always @(posedge adc_clk)
	begin
		rxn_d <= rxn[L2RX:0];	// mux selector needs to be delayed 1 clock
	
		if (rx_avail_A) transfer <= 1'b1;
		
		if (transfer)
		begin
			if (rxn == RX_CHANS)
			begin
				move <= 0;
				wr <= 1'b0;
				rxn <= 0;
				transfer <= 1'b0;
				if (count == nrx_samps)
				begin
					flip_A <= 1'b1;
					count <= 0;
				end
				else
				begin
					flip_A <= 1'b0;
					count <= count + 1'b1;
				end
				rd_i <= 1'b0;
				rd_q <= 1'b0;
			end
			else
			begin
				case (move)
					0: begin rd_i <= 1'b1; rd_q <= 1'b0;					move <= 1; end
					1: begin rd_i <= 1'b0; rd_q <= 1'b1;					move <= 2; end
					2: begin rd_i <= 1'b0; rd_q <= 1'b0; rxn <= rxn + 1'b1;	move <= 0; end
					3: begin rd_i <= 1'b0; rd_q <= 1'b0;					move <= 0; end
				endcase
				wr <= 1'b1;
				flip_A <= 1'b0;
			end
		end
		else
		begin
			rd_i <= 1'b0;
			rd_q <= 1'b0;
			move <= 0;
			wr <= 1'b0;
			rxn <= 0;
			flip_A <= 1'b0;
		end
	end

    wire get_rx_srq = rdReg && op[GET_RX_SRQ];
	
    reg srq_noted, srq_out;
    always @ (posedge cpu_clk)
    begin
        if (get_rx_srq) srq_noted <= flip_bufs_C;
        else			srq_noted <= flip_bufs_C | srq_noted;
        if (get_rx_srq) srq_out   <= srq_noted;
    end

	assign ser = srq_out;

	assign rx_rd_C = get_rx_samp_C;
	
	wire [15:0] rx_dout_A;
	MUX #(.WIDTH(16), .SEL(RX_CHANS)) rx_mux(.in(rxn_dout_A), .sel(rxn_d[L2RX:0]), .out(rx_dout_A));
	
	reg bank_C;
	wire bank_A;
	reg [9:0] waddr, raddr;
	
	SYNC_WIRE bank_inst (.in(bank_C), .out_clk(adc_clk), .out(bank_A));

	wire flip_bufs_C;
	SYNC_PULSE flip_inst (.in_clk(adc_clk), .in(flip_A), .out_clk(cpu_clk), .out(flip_bufs_C));

	always @ (posedge adc_clk)
		waddr <= flip_A? 0 : waddr + wr;
	
	always @ (posedge cpu_clk)
		if (flip_bufs_C)
		begin
			raddr <= 0;
			bank_C <= ~bank_C;
		end
		else
			raddr <= raddr + rd;

	wire rd = get_rx_samp_C;
	
	ipcore_bram_2k_16b rx_buf (
		.clka	(adc_clk),				.clkb	(cpu_clk),
		.addra	({bank_A, waddr}),		.addrb	({~bank_C, raddr + rd}),
		.dina	(rx_dout_A),			.doutb	(rx_dout_C),
		.wea	(wr)
	);

    
    //////////////////////////////////////////////////////////////////////////
    // waterfall(s)
    
    localparam L2WF = clog2(WF_CHANS) - 1;
    reg [L2WF:0] wf_channel_C;
	wire [WF_CHANS-1:0] wfn_sel_C = 1 << wf_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_wf_chan_C) wf_channel_C <= tos[L2WF:0];
    end
    
	wire [WF_CHANS*16-1:0] wfn_dout_C;
	MUX #(.WIDTH(16), .SEL(WF_CHANS)) wf_dout_mux(.in(wfn_dout_C), .sel(wf_channel_C), .out(wf_dout_C));

	wire rst_wf_sampler_C =	wrReg2 && op[WF_SAMPLER_RST];
	wire get_wf_samp_i_C =	wrEvt2 && op[GET_WF_SAMP_I];
	wire get_wf_samp_q_C =	wrEvt2 && op[GET_WF_SAMP_Q];
	assign wf_rd_C =		get_wf_samp_i_C || get_wf_samp_q_C;

`ifdef USE_WF_1CIC
	WATERFALL_1CIC #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst [WF_CHANS-1:0] (
`else
	WATERFALL #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst [WF_CHANS-1:0] (
`endif
		.adc_clk			(adc_clk),
		.adc_data			(rx_data),
		
		.wf_sel_C			(wfn_sel_C),
		.wf_dout_C			(wfn_dout_C),

		.cpu_clk			(cpu_clk),
		.tos				(tos),
		.freeze_tos			(freeze_tos_A),

		.set_wf_freq_C		(set_wf_freq_C),
		.set_wf_decim_C		(set_wf_decim_C),
		.rst_wf_sampler_C	(rst_wf_sampler_C),
		.get_wf_samp_i_C	(get_wf_samp_i_C),
		.get_wf_samp_q_C	(get_wf_samp_q_C)
	);

endmodule
