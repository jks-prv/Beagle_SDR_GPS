// generated file

// CIC: N=5 R=16 M=1 Bin=24 Bout=16
// Bin 24 + growth 20 = acc_max 44

wire signed [35:0] integrator0_data;
wire signed [35:0] integrator1_data;
wire signed [31:0] integrator2_data;
wire signed [27:0] integrator3_data;
wire signed [25:0] integrator4_data;
wire signed [22:0] integrator5_data;
wire signed [22:0] comb0_data;
wire signed [21:0] comb1_data;
wire signed [20:0] comb2_data;
wire signed [19:0] comb3_data;
wire signed [18:0] comb4_data;
wire signed [18:0] comb5_data;

assign integrator0_data = in_data;

cic_integrator #(.WIDTH(36)) cic_integrator1_inst(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator0_data[35 -:36]),	// trunc 0 bits
	.out_data(integrator1_data)
);

cic_integrator #(.WIDTH(32)) cic_integrator2_inst(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator1_data[35 -:32]),	// trunc 4 bits
	.out_data(integrator2_data)
);

cic_integrator #(.WIDTH(28)) cic_integrator3_inst(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator2_data[31 -:28]),	// trunc 4 bits
	.out_data(integrator3_data)
);

cic_integrator #(.WIDTH(26)) cic_integrator4_inst(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator3_data[27 -:26]),	// trunc 2 bits
	.out_data(integrator4_data)
);

cic_integrator #(.WIDTH(23)) cic_integrator5_inst(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator4_data[25 -:23]),	// trunc 3 bits
	.out_data(integrator5_data)
);

assign comb0_data = integrator5_data;

cic_comb #(.WIDTH(22)) cic_comb1_inst(
	.clock(clock),
	.strobe(out_strobe),
	.in_data(comb0_data[22 -:22]),	// trunc 1 bits
	.out_data(comb1_data)
);

cic_comb #(.WIDTH(21)) cic_comb2_inst(
	.clock(clock),
	.strobe(out_strobe),
	.in_data(comb1_data[21 -:21]),	// trunc 1 bits
	.out_data(comb2_data)
);

cic_comb #(.WIDTH(20)) cic_comb3_inst(
	.clock(clock),
	.strobe(out_strobe),
	.in_data(comb2_data[20 -:20]),	// trunc 1 bits
	.out_data(comb3_data)
);

cic_comb #(.WIDTH(19)) cic_comb4_inst(
	.clock(clock),
	.strobe(out_strobe),
	.in_data(comb3_data[19 -:19]),	// trunc 1 bits
	.out_data(comb4_data)
);

cic_comb #(.WIDTH(19)) cic_comb5_inst(
	.clock(clock),
	.strobe(out_strobe),
	.in_data(comb4_data[18 -:19]),	// trunc 0 bits
	.out_data(comb5_data)
);

assign out = comb5_data[18 -:16];	// trunc 3 bits
