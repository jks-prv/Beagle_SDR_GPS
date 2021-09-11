module ip_add_u30b (
	input  wire [29:0] 	a, b,
	input  wire 		c_in,
	output wire [30:0]	s
	);

`include "kiwi.gen.vh"

`ifdef USE_ISE
    ipcore_add_u30b add_u30b (.a(a), .b(b), .s(s), .c_in(c_in));
`endif

`ifdef USE_VIVADO
	// capitalization of port names changed in Vivado
    ipcore_add_u30b add_u30b (.A(a), .B(b), .S(s), .C_IN(c_in));
`endif

endmodule
