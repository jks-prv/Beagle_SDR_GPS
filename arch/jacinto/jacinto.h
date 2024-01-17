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

// Copyright (c) 2022 John Seamons, ZL4VO/KF6VO

#pragma once

// TDA4VM memory map (BBAI-64)
#ifdef CPU_TDA4VM
 #define PRCM_BASE	NULL        // already powered up
 #define PMUX_BASE	0x0011C000
 #define GPIO0_BASE	0x00600000

 #define SPI6_BASE	NULL        // SPI_PIO never used
 #define SPI_BASE	SPI6_BASE

 #define MMAP_SIZE	0x2000
#endif


// PMUX: pin mux (remember: Linux doesn't allow write of pmux via mmap -- use device tree (dts) mechanism instead)

#ifdef CPU_TDA4VM                   // TRM 5.1.3.3.1.1 pad configuration regs
                                    // J721E_registers1.pdf 6.404 pdfpg 1048 padconfig1 reg
                                    // CAUTION: padconfig0 reg is different from all others (fields missing)
                                    // padconfig1 hw reset value 0x8214007
 
 #define	PMUX_ISOBP	0x00800000  // 23 isolation bypass
 #define	PMUX_ISOOV	0x00400000  // 22 isolation override

 #define	PMUX_TXDIS	0x00200000  // 21 TX disable

 #define	PMUX_DRIVE	0x00180000  // 20:19 LVCMOS drive strength
 #define	PMUX_SLOW	0x00100000
 #define	PMUX_FAST	0x00080000
 #define	PMUX_NOM	0x00000000

 #define	PMUX_RXEN	0x00040000  // 18 RX enable
 
 #define	PMUX_PU		0x00020000  // 17 1 = pull-up
 #define	PMUX_PD		0x00000000  // 17 0 = pull-down
 #define	PMUX_PDIS	0x00010000  // 16 1 = pull disable
 
 #define	PMUX_ST		0x00004000  // 14 schmitt trigger
 #define    PMUX_ATTR_S 20
 #define    PMUX_ATTR_E 14

 #define	PMUX_MODE   0x0000000f  // mode bits
 #define	PMUX_SPI    0x00000004  // SPI6 = mode 4
 #define	PMUX_GPIO   0x00000007  // GPIO = mode 7
 #define	PMUX_OFF    0x0000000f  // mode 15
#endif

#define	PMUX_OUT	(PMUX_NOM | PMUX_PDIS)
#define	PMUX_OUT_PU	(PMUX_NOM | PMUX_PU)
#define	PMUX_OUT_PD	(PMUX_NOM | PMUX_PD)

#define	PMUX_IN		(PMUX_NOM | PMUX_TXDIS | PMUX_RXEN | PMUX_PDIS)
#define	PMUX_IN_PU	(PMUX_NOM | PMUX_TXDIS | PMUX_RXEN | PMUX_PU)
#define	PMUX_IN_PD	(PMUX_NOM | PMUX_TXDIS | PMUX_RXEN | PMUX_PD)

#define	PMUX_IO		(PMUX_NOM | PMUX_RXEN | PMUX_PDIS)
#define	PMUX_IO_PU	(PMUX_NOM | PMUX_RXEN | PMUX_PU)
#define	PMUX_IO_PD	(PMUX_NOM | PMUX_RXEN | PMUX_PD)

#define PMUX_NONE   -1

// GPIO

#ifdef CPU_TDA4VM
 #define	NBALL	        2   // max number of cpu package balls assigned to a single GPIO
 #define	GPIO0	        0
 #define	NGPIO	        1
 #define    GPIO_NPINS      128
 #define    GPIO_BANK(gpio) ((gpio).bank)
#endif

// J721E_registers4.pdf table 2-2 pdfpg 56
#define	_GPIO_IEN		    0x008   // enable = 1
#define	_GPIO_DIR		    0x010   // input = 1
#define	_GPIO_OUT			0x014
#define	_GPIO_SET			0x018
#define	_GPIO_CLR			0x01c
#define	_GPIO_IN			0x020

#ifndef _PASM_
#define GPIO_CLR_IRQ0(g)    // assume already cleared
#define GPIO_CLR_IRQ1(g)

// 32*4 = 128
// 0x10 + n*0x28
// 01:0x10 23:0x38 45:0x60 67:0x88
#define _GPIO_REG(g, off)   gpio_m[GPIO0][(((g).bit_div32 * 0x28) + (off)) >> 2]
#define GPIO_DIR(g)			_GPIO_REG(g, _GPIO_DIR)
#define GPIO_IN(g)			_GPIO_REG(g, _GPIO_IN)
#define GPIO_OUT(g)			_GPIO_REG(g, _GPIO_OUT)
#define GPIO_CLR(g)			_GPIO_REG(g, _GPIO_CLR)
#define GPIO_SET(g)			_GPIO_REG(g, _GPIO_SET)

#define	GPIO_OUTPUT(g)		GPIO_DIR(g) = GPIO_DIR(g) & ~(1 << ((g).bit_mod32));
#define	GPIO_INPUT(g)		GPIO_DIR(g) = GPIO_DIR(g) | (1 << ((g).bit_mod32));
#define	GPIO_isOUT(g)		((GPIO_DIR(g) & (1 << ((g).bit_mod32)))? 0:1)

#define	GPIO_CLR_BIT(g)		GPIO_CLR(g) = 1 << ((g).bit_mod32);
#define	GPIO_SET_BIT(g)		GPIO_SET(g) = 1 << ((g).bit_mod32);
#define	GPIO_READ_BIT(g)	((GPIO_IN(g) & (1 << ((g).bit_mod32)))? 1:0)
#define	GPIO_WRITE_BIT(g,b)	{ if (b) { GPIO_SET_BIT(g) } else { GPIO_CLR_BIT(g) } }

#define	P8			0x00
#define	P9			0x80
#define	PIN_BITS	0x7f	// pins 1..46
#define	PIN(P8_P9, pin)		(P8_P9 | (pin & PIN_BITS))

struct gpio_t {
	u1_t bank, bit, pin, eeprom_off;
    u1_t bit_div32, bit_mod32;

    void init() {
        bit_div32 = bit / 32;
        bit_mod32 = bit % 32;
    }
};

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
