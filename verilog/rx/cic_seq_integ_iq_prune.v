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

// Copyright (c) 2008 Alex Shovkoplyas, VE3NEA
// Copyright (c) 2013 Phil Harman, VK6APH
// Copyright (c) 2014 John Seamons, ZL/KF6VO


//
// Implements integrator part of muxed sequential logic comb stage.
// Version for an IQ signal with pruned integrator stages.
//
// Fixed differential delay (D) = 1
//

`include "kiwi.vh"

module cic_seq_integ_iq_prune (
	input wire clock,
	input wire reset,

	input wire in_strobe,
	input wire signed [IN_WIDTH-1:0] in_data_i, in_data_q,

	output reg out_strobe_i, out_strobe_q,
	output reg signed [OUT_WIDTH-1:0] out_data_i, out_data_q
	);

    // design parameters
    parameter INCLUDE = "required";
    parameter STAGES = "required";
    parameter DECIMATION = "required";  
    parameter IN_WIDTH = "required";
    parameter GROWTH = "required";
    parameter OUT_WIDTH = "required";
    
    localparam ACC_WIDTH = IN_WIDTH + GROWTH;
	localparam ASSERT_OUT_WIDTH = assert(OUT_WIDTH == ACC_WIDTH, "OUT_WIDTH");
    
    localparam MD = 18;		// assumes excess counter bits get optimized away
    
    reg [MD-1:0] sample_no;
    initial sample_no = {MD{1'b0}};
    reg integ_strobe;
    
    always @(posedge clock)
      if (in_strobe)
        begin
        if (sample_no == (DECIMATION-1))
          begin
          sample_no <= 0;
          integ_strobe <= 1;
          end
        else
          begin
          sample_no <= sample_no + 1'b1;
          integ_strobe <= 0;
          end
        end
      else
        integ_strobe <= 0;
    
    reg  signed [ACC_WIDTH-1:0] in_i, in_q;
    wire signed [ACC_WIDTH-1:0] integ_out_i, integ_out_q;
    
    always @(posedge clock)
    begin
        in_i <= in_data_i;      // will sign-extend since both declared signed
        in_q <= in_data_q;
    end

    generate
        if (INCLUDE == "rx3" && RX2_DECIM == RX2_12K_DECIM) begin : rx3_12k `include "cic_rx3_12k.vh" end
        if (INCLUDE == "rx3" && RX2_DECIM == RX2_20K_DECIM) begin : rx3_20k `include "cic_rx3_20k.vh" end
    endgenerate
    
    always @(posedge clock)
        if (integ_strobe)
        begin
            out_data_i <= integ_out_i;
            out_data_q <= integ_out_q;
        end

endmodule
