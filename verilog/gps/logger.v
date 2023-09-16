//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

`default_nettype none

// for capturing GPS IQ data

module LOGGER (
    input  wire clk,
    input  wire rst,
    input  wire rd,
    input  wire wr,
    input  wire [15:0] din,
    output wire [15:0] dout);
        
`include "kiwi.gen.vh"

`ifdef USE_LOGGER

    reg [9:0] waddr;
    reg [9:0] raddr;
    reg       full;
    
	ipcore_bram_1k_16b log_buf (
		.clka	(clk),          .clkb	(clk),
		.addra	(waddr),        .addrb	(raddr + rd),
		.dina	(din),          .doutb	(dout),
		.wea	(wr && ~full)
	);

    always @ (posedge clk)
        if (rst)
            {waddr, raddr, full} <= 0;
        else begin
            if (wr && ~full) {full, waddr} <= waddr + 1;
            raddr <= raddr + rd;
        end

`endif
         
endmodule
