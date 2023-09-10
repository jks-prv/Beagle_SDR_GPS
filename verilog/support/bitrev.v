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

// bit reverse a signal

module bitrev (
	input  wire [WIDTH-1:0]	in,
	output wire [WIDTH-1:0] out
	);

	parameter WIDTH = "required";

    genvar i;
    generate
        for (i = 0; i < WIDTH; i = i+1)
        begin: bitrev_c
            assign out[i] = in[WIDTH-1-i];
        end
    endgenerate
    
endmodule
