module ip_dds_sin_cos_13b_15b_48b (
	input  wire 		clk,
	input  wire [47:0] 	pinc_in,
	output wire [14:0]	sine,
	output wire [14:0]	cosine
	);

`include "kiwi.gen.vh"

`ifdef USE_ISE
    ipcore_dds_sin_cos_13b_15b_48b dds (
		.clk		(clk),
		.pinc_in	(pinc_in),
		.sine		(sine),
		.cosine		(cosine)
	);
`endif

`ifdef USE_VIVADO
	// Vivado only has AXI4 streaming interface (native interface removed).
	// Fortunately, our simple case is easy to implement.

	wire [31:0] tdata;
    ipcore_dds_sin_cos_13b_15b_48b dds (
		.aclk					(clk),
		.s_axis_phase_tvalid	(1'b1),
		.s_axis_phase_tdata		(pinc_in),
		.m_axis_data_tvalid		(),
		.m_axis_data_tdata		(tdata)
	);
	assign sine = tdata[16 +:15];
	assign cosine = tdata[0 +:15];
`endif

endmodule
