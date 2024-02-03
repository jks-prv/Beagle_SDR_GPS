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

// Copyright (c) 2014-2023 John Seamons, ZL4VO/KF6VO

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
    
    input  wire        ext_rd,
    input  wire [15:0] ext_dout,
    
    input  wire		   hb_ovfl,
    output wire		   hb_orst,

    output wire [15:0] host_dout,
    output wire        mem_rd,
    input  wire [15:0] mem_dout,
    output reg		   boot_done,
    
    input  wire [15:0] tos,
    input  wire [3:0]  op_4,
    input  wire        rdReg,
    input  wire        wrReg,
    input  wire        wrEvt
    );
    
`include "kiwi.gen.vh"

    //////////////////////////////////////////////////////////////////////////
    // Host instruction decoding

    wire host_rd  = rdReg & op_4[HOST_RX];
    wire host_wr  = wrReg & op_4[HOST_TX];
    wire host_rst = wrEvt & op_4[HOST_RST];
    wire host_rdy = wrEvt & op_4[HOST_RDY];

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
    wire hb_ack, ha_rdy;

    always @ (posedge hb_clk)
        if      (host_srq) hb_rdy <= 1'b0;
        else if (host_rdy) hb_rdy <= 1'b1;

	SYNC_WIRE sync_ha_ack (.in(ha_ack), .out_clk(hb_clk), .out(hb_ack));

	// DANGER!
	// ha_clk is NOT a continuous clock, but rather clocking just once
	// per SPI data bit. Since ha_rdy is sampled by ha_st[1] it must be valid on
	// the second SPI clock period. So force a single-FF synchronizer here (.NSYNC(1))
	// just as Andrew did in his original code.
	SYNC_WIRE #(.NSYNC(1)) sync_hb_rdy (.in(hb_rdy), .out_clk(ha_clk), .out(ha_rdy));

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
       rst[LOAD]        ________________/               \___	"Loading"
                     ___                                 ___
       rst[RUN]      ___\_______________________________/		"Run"
                     ___________________
       boot_rst      ___/               \___________________

       hb_addr                       000|001.....3FF|000
       hb_dout                           000.........3FF|000
       next_pc                           000.........3FF|000|001
       pc                                3FF|000.........3FF|000
       op            xxx|xxx|nop.........................nop|000
    */

    always @ (posedge hb_clk)
        if      (boot_halt) rst <= 2'b00; // Halt
        else if (boot_load) rst <= 2'b01; // Loading
        else if (boot_done) rst <= 2'b10; // Run

    wire boot_rst = ~|rst;
    wire boot_rd = rst[LOAD];

    //////////////////////////////////////////////////////////////////////////
    // Block host SRQ if busy

    reg [NST:0]   ha_st;
    reg			  ha_wr;

	// state shift-register
    always @ (posedge ha_clk or posedge ha_rst)
        if (ha_rst) ha_st <= 1'b1;
        else        ha_st <= {|ha_st[NST -:2], ha_st[NST-2:0], 1'b0};

    always @ (posedge ha_clk)
        if (ha_st[1]) ha_ack <= ha_rdy; // decision point

    always @ (posedge ha_clk or posedge ha_rst)		// SPI is always reading & writing simultaneously
        if      (ha_rst)   ha_wr <= 1'b0;
        else if (ha_st[2]) ha_wr <= ha_cs[BOOT] | ha_ack;

    //////////////////////////////////////////////////////////////////////////
    // Host serial I/O, byte aligned
    
    // SPI_32:
    //  ha_rst: ha_st[] <= 1
    //  ha_st[31] <= b31|b30 (i.e. stick)
    //  ha_st[30:0] <= {ha_st[29:0], 0} (i.e. shl)

    // status: first byte, sent msb first (L => R)
    // 1001     flags: busy, 0, 0, ~ha_ack
    //     1    ha_ovfl (adc ovfl)
    //      xxx available (unused)

    reg [IDL:0] ha_disr;
    reg [ODL:0] ha_dosr;
    wire        ha_dout;
    reg		    ha_out;

	always @ (posedge ha_clk or posedge ha_rst)
		if (ha_rst)	ha_out <= 1;	// busy flag
		else		ha_out <= | (ha_st & { ha_dosr[ODL], {(NST-4){1'b0}}, ha_ovfl, ~ha_ack, 2'b0 });
		// NB: ha_st scans out bits in increasing order (b0, b1, ...) from above expression.
		// And since SPI data is msb/MSB first the result is L => R as shown in "status:" above.

	// delay lines
    always @ (posedge ha_clk) begin
        ha_disr <= {ha_disr[IDL-1:0], spi_mosi};
        ha_dosr <= {ha_dosr[ODL-1:0], ha_dout};
    end

	// Be able to meet setup and hold times of Sitara MISO by adding
	// a DFF on the output that is clocked on the negative ha_clk edge.
	// This works because the first bit sent is always a '1' (busy flag) setup by
	// the preset of ha_rst that occurs before the first ha_clk.
	// All subsequent bits sent are pipelined properly.
	//
	// DFF met timing with synthesis, but not when implemented. Use an ODDR to improve timing.

	reg ha_out2;

`ifdef NOTDEF
	always @ (negedge ha_clk or posedge ha_rst)
		ha_out2 <= ha_rst? 1 : ha_out;
