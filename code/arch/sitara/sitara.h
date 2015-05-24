#ifndef _SITARA_H_
#define _SITARA_H_

//#include "bus.h"

// Sitara memory map
#define PRCM_BASE	0x44e00000		// power, reset, clock management
#define PMUX_BASE	0x44e10000		// control module for pin mux
#define GPIO0_BASE	0x44e07000
#define GPIO1_BASE	0x4804c000
#define GPIO2_BASE	0x481ac000
#define GPIO3_BASE	0x481ae000
#define SPI0_BASE	0x48030000
#define TIMER4_BASE	0x48044000
#define GPMC_BASE	0x50000000		// general purpose memory controller (not used presently)

#define MMAP_SIZE	0x1000


// PRCM: power, reset, clock management
#define PRCM_GPIO1	prcm[0x0ac>>2]
#define PRCM_GPIO2	prcm[0x0b0>>2]
#define PRCM_GPIO3	prcm[0x0b4>>2]
#define PRCM_SPI0	prcm[0x04c>>2]
#define PRCM_TIMER4	prcm[0x088>>2]
#define PRCM_TIMER5	prcm[0x0ec>>2]
#define PRCM_TIMER6	prcm[0x0f0>>2]
#define PRCM_TIMER7	prcm[0x07c>>2]
#define PRCM_PMUX	prcm[0x404>>2]
#define PRCM_GPIO0	prcm[0x408>>2]

#define MODMODE_ENA	0x2			// power-up module


// PMUX: pin mux (remember: Linux doesn't allow write of pmux via mmap -- use device tree (dts) mechanism instead)
#define	PMUX_SLOW	0x40		// slew rate
#define	PMUX_FAST	0x00
#define	PMUX_RXEN	0x20
#define	PMUX_TXEN	0x00		// for gpio done with GPIO_OE
#define	PMUX_PU		0x10		// 1 = pull-up
#define	PMUX_PD		0x00		// 0 = pull-down
#define	PMUX_PDIS	0x08		// 1 = pull disable
#define	PMUX_M7		0x07
#define	PMUX_M2		0x02

#define	PMUX_OUT	(PMUX_FAST | PMUX_TXEN | PMUX_PU)				// 0x10
#define	PMUX_IN		(PMUX_FAST | PMUX_RXEN | PMUX_PU)				// 0x30
#define	PMUX_INOUT	(PMUX_FAST | PMUX_TXEN | PMUX_RXEN | PMUX_PU)	// 0x30


// GPIO
#define	_GPIO_REVISION		0x000
#define	_GPIO_SYSCONFIG		0x010
#define	_GPIO_CLR_IRQ0		0x03c
#define	_GPIO_CLR_IRQ1		0x040
#define	_GPIO_OE			0x134
#define	_GPIO_IN			0x138
#define	_GPIO_OUT			0x13c
#define	_GPIO_CLR			0x190
#define	_GPIO_SET			0x194


#ifndef _PASM_
#define GPIO_REVISION(n)	gpio ## n[_GPIO_REVISION>>2]
#define GPIO_SYSCONFIG(n)	gpio ## n[_GPIO_SYSCONFIG>>2]
#define GPIO_CLR_IRQ0(n)	gpio ## n[_GPIO_CLR_IRQ0>>2]
#define GPIO_CLR_IRQ1(n)	gpio ## n[_GPIO_CLR_IRQ1>>2]
#define GPIO_OE(n)			gpio ## n[_GPIO_OE>>2]			// 0 = output
#define GPIO_IN(n)			gpio ## n[_GPIO_IN>>2]
#define GPIO_OUT(n)			gpio ## n[_GPIO_OUT>>2]
#define GPIO_CLR(n)			gpio ## n[_GPIO_CLR>>2]
#define GPIO_SET(n)			gpio ## n[_GPIO_SET>>2]

#define	GPIO_OUTPUT(n, bits)	GPIO_OE(n) = GPIO_OE(n) & ~(bits);
#define	GPIO_INPUT(n, bits)		GPIO_OE(n) = GPIO_OE(n) | (bits);
#endif

// TIMER
#define	T_TOGGLE		(1<<12)
#define	T_TRIG_OVFL		(1<<10)
#define	T_HIGH			(1<<7)
#define	T_RELOAD		(1<<1)
#define	T_START			(1<<0)
#define	T_MODE			(T_TOGGLE | T_TRIG_OVFL | T_RELOAD)

