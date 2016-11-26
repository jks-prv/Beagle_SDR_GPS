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

#ifndef _SPI_H_
#define _SPI_H_

#include "kiwi.h"

#include <inttypes.h>

enum SPI_CMD { // Embedded CPU commands, order must match 'Commands:' table in .asm code

	// receiver
    CmdSetRXFreq,
    CmdSetRXNsamps,
    CmdSetGen,
    CmdSetGenAttn,
    CmdPing,
    CmdLoad,
    CmdPing2,
    CmdGetRX,
    CmdClrRXOvfl,
    CmdSetWFFreq,
	CmdSetWFDecim,
    CmdWFReset,
    CmdGetWFSamples,
    CmdGetWFContSamps,
    CmdCPUCtrClr,
    CmdGetCPUCtr,
    CmdCtrlSet,
    CmdCtrlClr,
    CmdCtrlGet,
	CmdGetMem,
	CmdGetStatus,
	CmdFlush,
    CmdTestRead,
    CmdUploadStackCheck,

	// GPS
    CmdSample,
    CmdSetMask,
    CmdSetRateCA,
    CmdSetRateLO,
    CmdSetGainCA,
    CmdSetGainLO,
    CmdSetSV,
    CmdPause,
    CmdGetGPSSamples,
    CmdGetChan,
    CmdGetClocks,
    CmdGetGlitches,
    
    CmdCheckLast
};

static const char *cmds[] = {

	// receiver
    "CmdSetRXFreq",
    "CmdSetRXNsamps",
    "CmdSetGen",
    "CmdSetGenAttn",
    "CmdPing",
    "CmdLoad",
    "CmdPing2",
    "CmdGetRX",
    "CmdClrRXOvfl",
    "CmdSetWFFreq",
    "CmdSetWFDecim",
    "CmdWFReset",
    "CmdGetWFSamples",
    "CmdGetWFContSamps",
    "CmdCPUCtrClr",
    "CmdGetCPUCtr",
    "CmdCtrlSet",
    "CmdCtrlClr",
    "CmdCtrlGet",
    "CmdGetMem",
	"CmdGetStatus",
	"CmdFlush",
    "CmdTestRead",
    "CmdUploadStackCheck",

	// GPS
    "CmdSample",
    "CmdSetMask",
    "CmdSetRateCA",
    "CmdSetRateLO",
    "CmdSetGainCA",
    "CmdSetGainLO",
    "CmdSetSV",
    "CmdPause",
    "CmdGetGPSSamples",
    "CmdGetChan",
    "CmdGetClocks",
    "CmdGetGlitches",
};

#define DMA_ALIGNMENT __attribute__ ((aligned(256)))
#define	PAD_FRONT u4_t pad_front[256/4]
#define	PAD_BACK u4_t pad_back[256/4]

#ifdef SPI_8
	#define SPI_T		char
	#define SPI_ST		u1_t
	#define SPI_B2X(b)	b
	#define SPI_X2B(b)	b
	#define	SPI_NST		7
	#define	SPI_SFT		0
#endif
#ifdef SPI_16
	#define SPI_T		u2_t
	#define SPI_ST		u2_t
	#define SPI_B2X(b)	B2S(b)
	#define SPI_X2B(b)	S2B(b)
	#define	SPI_NST		15
	#define	SPI_SFT		8
#endif
#ifdef SPI_32
	#define SPI_T		u4_t
	#define SPI_ST		u4_t
	#define SPI_B2X(b)	B2I(b)
	#define SPI_X2B(b)	I2B(b)
	#define	SPI_NST		31
	#define	SPI_SFT		24
#endif

#define	NSPI_RX		2048		// limited by use of single 2K x 8 BRAM in host.v

struct spi_mosi_data_t {
	uint16_t cmd;
	uint16_t wparam;
	uint16_t lparam_lo;
	uint16_t lparam_hi;
	uint8_t _pad_; // 3 LSBs stay in ha_disr[2:0]
};

struct SPI_MOSI {
	PAD_FRONT;
	union {
		SPI_T msg[1];
		u1_t bytes[NSPI_RX];		// because tx/rx DMA sizes equal
		spi_mosi_data_t data;
	};
	PAD_BACK;
} DMA_ALIGNMENT;

struct SPI_MISO {
	PAD_FRONT;
#ifdef SPI_8
    u1_t _align_;
#endif
	union {
		SPI_T msg[1];
		struct {
			SPI_ST status;
			#define SPI_ST_ADC_OVFL (0x08 << SPI_SFT)
			union {
				char byte[NSPI_RX];
				uint16_t word[NSPI_RX/2];
			};
		} __attribute__((packed));
	};
	PAD_BACK;
	int len_xfers;
	uint16_t cmd;
	u4_t tid;
} __attribute__((packed)) DMA_ALIGNMENT;


extern u4_t spi_retry;

#define spi_set _spi_set
//#define spi_set spi_set_noduplex

#define spi_get _spi_get
//#define spi_get spi_get_noduplex

void spi_init();
void _spi_set(SPI_CMD cmd, uint16_t wparam=0, uint32_t lparam=0);
void _spi_get(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);
void spi_get_pipelined(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);
void spi_set_noduplex(SPI_CMD cmd, uint16_t wparam=0, uint32_t lparam=0);
void spi_get_noduplex(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);

#endif
