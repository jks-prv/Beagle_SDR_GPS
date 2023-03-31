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

// Copyright (c) 2023 John Seamons, ZL/KF6VO

module fir_iq (
	input  wire     adc_clk,
	input  wire     reset,
	input  wire     use_FIR_A,
	input  wire     in_strobe,
	output  reg     out_strobe,
	input  wire signed [WIDTH_:0] in_data_i,
	input  wire signed [WIDTH_:0] in_data_q,
	output  reg signed [WIDTH_:0] out_data_i,
	output  reg signed [WIDTH_:0] out_data_q
    );

`include "kiwi.gen.vh"

    // nominally ACCW(42) = WIDTH(24) + COEFF(18)
	parameter  WIDTH  = "required";
    localparam WIDTH_ = WIDTH - 1;
    localparam COEFF = 18;
    localparam COEFF_ = COEFF - 1;
  //localparam ACCW = WIDTH + COEFF;    // 42 =    24|18    uses 4 DSPs! why?
    localparam ACCW = 64;               // 64 = 22|24|18    uses 2 DSPs
  //localparam ACCW = 48;               // 48 =  6|24|18    uses 4 DSPs
    localparam ACCW_ = ACCW - 1;
    localparam ACCOUT_ = WIDTH + COEFF - 1;
    localparam NTAPS = 17;
    
    reg signed  [ACCW_:0]  accI, accQ;
    reg signed  [WIDTH_:0] bufI[0:NTAPS-1], bufQ[0:NTAPS-1];
    wire signed [COEFF_:0] taps[0:NTAPS-1];
    
    // NB: needs to be a SystemVerilog (.sv) file due to use of COEFF'()
    assign taps[0]  = COEFF'('sh3fffd);
    assign taps[1]  = COEFF'('sh00055);
    assign taps[2]  = COEFF'('sh3ff7f);
    assign taps[3]  = COEFF'('sh3fc56);
    assign taps[4]  = COEFF'('sh00c93);
    assign taps[5]  = COEFF'('sh3fb60);
    assign taps[6]  = COEFF'('sh3cd96);
    assign taps[7]  = COEFF'('sh087ef);
    assign taps[8]  = COEFF'('sh14cc1);
    assign taps[9]  = COEFF'('sh087ef);
    assign taps[10] = COEFF'('sh3cd96);
    assign taps[11] = COEFF'('sh3fb60);
    assign taps[12] = COEFF'('sh00c93);
    assign taps[13] = COEFF'('sh3fc56);
    assign taps[14] = COEFF'('sh3ff7f);
    assign taps[15] = COEFF'('sh00055);
    assign taps[16] = COEFF'('sh3fffd);
    
    reg decim_by_2;
    reg [4:0] tap, copy;
    always @ (posedge adc_clk)
    begin
    if (use_FIR_A)
    begin
        if (reset)
        begin
            tap <= 0;
            copy <= 0;
            decim_by_2 <= 0;
            out_strobe <= 0;
        end

        if (in_strobe)
        begin
            accI <= bufI[0] * taps[0];
            accQ <= bufQ[0] * taps[0];
            tap <= 1;
            copy <= NTAPS - 1;
            out_strobe <= 0;
        end else
        begin
            if (tap < NTAPS)
            begin
                accI <= accI + (bufI[tap] * taps[tap]);
                accQ <= accQ + (bufQ[tap] * taps[tap]);
                tap <= tap + 1'b1;
                out_strobe <= 0;
            end else
            if (tap == NTAPS)
            begin
                if (copy > 0)
                begin
                    bufI[copy] <= bufI[copy-1];
                    bufQ[copy] <= bufQ[copy-1];
                    copy <= copy - 1'b1;
                    out_strobe <= 0;
                end else
                begin
                    bufI[0] <= in_data_i;
                    bufQ[0] <= in_data_q;
                    if (decim_by_2 == 0)
                    //if (1)
                    begin
                        out_data_i <= accI[ACCOUT_ -:WIDTH];
                        out_data_q <= accQ[ACCOUT_ -:WIDTH];
                        out_strobe <= 1;
                    end
                    decim_by_2 <= decim_by_2 ^ 1'b1;
                    tap <= tap + 1'b1;
                end
            end else
                // wait for next in_strobe
                out_strobe <= 0;
        end
    end else
    begin
        if (in_strobe)
        begin
            out_data_i <= in_data_i;
            out_data_q <= in_data_q;
            out_strobe <= 1;
        end else
        begin
            out_strobe <= 0;
        end
    end
    end

endmodule