#define _TIMER_TCLR		0x38
#define _TIMER_TCRR		0x3c
#define _TIMER_TLDR		0x40

#ifndef _PASM_

#define TIMER_TCLR(n)	timer ## n[_TIMER_TCLR>>2]
#define TIMER_TCRR(n)	timer ## n[_TIMER_TCRR>>2]
#define TIMER_TLDR(n)	timer ## n[_TIMER_TLDR>>2]


#define CHECK_IRQ() ((GPIO_IN(0) & BUS_LIRQ) == 0)
#define CHECK_NMI() ((GPIO_IN(1) & BUS_LNMI) == 0)

#define TEST1_SET()
#define TEST1_CLR()
#define TEST2_SET()
#define TEST2_CLR()
#define TEST3_SET()
#define TEST3_CLR()

#define BUS_CLK_ASSERT() \
	TIMER_TCLR(4) = T_MODE | T_HIGH;

#define BUS_CLK_DEASSERT() \
	TIMER_TCLR(4) = T_MODE;

#define BUS_CLK_STOP() \
	TIMER_TCLR(4) = T_MODE;		/* lack of T_HIGH should leave BUS_CLK low */

#define BUS_CLK_START() \
	TIMER_TCRR(4) = -9; \
	TIMER_TCLR(4) = T_MODE | T_START;


extern u4_t g0_addr[], g1_addr[], g3_addr[];
extern u4_t g0_write[], g1_write[], g2_write[];

// remember: address bus is inverted (LAn) so sense of SET/CLR is reversed here
#define SET_ADDR(a) \
	GPIO_CLR(0) = g0_addr[a]; \
	GPIO_SET(0) = ~g0_addr[a] & G0_ADDR; \
	GPIO_CLR(1) = g1_addr[a]; \
	GPIO_SET(1) = ~g1_addr[a] & G1_ADDR; \
	GPIO_CLR(3) = g3_addr[a]; \
	GPIO_SET(3) = ~g3_addr[a] & G3_ADDR;

#define FAST_READ_CYCLE(addr, data) \
    SET_ADDR(addr); \
	\
	GPIO_SET(0) = BUS_DIR; \
	GPIO_CLR(0) = BUS_LRW; \
	GPIO_CLR(1) = BUS_LVMA; \
	BUS_CLK_ASSERT(); \
	\
	t = GPIO_IN(0); /* extra delay before read of bus */ \
	t = GPIO_IN(0); \
	data = ((t & BUS_LD0) >> 27) | ((t & BUS_LD3) >> 23) | ((t & BUS_LD4) >> 19); \
	t = GPIO_IN(1); \
	data |= ((t & BUS_LD1) >> 13) | ((t & BUS_LD2) >> 13) | ((t & BUS_LD5) >> 7) | ((t & BUS_LD6) >> 7); \
	t = GPIO_IN(2); \
	data |= ((t & BUS_LD7) << 3); \
	data = ~data & 0xff; /* remember: have to invert LDn */ \
	\
	BUS_CLK_DEASSERT(); \
	GPIO_SET(1) = BUS_LVMA;

// remember: data bus is inverted (LDn) so sense of SET/CLR is reversed here
#define FAST_WRITE_CYCLE(addr, d) \
    SET_ADDR(addr); \
	GPIO_CLR(0) = g0_write[d]; \
	GPIO_SET(0) = ~g0_write[d] & G0_DATA; \
	GPIO_CLR(1)= g1_write[d]; \
	GPIO_SET(1) = ~g1_write[d] & G1_DATA; \
	GPIO_CLR(2) = g2_write[d]; \
	GPIO_SET(2) = ~g2_write[d] & G2_DATA; \
	\
	/* pulse the bus clock */ \
	BUS_CLK_ASSERT(); \
	GPIO_CLR(0) = g0_write[d]; /* extra delay while clk asserted */ \
	BUS_CLK_DEASSERT();

