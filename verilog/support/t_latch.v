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

// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

// transparent latch
//
// Needed to avoid the Vivado "inferring latch" warning.

module t_latch (
	input  wire clk,
	input  wire en,
	input  wire d,
	output reg  q
	);
	
	// mux
    always @*
        q = en? d : latch;
    
    reg latch;
    always @ (posedge clk)
        if (en)
            latch <= d;

endmodule
