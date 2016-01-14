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

`include "kiwi.vh"

`default_nettype none

module GPS (
    input  wire		   clk,
    input  wire		   adc_clk,
    input  wire		   host_srq,

    input  wire		   I_data,
    output wire        gps_rd,
    output wire [15:0] gps_dout,
    
    output wire        ser,
    input  wire [31:0] tos,
    input  wire [15:0] op,
    input  wire        rdBit,
    input  wire        rdReg,
    input  wire        wrReg,
    input  wire        wrEvt
    );

    //////////////////////////////////////////////////////////////////////////
    // Channel select

    reg [3:0] cmd_chan;

    always @ (posedge clk)
        if (wrReg & op[SET_CHAN]) cmd_chan <= tos[3:0];

    //////////////////////////////////////////////////////////////////////////
    // Service request flags and masks

    reg  [GPS_CHANS-1:0] chan_mask;
    wire [GPS_CHANS-1:0] chan_srq;

    always @ (posedge clk)
        if (wrReg & op[SET_MASK]) chan_mask <= tos[GPS_CHANS-1:0];

	localparam NSRQ = GPS_CHANS;		// GPS_CHANS, not GPS_CHANS-1, allows for host_srq

    wire [NSRQ:0] srq_flags = {host_srq, chan_srq};		// lsb is highest servicing priority
    wire [NSRQ:0] srq_mask  = {1'b1,     chan_mask};

    //////////////////////////////////////////////////////////////////////////
    // Serial read

	// The use of rdReg like this has the side-effect of a tos <= 0 for subsequent rdBits,
	// plus latches ser_sel which is what a wrEvt alone could have done.
	
	// [2:0] = [GET_SNAPSHOT, GET_SRQ, GET_CHAN_IQ]
    wire [2:0] ser_data;
    reg  [2:0] ser_sel;		// rdReg latches ser_sel bits from op in addition to usual simul par read

    always @ (posedge clk)
        if (rdReg) ser_sel <= op[2:0];

    wire [2:1] ser_load = {2{rdReg}} & op[2:1];		// op[GET_SNAPSHOT, GET_SRQ] during rdReg
    wire [2:0] ser_next = {3{rdBit}} & ser_sel;		// any during subsequent rdBit

    assign ser = | (ser_data & ser_sel);	// presumably only one will be selected

    //////////////////////////////////////////////////////////////////////////
    // Read service requests; MSB = host request

    reg [NSRQ:0] srq_noted, srq_shift;

    always @ (posedge clk)
        if (ser_load[GET_SRQ]) srq_noted <= srq_flags;
        else				   srq_noted <= srq_flags | srq_noted;

    always @ (posedge clk)
        if      (ser_load[GET_SRQ]) srq_shift <= srq_noted & srq_mask;
        else if (ser_next[GET_SRQ]) srq_shift <= srq_shift << 1;	// one shift each rdBit

    assign ser_data[GET_SRQ] = srq_shift[NSRQ];	// always MSB, so that's why shift is used

    //////////////////////////////////////////////////////////////////////////
    // 48-bit high-resolution system clock (49 day overflow)
    
    reg [47:0] adc_ticks;
    wire [47:0] ticks;
    
    always @ (posedge adc_clk)
    	adc_ticks <= adc_ticks + 1'b1;
    
	SYNC_WIRE sync_adc_ticks [47:0] (.in(adc_ticks), .out_clk(clk), .out(ticks));
	
    //////////////////////////////////////////////////////////////////////////
    // Read clock replica snapshots

	// i.e. { ticks[47:0], srq[GPS_CHANS-1:0], {GPS_CHANS {clock_replica[15:0]}} }

    reg  [48+GPS_CHANS*17-1:0] snapshot;
    wire [48+GPS_CHANS*17-1:0] replicas;

    assign replicas[GPS_CHANS*17-1 -:GPS_CHANS] = chan_srq | srq_noted[GPS_CHANS-1:0]; // Unserviced epochs
    assign replicas[48+GPS_CHANS*17-1 -:48] = ticks;

    always @ (posedge clk)
        if      (ser_load[GET_SNAPSHOT]) snapshot <= replicas;
        else if (ser_next[GET_SNAPSHOT]) snapshot <= snapshot << 1;

    assign ser_data[GET_SNAPSHOT] = snapshot[48+GPS_CHANS*17-1];

    //////////////////////////////////////////////////////////////////////////
    // Sampling

    wire sampler_rst = wrEvt & op[GPS_SAMPLER_RST];
    wire sampler_rd  = wrEvt & op[GET_GPS_SAMPLES];

    wire [15:0] sampler_dout;
    reg         sample;

    always @ (posedge clk)
        sample <= I_data;

    SAMPLER sampler (
        .clk    (clk),
        .rst    (sampler_rst),
        .din    (sample),
        .rd     (sampler_rd),
        .dout   (sampler_dout));

    //////////////////////////////////////////////////////////////////////////
    // Logging

`ifdef USE_LOGGER
    wire log_rst = wrEvt & op[LOG_RST];
    wire log_rd  = wrEvt & op[GET_LOG];
    wire log_wr  = wrEvt & op[PUT_LOG];

    wire [15:0] log_dout;

    LOGGER log (
        .clk    (clk),
        .rst    (log_rst),
        .rd     (log_rd),
        .wr     (log_wr),
        .din    (tos[15:0]),
        .dout   (log_dout));
`endif

    //////////////////////////////////////////////////////////////////////////
    // Pause code generator (to align with SV)

    reg  [13:0] ca_cnt;
    wire [13:0] ca_nxt;
    wire        ca_resume;

    always @ (posedge clk)
        if (wrReg && op[SET_PAUSE])
            ca_cnt <= tos[13:0];
        else
            ca_cnt <= ca_nxt;

    assign {ca_resume, ca_nxt} = ca_cnt - 1'b1;

    //////////////////////////////////////////////////////////////////////////
    // Demodulators

    reg  [GPS_CHANS-1:0] chan_wrReg, chan_shift, chan_rst;
    wire [GPS_CHANS-1:0] chan_sout;

    always @* begin
        chan_rst = {GPS_CHANS{sampler_rst}} & ~chan_mask;
        chan_wrReg = 0;
        chan_shift = 0;
        chan_wrReg[cmd_chan] = wrReg;
        chan_shift[cmd_chan] = ser_next[GET_CHAN_IQ];
    end

    DEMOD demod [GPS_CHANS-1:0] (
        .clk            (clk),
        .rst            (chan_rst),
        .sample         (sample),
        .ca_resume      (ca_resume),
        .wrReg          (chan_wrReg),
        .op             (op),
        .tos            (tos),
        .shift          (chan_shift),
        .sout           (chan_sout),
        .ms0            (chan_srq),
        .replica        (replicas[GPS_CHANS*16-1:0])
    );

    assign ser_data[GET_CHAN_IQ] = chan_sout[cmd_chan];

    //////////////////////////////////////////////////////////////////////////
    // Connections

`ifdef USE_LOGGER
	assign gps_rd = sampler_rd | log_rd;
    assign gps_dout = sampler_rd? sampler_dout : log_dout;
`else
	assign gps_rd = sampler_rd;
    assign gps_dout = sampler_rd? sampler_dout : 16'b0;
`endif

endmodule