#ifdef DEBUG
#define FAST_WRITE_ENTER() \
	GPIO_CLR(0) = BUS_DIR; /* write */ \
	GPIO_SET(0) = G0_DATA; /* don't glitch bus with old data (makes it easier to understand bus activity on logic analyzer) */ \
	GPIO_SET(1) = G1_DATA; \
	GPIO_SET(2) = G2_DATA; \
    GPIO_OUTPUT(0, G0_DATA); /* drive data lines */ \
    GPIO_OUTPUT(1, G1_DATA); \
    GPIO_OUTPUT(2, G2_DATA); \
    \
	GPIO_SET(0) = BUS_LRW; \
	GPIO_CLR(1) = BUS_LVMA;
#else
#define FAST_WRITE_ENTER() \
	GPIO_CLR(0) = BUS_DIR; /* write */ \
    GPIO_OUTPUT(0, G0_DATA); /* drive data lines */ \
    GPIO_OUTPUT(1, G1_DATA); \
    GPIO_OUTPUT(2, G2_DATA); \
    \
	GPIO_SET(0) = BUS_LRW; \
	GPIO_CLR(1) = BUS_LVMA;
#endif

#define FAST_WRITE_EXIT() \
	GPIO_CLR(0) = BUS_LRW; \
	GPIO_SET(1) = BUS_LVMA; \
	\
    GPIO_INPUT(0, G0_DATA); \
    GPIO_INPUT(1, G1_DATA); \
    GPIO_INPUT(2, G2_DATA); \
	GPIO_SET(0) = BUS_DIR; /* read */


// to support PRU and hpib_fast_binary()
//
// The v3 PCB was designed in a huge hurry and assignments of address, data and control signals to
// gpio pins was done to facilitate the layout. As a result the consecutive bits of, say, the data bus
// are scrambled out-of-order across multiple gpio registers. The software has to manage this situation
// and there is a resulting performance penalty. The complex macros below are an attempt to increase
// efficiency for fast binary transfer mode by exploiting known facts about the data transfer loop code.
// The v4 PCB will definitely be redesigned to avoid this problem.

#define CONV_ADDR_DCL(avar) \
	u4_t _ ## avar ## 0, _ ## avar ## 0c, _ ## avar ## 1, _ ## avar ## 1c, _ ## avar ## 3, _ ## avar ## 3c;

// pre-compute address
#define CONV_ADDR(prefix, avar, addr) \
	prefix ## avar ## 0 = g0_addr[addr]; \
	prefix ## avar ## 0c = ~g0_addr[addr] & G0_ADDR; \
	prefix ## avar ## 1 = g1_addr[addr]; \
	prefix ## avar ## 1c = ~g1_addr[addr] & G1_ADDR; \
	prefix ## avar ## 3 = g3_addr[addr]; \
	prefix ## avar ## 3c = ~g3_addr[addr] & G3_ADDR;

#define CONV_DATA_DCL(dvar) \
	u4_t _ ## dvar ## 0, _ ## dvar ## 0c, _ ## dvar ## 1, _ ## dvar ## 1c, _ ## dvar ## 2, _ ## dvar ## 2c;

#define CONV_DATA_READ_DCL(dvar) \
	u4_t _ ## dvar ## 0, _ ## dvar ## 1, _ ## dvar ## 2;

// pre-compute write data
#define CONV_WRITE_DATA(prefix, dvar, data) \
	prefix ## dvar ## 0 = g0_write[data]; \
	prefix ## dvar ## 0c = ~g0_write[data] & G0_DATA; \
	prefix ## dvar ## 1 = g1_write[data]; \
	prefix ## dvar ## 1c = ~g1_write[data] & G1_DATA; \
	prefix ## dvar ## 2 = g2_write[data]; \
	prefix ## dvar ## 2c = ~g2_write[data] & G2_DATA;

// re-assemble read data
#define CONV_READ_DATA(t, data, prefix, dvar) \
	t = prefix ## dvar ## 0; \
	data = ((t & BUS_LD0) >> 27) | ((t & BUS_LD3) >> 23) | ((t & BUS_LD4) >> 19); \
	t = prefix ## dvar ## 1; \
	data |= ((t & BUS_LD1) >> 13) | ((t & BUS_LD2) >> 13) | ((t & BUS_LD5) >> 7) | ((t & BUS_LD6) >> 7); \
	t = prefix ## dvar ## 2; \
	data |= ((t & BUS_LD7) << 3);	/* remember: PRU has already inverted LDn */

