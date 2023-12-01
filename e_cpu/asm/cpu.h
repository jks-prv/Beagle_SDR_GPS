#define	CPU_RAM_SIZE	2048

#define OCM_NONE        0x0000

#define	OPT_RET			0x0080
#define	OPT_CIN			0x0040
#define	OPT_ROT			0x0020
#define	OPT_UNS			0x0010

#define OCM_CONST       0x7fff
#define	OC_PUSH			0x0000

#define	OC_NOP			0x8000
#define	OC_DUP			0x8100
#define	OC_SWAP			0x8200
#define	OC_SWAP16		0x8300
#define	OC_OVER  		0x8400
#define	OC_POP  		0x8500
#define	OC_ROT			0x8600
#define	OC_ADDI  		0x8700		/* op[6:0] = Immediate operand */
#define	OC_ADD			0x8800
#define	OC_SUB			0x8900
#define	OC_MULT  		0x8A00
#define	OC_AND			0x8B00
#define	OC_OR 			0x8C00
#define	OC_XOR			0x8D00
#define	OC_NOT			0x8E00
#define	OC_MULT20       0x8F00

#define	OC_SHL64 		0x9000
#define	OC_SHL			0x9100
#define	OC_ROL			(OC_SHL | OPT_ROT)
#define	OC_SHR			0x9200
#define	OC_ROR			(OC_SHR | OPT_ROT)
#define	OC_USR			(OC_SHR | OPT_UNS)

#define	OC_RDBIT0 		0x9310
#define	OC_RDBIT1 		0x9320
#define	OC_RDBIT2 		0x9340
#define	OC_FETCH16		0x9400
#define	OC_STORE16		0x9500		/* leaves address ( d a --> a ) */
#define	OC_STK_RD		0x9600
#define	OC_STK_WR		0x9700		/* no change ( d a --> d a ) */
#define	OC_SP_RP        0x9800		/* used by STACK_CHECK */

#define	OC_99			0x9900
#define	OC_9A			0x9A00

#define	OC_R			0x9B00
#define	OC_R_FROM		0x9C00
#define	OC_TO_R  		0x9D00
#define	OC_TO_LOOP      0x9E00
#define	OC_TO_LOOP2     0x9E01
#define	OC_LOOP_FROM    0x9F00
#define	OC_LOOP2_FROM   0x9F01

#define OCM_ADDR        0x0ffe
#define	OC_CALL			0xA000      /* op[11:1] = Destination PC */
#define	OC_BR			0xA001      /* ditto */
#define	OC_BRZ			0xB000      /* ditto */
#define	OC_BRNZ			0xB001      /* ditto */
#define	OC_LOOP         0xC000      /* ditto */
#define	OC_LOOP2        0xC001      /* ditto */

#define OCM_IO          0x07ff
#define	OC_RDREG		0xD000		/* op[10:0] = I/O selects */
#define	OC_RDREG2		0xD800		/* op[10:0] = I/O selects */
#define	OC_WRREG		0xE000		/* op[10:0] = I/O selects */
#define	OC_WRREG2		0xE800		/* op[10:0] = I/O selects */
#define	OC_WREVT		0xF000		/* op[10:0] = I/O selects */
#define	OC_WREVT2		0xF800		/* op[10:0] = I/O selects */
