`ifndef _KIWI_VH_
`define _KIWI_VH_

`include "kiwi.cfg.vh"
`include "kiwi.gen.vh"

parameter V_RX_CHANS = (RX_CFG == 4)? 4 : ((RX_CFG == 8)? 8 : ((RX_CFG == 3)? 3 : ((RX_CFG == 14)? 14 : 0)));
parameter V_WF_CHANS = (RX_CFG == 4)? 4 : ((RX_CFG == 8)? 2 : ((RX_CFG == 3)? 3 : ((RX_CFG == 14)?  0 : 0)));

parameter RXBUF_SIZE = (RX_CFG == 4)? RXBUF_SIZE_4CH : ((RX_CFG == 8)? RXBUF_SIZE_8CH : ((RX_CFG == 3)? RXBUF_SIZE_3CH : ((RX_CFG == 14)? RXBUF_SIZE_14CH : 0)));
parameter RXBUF_LARGE = (RX_CFG == 4)? 0 : ((RX_CFG == 8)? 1 : ((RX_CFG == 3)? 1 : ((RX_CFG == 14)? 2 : 0)));

parameter RX1_DECIM = (RX_CFG == 4)? RX1_12K_DECIM : ((RX_CFG == 8)? RX1_12K_DECIM : ((RX_CFG == 3)? RX1_20K_DECIM : ((RX_CFG == 14)? RX1_12K_DECIM : 0)));
parameter RX2_DECIM = (RX_CFG == 4)? RX2_12K_DECIM : ((RX_CFG == 8)? RX2_12K_DECIM : ((RX_CFG == 3)? RX2_20K_DECIM : ((RX_CFG == 14)? RX2_12K_DECIM : 0)));

parameter RX1_GROWTH = (RX_CFG == 4)? RX1_12K_GROWTH : ((RX_CFG == 8)? RX1_12K_GROWTH : ((RX_CFG == 3)? RX1_20K_GROWTH : ((RX_CFG == 14)? RX1_12K_GROWTH : 0)));
parameter RX2_GROWTH = (RX_CFG == 4)? RX2_12K_GROWTH : ((RX_CFG == 8)? RX2_12K_GROWTH : ((RX_CFG == 3)? RX2_20K_GROWTH : ((RX_CFG == 14)? RX2_12K_GROWTH : 0)));

parameter FPGA_ID = (RX_CFG == 4)? FPGA_ID_RX4_WF4 : ((RX_CFG == 8)? FPGA_ID_RX8_WF2 : ((RX_CFG == 3)? FPGA_ID_RX3_WF3 : ((RX_CFG == 14)? FPGA_ID_RX14_WF0 : 0)));

// rst[2:1]
parameter LOAD = 1;
parameter RUN = 2;

parameter ASSERT_MSGLEN = 64;

// use like this: localparam ASSERT_cond = assert(cond, "cond");
function integer assert(input integer cond, input reg [ASSERT_MSGLEN*8:1] msg);
	begin
		if (cond == 0) begin
			$error("assertion failed: %s", msg);
			assert = 0;
		end else
		begin
			assert = 1;
		end
	end 
endfunction

function integer assert_eq(input integer v1, input integer v2, input reg [ASSERT_MSGLEN*8:1] msg);
	begin
		if (v1 != v2) begin
			$error("%d != %d: %s", v1, v2, msg);
			assert_eq = 0;
		end else
		begin
			assert_eq = 1;
		end
	end 
endfunction

// ceil(log2(value)) e.g. clog2(9) = 4, clog2(8) = 3, clog2(7) = 3
// but with special cases clog2(0|1) = 1 instead of 0
function integer clog2(input integer value);
	begin
		if (value <= 1) begin
			clog2 = 1;
		end else
		begin
			value = value-1;
			for (clog2=0; value > 0; clog2 = clog2+1)
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

`endif
