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
    input  wire        cg_resume, 
    input  wire        chan_wrReg,
    input  wire        op_set_sat,
    input  wire        op_set_pause,
    input  wire        op_lo_nco,
    input  wire        op_cg_nco,
    input  wire [31:0] tos,
    
    output reg [E1B_CODEBITS-1:0] nchip,
    output wire        e1b_full_chip,
    input  wire        e1b_code,
    
    input  wire        shift,
    output wire        sout,
    output reg         ms0,
    output wire [GPS_REPL_BITS-1:0] replica
);

`include "kiwi.gen.vh"

	parameter E1B = "required";

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Select sat

    reg e1b_mode, g2_init;
    reg [10:1] init;

    always @ (posedge clk)
        if (chan_wrReg & op_set_sat) begin
            {e1b_mode, g2_init, init} <= tos[11:0];
        end

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Pause code generator (to align with sat)

    reg cg_en;

    always @ (posedge clk)
        if (chan_wrReg & op_set_pause)
            cg_en <= 0;
        else if (~cg_en)
            cg_en <= cg_resume;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // NCOs

    reg  [31:0] lo_rate = 0;
    wire [31:0] lo_phase;

    reg  [31:0] cg_rate = 0, cg_phase = 0;
    wire [31:0] cg_sum;

    wire full_chip;
    wire half_chip;
    wire quarter_chip;
    wire full_chip_en = full_chip & cg_en;
    
    ip_add_u30b cg_nco (.a(cg_phase[29:0]), .b(cg_rate[29:0]), .s({quarter_chip, cg_sum[29:0]}), .c_in(1'b0));
    
    wire a30 = cg_phase[30];
    wire b30 = cg_rate[30];
    wire ci30 = quarter_chip;
    
	wire d30 = a30 ^ b30;
	XORCY xorcy30 (         .LI(d30), .CI(ci30), .O(cg_sum[30]));	
	MUXCY muxcy30 (.S(d30), .DI(a30), .CI(ci30), .O(half_chip));

    wire a31 = cg_phase[31];
    wire b31 = cg_rate[31];
    wire ci31 = half_chip;
    
	wire d31 = a31 ^ b31;
	XORCY xorcy31 (         .LI(d31), .CI(ci31), .O(cg_sum[31]));	
	MUXCY muxcy31 (.S(d31), .DI(a31), .CI(ci31), .O(full_chip));

	ip_acc_u32b lo_nco (.clk(clk), .sclr(1'b0), .b(lo_rate), .q(lo_phase));

    always @ (posedge clk) begin
        if (chan_wrReg & op_lo_nco) lo_rate <= tos;
        if (chan_wrReg & op_cg_nco) cg_rate <= tos;
        if (rst) cg_phase <= 0; else if (cg_en) cg_phase <= cg_sum;
    end

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // code generators

    reg [E1B_CODEBITS-1:0] chips;
    reg cg_p, cg2_p, ms1, cg_l = 0, cg2_l = 0;

    always @ (posedge clk)
        ms1 <= ms0;
        
    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // C/A, E1B code and epoch

    wire ca_chip;
    reg e1b_latched_code;

    wire boc11 = cg_phase[31];      // bit that toggles at the half-chip rate = 1x chip frequency
    wire e1b_chip = e1b_latched_code ^ boc11;

    wire cg_e  = e1b_mode? e1b_chip  : ca_chip;
    wire cg2_e = e1b_latched_code;  // for E1B tracking need additional EPL correlated without BOC(1,1) i.e. PRN only

    assign e1b_full_chip = full_chip;

    always @ (posedge clk) begin
        if (rst)
            nchip <= 0;
        else
        
        // Due to BOC(1,1) need to do twice as many EPL latchings because e = code ^ boc11 has
        // twice as many transitions as e = code, i.e.
        //
        // FHQ  e=n, l=p
        // --Q  p=e
        // -HQ  l=p, chips, ms0
        // --Q  p=e
        //
        // Without BOC(1,1) was: (note missing second l=p and p=e)
        //
        // FHQ  e=n
        // --Q  p=e
        // -HQ  l=p, chips, ms0
        // --Q  

        if (e1b_mode) begin     // E1B
            if (full_chip_en)
                nchip <= (nchip == (E1B_CODELEN-1))? 0 : (nchip+1);

            if (full_chip) begin
                e1b_latched_code <= e1b_code;
                cg_l <= cg_p;
                cg2_l <= cg2_p;
            end

            if (quarter_chip && !full_chip) begin
                if (half_chip) begin
                    cg_l <= cg_p;
                    cg2_l <= cg2_p;
                    chips <= nchip;         // for replica
                    ms0 <= (nchip == 0);    // Epoch
                end
                else begin
                    cg_p <= cg_e;
                    cg2_p <= cg2_e;
                end

            end
            else
                ms0 <= 0;
        end
        else begin      // L1 C/A
            if (full_chip_en)
                nchip <= (nchip == (L1_CODELEN-1))? 0 : (nchip+1);
                
            if (half_chip)
                if (full_chip)
                    cg_l <= cg_p;
                else begin
                    cg_p <= cg_e;
                    chips <= nchip;         // for replica
                    ms0 <= (nchip == 0);    // Epoch
                end
            else
                ms0 <= 0;
        end
    end

    wire ca_rd = full_chip_en;
    CACODE ca (.rst(rst), .clk(clk), .g2_init(g2_init), .init(init), .rd(ca_rd), .chip(ca_chip));

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Quadrature final LO

    wire [3:0] lo_sin = 4'b1100;
    wire [3:0] lo_cos = 4'b0110;

    wire LO_I = lo_sin[lo_phase[31:30]];
    wire LO_Q = lo_cos[lo_phase[31:30]];

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Down-convert to baseband and
    // serial output of IQ accumulators to embedded CPU

    reg lsb, die, dqe, dip, dqp, dil, dql;
    // register length chosen to not overflow with our 16.368 MHz GPS clock and code length
    reg [GPS_INTEG_BITS:1] ie, qe, ip, qp, il, ql;
    wire [GPS_INTEG_BITS:1] integ_zero = {GPS_INTEG_BITS{1'b0}};

    always @ (posedge clk) begin

        // Mixers
        die <= sample ^ cg_e ^ LO_I; dqe <= sample ^ cg_e ^ LO_Q;
        dip <= sample ^ cg_p ^ LO_I; dqp <= sample ^ cg_p ^ LO_Q;
        dil <= sample ^ cg_l ^ LO_I; dql <= sample ^ cg_l ^ LO_Q;

        // Filters 
        ie <= (ms1? integ_zero : ie) + {GPS_INTEG_BITS{die}} + lsb;
        qe <= (ms1? integ_zero : qe) + {GPS_INTEG_BITS{dqe}} + lsb;
        ip <= (ms1? integ_zero : ip) + {GPS_INTEG_BITS{dip}} + lsb;
        qp <= (ms1? integ_zero : qp) + {GPS_INTEG_BITS{dqp}} + lsb;
        il <= (ms1? integ_zero : il) + {GPS_INTEG_BITS{dil}} + lsb;
        ql <= (ms1? integ_zero : ql) + {GPS_INTEG_BITS{dql}} + lsb;

        lsb <= ms1? 1'b0 : ~lsb; 
    end

    generate
        if (E1B == 1) begin
            reg die2, dqe2, dip2, dqp2, dil2, dql2;
            reg [GPS_INTEG_BITS:1] ie2, qe2, ip2, qp2, il2, ql2;
            
            always @ (posedge clk) begin
        
                // Mixers
                die2 <= sample ^ cg2_e ^ LO_I; dqe2 <= sample ^ cg2_e ^ LO_Q;
                dip2 <= sample ^ cg2_p ^ LO_I; dqp2 <= sample ^ cg2_p ^ LO_Q;
                dil2 <= sample ^ cg2_l ^ LO_I; dql2 <= sample ^ cg2_l ^ LO_Q;
        
                // Filters 
                ie2 <= (ms1? integ_zero : ie2) + {GPS_INTEG_BITS{die2}} + lsb;
                qe2 <= (ms1? integ_zero : qe2) + {GPS_INTEG_BITS{dqe2}} + lsb;
                ip2 <= (ms1? integ_zero : ip2) + {GPS_INTEG_BITS{dip2}} + lsb;
                qp2 <= (ms1? integ_zero : qp2) + {GPS_INTEG_BITS{dqp2}} + lsb;
                il2 <= (ms1? integ_zero : il2) + {GPS_INTEG_BITS{dil2}} + lsb;
                ql2 <= (ms1? integ_zero : ql2) + {GPS_INTEG_BITS{dql2}} + lsb;
            end

            localparam SER_IQ_MSB = (12 * GPS_INTEG_BITS) - 1;
            reg [SER_IQ_MSB:0] ser_iq;
            
            always @ (posedge clk)
                if (ms1)
                    ser_iq <= {ip, qp, ie, qe, il, ql, ip2, qp2, ie2, qe2, il2, ql2};
                else if (shift)
                    ser_iq <= ser_iq << 1;

            assign sout = ser_iq[SER_IQ_MSB];
        end
        else begin
            localparam SER_IQ_MSB = (6 * GPS_INTEG_BITS) - 1;
            reg [SER_IQ_MSB:0] ser_iq;
            
            always @ (posedge clk)
                if (ms1)
                    ser_iq <= {ip, qp, ie, qe, il, ql};
                else if (shift)
                    ser_iq <= ser_iq << 1;

            assign sout = ser_iq[SER_IQ_MSB];
        end
    endgenerate

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Clock replica

    generate
        if (GPS_REPL_BITS == 16) begin
            assign replica = {~cg_phase[31], cg_phase[30:26], chips[9:0]};
        end

        if (GPS_REPL_BITS == 18) begin
            assign replica = {~cg_phase[31], cg_phase[30:26], chips[9:0], chips[11:10]};
        end
    endgenerate

endmodule
