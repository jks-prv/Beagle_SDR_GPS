// generated file

// CIC: INTEG_COMB N=4 R=11 M=1 Bin=18 Bout=24
// growth 14 = ceil(N=4 * log2(R=11)=3)
// Bin 18 + growth 14 = acc_max 32

wire signed [31:0] integrator0_data;
wire signed [31:0] integrator1_data;
wire signed [31:0] integrator2_data;
wire signed [31:0] integrator3_data;
wire signed [29:0] integrator4_data;
wire signed [29:0] comb0_data;
wire signed [28:0] comb1_data;
wire signed [27:0] comb2_data;
wire signed [26:0] comb3_data;
wire signed [25:0] comb4_data;

// important that "in" be declared signed by wrapper code
// so this assignment will sign-extend:
assign integrator0_data = in;

cic_integrator #(.WIDTH(32)) cic_integrator1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator0_data[31 -:32]),	// trunc 0 bits
	.out_data(integrator1_data)
);

cic_integrator #(.WIDTH(32)) cic_integrator2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator1_data[31 -:32]),	// trunc 0 bits
	.out_data(integrator2_data)
);

cic_integrator #(.WIDTH(32)) cic_integrator3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator2_data[31 -:32]),	// trunc 0 bits
	.out_data(integrator3_data)
);

cic_integrator #(.WIDTH(30)) cic_integrator4_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator3_data[31 -:30]),	// trunc 2 bits
	.out_data(integrator4_data)
);

assign comb0_data = integrator4_data;

cic_comb #(.WIDTH(29)) cic_comb1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb0_data[29 -:29]),	// trunc 1 bits
	.out_data(comb1_data)
);

cic_comb #(.WIDTH(28)) cic_comb2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb1_data[28 -:28]),	// trunc 1 bits
	.out_data(comb2_data)
);

cic_comb #(.WIDTH(27)) cic_comb3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb2_data[27 -:27]),	// trunc 1 bits
	.out_data(comb3_data)
);

cic_comb #(.WIDTH(26)) cic_comb4_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb3_data[26 -:26]),	// trunc 1 bits
	.out_data(comb4_data)
);

assign out = comb4_data[25 -:24] + comb4_data[1];	// trunc 2 bits
