module ip_add_u32b (
	input  wire [31:0] 	a, b,
	input  wire 		c_in,
	output wire [32:0]	s
	);

`include "kiwi.gen.vh"

`ifdef USE_ISE
    ipcore_add_u32b add_u32b (.a(a), .b(b), .s(s), .c_in(c_in));
`endif

`ifdef USE_VIVADO
	// capitalization of port names changed in Vivado
    ipcore_add_u32b add_u32b (.A(a), .B(b), .S(s), .C_IN(c_in));
`endif

endmodule
