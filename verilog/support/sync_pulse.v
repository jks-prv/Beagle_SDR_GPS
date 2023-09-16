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

// Copyright (c) 2014 John Seamons, ZL4VO/KF6VO

// Synchronize a single pulse (flag) across a clock domain.
// from: fpga4fun.com/CrossClockDomain2.html

module SYNC_PULSE (
	input  wire in_clk,
	input  wire in,
	input  wire out_clk,
	output wire out
	);

	reg in_level;
	
	// create level from pulse
	// NB: only works if 'in' is active for one in_clk period. 
	always @ (posedge in_clk)
		in_level <= in_level ^ in;
	
	wire [1:0] in_sync_out;
	
	// synchronize level
	SYNC_WIRE #(.NOUT(2)) sync_inst (.in(in_level), .out_clk(out_clk), .out(in_sync_out));

	// re-create pulse in new clock domain
	assign out = in_sync_out[1] ^ in_sync_out[0];
	
endmodule
