// generated file

// CIC: INTEG_ONLY N=5 R=31 M=1 Bin=18 Bout=24
// growth 25 = ceil(N=5 * log2(R=31))
// Bin 18 + growth 25 = acc_max 43

wire signed [42:0] integrator0_data_i, integrator0_data_q;
wire signed [42:0] integrator1_data_i, integrator1_data_q;
wire signed [42:0] integrator2_data_i, integrator2_data_q;
wire signed [38:0] integrator3_data_i, integrator3_data_q;
wire signed [34:0] integrator4_data_i, integrator4_data_q;
wire signed [31:0] integrator5_data_i, integrator5_data_q;

assign integrator0_data_i = in_i;
assign integrator0_data_q = in_q;

cic_integrator #(.WIDTH(43)) cic_integrator1_inst_i(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator0_data_i[42 -:43]),	// trunc 0 bits
	.out_data(integrator1_data_i)
);

cic_integrator #(.WIDTH(43)) cic_integrator1_inst_q(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator0_data_q[42 -:43]),	// trunc 0 bits
	.out_data(integrator1_data_q)
);

cic_integrator #(.WIDTH(43)) cic_integrator2_inst_i(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator1_data_i[42 -:43]),	// trunc 0 bits
	.out_data(integrator2_data_i)
);

cic_integrator #(.WIDTH(43)) cic_integrator2_inst_q(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator1_data_q[42 -:43]),	// trunc 0 bits
	.out_data(integrator2_data_q)
);

cic_integrator #(.WIDTH(39)) cic_integrator3_inst_i(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator2_data_i[42 -:39]),	// trunc 4 bits
	.out_data(integrator3_data_i)
);

cic_integrator #(.WIDTH(39)) cic_integrator3_inst_q(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator2_data_q[42 -:39]),	// trunc 4 bits
	.out_data(integrator3_data_q)
);

cic_integrator #(.WIDTH(35)) cic_integrator4_inst_i(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator3_data_i[38 -:35]),	// trunc 4 bits
	.out_data(integrator4_data_i)
);

cic_integrator #(.WIDTH(35)) cic_integrator4_inst_q(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator3_data_q[38 -:35]),	// trunc 4 bits
	.out_data(integrator4_data_q)
);

cic_integrator #(.WIDTH(32)) cic_integrator5_inst_i(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator4_data_i[34 -:32]),	// trunc 3 bits
	.out_data(integrator5_data_i)
);

cic_integrator #(.WIDTH(32)) cic_integrator5_inst_q(
	.clock(clock),
	.strobe(in_strobe),
	.in_data(integrator4_data_q[34 -:32]),	// trunc 3 bits
	.out_data(integrator5_data_q)
);

assign integ_out_i = { integrator5_data_i, {43-32{1'b0}} };
assign integ_out_q = { integrator5_data_q, {43-32{1'b0}} };

