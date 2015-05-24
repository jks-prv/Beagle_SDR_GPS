`include "wrx.vh"

module ip_clkgen_80M_20M (
	input  wire			CLK_IN1,
	output wire 		CLK_OUT1,
	output wire 		CLK_OUT2
	);

`ifdef USE_ISE

	// configure with clk_valid output to avoid internal undefined errors in generated module
	ipcore_clkgen_80M_20M clkgen_inst (
		.CLK_IN1	(CLK_IN1),							// set no buf on input
		.CLK_OUT1	(CLK_OUT1),							// set BUFG on output
		.CLK_OUT2	(CLK_OUT2),							// set BUFG on output
		.CLK_VALID	()
	);
`endif

`ifdef USE_VIVADO

	// capitalization of port names changed in Vivado
	ipcore_clkgen_80M_20M clkgen_inst (
		.clk_in1	(CLK_IN1),							// set no buf on input
		.clk_out1	(CLK_OUT1),							// set BUFG on output
		.clk_out2	(CLK_OUT2)							// set BUFG on output
	);
`endif

endmodule
