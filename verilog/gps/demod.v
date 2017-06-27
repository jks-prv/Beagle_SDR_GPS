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

module DEMOD (
    input  wire        clk,
    input  wire        rst,
    input  wire        sample,
    input  wire        ca_resume, 
    input  wire        wrReg,
    input  wire [15:0] op,
    input  wire [31:0] tos,
    input  wire        shift,
    output wire        sout,
    output reg         ms0,
    output wire [15:0] replica
);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Select SV

    reg [3:0] T0, T1;

    always @ (posedge clk)
        if (wrReg && op[SET_SV])
            {T0, T1} <= tos[7:0];

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Pause code generator (to align with SV)

    reg ca_en;

    always @ (posedge clk)
        if (wrReg && op[SET_PAUSE])
            ca_en <= 0;
        else if (~ca_en)
            ca_en <= ca_resume;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NCOs

    reg  [31:0] lo_rate = 0;
    wire [31:0] lo_phase;

    reg  [31:0] ca_rate = 0, ca_phase = 0;
    wire [31:0] ca_sum;

    wire half_chip;
    wire full_chip;

    ip_add_u31b ca_nco (.a(ca_phase[30:0]), .b(ca_rate[30:0]), .s({half_chip, ca_sum[30:0]}), .c_in(1'b0));
    
    wire a = ca_phase[31];
    wire b = ca_rate[31];
    wire ci = half_chip;
    
	wire d = a ^ b;
	XORCY xorcy (       .LI(d), .CI(ci), .O(ca_sum[31]));	
	MUXCY muxcy (.S(d), .DI(a), .CI(ci), .O(full_chip));

	ip_acc_u32b lo_nco (.clk(clk), .sclr(1'b0), .b(lo_rate), .q(lo_phase));

    always @ (posedge clk) begin
        if (wrReg && op[SET_LO_NCO]) lo_rate <= tos;
        if (wrReg && op[SET_CA_NCO]) ca_rate <= tos;
        if (rst) ca_phase <= 0; else if (ca_en) ca_phase <= ca_sum;
    end

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // C/A code and epoch

    wire       ca_e;
    wire [9:0] g1_e;
    reg  [9:0] g1_p;
    reg        ca_p, ms1, ca_l = 0;

    always @ (posedge clk)
        if (half_chip)
            if (full_chip)
                ca_l <= ca_p;
            else begin
                ca_p <= ca_e;
                g1_p <= g1_e;
                ms0 <= &g1_e; // Epoch
            end
        else
            ms0 <= 0;

    always @ (posedge clk)
        ms1 <= ms0;
        
    wire ca_rd = full_chip & ca_en;

    CACODE ca (.rst(rst), .clk(clk), .T0(T0), .T1(T1), .rd(ca_rd), .g1(g1_e), .chip(ca_e));

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Quadrature final LO

    wire [3:0] lo_sin = 4'b1100;
    wire [3:0] lo_cos = 4'b0110;

    wire LO_I = lo_sin[lo_phase[31:30]];
    wire LO_Q = lo_cos[lo_phase[31:30]];

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Down-convert to baseband

    reg        lsb, die, dqe, dip, dqp, dil, dql;
    reg [16:1] ie, qe, ip, qp, il, ql;		// register length chosen to not overflow with our 16.384 MHz GPS clock

    always @ (posedge clk) begin

        // Mixers
        die <= sample^ca_e^LO_I; dqe <= sample^ca_e^LO_Q;
        dip <= sample^ca_p^LO_I; dqp <= sample^ca_p^LO_Q;
        dil <= sample^ca_l^LO_I; dql <= sample^ca_l^LO_Q;

        // Filters 
        ie <= (ms1? 16'b0 : ie) + {16{die}} + lsb;
        qe <= (ms1? 16'b0 : qe) + {16{dqe}} + lsb;
        ip <= (ms1? 16'b0 : ip) + {16{dip}} + lsb;
        qp <= (ms1? 16'b0 : qp) + {16{dqp}} + lsb;
        il <= (ms1? 16'b0 : il) + {16{dil}} + lsb;
        ql <= (ms1? 16'b0 : ql) + {16{dql}} + lsb;

        lsb <= ms1? 1'b0 : ~lsb; 

    end

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Serial output of IQ accumulators to embedded CPU

    reg [95:0] ser_iq;	// 16*6 = 96 bits

    always @ (posedge clk)
        if (ms1)
            ser_iq <= {ip, qp, ie, qe, il, ql};
       else if (shift)
          ser_iq <= ser_iq << 1;

    assign sout = ser_iq[95];

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Clock replica

    assign replica = {~ca_phase[31], ca_phase[30:26], g1_p};

endmodule
