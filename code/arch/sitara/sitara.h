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
#define	PMUX_RXEN	0x20		// TX can always be enabled with GPIO_OE
#define	PMUX_PU		0x10		// 1 = pull-up
#define	PMUX_PD		0x00		// 0 = pull-down
#define	PMUX_PDIS	0x08		// 1 = pull disable
#define	PMUX_M7		0x07		// GPIO
#define	PMUX_M2		0x02
#define	PMUX_M0		0x00

#define	PMUX_OUT	(PMUX_FAST | PMUX_PDIS)				// 0x08, rx not enabled
#define	PMUX_OUT_PU	(PMUX_FAST | PMUX_PU)				// 0x10, rx not enabled
#define	PMUX_OUT_PD	(PMUX_FAST | PMUX_PD)				// 0x00, rx not enabled

#define	PMUX_IN		(PMUX_FAST | PMUX_RXEN | PMUX_PDIS)	// 0x28, for doc purposes: don't expect output to be enabled
#define	PMUX_IN_PU	(PMUX_FAST | PMUX_RXEN | PMUX_PU)	// 0x30, for doc purposes: don't expect output to be enabled
#define	PMUX_IN_PD	(PMUX_FAST | PMUX_RXEN | PMUX_PD)	// 0x20, for doc purposes: don't expect output to be enabled

#define	PMUX_IO		(PMUX_FAST | PMUX_RXEN | PMUX_PDIS)	// 0x28
#define	PMUX_IO_PU	(PMUX_FAST | PMUX_RXEN | PMUX_PU)	// 0x30
#define	PMUX_IO_PD	(PMUX_FAST | PMUX_RXEN | PMUX_PD)	// 0x20


// GPIO
#define	GPIO0	0
#define	GPIO1	1
#define	GPIO2	2
#define	GPIO3	3
#define	NGPIO	4

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
#define GPIO_REVISION(g)	_gpio[(g).bank][_GPIO_REVISION>>2]
#define GPIO_SYSCONFIG(g)	_gpio[(g).bank][_GPIO_SYSCONFIG>>2]
#define GPIO_CLR_IRQ0(g)	_gpio[(g).bank][_GPIO_CLR_IRQ0>>2]
#define GPIO_CLR_IRQ1(g)	_gpio[(g).bank][_GPIO_CLR_IRQ1>>2]
#define GPIO_OE(g)			_gpio[(g).bank][_GPIO_OE>>2]			// 0 = output
#define GPIO_IN(g)			_gpio[(g).bank][_GPIO_IN>>2]
#define GPIO_OUT(g)			_gpio[(g).bank][_GPIO_OUT>>2]
#define GPIO_CLR(g)			_gpio[(g).bank][_GPIO_CLR>>2]
#define GPIO_SET(g)			_gpio[(g).bank][_GPIO_SET>>2]

#define	GPIO_OUTPUT(g)		GPIO_OE(g) = GPIO_OE(g) & ~(1 << (g).bit);
#define	GPIO_INPUT(g)		GPIO_OE(g) = GPIO_OE(g) | (1 << (g).bit);
#define	GPIO_isOE(g)		((GPIO_OE(g) & (1 << (g).bit))? 0:1)

#define	GPIO_CLR_BIT(g)		GPIO_CLR(g) = 1 << (g).bit;
#define	GPIO_SET_BIT(g)		GPIO_SET(g) = 1 << (g).bit;
#define	GPIO_READ_BIT(g)	((GPIO_IN(g) & (1 << (g).bit))? 1:0)
#define	GPIO_WRITE_BIT(g,b)	{ if (b) { GPIO_SET_BIT(g) } else { GPIO_CLR_BIT(g) } }

typedef struct {
	u1_t bank, bit;
} gpio_t;

extern gpio_t GPIO_NONE;
#define isGPIO(g)	((g).bit != 0xff)

#define GPIO_HIZ	-1

typedef enum { GPIO_DIR_IN, GPIO_DIR_OUT, GPIO_DIR_BIDIR } gpio_dir_e;
#endif

#endif
