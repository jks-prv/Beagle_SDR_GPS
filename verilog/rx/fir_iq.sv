/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2023 Christoph Mayer, DL1CH

module fir_iq #(
    int WIDTH = 0
) (
	input  wire     adc_clk,
	input  wire     reset,
	input  wire     in_strobe,
	output  reg     out_strobe,
	input  wire signed [WIDTH-1:0] in_data_i,
	input  wire signed [WIDTH-1:0] in_data_q,
	output  reg signed [WIDTH-1:0] out_data_i,
	output  reg signed [WIDTH-1:0] out_data_q
    );

`include "kiwi.gen.vh"

    // nominally ACCW(42) = WIDTH(24) + COEFF(18), uses 2 DSPs
    localparam COEFF = 18;
    localparam ACCW = WIDTH + COEFF;
    localparam ACCOUT = WIDTH + COEFF - 1;
    localparam NTAPS = (RX_CFG == 14 ? 17 : 65); // needs to be of the form 2N + 1 due to odd symmetry of taps table

    reg signed  [ACCW-1:0]  accI, accQ;
    reg signed  [WIDTH-1:0] bufI[NTAPS-1:0], bufQ[NTAPS-1:0];
    wire signed [COEFF-1:0] taps[(NTAPS-1)/2:0];

    if (RX_CFG == 3) begin                      // N=5, R=2, M=1, NTAPS=65, 20.25 kHz, cutoff at 8 kHz (80%)
        assign taps[ 0]  = COEFF'('sh0005f);
        assign taps[ 1]  = COEFF'('sh3ffa4);
        assign taps[ 2]  = COEFF'('sh3ff6c);
        assign taps[ 3]  = COEFF'('sh0003c);
        assign taps[ 4]  = COEFF'('sh000e6);
        assign taps[ 5]  = COEFF'('sh0000a);
        assign taps[ 6]  = COEFF'('sh3feb1);
        assign taps[ 7]  = COEFF'('sh3ff63);
        assign taps[ 8]  = COEFF'('sh001ad);
        assign taps[ 9]  = COEFF'('sh00199);
        assign taps[10]  = COEFF'('sh3fe3e);
        assign taps[11]  = COEFF'('sh3fcfe);
        assign taps[12]  = COEFF'('sh0013d);
        assign taps[13]  = COEFF'('sh004b1);
        assign taps[14]  = COEFF'('sh00036);
        assign taps[15]  = COEFF'('sh3f9b3);
        assign taps[16]  = COEFF'('sh3fd2b);
        assign taps[17]  = COEFF'('sh0074d);
        assign taps[18]  = COEFF'('sh006b9);
        assign taps[19]  = COEFF'('sh3f904);
        assign taps[20]  = COEFF'('sh3f444);
        assign taps[21]  = COEFF'('sh00481);
        assign taps[22]  = COEFF'('sh01177);
        assign taps[23]  = COEFF'('sh00126);
        assign taps[24]  = COEFF'('sh3e8ca);
        assign taps[25]  = COEFF'('sh3f490);
        assign taps[26]  = COEFF'('sh01bd5);
        assign taps[27]  = COEFF'('sh01d5a);
        assign taps[28]  = COEFF'('sh3e30e);
        assign taps[29]  = COEFF'('sh3bf82);
        assign taps[30]  = COEFF'('sh00f77);
        assign taps[31]  = COEFF'('sh0ac26);
        assign taps[32]  = COEFF'('sh0fd54);
    end else if (RX_CFG == 14) begin            // N=5, R=3, M=1, NTAPS=17, 12 kHz, cutoff at 5.35 kHz (90%)
        assign taps[ 0]  = COEFF'('sh001dd);
        assign taps[ 1]  = COEFF'('sh001dd);
        assign taps[ 2]  = COEFF'('sh001dd);
        assign taps[ 3]  = COEFF'('sh3f290);
        assign taps[ 4]  = COEFF'('sh3ee98);
        assign taps[ 5]  = COEFF'('sh006e8);
        assign taps[ 6]  = COEFF'('sh04205);
        assign taps[ 7]  = COEFF'('sh084ab);
        assign taps[ 8]  = COEFF'('sh0a235);
     end else begin                             // N=5, R=3, M=1, NTAPS=65, 12 kHz, cutoff at 5.35 kHz (90%)
        assign taps[ 0]  = COEFF'('sh00071);
        assign taps[ 1]  = COEFF'('sh3ffae);
        assign taps[ 2]  = COEFF'('sh3ff5b);
        assign taps[ 3]  = COEFF'('sh00029);
        assign taps[ 4]  = COEFF'('sh000f6);
        assign taps[ 5]  = COEFF'('sh0002a);
        assign taps[ 6]  = COEFF'('sh3fea6);
        assign taps[ 7]  = COEFF'('sh3ff32);
        assign taps[ 8]  = COEFF'('sh001aa);
        assign taps[ 9]  = COEFF'('sh001dc);
        assign taps[10]  = COEFF'('sh3fe5a);
        assign taps[11]  = COEFF'('sh3fcae);
        assign taps[12]  = COEFF'('sh000fb);
        assign taps[13]  = COEFF'('sh00503);
        assign taps[14]  = COEFF'('sh000a7);
        assign taps[15]  = COEFF'('sh3f96f);
        assign taps[16]  = COEFF'('sh3fc85);
        assign taps[17]  = COEFF'('sh0076b);
        assign taps[18]  = COEFF'('sh00793);
        assign taps[19]  = COEFF'('sh3f927);
        assign taps[20]  = COEFF'('sh3f33f);
        assign taps[21]  = COEFF'('sh00401);
        assign taps[22]  = COEFF'('sh01296);
        assign taps[23]  = COEFF'('sh00227);
        assign taps[24]  = COEFF'('sh3e7b0);
        assign taps[25]  = COEFF'('sh3f2dd);
        assign taps[26]  = COEFF'('sh01caf);
        assign taps[27]  = COEFF'('sh0200e);
        assign taps[28]  = COEFF'('sh3e310);
        assign taps[29]  = COEFF'('sh3bb4f);
        assign taps[30]  = COEFF'('sh00c0e);
        assign taps[31]  = COEFF'('sh0aeac);
        assign taps[32]  = COEFF'('sh1036e);
    end

    reg decim_by_2;
    reg [$clog2(NTAPS)-1:0] tap;
    initial decim_by_2 = 0;
    initial $display("#################### RX_CFG=%d NTAPS=%d $bits(tap)=%d ####################", RX_CFG, NTAPS, $bits(tap));

    /* Definitions for the FSM */
    typedef enum {
        STATE_IDLE,
        STATE_LATCH_INPUT,
        STATE_COMPUTE_SUM,
        STATE_COPY_OUTPUT
    } state_t;
    state_t state, next_state;
    initial state = STATE_IDLE;

    /* Implement the FSM */
    always_ff @(posedge adc_clk) begin
        if (reset) begin
            state <= STATE_IDLE;
        end else begin
            state <= next_state;
        end
    end

    /* Actions at state transitions */
    always_ff @(posedge adc_clk) begin
        if (reset) begin
            // when not enabled, pass through every second sample
            out_data_i <= in_data_i;
            out_data_q <= in_data_q;
            out_strobe <= in_strobe & decim_by_2;
            decim_by_2 <= decim_by_2 ^ in_strobe;
        end else begin
            if (next_state == STATE_IDLE) begin
                out_strobe <= 0;
            end else if (state == STATE_IDLE && next_state == STATE_LATCH_INPUT) begin
                // latch input
                bufI <= {bufI[NTAPS-2:0], in_data_i};
                bufQ <= {bufQ[NTAPS-2:0], in_data_q};
            end else if (state == STATE_LATCH_INPUT && next_state == STATE_COMPUTE_SUM) begin
                // NOP
            end else if (state == STATE_COMPUTE_SUM && next_state == STATE_COPY_OUTPUT) begin
                decim_by_2 <= decim_by_2 ^ 1'b1; // toggle
                out_data_i <= accI[ACCOUT -:WIDTH];
                out_data_q <= accQ[ACCOUT -:WIDTH];
                out_strobe <= decim_by_2;
            end
        end
    end

    /* Accumulate the sum */
    always_ff @(posedge adc_clk) begin
        if (reset) begin
            tap <= 0;
            accI <= 0;
            accQ <= 0;
        end else begin
            if (state == STATE_COMPUTE_SUM && next_state != STATE_COPY_OUTPUT) begin
                if (tap <= (NTAPS-1)/2) begin
                // if (~|tap[$bits(tap)-1:$bits(tap)-2]) begin
                    accI <= accI + (bufI[tap] * taps[tap]);
                    accQ <= accQ + (bufQ[tap] * taps[tap]);
                end else begin
                    accI <= accI + (bufI[tap] * taps[NTAPS-1-tap]);
                    accQ <= accQ + (bufQ[tap] * taps[NTAPS-1-tap]);
                end
                tap <= tap + 1'b1;
            end else begin
                tap <= 0;
                accI <= 0;
                accQ <= 0;
            end
        end
    end

    /* FSM next-state logic */
    always_comb begin
        next_state = state;

        unique case (state)
            STATE_IDLE: begin
                if (in_strobe) begin
                    next_state = STATE_LATCH_INPUT;
                end
            end

            STATE_LATCH_INPUT: begin
                next_state = STATE_COMPUTE_SUM;
            end

            STATE_COMPUTE_SUM: begin
                 if (tap == NTAPS) begin
                    next_state = STATE_COPY_OUTPUT;
                 end
            end

            STATE_COPY_OUTPUT: begin
                next_state = STATE_IDLE;
            end

            default:
                next_state = STATE_IDLE;

        endcase
    end

endmodule
