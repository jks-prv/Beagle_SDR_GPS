//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

`include "wrx.vh"

`default_nettype none

module HOST (
    input  wire        hb_clk,
    output reg	[2:1]  rst,
    input  wire        spi_sclk,
    input  wire [1:0]  spi_cs,
    input  wire        spi_mosi,
    output wire        spi_miso,
    output wire        host_srq,

    input  wire        gps_rd,
    input  wire [15:0] gps_dout,

    input  wire        rx_rd,
    input  wire [15:0] rx_dout,

    input  wire        wf_rd,
    input  wire [15:0] wf_dout,

    output wire [15:0] host_dout,
    output wire        mem_rd,
    input  wire [15:0] mem_dout,
    output reg		   boot_done,
    
    input  wire [31:0] tos,
    input  wire [15:0] op,
    input  wire        rdReg,
    input  wire        wrReg,
    input  wire        wrEvt,

    input  wire [15:0] ctrl			// fixme: debug
    );
    
    //////////////////////////////////////////////////////////////////////////
    // Host instruction decoding

    wire host_rd  = rdReg & op[HOST_RX];
    wire host_wr  = wrReg & op[HOST_TX];
    wire host_rst = wrEvt & op[HOST_RST];
    wire host_rdy = wrEvt & op[HOST_RDY];

    //////////////////////////////////////////////////////////////////////////
    // Host interface
    //
    // 				BBB_CS_N	spi_cs	ha_cs
    // idle/reset	11			00		00
    //				01			10		00
    // load/boot	00			11		01
    // host			10			01		10

    wire ha_clk, ha_rst;
    wire [2:1] ha_cs = {~spi_cs[1] & spi_cs[0], spi_cs[1] & spi_cs[0]};

    BUFG ha_clk_bufg (
        .I  (spi_sclk),
        .O  (ha_clk)
    );

    assign ha_rst = ~|ha_cs;

    //////////////////////////////////////////////////////////////////////////
    // Handshake

    reg  ha_ack, hb_rdy;
    wire ha_rdy, hb_ack;

    always @ (posedge hb_clk)
        if      (host_srq) hb_rdy <= 1'b0;
        else if (host_rdy) hb_rdy <= 1'b1;

	SYNC_WIRE sync_ha_ack (.in(ha_ack), .out_clk(hb_clk), .out(hb_ack));
	SYNC_WIRE sync_hb_rdy (.in(hb_rdy), .out_clk(ha_clk), .out(ha_rdy));

    //////////////////////////////////////////////////////////////////////////
    // Host strobes
    
    localparam BOOT = 1;
    localparam HOST = 2;

	wire [1:0] hb_boot, hb_host;
	SYNC_WIRE #(.NOUT(2)) sync_ha_boot (.in(ha_cs[BOOT]), .out_clk(hb_clk), .out(hb_boot));
	SYNC_WIRE #(.NOUT(2)) sync_ha_host (.in(ha_cs[HOST]), .out_clk(hb_clk), .out(hb_host));

    localparam RISE=2'b01, FALL=2'b10;

    wire boot_halt = hb_boot == RISE;
    wire boot_load = hb_boot == FALL;
    wire host_poll = hb_host == FALL;

    assign host_srq = host_poll & hb_ack;

    //////////////////////////////////////////////////////////////////////////
    /* Boot sequence
                     _______________
       hb_boot    __/               \_______________________
                     ___
       boot_halt  __/   \___________________________________
                                     ___
       boot_load  __________________/   \___________________
                                                     ___
       boot_done  __________________________________/   \___
                                         _______________
       rst[1] "Loading" ________________/               \___
                     ___                                 ___
       rst[2] "Run"  ___\_______________________________/
                     ___________________
       boot_rst      ___/               \___________________

       hb_addr                       000|001.....FFF|000
       hb_dout                           000.........FFF|000
       next_pc                           000.........FFF|000|001
       pc                                FFF|000.........FFF|000
       op            xxx|xxx|nop.........................nop|000
    */

    always @ (posedge hb_clk)
        if      (boot_halt) rst <= 2'b00; // Halt
        else if (boot_load) rst <= 2'b01; // Loading
        else if (boot_done) rst <= 2'b10; // Run

    wire boot_rst = ~|rst;
    wire boot_rd = rst[1];

    //////////////////////////////////////////////////////////////////////////
    // Block host SRQ if busy

    reg [NST:0] ha_st;
    reg         ha_wr;

    always @ (posedge ha_clk or posedge ha_rst)
        if (ha_rst) ha_st <= 1'b1;
        else        ha_st <= {|ha_st[NST -:2], ha_st[NST-2:0], 1'b0};

    always @ (posedge ha_clk)
        if (ha_st[1]) ha_ack <= ha_rdy; // decision point

    always @ (posedge ha_clk or posedge ha_rst)		// SPI is always reading & writing simultaneously
        if      (ha_rst)   ha_wr <= 1'b0;
        else if (ha_st[2]) ha_wr <= ha_cs[BOOT] | ha_ack;

    //////////////////////////////////////////////////////////////////////////
    // Receiver wakeup status

	localparam MAX_CH = 4;
	localparam check = assert(RX_CHANS <= MAX_CH);
    wire rx_wakeup [MAX_CH-1:0];

`ifdef NOTDEF
	// Doesn't work! Says can't assign to rx_wakeup. Due to SYNC_WIRE outputing a [0:0]?
	SYNC_WIRE sync_rx_wakeup [RX_CHANS-1:0] (.in(ctrl[CTRL_RX_WAKEUP+i]), .out_clk(ha_clk), .out(rx_wakeup));
`else
	genvar i;
	generate
		for (i=0; i < RX_CHANS; i=i+1)
	  	begin : gen_rx_wakeup
			SYNC_WIRE sync_rx_wakeup (.in(ctrl[CTRL_RX_WAKEUP+i]), .out_clk(ha_clk), .out(rx_wakeup[i]));
		end
	endgenerate
`endif

    //////////////////////////////////////////////////////////////////////////
    // Host serial I/O, byte aligned

    reg [IDL:0] ha_disr;
    reg [ODL:0] ha_dosr;
    wire        ha_dout;
    reg		    ha_out;

	always @ (posedge ha_clk or posedge ha_rst)
		if      (ha_rst)   	 ha_out <= 1;
		else if (ha_st[2])	 ha_out <= ~ha_ack; 		// status flag
		else if (ha_st[6])   ha_out <= rx_wakeup[MAX_CH-4];		// fixme: no good way to generate this expansion?
		else if (ha_st[5])   ha_out <= rx_wakeup[MAX_CH-3];
		else if (ha_st[4])   ha_out <= rx_wakeup[MAX_CH-2];
		else if (ha_st[3])   ha_out <= rx_wakeup[MAX_CH-1];
		else if (ha_st[NST]) ha_out <= ha_dosr[ODL];
		else               	 ha_out <= 0;

	// delay lines
    always @ (posedge ha_clk) begin
        ha_disr <= {ha_disr[IDL-1:0], spi_mosi};
        ha_dosr <= {ha_dosr[ODL-1:0], ha_dout};
    end

	// Be able to meet setup and hold times of Sitara MISO by adding
	// a DFF on the output that is clocked on the negative ha_clk edge.
	// This works because the first bit sent is always a '1' setup by
	// the preset of ha_rst that occurs before the first ha_clk.
	// All subsequent bits sent are pipelined properly.
	//
	// DFF met timing with synthesis, but not when implemented. Use an ODDR to improve timing.
	
	reg ha_out2;

	always @ (negedge ha_clk or posedge ha_rst)
		ha_out2 <= ha_rst? 1 : ha_out;

`ifdef ARTIX_7
	ODDR #(.DDR_CLK_EDGE("SAME_EDGE"), .SRTYPE("ASYNC"), .INIT(1'b1)) ha_out3 (
		.C(ha_clk), .CE(1'b1), .D1(ha_out2), .D2(ha_out), .S(ha_rst), .R(1'b0), .Q(spi_miso)
	);
