

// from verilog/kiwi.inline.vh

`include "kiwi.cfg.vh"

// Done this way because make_proj.tcl batch script modifies kiwi.cfg.vh for each build mode (e.g. rx4wf4)

localparam V_RX_CHANS = (RX_CFG == 4)? 4 : ((RX_CFG == 8)? 8 : ((RX_CFG == 3)? 3 : ((RX_CFG == 14)? 14 : ((RX_CFG == 1)? 1 : 0))));
localparam V_WF_CHANS = (RX_CFG == 4)? 4 : ((RX_CFG == 8)? 2 : ((RX_CFG == 3)? 3 : ((RX_CFG == 14)?  0 : ((RX_CFG == 1)? 1 : 0))));

localparam V_GPS_CHANS = (RX_CFG == 4)? GPS_MAX_CHANS : ((RX_CFG == 8)? GPS_MAX_CHANS : ((RX_CFG == 3)? GPS_MAX_CHANS : ((RX_CFG == 14)?  GPS_RX14_CHANS : ((RX_CFG == 1)?  GPS_MAX_CHANS : 0))));

localparam RXBUF_SIZE = (RX_CFG == 4)? RXBUF_SIZE_4CH : ((RX_CFG == 8)? RXBUF_SIZE_8CH : ((RX_CFG == 3)? RXBUF_SIZE_3CH : ((RX_CFG == 14)? RXBUF_SIZE_14CH : ((RX_CFG == 1)? RXBUF_SIZE_WB : 0))));
localparam RXBUF_LARGE = (RX_CFG == 4)? RXBUF_LARGE_4CH : ((RX_CFG == 8)? RXBUF_LARGE_8CH : ((RX_CFG == 3)? RXBUF_LARGE_3CH : ((RX_CFG == 14)? RXBUF_LARGE_14CH : ((RX_CFG == 1)? RXBUF_LARGE_WB : 0))));

localparam RX1_DECIM = (RX_CFG == 4)? RX1_STD_DECIM : ((RX_CFG == 8)? RX1_STD_DECIM : ((RX_CFG == 3)? RX1_WIDE_DECIM : ((RX_CFG == 14)? RX1_STD_DECIM : ((RX_CFG == 1)? RX1_WB_DECIM : 0))));
localparam RX2_DECIM = (RX_CFG == 4)? RX2_STD_DECIM : ((RX_CFG == 8)? RX2_STD_DECIM : ((RX_CFG == 3)? RX2_WIDE_DECIM : ((RX_CFG == 14)? RX2_STD_DECIM : ((RX_CFG == 1)? RX2_WB_DECIM : 0))));

localparam FPGA_ID = (RX_CFG == 4)? FPGA_ID_RX4_WF4 : ((RX_CFG == 8)? FPGA_ID_RX8_WF2 : ((RX_CFG == 3)? FPGA_ID_RX3_WF3 : ((RX_CFG == 14)? FPGA_ID_RX14_WF0 : ((RX_CFG == 1)? FPGA_ID_WB : FPGA_ID_OTHER))));

// rst[2:1]
localparam LOAD = 1;
localparam RUN = 2;

function integer assert_cond(input integer cond);
	begin
		if (cond == 0) begin
			$display("assertion failed");
			$finish(1);
			assert_cond = 0;
		end else
		begin
			assert_cond = 1;
		end
	end 
endfunction

function integer assert_zero(input integer cond);
	begin
		if (cond != 0) begin
			$display("assertion failed");
			$finish(1);
			assert_zero = 0;
		end else
		begin
			assert_zero = 1;
		end
	end 
endfunction

// valid only when value is power of 2
function integer clog2(input integer value);
	begin
		if (value <= 1) begin
			clog2 = 1;
		end else
		begin
			value = value-1;
			for (clog2=0; value>0; clog2=clog2+1)
				value = value >> 1;
		end
	end 
endfunction

function integer max(input integer v1, input integer v2);
	begin
		if (v1 >= v2) begin
			max = v1;
		end else
			max = v2;
	end 
endfunction

function integer min(input integer v1, input integer v2);
	begin
		if (v1 <= v2) begin
			min = v1;
		end else
			min = v2;
	end 
endfunction
