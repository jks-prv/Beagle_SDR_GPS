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

// 64K bit buffer and serial-write to 16-bit parallel-read converter
// uses 4 BRAM16s

module SAMPLER (
    input  wire clk,
    input  wire rst,
    input  wire din,
    input  wire rd,
    output wire [15:0] dout);
        
    reg [15:0] addra;
    reg [11:0] addrb;
    reg        full;
    
    wire [3:0] slice = 1'b1 << addra[3:2];

    ipcore_bram_gps_16k_1b_4k_4b sampler_ram [3:0] (
        .clka   (clk),								.clkb   (clk),
        .addra  ({ 4{addra[15:4], addra[1:0]} }),	.addrb  (addrb + rd),
        .dina	(din),								.doutb	(dout),
        .ena    (slice),
        .wea    (~full)
    );

    always @ (posedge clk)
        if (rst)
            {addra, addrb, full} <= 0;
        else begin
            if (~full) {full, addra} <= addra + 1;
            addrb <= addrb + rd;
         end
         
endmodule
