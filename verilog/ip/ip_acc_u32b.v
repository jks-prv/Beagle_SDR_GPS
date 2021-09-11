module ip_acc_u32b (
	input  wire			clk,
	input  wire 		sclr,
	input  wire [31:0] 	b,
	output wire [31:0]	q
	);

`include "kiwi.gen.vh"

`ifdef USE_ISE
	ipcore_acc_u32b acc_u32b (.clk(clk), .sclr(sclr), .b(b), .q(q));
`endif

`ifdef USE_VIVADO
	// capitalization of port names changed in Vivado
	ipcore_acc_u32b acc_u32b (.CLK(clk), .SCLR(sclr), .B(b), .Q(q));
`endif

endmodule
