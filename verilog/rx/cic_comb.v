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

//------------------------------------------------------------------------------
//           Copyright (c) 2008 Alex Shovkoplyas, VE3NEA
//------------------------------------------------------------------------------

module cic_comb (
	input wire clock,
	input wire reset,
	input wire strobe,
	input wire signed [WIDTH-1:0] in_data,
	output reg signed [WIDTH-1:0] out_data
	);

parameter WIDTH = "required";

reg signed [WIDTH-1:0] prev_data;
initial prev_data = 0;

always @(posedge clock)  
begin
	if (reset) 
		begin
		out_data <= {WIDTH{1'b0}};
		prev_data <= {WIDTH{1'b0}};
		end
	else
	if (strobe) 
		begin
		out_data <= in_data - prev_data;
		prev_data <= in_data;
		end
end

endmodule
