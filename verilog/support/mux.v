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

// useful for WIDTH > 1 cases when Verilog mux notation (wire[sel]) doesn't work

module MUX (
	input  wire [SEL*WIDTH-1:0]	in,
	input  wire [NSEL-1:0]		sel,
	output wire [WIDTH-1:0]		out
	);

`include "kiwi.gen.vh"

	parameter WIDTH  = "required";
	parameter SEL  = "required";
	
	localparam NSEL = clog2(SEL);
	
	wire [SEL*WIDTH-1:0] out_s = (in >> (sel * WIDTH));
	assign out = out_s[WIDTH-1:0];

endmodule
