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

// Copyright (c) 2014-2023 John Seamons, ZL/KF6VO

module RECEIVER (
	input wire		   adc_clk,
	input wire signed [ADC_BITS-1:0] adc_data,
	input wire         adc_ovfl,

    output wire        rx_rd_C,
    output wire [15:0] rx_dout_C,

    output wire        wf_rd_C,
    output wire [15:0] wf_dout_C,

    input  wire [47:0] ticks_A,
    output wire        adc_ovfl_C,
    output wire [31:0] adc_count_C,
    
	input  wire		   cpu_clk,
    output wire        ser,
    input  wire [31:0] tos,
    input  wire [10:0] op_11,
    input  wire        rdReg,
    input  wire        wrReg,
    input  wire        wrReg2,
    input  wire        wrEvt2,
    
    input  wire        use_gen_C,
    input  wire        gen_fir_C
	);
	
`include "kiwi.gen.vh"

	wire set_rx_chan_C =	wrReg2 & op_11[SET_RX_CHAN];
	wire freq_l =           wrReg2 & op_11[FREQ_L];
	wire set_rx_freqH_C =   (wrReg2 & op_11[SET_RX_FREQ]) && !freq_l;
	wire set_rx_freqL_C =   (wrReg2 & op_11[SET_RX_FREQ]) &&  freq_l;
	wire set_rx_nsamps_C =	wrReg2 & op_11[SET_RX_NSAMPS];
	
	wire set_wf_chan_C =	wrReg2 & op_11[SET_WF_CHAN];
	wire set_wf_freqH_C =   (wrReg2 & op_11[SET_WF_FREQ]) && !freq_l;
	wire set_wf_freqL_C =   (wrReg2 & op_11[SET_WF_FREQ]) &&  freq_l;
	wire set_wf_decim_C =	wrReg2 & op_11[SET_WF_DECIM];

	// The FREEZE_TOS event starts the process of latching and synchronizing of the ecpu TOS data
	// from the cpu_clk to the adc_clk domain. This is needed by subsequent wrReg instructions
	// that want to load TOS values into registers clocked in the adc_clk domain.
	//
	// e.g. an ecpu code sequence like this:
	//		rdReg HOST_RX; wrEvt2 FREEZE_TOS; nop; nop; wrReg2 SET_WF_FREQ

	wire freeze_C = wrEvt2 & op_11[FREEZE_TOS];

	wire [31:0] freeze_tos_A;
	SYNC_REG #(.WIDTH(32)) sync_latch_tos (
	    .in_strobe(freeze_C),   .in_reg(tos),               .in_clk(cpu_clk),
	    .out_strobe(),          .out_reg(freeze_tos_A),     .out_clk(adc_clk)
	);


    //////////////////////////////////////////////////////////////////////////
    // ADC overflow detection
    //////////////////////////////////////////////////////////////////////////
    
    // signal overflow only if a variable value of 64k consecutive samples have ADC overflow asserted
    localparam ADC_OVFL_CTR_BITS = 16;
	reg  [ADC_OVFL_CTR_BITS-1:0] adc_ovfl_ctr, adc_ovfl_cnt, cnt_mask_A;
    reg adc_ovfl_A;

	wire set_cnt_mask_C = wrReg2 & op_11[SET_CNT_MASK];
	wire set_cnt_mask_A;
	SYNC_PULSE sync_set_cnt_mask (.in_clk(cpu_clk), .in(set_cnt_mask_C), .out_clk(adc_clk), .out(set_cnt_mask_A));
    always @ (posedge adc_clk)
        if (set_cnt_mask_A) cnt_mask_A <= freeze_tos_A[ADC_OVFL_CTR_BITS-1:0];

    always @ (posedge adc_clk)
    begin
        if (adc_ovfl_ctr == ((1 << ADC_OVFL_CTR_BITS) - 1))
        begin
            adc_ovfl_A <= ((adc_ovfl_cnt & cnt_mask_A) != 0)? 1:0;
            adc_ovfl_cnt <= 0;
            adc_ovfl_ctr <= 0;
        end else
        begin
            adc_ovfl_A = 0;
            adc_ovfl_cnt <= adc_ovfl_cnt + adc_ovfl;
            adc_ovfl_ctr <= adc_ovfl_ctr + 1;
        end
    end

	SYNC_PULSE sync_adc_ovfl (.in_clk(adc_clk), .in(adc_ovfl_A), .out_clk(cpu_clk), .out(adc_ovfl_C));


    //////////////////////////////////////////////////////////////////////////
    // ADC level detection
    //////////////////////////////////////////////////////////////////////////

    wire [ADC_BITS-1:0] adc_data_abs = adc_data[ADC_BITS-1]? (-adc_data) : adc_data;
    reg [ADC_BITS-1:0] adc_level_A;
	wire set_adc_lvl_C = wrReg & op_11[SET_ADC_LVL];
    wire set_adc_lvl_A;
	SYNC_PULSE sync_set_adc_lvl (.in_clk(cpu_clk), .in(set_adc_lvl_C), .out_clk(adc_clk), .out(set_adc_lvl_A));
    always @ (posedge adc_clk)
        if (set_adc_lvl_A) adc_level_A <= freeze_tos_A[ADC_BITS-1:0];

    reg [31:0] adc_count_A;
    always @ (posedge adc_clk)
    begin
        if (!set_adc_lvl_A && (adc_data_abs >= adc_level_A)) adc_count_A <= adc_count_A + 1'b1;
        //if (!set_adc_lvl_A) adc_count_A <= adc_level_A;     // loopback test
        else
        if (set_adc_lvl_A) adc_count_A <= 0;
    end

    // continuously sync adc_count_A => adc_count_C
	SYNC_REG #(.WIDTH(32)) sync_adc_count (
	    .in_strobe(1),      .in_reg(adc_count_A),       .in_clk(adc_clk),
	    .out_strobe(),      .out_reg(adc_count_C),      .out_clk(cpu_clk)
	);


    //////////////////////////////////////////////////////////////////////////
    // optional signal GEN
    //////////////////////////////////////////////////////////////////////////
    
`ifdef USE_GEN
    `ifdef USE_WF
        localparam RX_IN_WIDTH = 18;

        wire use_gen_A;
        SYNC_WIRE sync_use_gen (.in(use_gen_C), .out_clk(adc_clk), .out(use_gen_A));

        wire signed [RX_IN_WIDTH-1:0] gen_data;
        wire signed [RX_IN_WIDTH-1:0] adc_ext_data = { adc_data, {RX_IN_WIDTH-ADC_BITS{1'b0}} };
    
        // only allow gen to be used on channel 0 to prevent disruption to others when multiple channels in use
        wire [(V_RX_CHANS * RX_IN_WIDTH)-1:0] rx_data = { {V_RX_CHANS-1{adc_ext_data}}, use_gen_A? gen_data : adc_ext_data };
        wire [(V_WF_CHANS * RX_IN_WIDTH)-1:0] wf_data = { {V_WF_CHANS-1{adc_ext_data}}, use_gen_A? gen_data : adc_ext_data };

        wire set_gen_freqH_C =  (wrReg2 & op_11[SET_GEN_FREQ]) && !freq_l;
        wire set_gen_freqL_C =  (wrReg2 & op_11[SET_GEN_FREQ]) &&  freq_l;
        wire set_gen_attn_C =	wrReg2 & op_11[SET_GEN_ATTN];

        GEN gen_inst (
            .adc_clk	        (adc_clk),
            .gen_data	        (gen_data),

            .cpu_clk	        (cpu_clk),
            .freeze_tos_A       (freeze_tos_A),

            .set_gen_freqH_C    (set_gen_freqH_C),
            .set_gen_freqL_C    (set_gen_freqL_C),
            .set_gen_attn_C     (set_gen_attn_C)
        );
    `else
        // sig gen doesn't fit when RX_CFG == 14
        // and USE_WF is conveniently not defined when RX_CFG == 14 (rx14_wf0 configuration)
        
        localparam RX_IN_WIDTH = ADC_BITS;
    
        wire [RX_IN_WIDTH-1:0] rx_data = adc_data;
        wire [RX_IN_WIDTH-1:0] wf_data = adc_data;

    `endif
`else
	localparam RX_IN_WIDTH = ADC_BITS;
	
	wire [RX_IN_WIDTH-1:0] rx_data = adc_data;
	wire [RX_IN_WIDTH-1:0] wf_data = adc_data;

