`default_nettype none

// IQ sampler, 8K x 2 x 16-bit, 8 36kb BRAMs
// clock domains: fully isolated

module IQ_SAMPLER_8K_16B (
    input  wire wr_clk,
    input  wire wr_rst,
    input  wire wr_continuous,		// if true, the wr_clk side doesn't stop capturing when buffer full
    input  wire wr,
    input  wire [15:0] wr_i,
    input  wire [15:0] wr_q,

    input  wire rd_clk,
    input  wire rd_rst,
    input  wire rd_sync,			// set rd_addr to (wr_addr + 15), +15 is crucial look-ahead to previous buffer contents
    input  wire rd_i,
    input  wire rd_q,
    output wire [15:0] rd_iq
    );
        
	// wr_clk side
    reg [12:0] wr_addr;
    reg        full;
    wire	   wr_en = wr_continuous? wr : (wr && ~full);
    
    always @ (posedge wr_clk)
        if (wr_rst) {wr_addr, full} <= 0;
        else
        if (wr_en) {full, wr_addr} <= wr_addr + 1;
	
	// rd_clk side
    reg [12:0] rd_addr;
	wire [12:0] rd_next = rd_addr + rd_q;
	
	wire [12:0] sync_wr_addr;
	SYNC_WIRE sync_wr_addr_inst [12:0] (.in(wr_addr), .out_clk(rd_clk), .out(sync_wr_addr));
	
    always @ (posedge rd_clk)
        if (rd_rst)
            rd_addr <= 0;
        else
        if (rd_sync)
        	rd_addr <= { (sync_wr_addr[12:9] + 4'd1), sync_wr_addr[8:0] };
        else begin
            rd_addr <= rd_next;
		end
	
	wire [15:0] rd_di, rd_dq;
	assign rd_iq = rd_i? rd_di : rd_dq;
	
	ipcore_bram_8k_16b iq_samp_i (
		.clka	(wr_clk),			.clkb	(rd_clk),
		.wea	(wr_en),
		.addra	(wr_addr),			.addrb	(rd_next),
		.dina	(wr_i),				.doutb	(rd_di)
	);
         
	ipcore_bram_8k_16b iq_samp_q (
		.clka	(wr_clk),			.clkb	(rd_clk),
		.wea	(wr_en),
		.addra	(wr_addr),			.addrb	(rd_next),
		.dina	(wr_q),				.doutb	(rd_dq)
	);
         
endmodule
