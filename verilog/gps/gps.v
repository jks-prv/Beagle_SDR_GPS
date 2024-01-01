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

`default_nettype none

module GPS (
    input  wire		   clk,
    input  wire		   adc_clk,
    input  wire		   host_srq,

    input  wire		   I_sign,
    input  wire		   I_mag,
    input  wire		   Q_sign,
    input  wire		   Q_mag,
    output wire        gps_rd,
    output wire [15:0] gps_dout,
    
    output wire [47:0] ticks_A,
    
    output wire        ser,
    input  wire [31:0] tos,
    input  wire [ 7:0] op_8,
    input  wire        rdBit,
    input  wire        rdReg,
    input  wire        wrReg,
    input  wire        wrEvt,
    
    output wire        unused_inputs
    );

`include "kiwi.gen.vh"

    assign unused_inputs = I_mag | Q_sign | Q_mag;

    //////////////////////////////////////////////////////////////////////////
    // Channel select

    reg [3:0] cmd_chan;

    always @ (posedge clk)
        if (wrReg & op_8[SET_GPS_CHAN]) cmd_chan <= tos[3:0];

    //////////////////////////////////////////////////////////////////////////
    // Service request flags and masks

    reg  [V_GPS_CHANS-1:0] chan_mask;
    wire [V_GPS_CHANS-1:0] chan_srq;

    always @ (posedge clk)
        if (wrReg & op_8[SET_GPS_MASK]) chan_mask <= tos[V_GPS_CHANS-1:0];

	localparam NSRQ = V_GPS_CHANS;		// V_GPS_CHANS, not V_GPS_CHANS-1, allows for host_srq

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
        if (rdReg) ser_sel <= op_8[2:0];

    wire [2:1] ser_load = {2{rdReg}} & op_8[2:1];   // op[GET_SNAPSHOT, GET_SRQ] during rdReg
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

    assign ser_data[GET_SRQ] = srq_shift[NSRQ];	// always uses MSB, so that's why shift is used above

    //////////////////////////////////////////////////////////////////////////
    // 48-bit high-resolution system clock (49 day overflow)
    
    reg [47:0] adc_ticks;
    wire [47:0] ticks;
    
    always @ (posedge adc_clk)
    	adc_ticks <= adc_ticks + 1'b1;
    
    assign ticks_A = adc_ticks;

    // continuously sync ticks_A => ticks
    // ticks_A is on adc_clk and exported to receiver for inclusion in audio stream
    // ticks is synced to gps_clk for inclusion in snapshot data below
	SYNC_REG #(.WIDTH(48)) sync_adc_ticks (
	    .in_strobe(1),      .in_reg(ticks_A),   .in_clk(adc_clk),
	    .out_strobe(),      .out_reg(ticks),    .out_clk(clk)
	);
	
    //////////////////////////////////////////////////////////////////////////
    // Read clock replica snapshots

	// i.e. { ticks[47:0], srq[V_GPS_CHANS-1:0], {V_GPS_CHANS {clock_replica[GPS_REPL_BITS-1:0]}} }

    localparam ALL_REPLICA_BITS = V_GPS_CHANS * GPS_REPL_BITS;
    localparam GPS_MSB = V_GPS_CHANS /* srq */ + ALL_REPLICA_BITS - 1;
    
    reg  [48+GPS_MSB:0] snapshot;
    wire [48+GPS_MSB:0] replicas;

    assign replicas[GPS_MSB -:V_GPS_CHANS] = chan_srq | srq_noted[V_GPS_CHANS-1:0]; // Unserviced epochs
    assign replicas[48+GPS_MSB -:48] = ticks;

    always @ (posedge clk)
        if      (ser_load[GET_SNAPSHOT]) snapshot <= replicas;
        else if (ser_next[GET_SNAPSHOT]) snapshot <= snapshot << 1;

    assign ser_data[GET_SNAPSHOT] = snapshot[48+GPS_MSB];    // always uses MSB, so that's why shift is used above

    //////////////////////////////////////////////////////////////////////////
    // Sampling

    wire sampler_rst = wrEvt & op_8[GPS_SAMPLER_RST];
    wire sampler_rd  = wrEvt & op_8[GET_GPS_SAMPLES];

    wire [15:0] sampler_dout;
    reg         sample;

    always @ (posedge clk)
        sample <= I_sign;

    SAMPLER sampler (
        .clk    (clk),
        .rst    (sampler_rst),
        .din    (sample),
        .rd     (sampler_rd),
        .dout   (sampler_dout));

    //////////////////////////////////////////////////////////////////////////
    // Logging

