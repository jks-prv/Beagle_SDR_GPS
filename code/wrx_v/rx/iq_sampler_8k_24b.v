`default_nettype none

// IQ sampler, 8K x 2 x 24-bit, 24 BRAMs

module IQ_SAMPLER_8K_24B (
    input  wire wr_clk,
    input  wire wr_rst,
    input  wire wr,
    input  wire [15:0] wr_i_h16,
    input  wire [7:0] wr_i_l8,
    input  wire [15:0] wr_q_h16,
    input  wire [7:0] wr_q_l8,

    input  wire rd_clk,
    input  wire rd_rst,
    input  wire rd_i,
    input  wire rd_q,
    input  wire rd_3,
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
	
	wire [15:0] rd_d3, rd_di, rd_dq;
	assign rd_iq = rd_3? rd_d3 : ( rd_i? rd_di : rd_dq );
	
	ipcore_bram_8k_16b iq_samp_i (
		.clka	(wr_clk),			.clkb	(rd_clk),
		.wea	(wr & ~full),
		.addra	(wr_addr),			.addrb	(rd_next),
		.dina	(wr_i_h16),			.doutb	(rd_di)
	);
         
	ipcore_bram_8k_16b iq_samp_q (
		.clka	(wr_clk),			.clkb	(rd_clk),
		.wea	(wr & ~full),
		.addra	(wr_addr),			.addrb	(rd_next),
		.dina	(wr_q_h16),			.doutb	(rd_dq)
	);
	
	wire [15:0] wr_iq_l8 = { wr_i_l8, wr_q_l8 };
         
	ipcore_bram_8k_16b iq_samp_3 (
		.clka	(wr_clk),			.clkb	(rd_clk),
		.wea	(wr & ~full),
		.addra	(wr_addr),			.addrb	(rd_next),
		.dina	(wr_iq_l8),			.doutb	(rd_d3)
	);
         
endmodule
