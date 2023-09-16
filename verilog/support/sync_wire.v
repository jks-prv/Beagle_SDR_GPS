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

// synchronize the signal 'in' in the clock domain 'out_clk' using TNSYNC flip-flops
// NB: as a consequence 'out' signal is delayed a few out_clk clocks
// NSYNC specifies custom synchronizer lengths
// NOUT specifies the output signal width

module SYNC_WIRE #(parameter NOUT=1, parameter NSYNC=2) (
	input  wire in,
	input  wire out_clk,
	output wire [NOUT-1:0] out
	);

	// total number of sync stages is increased by the number of outputs caller uses
	localparam TNSYNC = NSYNC-1 + NOUT-1;
	
	// forums.xilinx.com/t5/Timing-Analysis/Understanding-ASYNC-REG-attribute/m-p/774027#M11817
	(* ASYNC_REG = "TRUE" *) reg [TNSYNC:0] sync;

	generate
		if (TNSYNC == 0)
		begin
			always @ (posedge out_clk)
				sync <= in;
		end
		else
		begin
			always @ (posedge out_clk)
				sync <= {sync[TNSYNC-1:0], in};
		end
	endgenerate
	
	assign out = sync[TNSYNC -:NOUT];

endmodule