#define CONV_ADDR_DATA_DCL(advar) \
	u4_t _ ## advar ## 0, _ ## advar ##0c, _ ## advar ## 1, _ ## advar ## 1c, \
		_ ## advar ## 2, _ ## advar ## 2c, _ ## advar ## 3, _ ## advar ## 3c;

// pre-compute address and (write) data
#define CONV_ADDR_DATA(prefix, advar, addr, data) \
	prefix ## advar ## 0  = g0_addr[addr] | g0_write[data]; \
	prefix ## advar ## 0c = (~g0_addr[addr] & G0_ADDR) | (~g0_write[data] & G0_DATA); \
	prefix ## advar ## 1  = g1_addr[addr] | g1_write[data]; \
	prefix ## advar ## 1c = (~g1_addr[addr] & G1_ADDR) | (~g1_write[data] & G1_DATA); \
	prefix ## advar ## 2  = g2_write[data]; \
	prefix ## advar ## 2c = ~g2_write[data] & G2_DATA; \
	prefix ## advar ## 3  = g3_addr[addr]; \
	prefix ## advar ## 3c = ~g3_addr[addr] & G3_ADDR;

#define CONV_ADDR_PRF(avar) \
	printf("%s: %8x %8x %8x %8x %8x %8x\n", #avar, \
		avar ## 0, avar ##0c, avar ## 1, avar ## 1c, avar ## 3, avar ## 3c);

#define CONV_ADDR_DATA_PRF(advar) \
	printf("%s: %8x %8x %8x %8x %8x %8x %8x %8x\n", #advar, \
		advar ## 0, advar ##0c, advar ## 1, advar ## 1c, advar ## 2, advar ## 2c, advar ## 3, advar ## 3c);

#ifndef HPIB_FAST_BINARY_PRU

// set pre-computed address
#define SET_GPIO_ADDR(avar) \
	GPIO_CLR(0) = _ ## avar ## 0; \
	GPIO_SET(0) = _ ## avar ## 0c; \
	GPIO_CLR(1) = _ ## avar ## 1; \
	GPIO_SET(1) = _ ## avar ## 1c; \
	GPIO_CLR(3) = _ ## avar ## 3; \
	GPIO_SET(3) = _ ## avar ## 3c;

// the following way of computing 'data' doesn't work for some reason (timing marginal?)
// gives occasional wrong answers with fast binary hpib transfers
// but doesn't really matter since it's no faster than existing bitwise shift-and-or scheme
//	data = gpio_read[((GPIO_IN(0) >> 18) | (GPIO_IN(1) >> 12) | GPIO_IN(2)) & 0x33f];

// regular full byte read
#define FAST_READ_GPIO_CYCLE(avar, data) \
	SET_GPIO_ADDR(avar); \
	\
	BUS_CLK_ASSERT(); \
	GPIO_SET(0) = BUS_DIR; \
	GPIO_CLR(0) = BUS_LRW; \
	GPIO_CLR(1) = BUS_LVMA; \
	\
	t = GPIO_IN(0); \
	data = ((t & BUS_LD0) >> 27) | ((t & BUS_LD3) >> 23) | ((t & BUS_LD4) >> 19); \
	t = GPIO_IN(1); \
	data |= ((t & BUS_LD1) >> 13) | ((t & BUS_LD2) >> 13) | ((t & BUS_LD5) >> 7) | ((t & BUS_LD6) >> 7); \
	t = GPIO_IN(2); \
	data |= ((t & BUS_LD7) << 3); \
	data = ~data & 0xff; /* remember: have to invert LDn */ \
	\
	GPIO_SET(1) = BUS_LVMA; \
	BUS_CLK_DEASSERT();

// two byte read with sequential address of addr|0, addr|1
#define FAST_READ2_GPIO_CYCLE(avar, data, data2) \
	SET_GPIO_ADDR(avar); \
	\
	BUS_CLK_ASSERT(); \
	GPIO_SET(0) = BUS_DIR; \
	GPIO_CLR(0) = BUS_LRW; \
	GPIO_CLR(1) = BUS_LVMA; \
	\
	t = GPIO_IN(0); \
	data = ((t & BUS_LD0) >> 27) | ((t & BUS_LD3) >> 23) | ((t & BUS_LD4) >> 19); \
	t = GPIO_IN(1); \
	data |= ((t & BUS_LD1) >> 13) | ((t & BUS_LD2) >> 13) | ((t & BUS_LD5) >> 7) | ((t & BUS_LD6) >> 7); \
	t = GPIO_IN(2); \
	data |= ((t & BUS_LD7) << 3); \
	data = ~data & 0xff; /* remember: have to invert LDn */ \
	BUS_CLK_DEASSERT(); \
	\
	GPIO_CLR(3) = BUS_LA0; /* bump address to avar|1 */ \
	BUS_CLK_ASSERT(); \
	t = GPIO_IN(0); /* extra delay before read of bus */ \
	t = GPIO_IN(0); \
	data2 = ((t & BUS_LD0) >> 27) | ((t & BUS_LD3) >> 23) | ((t & BUS_LD4) >> 19); \
	t = GPIO_IN(1); \
	data2 |= ((t & BUS_LD1) >> 13) | ((t & BUS_LD2) >> 13) | ((t & BUS_LD5) >> 7) | ((t & BUS_LD6) >> 7); \
	t = GPIO_IN(2); \
	data2 |= ((t & BUS_LD7) << 3); \
	data2 = ~data2 & 0xff; /* remember: have to invert LDn */ \
	BUS_CLK_DEASSERT(); \
	\
	GPIO_SET(1) = BUS_LVMA;

// read from a single gpio
#define FAST_READ_GPIO1_CYCLE(gpio, data, avar) \
	SET_GPIO_ADDR(avar); \
	\
	BUS_CLK_ASSERT(); \
	GPIO_SET(0) = BUS_DIR; \
	GPIO_CLR(0) = BUS_LRW; \
	GPIO_CLR(1) = BUS_LVMA; \
	\
	data = ~GPIO_IN(gpio); /* remember: have to invert LDn */ \
	\
	GPIO_SET(1) = BUS_LVMA; \
	BUS_CLK_DEASSERT();

// read from two gpios
#define FAST_READ_GPIO2_CYCLE(gpio1, gpio2, data1, data2, avar) \
	SET_GPIO_ADDR(avar); \
	\
	BUS_CLK_ASSERT(); \
	GPIO_SET(0) = BUS_DIR; \
	GPIO_CLR(0) = BUS_LRW; \
	GPIO_CLR(1) = BUS_LVMA; \
	\
	data1 = ~GPIO_IN(gpio1); /* remember: have to invert LDn */ \
	data2 = ~GPIO_IN(gpio2); /* remember: have to invert LDn */ \
	\
	GPIO_SET(1) = BUS_LVMA; \
	BUS_CLK_DEASSERT();

// regular full byte write
#define FAST_WRITE_GPIO_CYCLE(advar) \
	GPIO_CLR(0) = _ ## advar ## 0; \
	GPIO_SET(0) = _ ## advar ## 0c; \
	GPIO_CLR(1) = _ ## advar ## 1; \
	GPIO_SET(1) = _ ## advar ## 1c; \
	GPIO_CLR(2) = _ ## advar ## 2; \
	GPIO_SET(2) = _ ## advar ## 2c; \
	GPIO_CLR(3) = _ ## advar ## 3; \
	GPIO_SET(3) = _ ## advar ## 3c; \
	\
	/* pulse the bus clock */ \
	BUS_CLK_ASSERT(); \
	BUS_CLK_DEASSERT();

// write allowing each gpio set/clr to be qualified (and the 'if' optimized away since test values are constant)
#define FAST_WRITE_GPIO_QUAL_CYCLE(advar, g0, g0c, g1, g1c, g2, g2c, g3, g3c) \
	if (g0) GPIO_CLR(0) = _ ## advar ## 0; \
	if (g0c) GPIO_SET(0) = _ ## advar ## 0c; \
	if (g1) GPIO_CLR(1) = _ ## advar ## 1; \
	if (g1c) GPIO_SET(1) = _ ## advar ## 1c; \
	if (g2) GPIO_CLR(2) = _ ## advar ## 2; \
	if (g2c) GPIO_SET(2) = _ ## advar ## 2c; \
	if (g3) GPIO_CLR(3) = _ ## advar ## 3; \
	if (g3c) GPIO_SET(3) = _ ## advar ## 3c; \
	\
	/* pulse the bus clock */ \
	BUS_CLK_ASSERT(); \
	BUS_CLK_DEASSERT();

#endif

void check_pmux();

#endif

#endif
