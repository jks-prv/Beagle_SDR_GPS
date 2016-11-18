`ifndef _KIWI_VH_
`define _KIWI_VH_

`include "kiwi.gen.vh"

// rst[2:1]
localparam LOAD = 1;
localparam RUN = 2;

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

`endif
