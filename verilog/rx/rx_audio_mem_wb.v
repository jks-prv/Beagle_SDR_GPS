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
	
`timescale 10ns / 10ns

module rx_audio_mem_wb (
	input wire		   adc_clk,
    input  wire [15:0] nrx_samps,
	input  wire		   rx_avail_A,
	input  wire		   rx_avail_wb_A,
    input  wire [15:0] rx_din_A,
    input  wire [47:0] ticks_A,
    output wire        ser,
    output reg         rd_getI,
    output reg         rd_getQ,
    output reg         rd_getWB,
    output wire [ 2:0] didx_o,
    output wire [15:0] waddr_o,
    output wire [15:0] count_o,
    output wire        debug,

	input  wire		   cpu_clk,
	input  wire		   get_rx_srq_C,
	input  wire		   get_rx_samp_C,
	input  wire		   reset_bufs_C,
	input  wire		   get_buf_ctr_C,
    output wire        rx_rd_C,
    output wire [15:0] rx_dout_C
	);
	
`include "kiwi.gen.vh"

    assign debug = use_ts;

    reg [2:0] didx;
    reg [1:0] done;
    reg [15:0] count;
	reg inc_A, wr, use_ts, use_ctr;
	reg transfer;
	reg [1:0] move;
	reg [1:0] tsel;

`ifdef SYNTHESIS
	wire reset = reset_bufs_A;
`else
	wire reset = reset_bufs_C;		// simulator doesn't simulate our "SYNC_PULSE sync_reset_bufs"
    reg debug_1, debug_2, debug_3;
`endif
	
	always @(posedge adc_clk)
	begin
		if (reset)      // reset state machine
		begin
			transfer <= 0;
			count <= 0;
			rd_getI <= 0;
			rd_getQ <= 0;
            rd_getWB <= 0;
			move <= 0;
			wr <= 0;
			done <= 0;
			didx <= 0;
			inc_A <= 0;
			use_ts <= 0;
            use_ctr <= 0;
            tsel <= 0;
		end
		else
		
		if (rx_avail_wb_A)
		begin
			done <= 1;      // transfer wb[0..N-1]
		    transfer <= 1;
		end
		
		if (transfer)
		begin
			if (done == 0)
			begin
				if ((count == nrx_samps) && !use_ts)    // keep going after last count and move ticks
				begin
					move <= 1;      // this state starts first move, below moves second and third
					wr <= 1;
					done <= 1;      // ticks is only 1 channels worth of data (3w)
					use_ts <= 1;
				    tsel <= 0;
`ifndef SYNTHESIS
debug_1 <= 1;
`endif
				end
				else
				
				if ((count == (nrx_samps+1)) && use_ts)     // keep going after last count and move buffer count
				begin
					wr <= 1;
					done <= 1;
					move <= 3;
					count <= count + 1;     // ensures only single word moved
					use_ts <= 0;
					use_ctr <= 1;           // move single counter word
`ifndef SYNTHESIS
debug_2 <= 1;
`endif
				end
				else
				
				if (count == (nrx_samps+3))
				begin   // all done, increment buffer count and reset
					move <= 0;
					wr <= 0;
					done <= 0;
					transfer <= 0;  // stop until next transfer available
					inc_A <= 1;
					count <= 0;
					use_ts <= 0;
					use_ctr <= 0;
                    tsel <= 0;
`ifndef SYNTHESIS
debug_3 <= 1;
`endif
				end
				else
				begin   // count = 0 .. nrx_samps, stop string of channel data writes until next transfer
					transfer <= 0;
					move <= 0;
					wr <= 0;
					done <= 0;
					inc_A <= 0;
				end
				
				rd_getI <= 0;
				rd_getQ <= 0;
                rd_getWB <= 0;
			end
			else
			begin
				// start a sequential string of iq3 * nrx_samps data writes
				case (move)
					0: begin rd_getI <= 1; rd_getQ <= 0; wr <= 1; move <= 1; tsel <= 0; end
					1: begin rd_getI <= 0; rd_getQ <= 1; wr <= 1; move <= 2; tsel <= 1; end
					2: begin rd_getI <= 0; rd_getQ <= 0; wr <= 1; move <= 3; tsel <= 2; end
					3: begin rd_getI <= 0; rd_getQ <= 0; wr <= 0; move <= 0; tsel <= 0;
					         done <= done - 1; didx <= didx + 1; count <= count + 1; end
				endcase
				inc_A <= 0;
				rd_getWB <= !use_ts && !use_ctr;
			end
		end
		else
		begin
		    // idle when no transfer
			rd_getI <= 0;
			rd_getQ <= 0;
            rd_getWB <= 0;
			move <= 0;
			wr <= 0;
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

	reg  [15:0] buf_ctr;
	wire [15:0] buf_ctr_C;

    // continuously sync buf_ctr => buf_ctr_C
	SYNC_REG #(.WIDTH(16)) sync_buf_ctr (
	    .in_strobe(1'b1),   .in_reg(buf_ctr),       .in_clk(adc_clk),
	    .out_strobe(),      .out_reg(buf_ctr_C),    .out_clk(cpu_clk)
	);

	always @ (posedge adc_clk)
		if (reset)
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
	
    assign didx_o = didx;
    //assign count_o = count;
    //assign waddr_o = waddr;
    assign count_o = count;
    assign waddr_o = nrx_samps;
	
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
