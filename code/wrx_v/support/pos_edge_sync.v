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

`include "wrx.vh"

module POS_EDGE_SYNC (
	input  wire in,
	input  wire out_clk,
	output reg out
	);

	localparam NREG = NSYNC+2;
	(* ASYNC_REG = "TRUE" *) reg [NREG:0] sync;

	always @ (posedge out_clk)
	begin
		sync <= {sync[NREG-1:0], in};
		out <= (sync[NREG -:2] == 2'b01);		// posedge
	end

endmodule
