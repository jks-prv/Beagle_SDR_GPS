/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2015-2016 John Seamons, ZL/KF6VO

#ifndef _SITARA_H_
#define _SITARA_H_

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

#define	P8			0x00
#define	P9			0x80
#define	PIN_BITS	0x7f	// pins 1..46
#define	PIN(P8_P9, pin)		(P8_P9 | (pin & PIN_BITS))

struct gpio_t {
	u1_t bank, bit, pin, eeprom_off;
} __attribute__((packed));

struct pin_t {
	gpio_t gpio;
	
	#define PIN_USED		0x8000
	#define PIN_DIR_IN		0x2000
	#define PIN_DIR_OUT		0x4000
	#define PIN_DIR_BIDIR	0x6000
	#define PIN_PMUX_BITS	0x007f
	u2_t attrs;
} __attribute__((packed));

#define	EE_NPINS 				74
extern pin_t eeprom_pins[EE_NPINS];
#define	EE_PINS_OFFSET_BASE		88

extern gpio_t GPIO_NONE;
#define isGPIO(g)	((g).bit != 0xff)

#define GPIO_HIZ	-1

typedef enum { GPIO_DIR_IN, GPIO_DIR_OUT, GPIO_DIR_BIDIR } gpio_dir_e;
#endif

#endif
