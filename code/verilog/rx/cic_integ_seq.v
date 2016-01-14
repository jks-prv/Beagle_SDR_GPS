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


//
// implements constant value decimation (R) using sequential logic integrator & comb to save slices.
//
// fixed differential delay (D) = 1
//

module cic_integ_seq (
	input wire clock,
	input wire in_strobe,
	output reg out_strobe,
	input wire signed [IN_WIDTH-1:0] in_data_i, in_data_q,
	output reg signed [OUT_WIDTH-1:0] out_data_i, out_data_q
	);

	// design parameters
	parameter STAGES = "required";
	parameter DECIMATION = "required";  
	parameter IN_WIDTH = "required";
	parameter GROWTH = "required";
	parameter OUT_WIDTH = "required";
	
	localparam ACC_WIDTH = IN_WIDTH + GROWTH;

	localparam MD = 13;		// max decim = 4096, assumes excess counter bits get optimized away
	
	reg [MD-1:0] sample_no;
	initial sample_no = {MD{1'b0}};
	reg integ_strobe;

	localparam NPOST_STAGES = 2;
	localparam NSTAGE = clog2(STAGES + NPOST_STAGES);
	reg [NSTAGE-1:0] stage;
	wire [NSTAGE-1:0] NSTAGES = STAGES;		// using STAGES is considered a 32-bit constant by default
	wire [NSTAGE-1:0] STAGE_ONE = 1, POST1_STAGE = STAGES+1, POST2_STAGE = STAGES+2;
	reg comb_stages;
	wire INTEG_STAGE = ~comb_stages, COMB_STAGE = comb_stages;

	localparam NSTATE = clog2(3);
	reg [NSTATE-1:0] state;
	
	localparam NADDR = NSTAGE + 4;
	reg [NADDR-1:0] Raddr, Waddr;
	
	reg Wen, d_q;
	wire R = d_q, W = ~d_q;
	localparam INTEG = 2'b00, COMB = 2'b01, PREV = 2'b10;
	reg signed [ACC_WIDTH-1:0] Wdata, t;
	wire signed [ACC_WIDTH-1:0] Rdata;
	
	reg iq;
	reg signed [IN_WIDTH-1:0] latch_in_q;
	wire signed [ACC_WIDTH-1:0] in_data = iq? latch_in_q : in_data_i;

	always @(posedge clock)
	begin

		//------------------------------------------------------------------------------
		//                                integrator
		//------------------------------------------------------------------------------
		
		if (INTEG_STAGE && (stage == 0))	// t = in_data
		begin
			// prev
				out_strobe <= 0;
			// cur
				if (in_strobe || iq) t <= in_data;
				if (in_strobe)
				begin
					latch_in_q <= in_data_q;
					if (sample_no == (DECIMATION-1))
					begin
					  sample_no <= 0;
					  integ_strobe <= 1;
					end else
					begin
						sample_no <= sample_no + 1'b1;
					end
				end
			// next
				if (in_strobe || iq) begin state <= 0; stage <= stage + 1'b1; end
		end else
		
		if (INTEG_STAGE && (stage <= NSTAGES))
		begin
			
			if (state == 0)		// t = integ[stage-1]
			begin
				// prev
					Wen <= 0;
				// cur
					if (stage != STAGE_ONE) t <= Rdata;
				// next
					//Raddr <= {iq, R, INTEG, stage};
					state <= state + 1'b1;
			end else
			
			// state == 1		// integ[stage] = t + integ[stage]
			begin
				// cur
					Waddr <= {iq, W, INTEG, stage}; Wen <= 1; Wdata <= t + Rdata;
				// next
					//Raddr <= {iq, R, INTEG, stage};		// not stage-1 because stage about to be incremented
					stage <= stage + 1'b1;
					state <= 0;
			end
		end else
			
		if (INTEG_STAGE && (stage == POST1_STAGE))
		begin
			// prev
				Wen <= 0;
			// cur
				iq <= ~iq;
			// next
				//Raddr <= {iq, R, INTEG, NSTAGES};			// setup for comb[0]
				stage <= 0;
				if (integ_strobe && iq) comb_stages <= 1;
		end else
		
		
		//------------------------------------------------------------------------------
		//                                comb
		//------------------------------------------------------------------------------
		
		if (COMB_STAGE && (stage == 0))		// comb[0] = integ[NSTAGES]
		begin
			// cur
				Waddr <= {iq, W, COMB, 0}; Wdata <= Rdata;
				if (integ_strobe || iq) Wen <= 1;
			// next
				//Raddr <= {iq, R, COMB, 0};
				if (integ_strobe || iq) begin state <= 0; stage <= stage + 1'b1; end
				if (integ_strobe) integ_strobe <= 0;	// fixme: check: conditional shouldn't be required
		end else
		
		if (COMB_STAGE && (stage <= NSTAGES))
		begin
			
			if (state == 0)		// t = comb[stage-1]
			begin
				// prev
					Wen <= 0;
				// cur
					t <= Rdata;
				// next
					//Raddr <= {iq, R, PREV, stage};
					state <= state + 1'b1;
			end else
			
			if (state == 1)		// comb[stage] = t - prev[stage]
			begin
				// cur
					Waddr <= {iq, W, COMB, stage}; Wen <= 1; Wdata <= t - Rdata;
				// next
					state <= state + 1'b1;
			end else
			
			// state == 2		// prev[stage] = t
			begin
				// cur
					Waddr <= {iq, W, PREV, stage}; Wdata <= t;		// Wen remains 1 because 2 writes in a row
				// next
					//Raddr <= {iq, R, COMB, stage};		// not stage-1 because stage about to be incremented
					stage <= stage + 1'b1;
					state <= 0;
			end
		end else
	
		if (COMB_STAGE && (stage == POST1_STAGE))
		begin
			// prev
				Wen <= 0;
			// next
				//Raddr <= {iq, R, COMB, NSTAGES};
				stage <= stage + 1'b1;
		end else
		
		// COMB_STAGE && (stage == POST2_STAGE)
		begin
			// cur
				if (iq)
				begin
					out_data_q <= Rdata[ACC_WIDTH-1 -:16] + Rdata[ACC_WIDTH-1-16];
					out_strobe <= 1;
					d_q <= ~d_q;
				end else
				begin
					out_data_i <= Rdata[ACC_WIDTH-1 -:16] + Rdata[ACC_WIDTH-1-16];
				end
				iq <= ~iq;
			// next
				//Raddr <= {iq, R, INTEG, NSTAGES};		// address of q integrator output
        		stage <= 0;
				if (iq) comb_stages <= 0;
		end
	end
	
	
	//------------------------------------------------------------------------------
	//                    parallel combinatorial state machine
	//------------------------------------------------------------------------------
		
	localparam X = 1'bx, REG_X = 2'bxx;
	wire [NSTAGE-1:0] STAGE_X = {NSTAGE{1'bx}};
	
	always @*
	begin
		if (INTEG_STAGE && (stage == 0))	// t = in_data
		begin
			// next
				Raddr = {X, X, REG_X, STAGE_X};
		end else
		
		if (INTEG_STAGE && (stage <= NSTAGES))
		begin
			
			if (state == 0)		// t = integ[stage-1]
			begin
				// next
					Raddr = {iq, R, INTEG, stage};
			end else
			
			// state == 1		// integ[stage] = t + integ[stage]
			begin
				// next
					Raddr = {iq, R, INTEG, stage};		// not stage-1 because stage about to be incremented
			end
		end else
			
		if (INTEG_STAGE && (stage == POST1_STAGE))
		begin
			// next
				Raddr = {iq, R, INTEG, NSTAGES};
		end else

		if (COMB_STAGE && (stage == 0))		// comb[0] = integ[NSTAGES]
		begin
			// next
				Raddr = {iq, R, COMB, 0};
		end else
		
		if (COMB_STAGE && (stage <= NSTAGES))
		begin
			
			if (state == 0)		// t = comb[stage-1]
			begin
				// next
					Raddr = {iq, R, PREV, stage};
			end else
			
			if (state == 1)		// comb[stage] = t - prev[stage]
			begin
				// next
					Raddr = {X, X, REG_X, STAGE_X};
			end else
			
			// state == 2		// prev[stage] = t
			begin
				// next
					 Raddr = {iq, R, COMB, stage};		// not stage-1 because stage about to be incremented
			end
		end else
	
		if (COMB_STAGE && (stage == POST1_STAGE))
		begin
			// next
				Raddr = {iq, R, COMB, NSTAGES};
		end else
		
		// COMB_STAGE && (stage == POST2_STAGE)
		begin
			// next
				Raddr = {iq, R, INTEG, NSTAGES};		// address of q integrator output
		end
	end
	
	
	ipcore_bram_256_64b iq_samp_i (
		.clka	(clock),			.clkb	(clock),
		.wea	(Wen),
		.addra	(Waddr),			.addrb	(Raddr),
		.dina	(Wdata),			.doutb	(Rdata)
	);

endmodule
