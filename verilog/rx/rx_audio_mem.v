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

// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO


//////////////////////////////////////////////////////////////////////////
// rx audio shared sample memory
// when the DDC samples are available, all the receiver outputs are interleaved into a common buffer
//////////////////////////////////////////////////////////////////////////
	
module rx_audio_mem (
	input wire		   adc_clk,
    input  wire [ 7:0] nrx_samps,
	input  wire		   rx_avail_A,
    input  wire [V_RX_CHANS*16-1:0] rxn_din_A,
    input  wire [47:0] ticks_A,
    output wire        ser,
    output reg         rd_getI,
    output reg         rd_getQ,

	input  wire		   cpu_clk,
	input  wire		   get_rx_srq_C,
	input  wire		   get_rx_samp_C,
	input  wire		   reset_bufs_C,
	input  wire		   get_buf_ctr_C,
    output wire        rx_rd_C,
    output wire [15:0] rx_dout_C
	);
	
`include "kiwi.gen.vh"

    localparam L2RX = max(1, clog2(V_RX_CHANS) - 1);

    reg [L2RX+1:0] rxn;		// careful: needs to hold V_RX_CHANS for the "rxn == V_RX_CHANS" test, not V_RX_CHANS-1
    reg [L2RX:0] rxn_d;
    reg [7:0] count;
	reg inc_A, wr, use_ts, use_ctr;
	reg transfer;
	reg [1:0] move;
	reg [1:0] tsel;
	
    /*
    
    // C version of Verilog state machine below
    
    while (adc_clk) {
    	if (reset_bufs_A) {	// reset state machine
    		transfer = count = rd_getI = rd_getQ = move = wr = rxn = inc_A = use_ts = use_ctr = tsel = 0;
    	} else
    	
		if (rx_avail_A) transfer = 1;	// happens at audio rate (e.g. 12 kHz, 83.3 us, => 5555 ADC clk ticks available)
		
		// state machine timing:
		// for the case of nrx_samps = 170, RX_CHANS = 4
    	// count iq3 rxn
    	// ----- --- ---
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
    	// cnt169 iq3 rx3	170*4*3 = 2040w moved
    	// (don't stop transfer)
    	// cnt170 ticks     +3w = 2043w moved
    	// (don't stop transfer)
    	// cnt170 w_ctr     +1w = 2044w moved
    	// -stop transfer-
    	// -inc buffer count-
    	// -srq e_cpu-
    	
		//  another way of looking at the state machine timing:
		//  for the case of nrx_samps = 170, RX_CHANS = 4
		//  count:  0                   1           168(nrx-2)          169(nrx-1)            170     0
		//  rxn:    rx0 rx1 rx2 rx3 rx4 rx0 ... rx4 rx0 rx1 rx2 rx3 rx4 rx0 rx1 rx2 rx3 rx4|3 rx3 rx4 rx0
		//  iq3:    iq3 iq3 iq3 iq3     iq3 ...     iq3 iq3 iq3 iq3     iq3 iq3 iq3 iq3 XYZ   xxC
		//  evts:  AT               S  AT       S  AT               S  AT               -         S  AT
		//                                                              L
		//  A: rx_avail_A
		//  T: transfer=1
		//  S(stop): transfer=0
		//  -: note no stop
		//  L: ticks latch, the last rx_avail_A latching ticks_A before it's copied to buffer.
		//  XYZ: ticks_A
		//  C: buf_ctr
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
                rd_getI = rd_getQ = 0;
            } else {
                // step through all channels: rxn = 0..V_RX_CHANS-1
                switch (move) {		// move i, q, iq3 on each channel
                    case 0: rd_getI = 1; rd_getQ = 0; move = 1; tsel = 0; break;
                    case 1: rd_getI = 0; rd_getQ = 1; move = 2; tsel = 1; break;
                    case 2: rd_getI = 0; rd_getQ = 0; move = 0; tsel = 2; rxn++; break;
                    case 3: rd_getI = 0; rd_getQ = 0; move = 0; tsel = 0; break;	// unused
                }
                wr = 1;     // start a sequential string of iq3 * rxn channel data writes
                inc_A = 0;
            }
        } else {
            rd_getI = rd_getQ = move = wr = rxn = inc_A = use_ts = use_ctr = tsel = 0;       // idle when no transfer
        }
    
    */
    
	always @(posedge adc_clk)
	begin
		rxn_d <= rxn[L2RX:0];	// mux selector needs to be delayed 1 clock
		
		if (reset_bufs_A)       // reset state machine
		begin
			transfer <= 0;
			count <= 0;
			rd_getI <= 0;
			rd_getQ <= 0;
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
				rd_getI <= 0;
				rd_getQ <= 0;
			end
			else
			begin
                // step through all channels: rxn = 0..V_RX_CHANS-1
				case (move)
					0: begin rd_getI <= 1; rd_getQ <= 0;					move <= 1; tsel <= 0; end
					1: begin rd_getI <= 0; rd_getQ <= 1;					move <= 2; tsel <= 1; end
					2: begin rd_getI <= 0; rd_getQ <= 0; rxn <= rxn + 1;	move <= 0; tsel <= 2; end
					3: begin rd_getI <= 0; rd_getQ <= 0;					move <= 0; tsel <= 0; end
				endcase
				wr <= 1;    // start a sequential string of iq3 * rxn channel data writes
				inc_A <= 0;
			end
		end
		else
		begin
		    // idle when no transfer
			rd_getI <= 0;
			rd_getQ <= 0;
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
	
    reg srq_noted, srq_out;
    always @ (posedge cpu_clk)
    begin
        if (get_rx_srq_C) srq_noted <= inc_C;
        else			  srq_noted <= inc_C | srq_noted;
        if (get_rx_srq_C) srq_out   <= srq_noted;
    end

	assign ser = srq_out;

	wire [15:0] rx_din_A;
	MUX #(.WIDTH(16), .SEL(V_RX_CHANS)) rx_mux(.in(rxn_din_A), .sel(rxn_d[L2RX:0]), .out(rx_din_A));
	
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
	
	wire [15:0] din =
	    use_ts?
	        ( (tsel == 0)? ticks_A[15 -:16] : ( (tsel == 1)? ticks_A[31 -:16] : ticks_A[47 -:16]) ) :
	        ( use_ctr? buf_ctr : rx_din_A );
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

endmodule
