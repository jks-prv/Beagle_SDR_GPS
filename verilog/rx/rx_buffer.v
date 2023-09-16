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

// Copyright (c) 2019 John Seamons, ZL4VO/KF6VO

module RX_BUFFER (
	input  wire clka,
	input  wire [ADDR_MSB:0] addra,
	input  wire [15:0] dina,
	input  wire wea,
	
	input  wire clkb,
	input  wire [ADDR_MSB:0] addrb,
	output wire [15:0] doutb
	);

`include "kiwi.gen.vh"

	parameter ADDR_MSB = "required";

// When building all configurations sequentially using the verilog/make_proj.tcl script
// the following doesn't work because of problems with the Vivado source code scanner marking the unused
// bram ip block "AutoDisabled" in KiwiSDR.xpr and then not being able to find it subsequently.

`ifdef NOT_DEF
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
`else
    wire [15:0] doutb_8k, doutb_16k, doutb_32k;
    
	assign doutb = (RXBUF_LARGE == 0)? doutb_8k : ((RXBUF_LARGE == 1)? doutb_16k : doutb_32k);

    // All but one of these will be optimized away because RXBUF_LARGE is a constant parameter
    // set in kiwi.gen.vh that depends on RX_CFG
    
    ipcore_bram_8k_16b rx_buf_8k (
        .clka	(clka),         .clkb	(clkb),
        .addra	(addra),        .addrb	(addrb),
        .dina	(dina),         .doutb	(doutb_8k),
        .wea	(wea)
    );

    ipcore_bram_16k_16b rx_buf_16k (
        .clka	(clka),         .clkb	(clkb),
        .addra	(addra),        .addrb	(addrb),
        .dina	(dina),         .doutb	(doutb_16k),
        .wea	(wea)
    );

    ipcore_bram_32k_16b rx_buf_32k (
        .clka	(clka),         .clkb	(clkb),
        .addra	(addra),        .addrb	(addrb),
        .dina	(dina),         .doutb	(doutb_32k),
        .wea	(wea)
    );
`endif

endmodule
