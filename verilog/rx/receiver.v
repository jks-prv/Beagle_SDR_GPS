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

    output wire [1:0]  ticks_sel,
    input  wire [15:0] ticks_A,
    
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
	
	
    //////////////////////////////////////////////////////////////////////////
    // optional signal GEN
    //////////////////////////////////////////////////////////////////////////
    
`ifdef USE_GEN
	localparam RX_IN_WIDTH = 18;

	wire use_gen_A;
	SYNC_WIRE sync_use_gen (.in(ctrl[CTRL_USE_GEN]), .out_clk(adc_clk), .out(use_gen_A));

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
	

    //////////////////////////////////////////////////////////////////////////
	// audio channels
    //////////////////////////////////////////////////////////////////////////
	
    localparam L2RX = clog2(RX_CHANS) - 1;
    reg [L2RX:0] rx_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_rx_chan_C) rx_channel_C <= tos[L2RX:0];
    end

	wire [RX_CHANS-1:0] rxn_sel_C = 1 << rx_channel_C;

	wire [RX_CHANS-1:0] rxn_avail_A;
	wire [RX_CHANS*16-1:0] rxn_dout_A;
	
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

	
    //////////////////////////////////////////////////////////////////////////
	// shared rx audio sample memory
	// when the DDC samples are available, all the receiver outputs are interleaved into a common buffer
    //////////////////////////////////////////////////////////////////////////
	
	wire rx_avail_A = rxn_avail_A[0];		// all DDCs should signal available at the same time since decimation is the same
	
    reg [L2RX+1:0] rxn;		// careful: needs to hold RX_CHANS for the "rxn == RX_CHANS" test, not RX_CHANS-1
    reg [L2RX:0] rxn_d;
    reg [6:0] count;
	reg inc_A, wr, use_ts, use_ctr;
	reg transfer;
	reg [1:0] move;
	
	wire set_nsamps;
	SYNC_PULSE sync_set_nsamps (.in_clk(cpu_clk), .in(set_rx_nsamps_C), .out_clk(adc_clk), .out(set_nsamps));
    reg [6:0] nrx_samps;
    always @ (posedge adc_clk)
        if (set_nsamps) nrx_samps <= freeze_tos_A;
    
    /*
    
    // C version of Verilog state machine below
    
    while (adc_clk) {
		if (rx_avail_A) transfer = 1;	// only happens at audio rate (9600 Hz, 104.2 us)
		
    	if (!transfer) {	// reset
    		rd_i = rd_q = move = wr = rxn = inc_A = use_ts = use_ctr = 0;
    		continue;
    	}
    	
    	// cnt00 iq3 rx0	3w moved
    	// cnt00 iq3 rx1
    	// cnt00 iq3 rx2
    	// cnt00 iq3 rx3	1*4*3 = 12w moved
    	// -stop transfer-
    	// ...
    	// cnt84 iq3 rx3	85*4*3 = 1020w moved
    	// (don't stop transfer)
    	// cnt85 iq3 ticks	+3w = 1023w moved
    	// cnt85 iq3 w_ctr	+1w = 1024w moved
    	// -stop transfer-
    	// -inc buffer count-
    	
		if (rx_avail) transfer = 1;
		
		if (transfer) {
            if (rxn == RX_CHANS) {      // after moving all channel data
                if (count == (nrx_samps-1) && !use_ts) {       // keep going after last count and move ticks
					move = 1;           // this state starts first move, case below moves second and third
                    rxn = RX_CHANS-1;	// ticks is only 1 channels worth of data (3w)
                    use_ts = 1;
                } else
                if (count == (nrx_samps-1) && use_ts) {       // keep going after last count and move buffer count
                    count++;
                    use_ts = 0;
                    use_ctr = 1;        // move counter word
                } else
                if (count == nrx_samps) {       // all done, increment buffer count and reset
                    wr = 0;
                    inc_A = 1;
                    count = 0;
                    use_ts = use_ctr = transfer = 0;      // stop until next transfer available
                } else {        // count = 0 .. (nrx_samps-2), stop string of channel data writes until next transfer
                    move = wr = rxn = transfer = 0;
                    inc_A = 0;
                    count++;
                }
                rd_i = rd_q = 0;
            } else {
                // step through all channels: rxn = 0..RX_CHANS-1
                switch (move) {		// move i, q, iq3 on each channel
                    case 0: rd_i = 1; rd_q = 0; move = 1; break;
                    case 1: rd_i = 0; rd_q = 1; move = 2; break;
                    case 2: rd_i = 0; rd_q = 0; move = 0; rxn++; break;
                    case 3: rd_i = 0; rd_q = 0; move = 0; break;	// unused
                }
                wr = 1;     // start a sequential string of iq3 * rxn channel data writes
                inc_A = 0;
            }
        } else {
            rd_i = rd_q = move = wr = rxn = inc_A = use_ts = use_ctr = 0;       // idle when no transfer
        }
    
    */
    
	always @(posedge adc_clk)
	begin
		rxn_d <= rxn[L2RX:0];	// mux selector needs to be delayed 1 clock
		
		if (reset_bufs_A)       // reset state machine
		begin
			transfer <= 0;
			count <= 0;
			rd_i <= 0;
			rd_q <= 0;
			move <= 0;
			wr <= 0;
			rxn <= 0;
			inc_A <= 0;
			use_ts <= 0;
            use_ctr <= 0;
		end
		else
		if (rx_avail_A) transfer <= 1;
		
		if (transfer)
		begin
			if (rxn == RX_CHANS)
			begin
				if ((count == (nrx_samps-1)) && !use_ts)
				begin
					move <= 1;      // this state starts first move, below moves second and third
					wr <= 1;
					rxn <= RX_CHANS-1;
					use_ts <= 1;
				end
				else
				if ((count == (nrx_samps-1)) && use_ts)
				begin
					wr <= 1;
					count <= count + 1;
					use_ts <= 0;
					use_ctr <= 1;       // move counter word
				end
				else
				if (count == nrx_samps)
				begin
					move <= 0;
					wr <= 0;
					rxn <= 0;
					transfer <= 0;
					inc_A <= 1;
					count <= 0;
					use_ts <= 0;
					use_ctr <= 0;
				end
				else
				begin
					move <= 0;
					wr <= 0;
					rxn <= 0;
					transfer <= 0;
					inc_A <= 0;
					count <= count + 1;
				end
				rd_i <= 0;
				rd_q <= 0;
			end
			else
			begin
				case (move)
					0: begin rd_i <= 1; rd_q <= 0;					move <= 1; end
					1: begin rd_i <= 0; rd_q <= 1;					move <= 2; end
					2: begin rd_i <= 0; rd_q <= 0; rxn <= rxn + 1;	move <= 0; end
					3: begin rd_i <= 0; rd_q <= 0;					move <= 0; end
				endcase
				wr <= 1;
				inc_A <= 0;
			end
		end
		else
		begin
			rd_i <= 0;
			rd_q <= 0;
			move <= 0;
			wr <= 0;
			rxn <= 0;
			inc_A <= 0;
			use_ts <= 0;
            use_ctr <= 0;
		end
	end

	wire inc_C;
	SYNC_PULSE sync_inc_C (.in_clk(adc_clk), .in(inc_A), .out_clk(cpu_clk), .out(inc_C));
    wire get_rx_srq = rdReg && op[GET_RX_SRQ];
	
    reg srq_noted, srq_out;
    always @ (posedge cpu_clk)
    begin
        if (get_rx_srq) srq_noted <= inc_C;
        else			srq_noted <= inc_C | srq_noted;
        if (get_rx_srq) srq_out   <= srq_noted;
    end

	assign ser = srq_out;

	wire get_rx_samp_C = wrEvt2 && op[GET_RX_SAMP];
	wire reset_bufs_C =  wrEvt2 && op[RX_BUFFER_RST];
	wire get_buf_ctr_C = wrEvt2 && op[RX_GET_BUF_CTR];
	
	wire [15:0] rx_dout_A;
	MUX #(.WIDTH(16), .SEL(RX_CHANS)) rx_mux(.in(rxn_dout_A), .sel(rxn_d[L2RX:0]), .out(rx_dout_A));
	
	reg [15:0] buf_ctr;
	
	wire [15:0] buf_ctr_C;
	SYNC_WIRE sync_buf_ctr [15:0] (.in(buf_ctr), .out_clk(cpu_clk), .out(buf_ctr_C));

	always @ (posedge adc_clk)
		if (reset_bufs_A)
		begin
			buf_ctr <= 0;
		end
		else
	    buf_ctr <= buf_ctr + inc_A;

	reg [12:0] waddr, raddr;
	
	wire reset_bufs_A;
	SYNC_PULSE sync_reset_bufs (.in_clk(cpu_clk), .in(reset_bufs_C), .out_clk(adc_clk), .out(reset_bufs_A));

	always @ (posedge adc_clk)
		if (reset_bufs_A)
		begin
			waddr <= 0;
		end
		else
		waddr <= waddr + wr;
	
	always @ (posedge cpu_clk)
		if (reset_bufs_C)
		begin
			raddr <= 0;
		end
		else
			raddr <= raddr + rd;

	wire rd = get_rx_samp_C;
	
	assign ticks_sel = move;
	wire [15:0] din = use_ts? ticks_A : (use_ctr? buf_ctr : rx_dout_A);
	//wire [15:0] din = use_ts? 16'hbeef : (use_ctr? buf_ctr : {3'b0, waddr});
	wire [15:0] dout;

	ipcore_bram_8k_16b rx_buf (
		.clka	(adc_clk),							.clkb	(cpu_clk),
		.addra	(waddr),					        .addrb	(raddr + rd),
		.dina	(din),		                        .doutb	(dout),
		.wea	(wr)
	);

	assign rx_rd_C = get_rx_samp_C | get_buf_ctr_C;
	assign rx_dout_C = get_buf_ctr_C? buf_ctr_C : dout;
	
    
    //////////////////////////////////////////////////////////////////////////
    // waterfall(s)
    //////////////////////////////////////////////////////////////////////////

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
