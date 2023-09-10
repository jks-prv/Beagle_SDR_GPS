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

// Copyright (c) 2015-2022 John Seamons, ZL4VO/KF6VO

#pragma once

// AM3359 memory map (BBB/G)
#ifdef CPU_AM3359
 #define PRCM_BASE	0x44e00000		// power, reset, clock management
 #define PMUX_BASE	0x44e10000		// control module for pin mux
 #define GPIO0_BASE	0x44e07000
 #define GPIO1_BASE	0x4804c000
 #define GPIO2_BASE	0x481ac000
 #define GPIO3_BASE	0x481ae000

 #define SPI0_BASE	0x48030000
 #define SPI_BASE	SPI0_BASE

 #define MMAP_SIZE	0x1000
#endif

// AM5729 memory map (BBAI)
#ifdef CPU_AM5729
 #define PRCM_BASE	0x4A009000		// power, reset, clock management for GPIO2-8 (TRM 3.12.26 CM_CORE_L4PER)
 #define PMUX_BASE	0x4A002000		// control module for pin mux (TRM 2.3.1, 18.5.2.1 CTRL_MODULE_CORE)
 #define GPIO1_BASE	0x4AE10000      // NB: don't currently use because PRCM base addr is different (WKUP, not L4PER)
 #define GPIO2_BASE	0x48055000      // TRM 2.4.1
 #define GPIO3_BASE	0x48057000
 #define GPIO4_BASE	0x48059000
 #define GPIO5_BASE	0x4805b000
 #define GPIO6_BASE	0x4805d000
 #define GPIO7_BASE	0x48051000
 #define GPIO8_BASE	0x48053000

 #define SPI2_BASE	0x4809A000
 #define SPI_BASE	SPI2_BASE

 #define MMAP_SIZE	0x2000
#endif


// PRCM: power, reset, clock management

#ifdef CPU_AM3359
 // CM_PER_*_CLKCTRL
 #define PRCM_GPIO1	    prcm_m[0x0ac>>2]
 #define PRCM_GPIO2	    prcm_m[0x0b0>>2]
 #define PRCM_GPIO3	    prcm_m[0x0b4>>2]
 #define PRCM_SPI0	    prcm_m[0x04c>>2]
 // CM_WKUP_*_CLKCTRL
 #define PRCM_PMUX	    prcm_m[0x404>>2]      // info only
 #define PRCM_GPIO0	    prcm_m[0x408>>2]      // info only

 #define MODMODE_ENA	0x2			// power-up module
#endif

#ifdef CPU_AM5729
 // CM_L4PER_*_CLKCTRL (NB: not for GPIO1)
 #define PRCM_GPIO2	    prcm_m[0x760>>2]
 #define PRCM_GPIO3	    prcm_m[0x768>>2]
 #define PRCM_GPIO4	    prcm_m[0x770>>2]
 #define PRCM_GPIO5	    prcm_m[0x778>>2]
 #define PRCM_GPIO6	    prcm_m[0x780>>2]
 #define PRCM_GPIO7	    prcm_m[0x810>>2]
 #define PRCM_GPIO8	    prcm_m[0x818>>2]
 #define PRCM_SPI2	    prcm_m[0x7f8>>2]

 #define MODMODE_GPIO_ENA	0x1			// power-up module
 #define MODMODE_SPI_ENA	0x2			// power-up module
#endif


// PMUX: pin mux (remember: Linux doesn't allow write of pmux via mmap -- use device tree (dts) mechanism instead)

#ifdef CPU_AM3359
 #define	PMUX_SLOW	0x40		// slew rate
 #define	PMUX_FAST	0x00
 #define	PMUX_RXEN	0x20		// TX can always be enabled with GPIO_OE
 #define	PMUX_PU		0x10		// 1 = pull-up
 #define	PMUX_PD		0x00		// 0 = pull-down
 #define	PMUX_PDIS	0x08		// 1 = pull disable

 #define	PMUX_SPI    0x00        // SPI2 = mode 0
 #define	PMUX_GPIO   0x07		// GPIO = mode 7
 #define	PMUX_MODE   0x07        // mode bits
 #define    PMUX_BITS	0x7f
#endif

