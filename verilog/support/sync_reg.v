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

// Copyright (c) 2019 John Seamons, ZL4VO/KF6VO

// Use the synchronized handshake method of arbitrating a register transfer between two clock domains.
// WIDTH specifies the register width.
//
// See: zipcpu.com/blog/2017/10/20/cdc.html

module SYNC_REG (
	input  wire in_strobe,
	input  wire [WIDTH-1:0] in_reg,
	input  wire in_clk,

	output reg  out_strobe,
	output reg  [WIDTH-1:0] out_reg,
	input  wire out_clk
	);
	
	parameter WIDTH = 1;
	
	// forums.xilinx.com/t5/Timing-Analysis/Understanding-ASYNC-REG-attribute/m-p/774027#M11817
	(* ASYNC_REG = "TRUE" *) reg [WIDTH-1:0] shared_reg;
	reg valid_data;
	
	reg in_req;
	wire in_ack;
	wire busy = (in_req || in_ack);
	
	wire [1:0] out_reqs;
    localparam RISE = 2'b01;
    wire req_posedge = (out_reqs == RISE);
	wire out_req_last = out_reqs[1];

    always @ (posedge in_clk)
        if (!busy && in_strobe)
        begin
            shared_reg <= in_reg;
            valid_data <= 1'b1;
        end else
        if (in_ack)
            valid_data <= 1'b0;

    always @ (posedge in_clk)
        if (!busy && valid_data)
            in_req <= 1'b1;
        else
        if (in_ack)
            in_req <= 1'b0;
    
    // synchronize request from in_clk domain to out_clk domain
	SYNC_WIRE #(.NOUT(2)) req_inst (.in(in_req),       .out_clk(out_clk), .out(out_reqs));
	
	// synchronize ack from out_clk domain to in_clk domain
	SYNC_WIRE             ack_inst (.in(out_req_last), .out_clk( in_clk), .out( in_ack));

    always @ (posedge out_clk)
        if (req_posedge)
        begin
            out_reg <= shared_reg;
            out_strobe <= 1'b1;
        end else
            out_strobe <= 1'b0;

endmodule
