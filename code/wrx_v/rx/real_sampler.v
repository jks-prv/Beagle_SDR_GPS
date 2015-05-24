`default_nettype none

// real sampler, 4K x 16-bit, 4 BRAMs

module REAL_SAMPLER (
    input  wire clk,
    input  wire rst,
    input  wire wr,
    input  wire [15:0] din,
    input  wire rd,
    output wire [15:0] dout
    );
        
    reg [11:0] rd_addr;
    reg [11:0] wr_addr;
    reg       full;
    
    always @ (posedge clk)
        if (rst)
            {rd_addr, wr_addr, full} <= 0;
        else begin
            if (wr && ~full) {full, wr_addr} <= wr_addr + 1;
            rd_addr <= rd_addr + rd;
		end
         
	ipcore_bram_4k_16b iq_samp_i (
		.clka	(clk),				.clkb	(clk),
		.wea	(wr & ~full),
		.addra	(wr_addr),			.addrb	(rd_addr + rd),
		.dina	(din),				.doutb	(dout)
	);

endmodule