`endif

`ifdef SPARTAN_6
`ifdef NOTDEF
	// doesn't work: ping data shifted
	ODDR2 #(.DDR_ALIGNMENT("C0"), .SRTYPE("ASYNC"), .INIT(1'b1)) ha_out3 (
		.C0(ha_clk), .C1(~ha_clk), .CE(1'b1), .D0(ha_out2), .D1(ha_out), .S(ha_rst), .R(1'b0), .Q(spi_miso)
	);
`else
	assign spi_miso = ha_out2;
`endif
`endif

    //////////////////////////////////////////////////////////////////////////
    // Host FIFO - port A

    reg  [13:0] ha_cnt;
    wire [13:0] ha_addr;

    always @ (posedge ha_clk or posedge ha_rst)
        if (ha_rst) ha_cnt <= 0;
        else        ha_cnt <= ha_cnt + ha_wr;

`ifdef SPI_8
	localparam NST = 7;
	localparam IDL = 3-1;
	localparam ODL = 3-1;
    assign ha_addr = ha_cnt ^ 3'b111;	// SPI is MSB first
`endif
`ifdef SPI_16
	localparam NST = 15;
	localparam IDL = 3-1;
	localparam ODL = 8+3-1;
    assign ha_addr = ha_cnt ^ 4'b1111;	// SPI is MSB first
`endif
`ifdef SPI_32
	localparam NST = 31;
	localparam IDL = 3-1;
	localparam ODL = 24+3-1;
    assign ha_addr = ha_cnt ^ 5'b11111;	// SPI is MSB first
`endif

    //////////////////////////////////////////////////////////////////////////
    // Host FIFO - port B

    reg [9:0] hb_addr, hb_pos;

    wire hb_wr  = host_wr  | gps_rd | rx_rd | wf_rd | mem_rd;
    wire hb_rd  = host_rd  | boot_rd;
    wire hb_rst = host_rst | boot_rst;

    always @* {boot_done, hb_addr} = hb_rst? 11'b0 : hb_pos + hb_rd;

    always @ (posedge hb_clk) hb_pos <= hb_addr + hb_wr;

    //////////////////////////////////////////////////////////////////////////
    // Host "bridge" FIFO

    wire [15:0] hb_dout;
    reg  [15:0] hb_din;

    //.WRITE_MODE_A("READ_FIRST")	// Read MISO before writing MOSI
    ipcore_bram_16k_1b_1k_16b host_fifo (
        .clka   (ha_clk),       .clkb   (hb_clk),
        .dina	(ha_disr[IDL]), .dinb	(hb_din),
        .wea	(ha_wr),        .web	(hb_wr),
        .addra	(ha_addr),      .addrb	(hb_addr),
        .douta	(ha_dout),      .doutb	(hb_dout)
    );

    //////////////////////////////////////////////////////////////////////////
    // Parallel data MUXing

    assign mem_rd = wrEvt & op[GET_MEMORY];

    always @*
        if (gps_rd) hb_din = gps_dout; else
        if (mem_rd) hb_din = mem_dout; else
        if (rx_rd)  hb_din = rx_dout;  else
        if (wf_rd)  hb_din = wf_dout;  else
					hb_din = tos[15:0];	// default: host_wr (HOST_TX)

    assign host_dout = hb_rd? hb_dout : 16'b0;

endmodule
