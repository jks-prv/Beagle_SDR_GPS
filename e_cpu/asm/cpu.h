#define	CPU_RAM_SIZE	2048

#define	OPT_RET			0x0080
#define	OPT_CIN			0x0040

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
#define	OC_MULT18       0x8F00

#define	OC_SHL64 		0x9000
#define	OC_SHL			0x9100
#define	OC_SHR			0x9200
#define	OC_RDBIT 		0x9300
#define	OC_RDBIT2 		0x9340
#define	OC_FETCH16		0x9400
#define	OC_STORE16		0x9500		/* leaves address ( d a --> a ) */

//#define	OC_96			0x9600
#define	OC_SP			0x9600		/* STACK_CHECK */
//#define	OC_97			0x9700
#define	OC_RP			0x9700
#define	OC_98			0x9800
#define	OC_99			0x9900
#define	OC_9A			0x9A00
#define	OC_9B			0x9B00

#define	OC_R			0x9C00
#define	OC_R_FROM		0x9D00
#define	OC_TO_R  		0x9E00

#define	OC_9F			0x9F00

// op5
#define	OC_CALL			0xA000      /* op[11:1] = Destination PC */
#define	OC_BR			0xA001      /* ditto */
#define	OC_BRZ			0xB000      /* ditto */
#define	OC_BRNZ			0xB001      /* ditto */

// op4
#define	OC_RDREG		0xC000		/* op[10:0] = I/O selects */
#define	OC_RDREG2		0xC800		/* op[10:0] = I/O selects */
#define	OC_WRREG		0xD000		/* op[10:0] = I/O selects */
#define	OC_WRREG2		0xD800		/* op[10:0] = I/O selects */
#define	OC_WREVT		0xE000		/* op[10:0] = I/O selects */
#define	OC_WREVT2		0xE800		/* op[10:0] = I/O selects */
#define OC_F0			0xF000
