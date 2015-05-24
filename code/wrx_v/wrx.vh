`ifndef _WRX_VH_
`define _WRX_VH_

`include "wrx.gen.vh"

function integer assert(input integer cond);
	begin
		if (!cond) begin
			$display("assertion failed");
			$finish(1);
			assert = 0;
		end else
		begin
			assert = 0;
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