`endif
	

    //////////////////////////////////////////////////////////////////////////
	// audio channels
    //////////////////////////////////////////////////////////////////////////
	
    localparam L2RX = max(1, clog2(V_RX_CHANS) - 1);
    reg [L2RX:0] rx_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_rx_chan_C) rx_channel_C <= tos[L2RX:0];
    end

	wire [V_RX_CHANS-1:0] rxn_sel_C = 1 << rx_channel_C;

	wire [V_RX_CHANS-1:0] rxn_avail_A;
	wire [V_RX_CHANS*16-1:0] rxn_dout_A;
	
	// Verilog note: if rd_i & rd_q are not declared before use in arrayed module RX below
	// then automatic fanout of single-bit signal to all RX instances doesn't occur and
	// an "undriven" error for rd_* results.
	reg rd_i, rd_q;

    wire use_FIR_A;
    SYNC_WIRE sync_use_FIR (.in(gen_fir_C), .out_clk(adc_clk), .out(use_FIR_A));

	RX #(.IN_WIDTH(RX_IN_WIDTH)) rx_inst [V_RX_CHANS-1:0] (
		.adc_clk		(adc_clk),
		.adc_data		(rx_data),
		
		.rx_sel_C		(rxn_sel_C),
		.use_FIR_A      (use_FIR_A),

		.rd_i			(rd_i),
		.rd_q			(rd_q),
		.rx_dout_A		(rxn_dout_A),
		.rx_avail_A		(rxn_avail_A),

		.cpu_clk		(cpu_clk),
		.freeze_tos_A   (freeze_tos_A),
		
		.set_rx_freqH_C	(set_rx_freqH_C),
		.set_rx_freqL_C	(set_rx_freqL_C)
	);

	
    //////////////////////////////////////////////////////////////////////////
	// shared rx audio sample memory
	// when the DDC samples are available, all the receiver outputs are interleaved into a common buffer
    //////////////////////////////////////////////////////////////////////////
	
	wire rx_avail_A = rxn_avail_A[0];		// all DDCs should signal available at the same time since decimation is the same
	
    reg [L2RX+1:0] rxn;		// careful: needs to hold V_RX_CHANS for the "rxn == V_RX_CHANS" test, not V_RX_CHANS-1
    reg [L2RX:0] rxn_d;
    reg [7:0] count;
	reg inc_A, wr, use_ts, use_ctr;
	reg transfer;
	reg [1:0] move;
	reg [1:0] tsel;
	
	wire set_nsamps;
	SYNC_PULSE sync_set_nsamps (.in_clk(cpu_clk), .in(set_rx_nsamps_C), .out_clk(adc_clk), .out(set_nsamps));
    reg [7:0] nrx_samps;
    always @ (posedge adc_clk)
        if (set_nsamps) nrx_samps <= freeze_tos_A;
    
    /*
    
    // C version of Verilog state machine below
    
    while (adc_clk) {
    	if (reset_bufs_A) {	// reset state machine
    		transfer = count = rd_i = rd_q = move = wr = rxn = inc_A = use_ts = use_ctr = tsel = 0;
    	} else
    	
		if (rx_avail_A) transfer = 1;	// happens at audio rate (e.g. 12 kHz, 83.3 us)
		
    	// count     rxn
    	// cnt00 i   rx0
    	// cnt00 q   rx0
    	// cnt00 iq3 rx0	3w moved
    	// cnt00 i   rx1
    	// cnt00 q   rx1
    	// cnt00 iq3 rx1
    	// cnt00 i   rx2
    	// cnt00 q   rx2
    	// cnt00 iq3 rx2
    	// cnt00 i   rx3
    	// cnt00 q   rx3
    	// cnt00 iq3 rx3	1*4*3 = 12w moved
    	// -stop transfer-
    	// cnt01 ...
    	// -stop transfer-
    	// ...
    	// cnt83 iq3 rx3	84*4*3 = 1008w moved
    	// (don't stop transfer)
    	// cnt84 ticks      +3w = 1011w moved
    	// (don't stop transfer)
    	// cnt84 w_ctr      +1w = 1012w moved
    	// -stop transfer-
    	// -inc buffer count-
    	// -srq e_cpu-
    	
		//  another way of looking at the state machine timing:
		//  with nrx_samps = 84, R_CHANS = 4
		//  count:  0                   1           82(nrx-2)           83(nrx-1)             84      0
		//  rxn:    rx0 rx1 rx2 rx3 rx4 rx0 ... rx4 rx0 rx1 rx2 rx3 rx4 rx0 rx1 rx2 rx3 rx4|3 rx3 rx4 rx0
		//  iq3:    iq3 iq3 iq3 iq3     iq3 ...     iq3 iq3 iq3 iq3     iq3 iq3 iq3 iq3 XYZ   xxC
		//  evts:  AT               S  AT       S  AT               S ALT               -         S  AT
		//  A:rx_avail_A, T:transfer=1, S(stop):transfer=0, -:note_no_stop, L:ticks_latch, XYZ:ticks_A, C:buf_ctr
		//  NB: "rx4" is a pseudo channel number that encodes "rxn == V_RX_CHANS" to signal all channel data moved.
		
		if (transfer) {
            if (rxn == V_RX_CHANS) {      // after moving all channel data
                if (count == (nrx_samps-1) && !use_ts) {       // keep going after last count and move ticks
					move = 1;           // this state starts first move, case below moves second and third
					wr = 1;
                    rxn = V_RX_CHANS-1;	// ticks is only 1 channels worth of data (3w)
                    use_ts = 1;
                    tsel = 0;
                } else
                if (count == (nrx_samps-1) && use_ts) {     // keep going after last count and move buffer count
					wr = 1;
                    count++;            // ensures only single word moved
                    use_ts = 0;
                    use_ctr = 1;        // move single counter word
                } else
                if (count == nrx_samps) {       // all done, increment buffer count and reset
                    move = wr = rxn = count = use_ts = use_ctr = 0;
                    transfer = 0;   // stop until next transfer available
                    inc_A = 1;
                } else {        // count = 0 .. (nrx_samps-2), stop string of channel data writes until next transfer
                    move = wr = rxn = transfer = 0;
                    inc_A = 0;
                    count++;
                }
                rd_i = rd_q = 0;
            } else {
                // step through all channels: rxn = 0..V_RX_CHANS-1
                switch (move) {		// move i, q, iq3 on each channel
                    case 0: rd_i = 1; rd_q = 0; move = 1; tsel = 0; break;
                    case 1: rd_i = 0; rd_q = 1; move = 2; tsel = 1; break;
                    case 2: rd_i = 0; rd_q = 0; move = 0; tsel = 2; rxn++; break;
                    case 3: rd_i = 0; rd_q = 0; move = 0; tsel = 0; break;	// unused
                }
                wr = 1;     // start a sequential string of iq3 * rxn channel data writes
                inc_A = 0;
            }
        } else {
            rd_i = rd_q = move = wr = rxn = inc_A = use_ts = use_ctr = tsel = 0;       // idle when no transfer
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
            tsel <= 0;
		end
		else
		if (rx_avail_A) transfer <= 1;      // happens at audio rate (e.g. 12 kHz, 83.3 us)
		
		if (transfer)
		begin
			if (rxn == V_RX_CHANS)    // after moving all channel data
			begin
				if ((count == (nrx_samps-1)) && !use_ts)    // keep going after last count and move ticks
				begin
					move <= 1;          // this state starts first move, below moves second and third
					wr <= 1;
					rxn <= V_RX_CHANS-1;  // ticks is only 1 channels worth of data (3w)
					use_ts <= 1;
				    tsel <= 0;
				end
				else
				if ((count == (nrx_samps-1)) && use_ts)     // keep going after last count and move buffer count
				begin
					wr <= 1;
					count <= count + 1;     // ensures only single word moved
					use_ts <= 0;
					use_ctr <= 1;           // move single counter word
				end
				else
				if (count == nrx_samps)
				begin   // all done, increment buffer count and reset
					move <= 0;
					wr <= 0;
					rxn <= 0;
					transfer <= 0;  // stop until next transfer available
					inc_A <= 1;
					count <= 0;
					use_ts <= 0;
					use_ctr <= 0;
                    tsel <= 0;
				end
				else
				begin   // count = 0 .. (nrx_samps-2), stop string of channel data writes until next transfer
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
                // step through all channels: rxn = 0..V_RX_CHANS-1
				case (move)
					0: begin rd_i <= 1; rd_q <= 0;					move <= 1; tsel <= 0; end
					1: begin rd_i <= 0; rd_q <= 1;					move <= 2; tsel <= 1; end
					2: begin rd_i <= 0; rd_q <= 0; rxn <= rxn + 1;	move <= 0; tsel <= 2; end
					3: begin rd_i <= 0; rd_q <= 0;					move <= 0; tsel <= 0; end
				endcase
				wr <= 1;    // start a sequential string of iq3 * rxn channel data writes
				inc_A <= 0;
			end
		end
		else
		begin
		    // idle when no transfer
			rd_i <= 0;
			rd_q <= 0;
			move <= 0;
			wr <= 0;
			rxn <= 0;
			inc_A <= 0;
			use_ts <= 0;
            use_ctr <= 0;
            tsel <= 0;
		end
	end

	wire inc_C;
	SYNC_PULSE sync_inc_C (.in_clk(adc_clk), .in(inc_A), .out_clk(cpu_clk), .out(inc_C));
    wire get_rx_srq = rdReg & op_11[GET_RX_SRQ];
	
    reg srq_noted, srq_out;
    always @ (posedge cpu_clk)
    begin
        if (get_rx_srq) srq_noted <= inc_C;
        else			srq_noted <= inc_C | srq_noted;
        if (get_rx_srq) srq_out   <= srq_noted;
    end

	assign ser = srq_out;

	wire get_rx_samp_C = wrEvt2 & op_11[GET_RX_SAMP];
	wire reset_bufs_C =  wrEvt2 & op_11[RX_BUFFER_RST];
	wire get_buf_ctr_C = wrEvt2 & op_11[RX_GET_BUF_CTR];
	
	wire [15:0] rx_dout_A;
	MUX #(.WIDTH(16), .SEL(V_RX_CHANS)) rx_mux(.in(rxn_dout_A), .sel(rxn_d[L2RX:0]), .out(rx_dout_A));
	
	reg  [15:0] buf_ctr;	
	wire [15:0] buf_ctr_C;

    // continuously sync buf_ctr => buf_ctr_C
	SYNC_REG #(.WIDTH(16)) sync_buf_ctr (
	    .in_strobe(1),      .in_reg(buf_ctr),       .in_clk(adc_clk),
	    .out_strobe(),      .out_reg(buf_ctr_C),    .out_clk(cpu_clk)
	);

	always @ (posedge adc_clk)
		if (reset_bufs_A)
		begin
			buf_ctr <= 0;
		end
		else
	    buf_ctr <= buf_ctr + inc_A;

    localparam RXBUF_MSB = clog2(RXBUF_SIZE) - 1;
	reg [RXBUF_MSB:0] waddr, raddr;
	
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
	
	reg [47:0] ticks_latched_A;
	
	always @ (posedge adc_clk)
		if (rx_avail_A)
		    ticks_latched_A <= ticks_A;

	wire [15:0] din =
	    use_ts?
	        ( (tsel == 0)? ticks_latched_A[15 -:16] : ( (tsel == 1)? ticks_latched_A[31 -:16] : ticks_latched_A[47 -:16]) ) :
	        ( use_ctr? buf_ctr : rx_dout_A );
    wire [15:0] dout;

    // Transfer size is 1012 16-bit words to match 2kB limit of SPI transfers,
    // so this 8k x 16b buffer allows writer to get about 8x ahead of reader.
    // Read and write addresses just wrap and are reset at the start.

	RX_BUFFER #(.ADDR_MSB(RXBUF_MSB)) rx_buffer (
		.clka	(adc_clk),      .clkb	(cpu_clk),
		.addra	(waddr),        .addrb	(raddr + rd),
		.dina	(din),          .doutb	(dout),
		.wea	(wr)
	);

	assign rx_rd_C = get_rx_samp_C | get_buf_ctr_C;
	assign rx_dout_C = get_buf_ctr_C? buf_ctr_C : dout;
	
    
    //////////////////////////////////////////////////////////////////////////
    // waterfall(s)
    //////////////////////////////////////////////////////////////////////////

`ifdef USE_WF
    localparam L2WF = max(1, clog2(V_WF_CHANS) - 1);
    reg [L2WF:0] wf_channel_C;
	wire [V_WF_CHANS-1:0] wfn_sel_C = 1 << wf_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_wf_chan_C) wf_channel_C <= tos[L2WF:0];
    end
    
	wire [V_WF_CHANS*16-1:0] wfn_dout_C;
	MUX #(.WIDTH(16), .SEL(V_WF_CHANS)) wf_dout_mux(.in(wfn_dout_C), .sel(wf_channel_C), .out(wf_dout_C));

	wire rst_wf_sampler_C =	wrReg2 & op_11[WF_SAMPLER_RST];
	wire get_wf_samp_i_C =	wrEvt2 & op_11[GET_WF_SAMP_I];
	wire get_wf_samp_q_C =	wrEvt2 & op_11[GET_WF_SAMP_Q];
	assign wf_rd_C =		get_wf_samp_i_C || get_wf_samp_q_C;

	wire samp_wf_rd_rst_C = tos[WF_SAMP_RD_RST];
	wire samp_wf_sync_C   = tos[WF_SAMP_SYNC];

`ifdef USE_WF_1CIC
	WATERFALL_1CIC #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst [V_WF_CHANS-1:0] (
`else
	WATERFALL #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst [V_WF_CHANS-1:0] (
`endif
		.adc_clk			(adc_clk),
		.adc_data			(wf_data),
		
		.wf_sel_C			(wfn_sel_C),
		.wf_dout_C			(wfn_dout_C),

		.cpu_clk			(cpu_clk),
		.freeze_tos_A       (freeze_tos_A),

		.samp_wf_rd_rst_C   (samp_wf_rd_rst_C),
		.samp_wf_sync_C		(samp_wf_sync_C),
		.set_wf_freqH_C		(set_wf_freqH_C),
		.set_wf_freqL_C		(set_wf_freqL_C),
		.set_wf_decim_C		(set_wf_decim_C),
		.rst_wf_sampler_C	(rst_wf_sampler_C),
		.get_wf_samp_i_C	(get_wf_samp_i_C),
		.get_wf_samp_q_C	(get_wf_samp_q_C)
	);
`else
    assign wf_dout_C = 16'b0;
`endif

endmodule
