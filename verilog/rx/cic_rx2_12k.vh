// generated file

// CIC: INTEG_COMB|MODE_REAL N=5 R=25 M=1 Bin=18 Bout=24
// growth 24 = ceil(N=5 * log2(R=25)=5)
// Bin 18 + growth 24 = acc_max 42 

wire signed [41:0] integrator0_data;
wire signed [41:0] integrator1_data;
wire signed [41:0] integrator2_data;
wire signed [37:0] integrator3_data;
wire signed [34:0] integrator4_data;
wire signed [31:0] integrator5_data;
wire signed [31:0] comb0_data;
wire signed [29:0] comb1_data;
wire signed [28:0] comb2_data;
wire signed [27:0] comb3_data;
wire signed [26:0] comb4_data;
wire signed [26:0] comb5_data;

// important that "in" be declared signed by wrapper code
// so this assignment will sign-extend:
assign integrator0_data = in;

cic_integrator #(.WIDTH(42)) cic_integrator1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator0_data[41 -:42]),	// trunc 0 bits (should always be zero)
	.out_data(integrator1_data)
);

cic_integrator #(.WIDTH(42)) cic_integrator2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator1_data[41 -:42]),	// trunc 0 bits 
	.out_data(integrator2_data)
);

cic_integrator #(.WIDTH(38)) cic_integrator3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator2_data[41 -:38]),	// trunc 4 bits 
	.out_data(integrator3_data)
);

cic_integrator #(.WIDTH(35)) cic_integrator4_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator3_data[37 -:35]),	// trunc 3 bits 
	.out_data(integrator4_data)
);

cic_integrator #(.WIDTH(32)) cic_integrator5_inst(
	.clock(clock),
	.reset(reset),
	.strobe(in_strobe),
	.in_data(integrator4_data[34 -:32]),	// trunc 3 bits 
	.out_data(integrator5_data)
);

assign comb0_data = integrator5_data;

cic_comb #(.WIDTH(30)) cic_comb1_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb0_data[31 -:30]),	// trunc 2 bits 
	.out_data(comb1_data)
);

cic_comb #(.WIDTH(29)) cic_comb2_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb1_data[29 -:29]),	// trunc 1 bits 
	.out_data(comb2_data)
);

cic_comb #(.WIDTH(28)) cic_comb3_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb2_data[28 -:28]),	// trunc 1 bits 
	.out_data(comb3_data)
);

cic_comb #(.WIDTH(27)) cic_comb4_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb3_data[27 -:27]),	// trunc 1 bits 
	.out_data(comb4_data)
);

cic_comb #(.WIDTH(27)) cic_comb5_inst(
	.clock(clock),
	.reset(reset),
	.strobe(out_strobe),
	.in_data(comb4_data[26 -:27]),	// trunc 0 bits 
	.out_data(comb5_data)
);

assign out = comb5_data[26 -:24] + comb5_data[2];	// trunc 3 bits, rounding applied
