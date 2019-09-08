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

// Copyright (c) 2019 John Seamons, ZL/KF6VO

`include "kiwi.vh"

module RX_BUFFER (
	input  wire clka,
	input  wire [ADDR_MSB:0] addra,
	input  wire [15:0] dina,
	input  wire wea,
	
	input  wire clkb,
	input  wire [ADDR_MSB:0] addrb,
	output wire [15:0] doutb
	);

	parameter ADDR_MSB = "required";

	generate
		if (RXBUF_LARGE == 0)
		begin
	        ipcore_bram_8k_16b rx_buf (
                .clka	(clka),         .clkb	(clkb),
                .addra	(addra),        .addrb	(addrb),
                .dina	(dina),         .doutb	(doutb),
                .wea	(wea)
            );
		end
		else
	        ipcore_bram_16k_16b rx_buf (
                .clka	(clka),         .clkb	(clkb),
                .addra	(addra),        .addrb	(addrb),
                .dina	(dina),         .doutb	(doutb),
                .wea	(wea)
            );
		begin
		end
	endgenerate

endmodule
