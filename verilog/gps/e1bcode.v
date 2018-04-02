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

`include "kiwi.vh"

`default_nettype none

// 4k bit buffer and 16-bit parallel-write to serial-read converter
// uses 1/4 BRAM16

module E1BCODE (
    input  wire rst,
    input  wire clk,

    input  wire wrReg,
    input  wire [15:0] op,
    input  wire [31:0] tos,

    input  wire [E1B_CODEBITS-1:0] nchip,
    output wire code
);

    wire wr = wrReg && op[SET_E1B_CODE];

    reg [7:0] waddr;    // 4k/16 = 256
	reg [E1B_CODEBITS-1:0] raddr;

    always @ (posedge clk)
        if (rst)
            waddr <= 0;
        else begin
            waddr <= waddr + wr;
            raddr <= (nchip == (E1B_CODELEN-1))? 0 : (nchip+1);     // BRAM read delay
        end

	ipcore_bram_256_16b_4k_1b e1b_code (
		.clka	(clk),          .clkb	(clk),
		.addra	(waddr),        .addrb	(raddr),
		.dina	(tos[15:0]),    .doutb	(code),
		.wea	(wr)
	);
	
endmodule
