//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

// Copyright (c) 2018-2023 John Seamons, ZL4VO/KF6VO

`default_nettype none

// E1B code memory gadget
// uses 1-BRAM36, 1-BRAM18
//
// The following is extremely subtle.
// Note to future self: see notebook #6 for timing diagrams.

module E1BCODE (
    input  wire rst,
    input  wire clk,

    input  wire wr,
    input  wire [11:0] tos,

    input  wire [(V_GPS_CHANS * E1B_CODEBITS)-1:0] nchip_n,
    input  wire [V_GPS_CHANS-1:0] full_chip,
    output wire [V_GPS_CHANS-1:0] code_o
);

`include "kiwi.gen.vh"

    localparam CH_MSB = clog2(V_GPS_CHANS) - 1;
    reg [CH_MSB:0] ch_p = 0;

    wire [V_GPS_CHANS-1:0] code_t;
    reg code_n [V_GPS_CHANS-1:0];

    // can't get the timing on this mux to work, so do the nchip_n demuxing manually
	//MUX #(.WIDTH(E1B_CODEBITS), .SEL(V_GPS_CHANS)) nchip_mux(.in(nchip_n), .sel(?), .out(nchip));


    // sequential producer

    wire [E1B_CODEBITS-1:0] nchip [V_GPS_CHANS-1:0];
	wire [E1B_CODEBITS-1:0] raddr_m [V_GPS_CHANS-1:0];

    genvar ch_m;
    generate
        for (ch_m = 0; ch_m < V_GPS_CHANS; ch_m = ch_m + 1)
        begin
            assign nchip[ch_m] = nchip_n[(ch_m * E1B_CODEBITS) +:E1B_CODEBITS];
            assign raddr_m[ch_m] = (nchip[(ch_m+2) % V_GPS_CHANS] == (E1B_CODELEN-1))? 0 : (nchip[(ch_m+2) % V_GPS_CHANS]+1);
        end
    endgenerate

    always @ (posedge clk)
    begin
        raddr <= raddr_m[ch_p];
        code_n[ch_p] <= code_t[ch_p];
        ch_p <= (ch_p == (V_GPS_CHANS - 1))? 0 : (ch_p + 1);
    end


    // parallel consumers

    genvar ch_c;
    generate
        for (ch_c = 0; ch_c < V_GPS_CHANS; ch_c = ch_c + 1)
        begin : e1b_code_c
        
            // transparent latch, i.e. uses combi mux to get code_n in clk prior to latched value
            t_latch code_latch(clk, full_chip[ch_c], code_n[ch_c], code_o[ch_c]);
        end
    endgenerate


	reg [E1B_CODEBITS-1:0] waddr, raddr;

    always @ (posedge clk)
        if (rst)
        begin
            waddr <= 0;
        end
        else begin
            waddr <= waddr + wr;
        end

	ipcore_bram_gps_4k_12b e1b_code (
		.clka	(clk),          .clkb	(clk),
		.addra	(waddr),        .addrb	(raddr),
		.dina	(tos[11:0]),    .doutb	(code_t),
		.wea	(wr)
	);

endmodule
