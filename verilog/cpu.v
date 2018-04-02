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

`include "kiwi.vh"

module CPU (
    input  wire        clk,
    input  wire [2:1]  rst,
    
    input  wire [15:0] par,
    input  wire [1:0]  ser,
    input  wire        mem_rd,      // current insn is a mem read which should ++ the mem addr in TOS
    output wire [15:0] mem_dout,
    input  wire        boot_done,
    
    output reg  [31:0] tos,
    output wire [15:0] op,
    output wire        rdBit,
    output wire        rdBit2,
    output wire        rdReg,
    output wire        rdReg2,
    output wire        wrReg,
    output wire        wrReg2,
    output wire        wrEvt,
    output wire        wrEvt2
	);

    //////////////////////////////////////////////////////////////////////////
    // Instruction set
    //
    //	1111 110000000000
    //	5432 109876543210
    
    //	0... ............ push 0..0x7fff

    //	bbbb bbbb-------- op8 [7:0]
    //	100p ppppR....... alu insns, R = rtn
    //	100p ppppRiiiiiii  addi, imm [6:0] 0-127
    //	100p ppppRCxxxxxx  add, C = carry-in
    //	100p ppppRSxxxxxx  rdBit, S selects 'ser' bit input
    //	100p ppppRxxxxxxx  others, EXCEPT illegal for: r, r_from, to_r

	//	bbbb xxxxxxxxxxxb op5 [15:0]
    //	1010 ddddddddddd0 call 11:1 (:1 because of 16-bit insn width)
    //	1010 ddddddddddd1 BR 11:1
    //	1011 ddddddddddd0 BZ 11:1
    //	1011 ddddddddddd1 BN 11:1

    //	bbbb ------------ op4 [3:0]
    //	1100 0........... rdReg 10:0
    //	1100 1........... rdReg2 10:0
    //	1101 0........... wrReg 10:0
    //	1101 1........... wrReg2 10:0
    //	1110 0........... wrEvt 10:0
    //	1110 1........... wrEvt2 10:0

    localparam op_nop       = 16'h8000,
               op_dup       =  8'h81,
               op_swap      =  8'h82,
               op_swap16    =  8'h83,
               op_over      =  8'h84,
               op_drop      =  8'h85,
               op_rot       =  8'h86,
               op_addi      =  8'h87,        // op[6:0] = Immediate operand
               op_add       =  8'h88,
               op_sub       =  8'h89,
               op_mult      =  8'h8A,
               op_and       =  8'h8B,
               op_or        =  8'h8C,
               op_xor       =  8'h8D,
               op_not       =  8'h8E,
               op_mult18    =  8'h8F,

               op_shl64     =  8'h90,
               op_shl       =  8'h91,
               op_shr       =  8'h92,
               op_rdBit     =  8'h93,
               op_fetch16   =  8'h94,
               op_store16   =  8'h95,        // leaves address+2 ( d a --> a+2 )

               op_sp        =  8'h96,		// STACK_CHECK
               op_rp        =  8'h97,

               op_r         =  8'h9C,
               op_r_from    =  8'h9D,
               op_to_r      =  8'h9E,

               op_call      = 16'hA000,      // op[11:1] = Destination PC
               op_branch    = 16'hA001,      // ditto
               op_branchZ   = 16'hB000,      // ditto
               op_branchNZ  = 16'hB001,      // ditto
               
               op_rdReg     =  4'hC,         // op[10:0] = I/O selects
               op_wrReg     =  4'hD,         // ditto
               op_wrEvt     =  4'hE;         // ditto

    wire op_push = ~op[15]; // op[14:0] --> TOS
    
    wire opt_ret = op[7] && op[15:13]==3'b100;
    wire opt_cin = op[6];
    wire ser_sel = op[6];
    wire serial = ser_sel? ser[1] : ser[0];

    //////////////////////////////////////////////////////////////////////////
    // Instruction decode

    wire [ 3:0] op4 = op[15:12];
    wire [15:0] op5 = op & 16'hF001;
    wire [ 7:0] op8 = op[15: 8];

    wire nz = |tos[15:0];

    wire jump = op5==op_branchNZ && nz || op5==op_branch ||
                op5==op_branchZ && ~nz || op5==op_call;

    wire inc_sp = op_push || op4==op_rdReg || op8==op_dup  || op8==op_r
                                           || op8==op_over || op8==op_r_from;

    wire dec_sp = op4==op_wrReg    || op8==op_drop || op8==op_and || op8==op_mult ||
                  op5==op_branchZ  || op8==op_add  || op8==op_or  || op8==op_to_r ||
                  op5==op_branchNZ || op8==op_sub  || op8==op_xor || op8==op_store16;

    wire inc_rp = op8==op_to_r   || op5==op_call;
    wire dec_rp = op8==op_r_from || opt_ret;

    wire dstk_wr = op8==op_rot  || inc_sp;
    wire rstk_wr = op8==op_to_r || op5==op_call;

    //////////////////////////////////////////////////////////////////////////
    // Next on stack

    reg  [31:0] nos;
    wire [31:0] dstk_dout;

    always @ (posedge clk)
        case (op8)
            op_swap, op_rot    : nos <= tos;
            op_shl64           : nos <= {nos[30:0], tos[31]};
          //op_mult18          : nos <= {{28{nos_x_tos[35]}}, nos_x_tos[35:32]};    // 64-bit sign extend
            op_mult18          : nos <= {{24{prod40[39]}}, prod40[39:32]};          // 64-bit sign extend
            default :
                if      (inc_sp) nos <= tos;
                else if (dec_sp) nos <= dstk_dout;
        endcase

    //////////////////////////////////////////////////////////////////////////
    // ALU

    reg [17:0] xa, xb;
    wire [35:0] nos_x_tos;
    wire [19:0] xa20, xb20;
    wire [39:0] prod40;
    wire [31:0] sum;
    wire co;
    wire ci = (op8==op_add && opt_cin && carry) || op8==op_sub;
    reg  [31:0] a, b, alu;
    reg         carry;

	// fixme: use a c_out instead and reduce s width to 32 from 33?
	// FIXME: combine adder & multipler into a single DSP slice
    ip_add_u32b cpu_sum (.a(a), .b(b), .s({co, sum}), .c_in(ci));

    always @ (posedge clk) if (op8==op_add) carry <= co;

    always @*
        if (op8==op_addi)	a = op[6:0];
        else if (mem_rd)	a = 2;
        else				a = nos;

    always @*
        if (op8==op_sub)	b = ~tos;
        else				b =  tos;

    always @*
        if     (op_push)    alu = op;       // side-effect alu[31:16] <= 0
        else if (mem_rd)    alu = sum;
        else case (op8)
            op_add, op_addi,
            op_sub          : alu = sum;
          //op_mult,
          //op_mult18       : alu = nos_x_tos[31:0];
            op_mult         : alu = nos_x_tos[31:0];
            op_mult18       : alu = prod40[31:0];
            op_and          : alu = nos & tos;
            op_or           : alu = nos | tos;
//			op_xor          : alu = nos ^ tos;
			op_not          : alu =     ~ tos;
            op_shl, op_shl64: alu = {tos[30:0], 1'b0};
            op_shr          : alu = {tos[31], tos[31:1]};   // really an asr preserving the sign bit
            default         : alu = tos;
        endcase

    always @*
        if (op8==op_mult) begin
            xa = {{2{nos[15]}}, nos[15:0]};
            xb = {{2{tos[15]}}, tos[15:0]};
        end
        else begin
            xa = nos[17:0];
            xb = tos[17:0];
        end

    MULT18X18 mult(.P(nos_x_tos), .A(xa), .B(xb));

    assign xa20 = nos[19:0];
    assign xb20 = tos[19:0];
    ipcore_mult_20b_20b_40b mult20(.P(prod40), .A(xa20), .B(xb20));

    //////////////////////////////////////////////////////////////////////////
    // Top of stack

    wire [31:0] rstk_dout;
    reg  [31:0] next_tos;

    always @*
        case (op4)
            op_branchZ[15:12], op_wrReg: next_tos = nos;    // branchNZ also
                               op_rdReg: next_tos = par;    // NB {16'b0, par}
            default :
                case (op8)
                    op_swap, op_to_r,
                    op_over, op_drop   : next_tos = nos;
                    op_rot             : next_tos = dstk_dout;
                    op_r_from, op_r    : next_tos = rstk_dout;
                    op_swap16          : next_tos = {tos[15:0], tos[31:16]};
                    op_rdBit           : next_tos = {tos[30:0], serial};        // 32-bit left shift
                    op_fetch16         : next_tos = mem_dout;                   // NB {16'b0, mem_dout}
                    
                    op_sp			   : next_tos = sp;		// STACK_CHECK
                    op_rp			   : next_tos = rp;
                    
                    default            : next_tos = alu;
                endcase
        endcase

    always @ (posedge clk) tos <= next_tos;

    //////////////////////////////////////////////////////////////////////////
    // I/O

    assign rdBit  = (op8==op_rdBit) && !ser_sel;
    assign rdBit2 = (op8==op_rdBit) && ser_sel;
    assign rdReg  = (op4==op_rdReg) && !op[11];
    assign rdReg2 = (op4==op_rdReg) && op[11];
    assign wrReg  = (op4==op_wrReg) && !op[11];
    assign wrReg2 = (op4==op_wrReg) && op[11];
    assign wrEvt  = (op4==op_wrEvt) && !op[11];
    assign wrEvt2 = (op4==op_wrEvt) && op[11];

    //////////////////////////////////////////////////////////////////////////
    // Program counter and stack pointers

    reg  [11:1] pc, next_pc;
    reg  [ 7:0] sp, next_sp, rp, next_rp;

    wire [11:0] pc_plus_2 = {pc + 1'b1, 1'b0};

	// since code memory is now twice as large, and only first half is loaded by boot,
	// must reset pc with 'boot_done' instead of letting it roll-over as before
	// FIXME: this is not quite right as first two insns in insn BRAM are not executed!
    always @ (posedge clk) pc <= (|rst)? ( boot_done? 11'b0 : next_pc ) : 11'h7ff;
    always @ (posedge clk) sp <= rst[RUN]? next_sp :  8'b0;
    always @ (posedge clk) rp <= rst[RUN]? next_rp :  8'b0;

    always @*
        if   (opt_ret) next_pc = rstk_dout[11:1];
        else if (jump) next_pc = op       [11:1];
        else           next_pc = pc_plus_2[11:1];

    always @* next_sp = sp + inc_sp - dec_sp;
    always @* next_rp = rp + inc_rp - dec_rp;

    //////////////////////////////////////////////////////////////////////////
    // 256 x 32-bit data and return stacks
    // WRITE_MODE doesn't matter because address spaces don't overlap
    // But on Artix / Vivado WRITE_MODE = WRITE_FIRST on both ports is needed
    // for correct functioning.

    wire [8:0] dstk_addr = {1'b0, next_sp};
    wire [8:0] rstk_addr = {1'b1, next_rp};

    ipcore_bram_512_32b cpu_stacks (
        .clka   (clk),          .clkb   (clk),
        .ena    (rst[RUN]),		.enb    (rst[RUN]),
        .addra	(dstk_addr),    .addrb	(rstk_addr),
        .douta	(dstk_dout),    .doutb	(rstk_dout),
        .dina	(nos),          .dinb	(op5==op_call? pc_plus_2 : tos),
        .wea	(dstk_wr),      .web	(rstk_wr)
    );

    //////////////////////////////////////////////////////////////////////////
    // 2048 x 16-bit code and data memory
    // WRITE_MODE = WRITE_FIRST
    // bram init values and reset value set to = op_nop
    // N.B. Unlike stack mem above, address space here is _not_ separate.
    // There are just two ports so opcode fetch and data i/o can overlap.
    
    ipcore_bram_cpu_2k_16b cpu_code_data (
        .clka   (clk),          .clkb   (clk),
        .rsta	(~rst[RUN]),
        .ena	(1'b1),         .enb	(rst[RUN]),
        .addra	(next_pc),    	.addrb	(next_tos[11:1]),
        .douta	(op),      		.doutb	(mem_dout),
        .dina	(par),     		.dinb	(nos[15:0]),
        .wea	(rst[LOAD]),	.web	(op8==op_store16)
    );

endmodule
