// generated file

// CIC: INTEG_COMB N=4 R=4 M=1 Bin=18 Bout=24
// growth 8 = ceil(N=4 * log2(R=4)=2)
// Bin 18 + growth 8 = acc_max 26

wire signed [25:0] integrator0_data;
wire signed [25:0] integrator1_data;
wire signed [25:0] integrator2_data;
wire signed [25:0] integrator3_data;
wire signed [25:0] integrator4_data;
wire signed [25:0] comb0_data;
wire signed [25:0] comb1_data;
wire signed [25:0] comb2_data;
wire signed [25:0] comb3_data;
wire signed [25:0] comb4_data;

// important that "in" be declared signed by wrapper code
// so this assignment will sign-extend:
assign integrator0_data = in;

cic_integrator #(.WIDTH(26)) cic_integrator1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator0_data[25 -:26]),	// trunc 0 bits
	.out_data(integrator1_data)
);

cic_integrator #(.WIDTH(26)) cic_integrator2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator1_data[25 -:26]),	// trunc 0 bits
	.out_data(integrator2_data)
);

cic_integrator #(.WIDTH(26)) cic_integrator3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator2_data[25 -:26]),	// trunc 0 bits
	.out_data(integrator3_data)
);

cic_integrator #(.WIDTH(26)) cic_integrator4_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator3_data[25 -:26]),	// trunc 0 bits
	.out_data(integrator4_data)
);

assign comb0_data = integrator4_data;

cic_comb #(.WIDTH(26)) cic_comb1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb0_data[25 -:26]),	// trunc 0 bits
	.out_data(comb1_data)
);

cic_comb #(.WIDTH(26)) cic_comb2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb1_data[25 -:26]),	// trunc 0 bits
	.out_data(comb2_data)
);

cic_comb #(.WIDTH(26)) cic_comb3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb2_data[25 -:26]),	// trunc 0 bits
	.out_data(comb3_data)
);

cic_comb #(.WIDTH(26)) cic_comb4_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb3_data[25 -:26]),	// trunc 0 bits
	.out_data(comb4_data)
);

assign out = comb4_data[25 -:24] + comb4_data[1];	// trunc 2 bits
