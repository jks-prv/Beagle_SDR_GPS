`include "wrx.vh"

module GEN_SIN (
	input wire clk,
	input wire [31:0] phase_inc,
    output reg signed [OUT_WIDTH-1:0] sin
	);

	parameter OUT_WIDTH  = "required";

	reg signed [31:0] phase;

	localparam NTABLE = clog2(8192);
	//localparam NTABLE = clog2(2048);
	
	wire signed [17:0] rom_sin;
	
	always @ (posedge clk)
		begin
			phase <= phase + phase_inc;
			sin <= rom_sin[17 -:OUT_WIDTH];
		end

	ipcore_rom_sin_8k_18b sin_table (
	//ipcore_rom_sin_2k_18b sin_table (
		.clka(clk),
		.addra(phase[31 -:NTABLE]),
		.douta(rom_sin)
	);

endmodule
