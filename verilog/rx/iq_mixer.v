`include "kiwi.vh"

`default_nettype none

// IQ mixer using multiplies as a slice-saving alternative to CORDIC

module IQ_MIXER (
	input wire clk,
	input wire signed [31:0] phase_inc,
	input wire signed [IN_WIDTH-1:0] in_data,
	output wire signed [OUT_WIDTH-1:0] out_i,
	output wire signed [OUT_WIDTH-1:0] out_q
    );
    
	parameter IN_WIDTH  = "required";
	parameter OUT_WIDTH  = "required";

	localparam EXT			= 18 - IN_WIDTH;

	localparam SIGN			= 35;
	localparam MANTISSA		= 33;
	localparam MANTISSA_W	= OUT_WIDTH-1;
	localparam RND			= MANTISSA - MANTISSA_W;

	wire signed [17:0] sin, cos;

    reg signed [17:0] mx, my_i, my_q;

	always @(posedge clk)
	begin
		mx <= { {EXT{in_data[IN_WIDTH-1]}}, in_data };
		my_i <= cos;
		my_q <= sin;
	end
	
	reg signed [35:0] prod_i, prod_q;
	assign out_i = { prod_i[SIGN], prod_i[MANTISSA -:MANTISSA_W] } + prod_i[RND];
	assign out_q = { prod_q[SIGN], prod_q[MANTISSA -:MANTISSA_W] } + prod_q[RND];

	always @(posedge clk)
	begin
		prod_i <= mx * my_i;
		prod_q <= mx * my_q;
	end

	wire signed [14:0] dds_sin, dds_cos;
	assign sin = {dds_sin, 3'b0};
	assign cos = {dds_cos, 3'b0};
	//wire signed [15:0] dds_sin, dds_cos;
	//assign sin = {dds_sin, 2'b0};
	//assign cos = {dds_cos, 2'b0};

	ip_dds_sin_cos_13b_15b sin_cos_osc (
	//ip_dds_sin_cos_14b_16b sin_cos_osc (
		.clk		(clk),
		.pinc_in	(phase_inc),
		.sine		(dds_sin),
		.cosine		(dds_cos)
	);

endmodule
