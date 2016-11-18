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

// Copyright (c) 2014 John Seamons, ZL/KF6VO

`include "kiwi.vh"

module GEN (
	input wire		   			adc_clk,
    output wire signed [17:0]	gen_data,

	input wire		   			cpu_clk,
    input wire [31:0]        	freeze_tos,
    input wire [15:0]			op,
    input wire        			wrReg2
	);

	localparam SIGN			= 35;
	localparam MANTISSA		= 33;
	localparam MANTISSA_W	= 18-1;
	localparam RND			= MANTISSA - MANTISSA_W;
	
	reg [31:0] gen_phase_inc;
    reg signed [17:0] gen_attn;
    
	wire set_gen_C =		wrReg2 & op[SET_GEN];
	wire set_gen_attn_C =	wrReg2 & op[SET_GEN_ATTN];

	wire set_gen_A, set_gen_attn_A;
	SYNC_PULSE gen_phase_inst (.in_clk(cpu_clk), .in(set_gen_C), .out_clk(adc_clk), .out(set_gen_A));
	SYNC_PULSE gen_attn_inst (.in_clk(cpu_clk), .in(set_gen_attn_C), .out_clk(adc_clk), .out(set_gen_attn_A));

    wire signed [17:0] sine;
    reg signed [17:0] sine_L, gen_rnd;
	reg signed [35:0] gen_mul;
	assign gen_data = gen_rnd;
	
    always @ (posedge adc_clk)
    begin
        if (set_gen_A) gen_phase_inc <= freeze_tos;
        if (set_gen_attn_A) gen_attn <= freeze_tos[17:0];
        
        // seems like need a level of registers to meet timing of DDS output
        sine_L <= sine;
        gen_mul <= sine_L * gen_attn;
        gen_rnd <= { gen_mul[SIGN], gen_mul[MANTISSA -:MANTISSA_W] } + gen_mul[RND];
    end

	wire signed [14:0] dds_sin;
	assign sine = {dds_sin, 3'b0};

	ip_dds_sin_cos_13b_15b sin_cos_osc (
		.clk		(adc_clk),
		.pinc_in	(gen_phase_inc),
		.sine		(dds_sin),
		.cosine		()
	);

endmodule
