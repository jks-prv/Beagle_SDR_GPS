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

#pragma once

#include "kiwi.h"
#include "other.gen.h"

#ifdef USE_OTHER
    #include "spi.other.h"
#endif

#include <inttypes.h>

typedef enum { // Embedded CPU commands, order must match 'Commands:' table in .asm code

    // general
    CmdPing = 0,
    CmdSetMem,
    CmdPing2,
    CmdGetCPUCtr,
    CmdCtrlClrSet,
    CmdCtrlPulse,
    CmdCtrlGet,
	CmdGetMem,
	CmdGetStatus,
	CmdFlush,
    CmdTestRead,
    CmdUploadStackCheck,
    CmdGetSPRP,

	// SDR
#ifdef USE_SDR
    CmdSetRXFreq,
    CmdSetRXNsamps,
    CmdSetGenFreq,
    CmdSetGenAttn,
    CmdGetRX,
    CmdClrRXOvfl,
    CmdSetWFFreq,
	CmdSetWFDecim,
    CmdWFReset,
    CmdGetWFSamples,
    CmdGetWFContSamps,
	CmdSetOVMask,
	CmdGetADCCtr,
	CmdSetADCLvl,
#endif

	// GPS
#ifdef USE_GPS
    CmdSample,
    CmdSetChans,
    CmdSetMask,
    CmdSetRateCG,
    CmdSetRateLO,
    CmdSetGainCG,
    CmdSetGainLO,
    CmdSetSat,
    CmdSetE1Bcode,
    CmdSetPolarity,
    CmdPause,
    CmdGetGPSSamples,
    CmdGetChan,
    CmdGetClocks,
    CmdGetGlitches,
    CmdIQLogReset,
    CmdIQLogGet,
#endif
    
#ifdef USE_OTHER
    SPI_OTHER_ENUM
#endif

    CmdCheckLast,
    
    // pseudo for debugging
    CmdPumpFlush,

#ifndef USE_SDR
    CmdSetRXFreq = 0,
    CmdSetRXNsamps = 0,
    CmdSetGenFreq = 0,
    CmdSetGenAttn = 0,
    CmdGetRX = 0,
    CmdClrRXOvfl = 0,
    CmdSetWFFreq = 0,
	CmdSetWFDecim = 0,
    CmdWFReset = 0,
    CmdGetWFSamples = 0,
    CmdGetWFContSamps = 0,
	CmdSetOVMask = 0,
	CmdGetADCCtr = 0,
	CmdSetADCLvl = 0,
#endif

#ifndef USE_GPS
    CmdSample = 0,
    CmdSetChans = 0,
    CmdSetMask = 0,
    CmdSetRateCG = 0,
    CmdSetRateLO = 0,
    CmdSetGainCG = 0,
    CmdSetGainLO = 0,
    CmdSetSat = 0,
    CmdSetE1Bcode = 0,
    CmdSetPolarity = 0,
    CmdPause = 0,
    CmdGetGPSSamples = 0,
    CmdGetChan = 0,
    CmdGetClocks = 0,
    CmdGetGlitches = 0,
    CmdIQLogReset = 0,
    CmdIQLogGet = 0,
#endif

} SPI_CMD;

static const char *cmds[] = {

    // general
    "CmdPing",
    "CmdSetMem",
    "CmdPing2",
    "CmdGetCPUCtr",
    "CmdCtrlClrSet",
    "CmdCtrlPulse",
    "CmdCtrlGet",
    "CmdGetMem",
	"CmdGetStatus",
	"CmdFlush",
    "CmdTestRead",
    "CmdUploadStackCheck",
    "CmdGetSPRP",

	// SDR
#ifdef USE_SDR
    "CmdSetRXFreq",
    "CmdSetRXNsamps",
    "CmdSetGenFreq",
    "CmdSetGenAttn",
    "CmdGetRX",
    "CmdClrRXOvfl",
    "CmdSetWFFreq",
    "CmdSetWFDecim",
    "CmdWFReset",
    "CmdGetWFSamples",
    "CmdGetWFContSamps",
    "CmdSetOVMask",
    "CmdGetADCCtr",
    "CmdSetADCLvl",
#endif

	// GPS
#ifdef USE_GPS
    "CmdSample",
    "CmdSetChans",
    "CmdSetMask",
    "CmdSetRateCG",
    "CmdSetRateLO",
    "CmdSetGainCG",
    "CmdSetGainLO",
    "CmdSetSat",
    "CmdSetE1Bcode",
    "CmdSetPolarity",
    "CmdPause",
    "CmdGetGPSSamples",
    "CmdGetChan",
    "CmdGetClocks",
    "CmdGetGlitches",
    "CmdIQLogReset",
    "CmdIQLogGet",
#endif

#ifdef USE_OTHER
    SPI_OTHER_ENUM_STR
#endif

    // pseudo for debugging
    #define NSPI_CMD_PSEUDO 2
    "(CmdCheckLast)",
    "CmdPumpFlush"
};


