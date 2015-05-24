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

#include <wrx.h>
#include <inttypes.h>

enum SPI_CMD { // Embedded CPU commands, order must match 'Commands:' table in .asm code

	// receiver
    CmdSetRXFreq,
    CmdSetGen,
    CmdSetGenAttn,
    CmdPing,
    CmdLoad,
    CmdPing2,
    CmdEnableRX,
    CmdGetRX,
    CmdClrRXOvfl,
    CmdSetWFFreq,
	CmdSetWFDecim,
    CmdWFSample,
    CmdGetWFSamples,
    CmdCPUCtrClr,
    CmdGetCPUCtr,
    CmdCtrlSet,
    CmdCtrlClr,
	CmdGetMem,
	CmdGetStatus,
	CmdDummy,
    CmdTestRead,

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
    
    // virtual
    CmdDuplex,
    CmdNoDuplex,
};

static const char *cmds[] = {

	// receiver
    "CmdSetRXFreq",
    "CmdSetGen",
    "CmdSetGenAttn",
    "CmdPing",
    "CmdLoad",
    "CmdPing2",
    "CmdEnableRX",
    "CmdGetRX",
    "CmdClrRXOvfl",
    "CmdSetWFFreq",
    "CmdSetWFDecim",
    "CmdWFSample",
    "CmdGetWFSamples",
    "CmdCPUCtrClr",
    "CmdGetCPUCtr",
    "CmdCtrlSet",
    "CmdCtrlClr",
    "CmdGetMem",
	"CmdGetStatus",
	"CmdDummy",
    "CmdTestRead",

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

    // virtual
    "CmdDuplex",
    "CmdNoDuplex",
};

#ifdef SPI_8
	#define SPI_T		char
	#define SPI_ST		u1_t
	#define SPI_B2X(b)	b
	#define SPI_X2B(b)	b
	#define	SPI_SFT		0
#endif
#ifdef SPI_16
	#define SPI_T		u2_t
	#define SPI_ST		u2_t
	#define SPI_B2X(b)	B2S(b)
	#define SPI_X2B(b)	S2B(b)
	#define	SPI_SFT		8
#endif
#ifdef SPI_32
	#define SPI_T		u4_t
	#define SPI_ST		u4_t
	#define SPI_B2X(b)	B2I(b)
	#define SPI_X2B(b)	I2B(b)
	#define	SPI_SFT		24
#endif

union SPI_MOSI {
    SPI_T msg[1];
    struct {
        uint16_t cmd;
        uint16_t wparam;
        uint32_t lparam;
        uint8_t _pad_; // 3 LSBs stay in ha_disr[2:0]
    };
    //SPI_MOSI(uint16_t c, uint16_t w=0, uint32_t l=0) :
    //    cmd(c), wparam(w), lparam(l), _pad_(0) {}
};

#define	NSPI_RX		2048		// limited by use of single 2K x 8 BRAM in host.v

struct SPI_MISO {
#ifdef SPI_8
    u1_t _align_;
#endif
    union {
        SPI_T msg[1];
        struct {
            SPI_ST status;
            union {
                char byte[NSPI_RX];
                uint16_t word[NSPI_RX/2];
            };
        }__attribute__((packed));
    };
    int len_xfers;
	uint16_t cmd;
}__attribute__((packed));

void spi_init();
void spi_set(SPI_CMD cmd, uint16_t wparam=0, uint32_t lparam=0);
void spi_get(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);
void spi_set_noduplex(SPI_CMD cmd, uint16_t wparam=0, uint32_t lparam=0);
void spi_get_noduplex(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam=0, uint32_t lparam=0);

#endif
