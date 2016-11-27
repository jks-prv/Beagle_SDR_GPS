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
// implements 11-bit fixed (0-2048) and 5/11-bit 2**n variable decimation (R)
//
// NB: when variable decimation is specified by .DECIMATION(<0) then .GROWTH() must
// specify for the largest expected decimation.
//
// Fixed differential delay (D) = 1
//

`include "kiwi.vh"

module cic (
	input wire clock,
	input wire reset,
	input wire [MD-1:0] decimation,
	input wire in_strobe,
	output reg out_strobe,
	input wire signed [IN_WIDTH-1:0] in_data,
	output reg signed [OUT_WIDTH-1:0] out_data
	);

// design parameters
parameter STAGES = "required";
parameter DECIMATION = "required";  
parameter IN_WIDTH = "required";
parameter GROWTH = "required";
parameter OUT_WIDTH = "required";

localparam ACC_WIDTH = IN_WIDTH + GROWTH;

// trade-off: less output width means more quantization noise, but of course this effects
// input width of subsequent stages

//------------------------------------------------------------------------------
//                               control
//------------------------------------------------------------------------------

localparam MD = 14;		// assumes excess counter bits get optimized away

reg [MD-1:0] sample_no;
initial sample_no = {MD{1'b0}};
wire [MD-1:0] decim;

generate
	if (DECIMATION < 0) begin assign decim = decimation; end	// variable
	if (DECIMATION > 0) begin assign decim = DECIMATION; end	// fixed
endgenerate

always @(posedge clock)
  if (in_strobe)
    begin
    if (sample_no == (decim-1))
      begin
      sample_no <= 0;
      out_strobe <= 1;
      end
    else
      begin
      sample_no <= sample_no + 1'b1;
      out_strobe <= 0;
      end
    end
  else
    out_strobe <= 0;

//------------------------------------------------------------------------------
//                                stages
//------------------------------------------------------------------------------
wire signed [ACC_WIDTH-1:0] integrator_data [0:STAGES], comb_data [0:STAGES], out;

assign integrator_data[0] = in_data;	// will sign-extend since both declared signed
assign comb_data[0] = integrator_data[STAGES];
assign out = comb_data[STAGES];


genvar i;

generate
	for (i=0; i<STAGES; i=i+1)
	begin : cic_stages

		cic_integrator #(.WIDTH(ACC_WIDTH)) cic_integrator_inst(
		  .clock		(clock),
		  .reset		(reset),
		  .strobe		(in_strobe),
		  .in_data		(integrator_data[i]),
		  .out_data		(integrator_data[i+1])
		  );
	
		cic_comb #(.WIDTH(ACC_WIDTH)) cic_comb_inst(
		  .clock		(clock),
		  .reset		(reset),
		  .strobe		(out_strobe),
		  .in_data		(comb_data[i]),
		  .out_data		(comb_data[i+1])
		  );
	end
endgenerate

//------------------------------------------------------------------------------
//                            output rounding
//------------------------------------------------------------------------------

	localparam GROWTH_R2	= STAGES * clog2(2);
	localparam GROWTH_R4	= STAGES * clog2(4);
	localparam GROWTH_R8	= STAGES * clog2(8);
	localparam GROWTH_R16	= STAGES * clog2(16);
	localparam GROWTH_R32	= STAGES * clog2(32);
	localparam GROWTH_R64	= STAGES * clog2(64);
	localparam GROWTH_R128	= STAGES * clog2(128);
	localparam GROWTH_R256	= STAGES * clog2(256);
	localparam GROWTH_R512	= STAGES * clog2(512);
	localparam GROWTH_R1024	= STAGES * clog2(1024);
	localparam GROWTH_R2048	= STAGES * clog2(2048);
	
	localparam ACC_R2		= IN_WIDTH + GROWTH_R2;
	localparam MSB_R2		= ACC_R2 - 1;
	localparam LSB_R2		= ACC_R2 - OUT_WIDTH;
	
	localparam ACC_R4		= IN_WIDTH + GROWTH_R4;
	localparam MSB_R4		= ACC_R4 - 1;
	localparam LSB_R4		= ACC_R4 - OUT_WIDTH;
	
	localparam ACC_R8		= IN_WIDTH + GROWTH_R8;
	localparam MSB_R8		= ACC_R8 - 1;
	localparam LSB_R8		= ACC_R8 - OUT_WIDTH;
	
	localparam ACC_R16		= IN_WIDTH + GROWTH_R16;
	localparam MSB_R16		= ACC_R16 - 1;
	localparam LSB_R16		= ACC_R16 - OUT_WIDTH;
	
	localparam ACC_R32		= IN_WIDTH + GROWTH_R32;
	localparam MSB_R32		= ACC_R32 - 1;
	localparam LSB_R32		= ACC_R32 - OUT_WIDTH;
	
	localparam ACC_R64		= IN_WIDTH + GROWTH_R64;
	localparam MSB_R64		= ACC_R64 - 1;
	localparam LSB_R64		= ACC_R64 - OUT_WIDTH;
	
	localparam ACC_R128		= IN_WIDTH + GROWTH_R128;
	localparam MSB_R128		= ACC_R128 - 1;
	localparam LSB_R128		= ACC_R128 - OUT_WIDTH;
	
	localparam ACC_R256		= IN_WIDTH + GROWTH_R256;
	localparam MSB_R256		= ACC_R256 - 1;
	localparam LSB_R256		= ACC_R256 - OUT_WIDTH;
	
	localparam ACC_R512		= IN_WIDTH + GROWTH_R512;
	localparam MSB_R512		= ACC_R512 - 1;
	localparam LSB_R512		= ACC_R512 - OUT_WIDTH;
	
	localparam ACC_R1024	= IN_WIDTH + GROWTH_R1024;
	localparam MSB_R1024	= ACC_R1024 - 1;
	localparam LSB_R1024	= ACC_R1024 - OUT_WIDTH;
	
	localparam ACC_R2048	= IN_WIDTH + GROWTH_R2048;
	localparam MSB_R2048	= ACC_R2048 - 1;
	localparam LSB_R2048	= ACC_R2048 - OUT_WIDTH;
	
generate
	if (DECIMATION == -32)
	begin
	
	always @(posedge clock)
		if (out_strobe)
		case (decim)
			   1: out_data <= in_data[IN_WIDTH-1 -:OUT_WIDTH];
			   2: out_data <= out[MSB_R2:LSB_R2]	+ out[LSB_R2-1];
			   4: out_data <= out[MSB_R4:LSB_R4]	+ out[LSB_R4-1];
			   8: out_data <= out[MSB_R8:LSB_R8]	+ out[LSB_R8-1];
			  16: out_data <= out[MSB_R16:LSB_R16]	+ out[LSB_R16-1];
			  32: out_data <= out[MSB_R32:LSB_R32]	+ out[LSB_R32-1];
		endcase
	end
endgenerate

generate
	if (DECIMATION == -2048)
	begin
	
	always @(posedge clock)
		if (out_strobe)
		case (decim)
			   1: out_data <= in_data[IN_WIDTH-1 -:OUT_WIDTH];
			   2: out_data <= out[MSB_R2:LSB_R2]		+ out[LSB_R2-1];
			   4: out_data <= out[MSB_R4:LSB_R4]		+ out[LSB_R4-1];
			   8: out_data <= out[MSB_R8:LSB_R8]		+ out[LSB_R8-1];
			  16: out_data <= out[MSB_R16:LSB_R16]		+ out[LSB_R16-1];
			  32: out_data <= out[MSB_R32:LSB_R32]		+ out[LSB_R32-1];
			  64: out_data <= out[MSB_R64:LSB_R64]		+ out[LSB_R64-1];
			 128: out_data <= out[MSB_R128:LSB_R128]	+ out[LSB_R128-1];
			 256: out_data <= out[MSB_R256:LSB_R256]	+ out[LSB_R256-1];
			 512: out_data <= out[MSB_R512:LSB_R512]	+ out[LSB_R512-1];
			1024: out_data <= out[MSB_R1024:LSB_R1024]	+ out[LSB_R1024-1];
			2048: out_data <= out[MSB_R2048:LSB_R2048]	+ out[LSB_R2048-1];
		endcase
	end
endgenerate

generate
	if (DECIMATION > 0)
	begin
	
	localparam MSB = ACC_WIDTH - 1;
	localparam LSB = ACC_WIDTH - OUT_WIDTH;
	
	always @(posedge clock)
		if (out_strobe) out_data <= out[MSB:LSB] + out[LSB-1];
	
	end
endgenerate

endmodule