#ifdef CPU_AM5729                   // TRM 18.4.6.1.1, 18.5.2.2
 #define	PMUX_SLOW	0x00080000  // slew rate
 #define	PMUX_FAST	0x00000000
 #define	PMUX_RXEN	0x00040000  // TX can always be enabled with GPIO_OE
 #define	PMUX_PU		0x00020000  // 1 = pull-up
 #define	PMUX_PD		0x00000000  // 0 = pull-down
 #define	PMUX_PDIS	0x00010000  // 1 = pull disable
 #define    PMUX_ATTR_S 19
 #define    PMUX_ATTR_E 16

 #define	PMUX_SPI    0x00000000  // SPI2 = mode 0
 #define	PMUX_GPIO   0x0000000e  // GPIO = mode 14
 #define	PMUX_OFF    0x0000000f  // mode 15
 #define	PMUX_MODE   0x0000000f  // mode bits
#endif

                                                        // 3359 5729
#define	PMUX_OUT	(PMUX_FAST | PMUX_PDIS)				// 0x08 0x0001  rx not enabled
#define	PMUX_OUT_PU	(PMUX_FAST | PMUX_PU)				// 0x10 0x0002  rx not enabled
#define	PMUX_OUT_PD	(PMUX_FAST | PMUX_PD)				// 0x00 0x0000  rx not enabled

#define	PMUX_IN		(PMUX_FAST | PMUX_RXEN | PMUX_PDIS)	// 0x28 0x0005  for doc purposes: don't expect output to be enabled
#define	PMUX_IN_PU	(PMUX_FAST | PMUX_RXEN | PMUX_PU)	// 0x30 0x0006  for doc purposes: don't expect output to be enabled
#define	PMUX_IN_PD	(PMUX_FAST | PMUX_RXEN | PMUX_PD)	// 0x20 0x0004  for doc purposes: don't expect output to be enabled

#define	PMUX_IO		(PMUX_FAST | PMUX_RXEN | PMUX_PDIS)	// 0x28 0x0005
#define	PMUX_IO_PU	(PMUX_FAST | PMUX_RXEN | PMUX_PU)	// 0x30 0x0006
#define	PMUX_IO_PD	(PMUX_FAST | PMUX_RXEN | PMUX_PD)	// 0x20 0x0004

#define PMUX_NONE   -1

// GPIO

#ifdef CPU_AM3359
 #define	GPIO0	0
 #define	GPIO1	1
 #define	GPIO2	2
 #define	GPIO3	3
 #define	NGPIO	4
 #define    GPIO_BANK(gpio)     ((gpio).bank)
#endif

#ifdef CPU_AM5729
 #define	NBALL	2   // max number of cpu package balls assigned to a single GPIO
 #define	GPIO1	0
 #define	GPIO2	1
 #define	GPIO3	2
 #define	GPIO4	3
 #define	GPIO5	4
 #define	GPIO6	5
 #define	GPIO7	6
 #define	GPIO8	7
 #define	NGPIO	8
 #define    GPIO_BANK(gpio) ((gpio).bank + 1)
#endif

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
#define GPIO_REVISION(g)	gpio_m[(g).bank][_GPIO_REVISION>>2]
#define GPIO_SYSCONFIG(g)	gpio_m[(g).bank][_GPIO_SYSCONFIG>>2]
#define GPIO_CLR_IRQ0(g)	gpio_m[(g).bank][_GPIO_CLR_IRQ0>>2] = 1 << (g).bit
#define GPIO_CLR_IRQ1(g)	gpio_m[(g).bank][_GPIO_CLR_IRQ1>>2] = 1 << (g).bit
#define GPIO_OE(g)			gpio_m[(g).bank][_GPIO_OE>>2]			// 0 = output
#define GPIO_IN(g)			gpio_m[(g).bank][_GPIO_IN>>2]
#define GPIO_OUT(g)			gpio_m[(g).bank][_GPIO_OUT>>2]
#define GPIO_CLR(g)			gpio_m[(g).bank][_GPIO_CLR>>2]
#define GPIO_SET(g)			gpio_m[(g).bank][_GPIO_SET>>2]

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

typedef struct {
	u1_t bank, bit, pin, eeprom_off;
} __attribute__((packed)) gpio_t;

typedef struct {
	gpio_t gpio;
	
	#define PIN_USED		0x8000
	#define PIN_DIR_IN		0x2000
	#define PIN_DIR_OUT		0x4000
	#define PIN_DIR_BIDIR	0x6000
	#define PIN_PMUX_BITS	0x007f
	u2_t attrs;
} __attribute__((packed)) pin_t;

#define	EE_NPINS 				74
extern pin_t eeprom_pins[EE_NPINS];
#define	EE_PINS_OFFSET_BASE		88

extern gpio_t GPIO_NONE;
#define isGPIO(g)	((g).bit != 0xff)

#define GPIO_HIZ	-1

typedef enum { GPIO_DIR_IN, GPIO_DIR_OUT, GPIO_DIR_BIDIR } gpio_dir_e;
#endif
