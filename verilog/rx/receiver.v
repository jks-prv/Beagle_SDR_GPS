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

// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO

module receiver (
	input wire		   adc_clk,
	input wire signed [ADC_BITS-1:0] adc_data,
	input wire         adc_ovfl,

    output wire        rx_rd_C,
    output wire [15:0] rx_dout_C,

    output wire        wf_rd_C,
    output wire [15:0] wf_dout_C,

    input  wire [47:0] ticks_A,
    output wire        adc_ovfl_C,
    output wire [31:0] adc_count_C,
    
	input  wire		   cpu_clk,
    output wire        ser,
    input  wire [31:0] tos,
    input  wire [10:0] op_11,
    input  wire        rdReg,
    input  wire        wrReg,
    input  wire        wrReg2,
    input  wire        wrEvt2,
    
    input  wire        use_gen_C,
    
    input  wire        self_test_en_C,
    output wire        self_test
	);
	
`include "kiwi.gen.vh"

	wire set_rx_chan_C  =  wrReg2 & op_11[SET_RX_CHAN];
	wire freq_l         =  wrReg2 & op_11[FREQ_L];
	wire set_rx_freqH_C = (wrReg2 & op_11[SET_RX_FREQ]) && !freq_l;
	wire set_rx_freqL_C = (wrReg2 & op_11[SET_RX_FREQ]) &&  freq_l;
	
	wire set_wf_chan_C  =  wrReg2 & op_11[SET_WF_CHAN];
	wire set_wf_freqH_C = (wrReg2 & op_11[SET_WF_FREQ]) && !freq_l;
	wire set_wf_freqL_C = (wrReg2 & op_11[SET_WF_FREQ]) &&  freq_l;
	wire set_wf_decim_C =  wrReg2 & op_11[SET_WF_DECIM];

	// The FREEZE_TOS event starts the process of latching and synchronizing of the ecpu TOS data
	// from the cpu_clk to the adc_clk domain. This is needed by subsequent wrReg instructions
	// that want to load TOS values into registers clocked in the adc_clk domain.
	//
	// e.g. an ecpu code sequence like this:
	//		rdReg HOST_RX; wrEvt2 FREEZE_TOS; nop; nop; wrReg2 SET_WF_FREQ

	wire freeze_C = wrEvt2 & op_11[FREEZE_TOS];

	wire [31:0] freeze_tos_A;
	SYNC_REG #(.WIDTH(32)) sync_latch_tos (
	    .in_strobe(freeze_C),   .in_reg(tos),               .in_clk(cpu_clk),
	    .out_strobe(),          .out_reg(freeze_tos_A),     .out_clk(adc_clk)
	);


    //////////////////////////////////////////////////////////////////////////
    // ADC overflow detection
    //////////////////////////////////////////////////////////////////////////
    
    // signal overflow only if a variable value of 64k consecutive samples have ADC overflow asserted
    localparam ADC_OVFL_CTR_BITS = 16;
	reg  [ADC_OVFL_CTR_BITS-1:0] adc_ovfl_ctr, adc_ovfl_cnt, cnt_mask_A;
    reg adc_ovfl_A;

	wire set_cnt_mask_C = wrReg & op_11[SET_CNT_MASK];
	wire set_cnt_mask_A;
	SYNC_PULSE sync_set_cnt_mask (.in_clk(cpu_clk), .in(set_cnt_mask_C), .out_clk(adc_clk), .out(set_cnt_mask_A));
    always @ (posedge adc_clk)
        if (set_cnt_mask_A) cnt_mask_A <= freeze_tos_A[ADC_OVFL_CTR_BITS-1:0];

    always @ (posedge adc_clk)
    begin
        if (adc_ovfl_ctr == ((1 << ADC_OVFL_CTR_BITS) - 1))
        begin
            adc_ovfl_A <= ((adc_ovfl_cnt & cnt_mask_A) != 0)? 1:0;
            adc_ovfl_cnt <= 0;
            adc_ovfl_ctr <= 0;
        end else
        begin
            adc_ovfl_A <= 0;
            adc_ovfl_cnt <= adc_ovfl_cnt + adc_ovfl;
            adc_ovfl_ctr <= adc_ovfl_ctr + 1;
        end
    end

	SYNC_PULSE sync_adc_ovfl (.in_clk(adc_clk), .in(adc_ovfl_A), .out_clk(cpu_clk), .out(adc_ovfl_C));


    //////////////////////////////////////////////////////////////////////////
    // ADC level detection
    //////////////////////////////////////////////////////////////////////////

    wire [ADC_BITS-1:0] adc_data_abs = (adc_data[ADC_BITS-1]? (-adc_data) : adc_data);
    wire [ADC_BITS-2:0] adc_data_mag = adc_data_abs[ADC_BITS-2:0];
    reg [ADC_BITS-2:0] adc_level_A;
	wire set_adc_lvl_C = wrReg & op_11[SET_ADC_LVL];
    wire set_adc_lvl_A;
	SYNC_PULSE sync_set_adc_lvl (.in_clk(cpu_clk), .in(set_adc_lvl_C), .out_clk(adc_clk), .out(set_adc_lvl_A));
	reg adc_lvl_ovfl;
    always @ (posedge adc_clk)
        // 11 11
        // 32 1098 7654 3210
        // wb bbbb bbbb bbbb = (1,13[12:0]) = (1,ADC_BITS-1[ADC_BITS-2:0])
        if (set_adc_lvl_A) { adc_lvl_ovfl, adc_level_A } <= freeze_tos_A[ADC_BITS-1:0];

    reg [31:0] adc_count_A;
    always @ (posedge adc_clk)
    begin
        if (!set_adc_lvl_A && !adc_lvl_ovfl && (adc_data_mag >= adc_level_A)) adc_count_A <= adc_count_A + 1'b1;
        if (!set_adc_lvl_A &&  adc_lvl_ovfl)                                  adc_count_A <= adc_count_A + adc_ovfl;
        //if (!set_adc_lvl_A) adc_count_A <= adc_level_A;     // loopback test
        else
        if (set_adc_lvl_A) adc_count_A <= 0;
    end

    // continuously sync adc_count_A => adc_count_C
	SYNC_REG #(.WIDTH(32)) sync_adc_count (
	    .in_strobe(1),      .in_reg(adc_count_A),       .in_clk(adc_clk),
	    .out_strobe(),      .out_reg(adc_count_C),      .out_clk(cpu_clk)
	);


    //////////////////////////////////////////////////////////////////////////
    // optional signal gen
    //////////////////////////////////////////////////////////////////////////
    
`ifdef USE_GEN
    localparam RX_IN_WIDTH = 18;

    wire use_gen_A;
    SYNC_WIRE sync_use_gen (.in(use_gen_C && !self_test_en_C), .out_clk(adc_clk), .out(use_gen_A));

    wire signed [RX_IN_WIDTH-1:0] gen_data;
    wire signed [RX_IN_WIDTH-1:0] adc_ext_data = { adc_data, {RX_IN_WIDTH-ADC_BITS{1'b0}} };

    // only allow gen to be used on channel 0 to prevent disruption to others when multiple channels in use
    wire [(V_RX_CHANS * RX_IN_WIDTH)-1:0] rx_data = { {V_RX_CHANS-1{adc_ext_data}}, use_gen_A? gen_data : adc_ext_data };
`ifdef USE_WF
    wire [(V_WF_CHANS * RX_IN_WIDTH)-1:0] wf_data = { {V_WF_CHANS-1{adc_ext_data}}, use_gen_A? gen_data : adc_ext_data };
`endif
    
    assign self_test = gen_data[RX_IN_WIDTH-1] & self_test_en_C;

    wire set_gen_freqH_C = (wrReg2 & op_11[SET_GEN_FREQ]) && !freq_l;
    wire set_gen_freqL_C = (wrReg2 & op_11[SET_GEN_FREQ]) &&  freq_l;
    wire set_gen_attn_C  =	wrReg2 & op_11[SET_GEN_ATTN];

    GEN gen_inst (
        .adc_clk	        (adc_clk),
        .gen_data	        (gen_data),

        .cpu_clk	        (cpu_clk),
        .freeze_tos_A       (freeze_tos_A),

        .set_gen_freqH_C    (set_gen_freqH_C),
        .set_gen_freqL_C    (set_gen_freqL_C),
        .set_gen_attn_C     (set_gen_attn_C)
    );
`else
	localparam RX_IN_WIDTH = ADC_BITS;
	
	wire [RX_IN_WIDTH-1:0] rx_data = adc_data;
`ifdef USE_WF
	wire [RX_IN_WIDTH-1:0] wf_data = adc_data;
`endif
        
    assign self_test = 0;

`endif
	

    //////////////////////////////////////////////////////////////////////////
	// rx audio channels
    //////////////////////////////////////////////////////////////////////////
	
    localparam L2RX = max(1, clog2(V_RX_CHANS) - 1);
    reg [L2RX:0] rx_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_rx_chan_C) rx_channel_C <= tos[L2RX:0];
    end

	wire [V_RX_CHANS-1:0] rxn_sel_C = 1 << rx_channel_C;

	wire [V_RX_CHANS-1:0] rxn_avail_A;
	wire [V_RX_CHANS*16-1:0] rxn_data_A;
	
	// Verilog note: if rd_getI & rd_getQ are not declared before use in arrayed module RX below
	// then automatic fanout of single-bit signal to all RX instances doesn't occur and
	// an "undriven" error for rd_* results.
	wire rd_getI, rd_getQ;

	rx #(.IN_WIDTH(RX_IN_WIDTH)) rx_inst [V_RX_CHANS-1:0] (
		.adc_clk		(adc_clk),
		.adc_data		(rx_data),
		.rd_getI        (rd_getI),
		.rd_getQ        (rd_getQ),
		// o
		.rx_avail_A		(rxn_avail_A),
		.rx_dout_A		(rxn_data_A),

		.cpu_clk		(cpu_clk),
		.freeze_tos_A   (freeze_tos_A),
		.rx_sel_C		(rxn_sel_C),
		.set_rx_freqH_C	(set_rx_freqH_C),
		.set_rx_freqL_C	(set_rx_freqL_C)
	);


    //////////////////////////////////////////////////////////////////////////
	// rx audio shared sample memory
    //////////////////////////////////////////////////////////////////////////

	wire set_rx_nsamps_C = wrReg2 & op_11[SET_RX_NSAMPS];
    wire get_rx_srq_C    = rdReg  & op_11[GET_RX_SRQ];
	wire get_rx_samp_C   = wrEvt2 & op_11[GET_RX_SAMP];
	wire reset_bufs_C    = wrEvt2 & op_11[RX_BUFFER_RST];
	wire get_buf_ctr_C   = wrEvt2 & op_11[RX_GET_BUF_CTR];

	wire set_nsamps_A;
	SYNC_PULSE sync_set_nsamps_A (.in_clk(cpu_clk), .in(set_rx_nsamps_C), .out_clk(adc_clk), .out(set_nsamps_A));
    reg [7:0] nrx_samps_A;
    always @ (posedge adc_clk)
        if (set_nsamps_A) nrx_samps_A <= freeze_tos_A;
    
	reg [47:0] ticks_latched_A;
	always @ (posedge adc_clk)
		if (rxn_avail_A[0])
		    ticks_latched_A <= ticks_A;

	rx_audio_mem rx_audio_mem_inst (
		.adc_clk		(adc_clk),
		.nrx_samps      (nrx_samps_A),
		.rx_avail_A     (rxn_avail_A[0]),   // all DDCs should signal available at the same time since decimation is the same
		.rxn_din_A      (rxn_data_A),
		.ticks_A        (ticks_latched_A),
        // o
		.ser            (ser),
		.rd_getI        (rd_getI),
		.rd_getQ        (rd_getQ),
		
		.cpu_clk        (cpu_clk),
		.get_rx_srq_C   (get_rx_srq_C),
		.get_rx_samp_C  (get_rx_samp_C),
		.reset_bufs_C   (reset_bufs_C),
		.get_buf_ctr_C  (get_buf_ctr_C),
		// o
		.rx_rd_C        (rx_rd_C),
		.rx_dout_C      (rx_dout_C)
    );


    //////////////////////////////////////////////////////////////////////////
    // waterfall(s)
    //////////////////////////////////////////////////////////////////////////

`ifdef USE_WF
    localparam L2WF = max(1, clog2(V_WF_CHANS) - 1);
    reg [L2WF:0] wf_channel_C;
	wire [V_WF_CHANS-1:0] wfn_sel_C = 1 << wf_channel_C;
	
    always @ (posedge cpu_clk)
    begin
    	if (set_wf_chan_C) wf_channel_C <= tos[L2WF:0];
    end
    
	wire [V_WF_CHANS*16-1:0] wfn_dout_C;
	MUX #(.WIDTH(16), .SEL(V_WF_CHANS)) wf_dout_mux(.in(wfn_dout_C), .sel(wf_channel_C), .out(wf_dout_C));

	wire rst_wf_sampler_C =	wrReg2 & op_11[WF_SAMPLER_RST];
	wire get_wf_samp_i_C =	wrEvt2 & op_11[GET_WF_SAMP_I];
	wire get_wf_samp_q_C =	wrEvt2 & op_11[GET_WF_SAMP_Q];
	assign wf_rd_C =		get_wf_samp_i_C || get_wf_samp_q_C;

	wire samp_wf_rd_rst_C = tos[WF_SAMP_RD_RST];
	wire samp_wf_sync_C   = tos[WF_SAMP_SYNC];

`ifdef USE_WF_1CIC
	WATERFALL_1CIC #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst [V_WF_CHANS-1:0] (
`else
	WATERFALL #(.IN_WIDTH(RX_IN_WIDTH)) waterfall_inst [V_WF_CHANS-1:0] (
`endif
		.adc_clk			(adc_clk),
		.adc_data			(wf_data),
		
		.wf_sel_C			(wfn_sel_C),
		.wf_dout_C			(wfn_dout_C),

		.cpu_clk			(cpu_clk),
		.freeze_tos_A       (freeze_tos_A),

		.samp_wf_rd_rst_C   (samp_wf_rd_rst_C),
		.samp_wf_sync_C		(samp_wf_sync_C),
		.set_wf_freqH_C		(set_wf_freqH_C),
		.set_wf_freqL_C		(set_wf_freqL_C),
		.set_wf_decim_C		(set_wf_decim_C),
		.rst_wf_sampler_C	(rst_wf_sampler_C),
		.get_wf_samp_i_C	(get_wf_samp_i_C),
		.get_wf_samp_q_C	(get_wf_samp_q_C)
	);
`else
    assign wf_rd_C = 0;
    assign wf_dout_C = 16'b0;
`endif

endmodule
