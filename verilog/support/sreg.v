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

// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

// variable length left shift register

module sreg (
	input  wire clk,
	input  wire [WIDTH-1:0] pin,
	input  wire load,
	input  wire sft,
	input  wire sin,
	output wire sout
	);
	
	parameter WIDTH = 16;

	reg [WIDTH-1:0] sr;
	assign sout = sr[WIDTH-1];

    always @ (posedge clk)
        if (load)
            sr <= pin;
        else
        if (sft)
            sr <= { sr[WIDTH-2:0], sin };
	    else
	        sr <= sr;

endmodule