typedef struct {
    u4_t xfers, flush, bytes;
    u4_t retry;
    
    #define NRETRY_HIST 8
    u4_t retry_hist[NRETRY_HIST];
} spi_t;

extern spi_t spi;

#define DMA_ALIGNMENT __attribute__ ((aligned(256)))
#define	PAD_FRONT u4_t pad_front[256/4]
#define	PAD_BACK u4_t pad_back[256/4]
#define N_SPI_TX 8

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

typedef struct {
	uint16_t cmd;
	uint16_t wparam;
	uint16_t lparam_lo;
	uint16_t lparam_hi;
	uint16_t w2param;
	uint8_t _pad_;      // 3 LSBs stay in ha_disr[2:0]
} spi_mosi_data_t;

typedef struct {
	uint16_t cmd;
	uint16_t wparam;
	uint16_t w2param;
	uint16_t w3param;
	uint16_t w4param;
	uint8_t _pad_;      // 3 LSBs stay in ha_disr[2:0]
} spi_mosi_data2_t;

typedef struct {
	PAD_FRONT;
	union {
		SPI_T msg[1];
		u1_t bytes[SPIBUF_B];		// because tx/rx DMA sizes equal
        u2_t words[SPIBUF_W];
		spi_mosi_data_t data;
		spi_mosi_data2_t data2;
	};
	PAD_BACK;
} DMA_ALIGNMENT SPI_MOSI;

typedef struct {
	PAD_FRONT;
#ifdef SPI_8
    u1_t _align_;
#endif
	union {
		SPI_T msg[1];
		struct {
            #define SPI_BUSY        (0x90 << SPI_SFT)   // previous request not yet serviced by embedded CPU
            #define SPI_ACK         (0x80 << SPI_SFT)   // ~ha_ack in is bit 0x10 (host.v)
            #define SPI_BUSY_MASK   (0xf0 << SPI_SFT)
			#define SPI_ADC_OVFL    (0x08 << SPI_SFT)
			#define SPI_AVAILABLE   (0x07 << SPI_SFT)
			SPI_ST status;

			union {
				char byte[SPIBUF_B];
				uint16_t word[SPIBUF_W];
			};
		} __attribute__((packed));
	};
	PAD_BACK;
	u2_t len_xfers, len_bytes;
	uint16_t cmd;
	u2_t tid;
} __attribute__((packed)) DMA_ALIGNMENT SPI_MISO;


extern u4_t spi_retry;

#define spi_set _spi_set
//#define spi_set spi_set_noduplex

#define spi_get _spi_get
//#define spi_get spi_get_noduplex

void spi_init();
void spi_stats();

void _spi_set(SPI_CMD cmd, uint16_t wparam=0, uint32_t lparam=0);
void spi_set3(SPI_CMD cmd, uint16_t wparam, uint32_t lparam, uint16_t w2param);
void spi_set_noduplex(SPI_CMD cmd, uint16_t wparam=0, uint32_t lparam=0);
void spi_set4_noduplex(SPI_CMD cmd, uint16_t wparam=0, uint16_t w2param=0, uint16_t w3param=0, uint16_t w4param=0);
void spi_set_buf_noduplex(SPI_CMD cmd, SPI_MOSI *tx, int bytes);

void _spi_get(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);
void spi_get_pipelined(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);
void spi_get_noduplex(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);
void spi_get3_noduplex(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint16_t w2param=0, uint16_t w3param=0);