`endif

`ifdef SERIES_7
	// doesn't work, but fails timing without
`ifdef NOTDEF
	ODDR #(.DDR_CLK_EDGE("SAME_EDGE"), .SRTYPE("ASYNC"), .INIT(1'b1)) ha_out3 (
		.C(ha_clk), .CE(1'b1), .D1(ha_out2), .D2(ha_out), .S(ha_rst), .R(1'b0), .Q(spi_miso)
	);
`else
	assign spi_miso = ha_out;		// fixme: check that this meets timing analyzer
`endif
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

	wire ha_ovfl;
	SYNC_WIRE sync_ha_ovfl (.in(hb_ovfl), .out_clk(ha_clk), .out(ha_ovfl));

	reg ha_orst;
	always @ (posedge ha_clk)
		ha_orst <= ha_st & ha_ovfl;		// set after overflow has gone out in SPI status

	SYNC_PULSE sync_ha_orst (.in_clk(ha_clk), .in(ha_orst), .out_clk(hb_clk), .out(hb_orst));

    //////////////////////////////////////////////////////////////////////////
    // Host FIFO - port A

    localparam HA_MSB = clog2(SPIBUF_W * 16) - 1;

	reg			    full;       // keep MOSI from overrunning BRAM
    reg  [HA_MSB:0] ha_cnt;
    wire [HA_MSB:0] ha_addr;

    always @ (posedge ha_clk or posedge ha_rst)
        if (ha_rst) { full, ha_cnt } <= 0;
        else        { full, ha_cnt } <= ha_cnt + ha_wr;

`ifdef SPI_8
	localparam NST = 7;
	localparam SFT = 0;
    assign ha_addr = ha_cnt ^ 3'b111;	// SPI is MSB first
`endif
`ifdef SPI_16
	localparam NST = 15;
	localparam SFT = 8;
    assign ha_addr = ha_cnt ^ 4'b1111;	// SPI is MSB first
`endif
`ifdef SPI_32
	localparam NST = 31;
	localparam SFT = 24;
    assign ha_addr = ha_cnt ^ 5'b11111;	// SPI is MSB first
`endif

	// 3 stages of delay because ha_wr doesn't assert for 3 ha_clk periods (ha_ack sampled on ha_st[2])
	localparam DLY = 3;
	localparam IDL = DLY-1;
	localparam ODL = SFT+DLY-1;

    //////////////////////////////////////////////////////////////////////////
    // Host FIFO - port B

    localparam HB_MSB = clog2(SPIBUF_W) - 1;

    reg [HB_MSB:0] hb_addr, hb_pos;

    wire hb_wr  = host_wr  | gps_rd | rx_rd | wf_rd | ext_rd | mem_rd;
    wire hb_rd  = host_rd  | boot_rd;
    wire hb_rst = host_rst | boot_rst;

    always @* { boot_done, hb_addr } = hb_rst? 0 : hb_pos + hb_rd;

    always @ (posedge hb_clk) hb_pos <= hb_addr + hb_wr;

    //////////////////////////////////////////////////////////////////////////
    // Host "bridge" FIFO

    wire [15:0] hb_dout;
    reg  [15:0] hb_din;

    //.WRITE_MODE_A("READ_FIRST")	// Read MISO before writing MOSI
    ipcore_bram_32k_1b_2k_16b host_fifo (
        .clka   (ha_clk),			.clkb   (hb_clk),
        .dina	(ha_disr[IDL]),		.dinb	(hb_din),
        .wea	(ha_wr && ~full),	.web	(hb_wr),
        .addra	(ha_addr),			.addrb	(hb_addr),
        .douta	(ha_dout),			.doutb	(hb_dout)
    );

    //////////////////////////////////////////////////////////////////////////
    // Parallel data MUXing

    assign mem_rd = wrEvt & op_4[GET_MEMORY];

    always @*
        if (gps_rd)  hb_din = gps_dout;  else
        if (mem_rd)  hb_din = mem_dout;  else
        if (rx_rd)   hb_din = rx_dout;   else
        if (wf_rd)   hb_din = wf_dout;   else
        if (ext_rd)  hb_din = ext_dout; else
					 hb_din = tos[15:0];	// default: host_wr (HOST_TX)

	// 16'b0 assignment very important because some rdRegs want to push a zero on the stack as a side-effect
    assign host_dout = hb_rd? hb_dout : 16'b0;

endmodule
