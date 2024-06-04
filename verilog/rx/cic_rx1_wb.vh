// generated file

// CIC: INTEG_COMB|MODE_REAL N=3 R=463 M=1 Bin=22 Bout=18
// growth 27 = ceil(N=3 * log2(R=463)=9)
// Bin 22 + growth 27 = acc_max 49 

wire signed [48:0] integrator0_data;
wire signed [48:0] integrator1_data;
wire signed [48:0] integrator2_data;
wire signed [25:0] integrator3_data;
wire signed [25:0] comb0_data;
wire signed [21:0] comb1_data;
wire signed [20:0] comb2_data;
wire signed [19:0] comb3_data;

// important that "in" be declared signed by wrapper code
// so this assignment will sign-extend:
assign integrator0_data = in;

cic_integrator #(.WIDTH(49)) cic_integrator1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator0_data[48 -:49]),	// trunc 0 bits (should always be zero)
	.out_data(integrator1_data)
);

cic_integrator #(.WIDTH(49)) cic_integrator2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator1_data[48 -:49]),	// trunc 0 bits 
	.out_data(integrator2_data)
);

cic_integrator #(.WIDTH(26)) cic_integrator3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator2_data[48 -:26]),	// trunc 23 bits 
	.out_data(integrator3_data)
);

assign comb0_data = integrator3_data;

cic_comb #(.WIDTH(22)) cic_comb1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb0_data[25 -:22]),	// trunc 4 bits 
	.out_data(comb1_data)
);

cic_comb #(.WIDTH(21)) cic_comb2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb1_data[21 -:21]),	// trunc 1 bits 
	.out_data(comb2_data)
);

cic_comb #(.WIDTH(20)) cic_comb3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb2_data[20 -:20]),	// trunc 1 bits 
	.out_data(comb3_data)
);

assign out = comb3_data[19 -:18] + comb3_data[1];	// trunc 2 bits, rounding applied
