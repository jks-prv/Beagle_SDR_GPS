`ifndef _KIWI_VH_
`define _KIWI_VH_

`include "kiwi.cfg.vh"
`include "kiwi.gen.vh"

parameter V_RX_CHANS = (RX_CFG == 4)? 4 : ((RX_CFG == 8)? 8 : ((RX_CFG == 3)? 3 : ((RX_CFG == 14)? 14 : 0)));
parameter V_WF_CHANS = (RX_CFG == 4)? 4 : ((RX_CFG == 8)? 2 : ((RX_CFG == 3)? 3 : ((RX_CFG == 14)?  0 : 0)));

parameter RXBUF_SIZE = (RX_CFG == 4)? RXBUF_SIZE_4CH : ((RX_CFG == 8)? RXBUF_SIZE_8CH : ((RX_CFG == 3)? RXBUF_SIZE_3CH : ((RX_CFG == 14)? RXBUF_SIZE_14CH : 0)));
parameter RXBUF_LARGE = (RX_CFG == 4)? 0 : ((RX_CFG == 8)? 1 : ((RX_CFG == 3)? 1 : ((RX_CFG == 14)? 2 : 0)));

parameter RX1_DECIM = (RX_CFG == 4)? RX1_STD_DECIM : ((RX_CFG == 8)? RX1_STD_DECIM : ((RX_CFG == 3)? RX1_WIDE_DECIM : ((RX_CFG == 14)? RX1_STD_DECIM : 0)));
parameter RX2_DECIM = (RX_CFG == 4)? RX2_STD_DECIM : ((RX_CFG == 8)? RX2_STD_DECIM : ((RX_CFG == 3)? RX2_WIDE_DECIM : ((RX_CFG == 14)? RX2_STD_DECIM : 0)));

parameter FPGA_ID = (RX_CFG == 4)? FPGA_ID_RX4_WF4 : ((RX_CFG == 8)? FPGA_ID_RX8_WF2 : ((RX_CFG == 3)? FPGA_ID_RX3_WF3 : ((RX_CFG == 14)? FPGA_ID_RX14_WF0 : 0)));

// rst[2:1]
parameter LOAD = 1;
parameter RUN = 2;

function integer assert(input integer cond);
	begin
		if (cond == 0) begin
			$display("assertion failed");
			$finish(1);
			assert = 0;
		end else
		begin
			assert = 1;
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

`endif