`ifdef USE_LOGGER
    wire log_rst = wrEvt & op_8[LOG_RST];
    wire log_rd  = wrEvt & op_8[GET_LOG];
    wire log_wr  = wrEvt & op_8[PUT_LOG];

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
    // Pause code generator (to align with sat)

    // must fit (FS_I/1000 * code_period_ms)-1
    // C/A (16368 * 1 ms)-1 = 16367 0x3fef
    // E1B (16368 * 4 ms)-1 = 65471 0xffbf
    reg  [15:0] cg_cnt;
    wire [15:0] cg_nxt;
    wire        cg_resume;

    always @ (posedge clk)
        if (wrReg & op_8[SET_PAUSE])
            cg_cnt <= tos[15:0];
        else
            cg_cnt <= cg_nxt;

    assign {cg_resume, cg_nxt} = cg_cnt - 1'b1;

    //////////////////////////////////////////////////////////////////////////
    // Demodulators

    reg  [V_GPS_CHANS-1:0] chan_wrReg, chan_shift, chan_rst;
    wire [V_GPS_CHANS-1:0] chan_sout;

    always @* begin

        // Upon sampler reset, reset code generators of all free channels.
        // This idea is very important. When the acquisition process determines the code phase
        // it is relative to the point at which the code generator was reset, namely here at the
        // beginning of the sampling process (chan_rst below is derived directly from sampler_rst).
        
        chan_rst = {V_GPS_CHANS{sampler_rst}} & ~chan_mask;
        chan_wrReg = 0;
        chan_shift = 0;
        chan_wrReg[cmd_chan] = wrReg;
        chan_shift[cmd_chan] = ser_next[GET_CHAN_IQ];
    end
    
    wire [(V_GPS_CHANS * E1B_CODEBITS)-1:0] nchip;
    wire [V_GPS_CHANS-1:0] e1b_full_chip, e1b_code;
    
    wire e1b_wr = wrReg & op_8[SET_E1B_CODE];

    E1BCODE e1b (
        .rst            (sampler_rst),
        .clk            (clk),
        .wr             (e1b_wr),
        .tos            (tos[11:0]),
        .nchip_n        (nchip),
        .full_chip      (e1b_full_chip),
        .code_o         (e1b_code)
    );
    
    wire op_set_sat   = op_8[SET_SAT];
    wire op_set_pause = op_8[SET_PAUSE];
    wire op_lo_nco    = op_8[SET_LO_NCO];
    wire op_cg_nco    = op_8[SET_CG_NCO];

    //DEMOD #(.E1B(1)) demod [V_GPS_CHANS-1:0] (      // not used currently
    DEMOD #(.E1B(0)) demod [V_GPS_CHANS-1:0] (
        .clk            (clk),
        .rst            (chan_rst),
        .sample         (sample),
        .cg_resume      (cg_resume),
        .chan_wrReg     (chan_wrReg),
        .op_set_sat     (op_set_sat),
        .op_set_pause   (op_set_pause),
        .op_lo_nco      (op_lo_nco),
        .op_cg_nco      (op_cg_nco),
        .tos            (tos),

        .nchip          (nchip),
        .e1b_full_chip  (e1b_full_chip),
        .e1b_code       (e1b_code),

        .shift          (chan_shift),
        .sout           (chan_sout),
        .ms0            (chan_srq),
        .replica        (replicas[ALL_REPLICA_BITS-1:0])
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
