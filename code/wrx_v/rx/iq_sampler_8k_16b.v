`default_nettype none

// IQ sampler, 8K x 2 x 16-bit, 16 BRAMs
// clock domains: fully isolated

module IQ_SAMPLER_8K_16B (
    input  wire wr_clk,
    input  wire wr_rst,
    input  wire wr,
    input  wire [15:0] wr_i,
    input  wire [15:0] wr_q,

    input  wire rd_clk,
    input  wire rd_rst,
    input  wire rd_i,
    input  wire rd_q,
    output wire [15:0] rd_iq
    );
        
    reg [12:0] rd_addr;
    reg [12:0] wr_addr;
    reg        full;
    
    always @ (posedge wr_clk)
        if (wr_rst)
            {wr_addr, full} <= 0;
        else begin
            if (wr && ~full) {full, wr_addr} <= wr_addr + 1;
		end
	
	wire [12:0] rd_next = rd_addr + rd_q;
	
    always @ (posedge rd_clk)
        if (rd_rst)
            rd_addr <= 0;
        else begin
            rd_addr <= rd_next;
		end
	
	wire [15:0] rd_di, rd_dq;
	assign rd_iq = rd_i? rd_di : rd_dq;
	
	ipcore_bram_8k_16b iq_samp_i (
		.clka	(wr_clk),			.clkb	(rd_clk),
		.wea	(wr & ~full),
		.addra	(wr_addr),			.addrb	(rd_next),
		.dina	(wr_i),				.doutb	(rd_di)
	);
         
	ipcore_bram_8k_16b iq_samp_q (
		.clka	(wr_clk),			.clkb	(rd_clk),
		.wea	(wr & ~full),
		.addra	(wr_addr),			.addrb	(rd_next),
		.dina	(wr_q),				.doutb	(rd_dq)
	);
         
endmodule
