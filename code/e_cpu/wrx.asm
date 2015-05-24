
; ============================================================================
; Homemade GPS Receiver
; Copyright (C) 2013 Andrew Holme
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
; http://www.holmea.demon.co.uk/GPS/Main.htm
; ============================================================================

; Copyright (c) 2014 John Seamons, ZL/KF6VO

#include ../wrx.config

				MACRO	RdReg32 reg
				 rdReg	reg							; 0,l
				 rdReg	reg							; 0,l 0,h
				 swap16								; 0,l h,0
				 add								; h,l
				ENDM

				MACRO	WrReg32 reg
				 dup								; h,l h,l
				 swap16								; h,l l,h
				 wrReg	reg							; h,l
				 wrReg	reg							;
				ENDM

				MACRO	SetReg reg
				 rdReg	HOST_RX
				 wrReg	reg
				ENDM

				MACRO	SvcGPS chan					; ... flag
				 brZ	_svcGPS # chan
			wrEvt2	CPU_CTR_ENA
				 push	chan * sizeof GPS_CHAN + GPS_channels
				 push	chan
				 call	GPS_Method
			wrEvt2	CPU_CTR_DIS
_svcGPS # chan # :										; ...
				ENDM

				MACRO	SvcRX chan					; ... flag
				 brZ	_svcRX # chan
			wrEvt2	CPU_CTR_ENA
				 push	1 << chan
				 push	chan * sizeof RX_CHAN + RX_channels
				 push	chan
				 call	RX_Buffer
			wrEvt2	CPU_CTR_DIS
_svcRX # chan # :										; ...
				ENDM

; ============================================================================

Entry:
				push	0							; disable any channel srqs
				wrReg	SET_MASK
				rdReg	GET_SRQ
				pop

Ready:
			wrEvt2	CPU_CTR_DIS
				wrEvt	HOST_RDY

Main:
#if USE_HBEAT
				call	heartbeat
#endif

				// poll for srqs
				rdReg	GET_SRQ						; 0
				rdBit								; host_srq
	
#if GPS_CHANS
				REPEAT	GPS_CHANS
				 push	0
				 rdBit
				ENDR								; host_srq f(GPS_CHANS-1) ... f(0)
#endif

				wrEvt2	GET_RX_SRQ
				
				REPEAT	RX_CHANS
				 push	0
				 rdBit2
				ENDR								; host_srq gps_srq(GPS_CHANS-1) ... (0) rx_srq(RX_CHANS-1) ... (0)
				
				REPEAT	RX_CHANS
				 SvcRX	<iter>
				ENDR								; host_srq gps_srq(GPS_CHANS-1) ... (0)

#if GPS_CHANS
				REPEAT	GPS_CHANS
				 SvcGPS	<iter>
				ENDR								; host_srq
#endif
				
				brZ		Main
			wrEvt2	CPU_CTR_ENA
				wrEvt	HOST_RST
				rdReg	HOST_RX						; cmd
				shl
				push	Commands
				add									; &Commands[cmd]
				fetch16
				push	Ready
				to_r
				to_r
				ret

; ============================================================================

// CmdPing and CmdLoad must be in first 1K of CPU RAM (i.e. loaded by boot)
CmdPing:
				wrEvt	HOST_RST
                push	0xcafe
                wrReg	HOST_TX
                ret

CmdLoad:		rdReg	HOST_RX						; insn
				rdReg	HOST_RX						; insn addr
				store16								; addr
                drop.r

CmdGetStatus:
				wrEvt	HOST_RST
                rdReg	GET_STATUS
                push	STAT_FW_ID		// fixme: fix assembler to handle '~ const'
                not
                and
                push	FW_ID
                or
                wrReg	HOST_TX
                ret

CmdGetMem:
#if USE_DEBUG
				rdReg	HOST_RX
				dup
				wrEvt	HOST_RST
				fetch16
                wrReg	HOST_TX
                wrReg	HOST_TX
                ret
#else
				ret
#endif

CmdDummy:		ret

CmdTestRead:
				wrEvt	HOST_RST
#if SPI_32
				push	2048 / 2 / 2
				//push	1024 / 2 / 2
tr_more:
				dup
                wrReg	HOST_TX
				push	tr_id
				fetch16
                wrReg	HOST_TX
#else
				push	2048 / 8 / 4
tr_more:
				push	0
				REPEAT	8
				 wrEvt	GET_MEMORY
				 wrEvt	GET_MEMORY
				ENDR
				pop
#endif
				push	1
				sub
				dup
				brNZ	tr_more
#if SPI_32
				push	tr_id
				fetch16
                addi	1
				push	tr_id
				store16
				pop
				drop.r

tr_id:			u16		0
#else
				drop.r
#endif

#if USE_HBEAT
heartbeat:
				push	CTRL_LED_0
				push	hb
				fetch32
				swap16
				push	0x0001
				and
				brZ		hb_z
				call	ctrl_set
				br		hb_cont

hb:				u32		0

hb_z:
				call	ctrl_clr
hb_cont:
				push	hb
				fetch32
				addi	1
				push	hb
				store32
				drop.r
#endif

; ============================================================================

				// order must match that in spi.h
Commands:
				u16		CmdSetRXFreq
				u16		CmdSetGen
				u16		CmdSetGenAttn
				u16		CmdPing
				u16		CmdLoad
				u16		CmdPing2
				u16		CmdEnableRX
				u16		CmdGetRX
				u16		CmdClrRXOvfl
				u16		CmdSetWFFreq
				u16		CmdSetWFDecim
				u16		CmdWFSample
				u16		CmdGetWFSamples
				u16		CmdCPUCtrClr
				u16		CmdGetCPUCtr
				u16		CmdCtrlSet
				u16		CmdCtrlClr
				u16		CmdGetMem
				u16		CmdGetStatus
				u16		CmdDummy
				u16		CmdTestRead
#if GPS_CHANS
				u16		CmdSample
				u16		CmdSetMask
				u16		CmdSetRateCA
				u16		CmdSetRateLO
				u16		CmdSetGainCA
				u16		CmdSetGainLO
				u16		CmdSetSV
				u16		CmdPause
				u16		CmdGetGPSSamples
				u16		CmdGetChan
				u16		CmdGetClocks
				u16		CmdGetGlitches
#endif


; ============================================================================
; pseudo instructions
; ============================================================================

fetch64:									; a                            19
                dup							; a a
                addi	4					; a a+4
                fetch32						; a [63:32]
                swap						; [63:32] a
                // fall through ...

fetch32:									; a                             8
                dup							; a a
                fetch16						; a [15:0]
                swap						; [15:0] a
                addi	2					; [15:0] a+2
                fetch16						; [15:0] [31:16]
                swap16						; [15:0] [31:16]<<16
                add.r						; [31:0]

// NB
// in mem: a: [31:0] a+4: [63:32]
// on stk: nos: [63:32] tos: [31:0]
store64:									; [63:32] [31:0] a             17
                store32						; [63:32] a
                addi	4					; [63:32] a+4	NB: means store64 returns a+4, not a!
                // fall through ...

store32:									; [31:0] a                      8
                over						; [31:0] a [31:0]
                swap16						; [15:0] a [31:16]
                over						; [15:0] a [31:16] a
                addi	2					; [15:0] a [31:16] a+2
                store16						; [15:0] a a+2
                drop						; [15:0] a
                store16.r					; a

swap64:										; ah al bh bl                   6
                rot							; ah bh bl al
                to_r						; ah bh bl          ; al
                rot							; bh bl ah          ; al
                r_from						; bh bl ah al
                ret

add64:										; ah al bh bl                   7
                rot							; ah bh bl al
                add							; ah bh sl
                to_r						; ah bh             ; sl
                add.cin						; sh                ; sl
                r_from						; sh sl
                ret

add64nc:										; ah al bh bl                   7
                rot							; ah bh bl al
                add							; ah bh sl
                to_r						; ah bh             ; sl
                add						; sh                ; sl
                r_from						; sh sl
                ret

sub64:										; ah al bh bl
				not
				swap
				not
				swap						; ah al ~bh ~bl
				push	0
				push	1					; ah al ~bh ~bl 0 1
				add64						; ah al (~b+1)h (~b+1)l
				add64						; (a-b)h (a-b)l
                ret

shl64_n:									; i64 n                       n+8
                push	Shifted				; i64 n Shifted
                swap						; i64 Shifted n
                shl							; i64 Shifted n*2
                sub							; i64 Shifted-n*2
                to_r						; i64               ; Shifted-n*2
                ret							; computed jump

                REPEAT	32
                 shl64						; i64<<n
                ENDR
Shifted:        ret

neg32:			push	0					; i32 0
				swap						; 0 i32
				sub.r						; -i32

extend:										; i32                           10
				push	0					; i32 0
				over						; i32 0 i32
				shl64						; i32 sgn xxx
				pop							; i32 sgn
				neg32						; i32 -sgn
				swap.r						; i64

// returns [31] in [15] i.e. if tos is a negative signed value
sgn32:			swap16						; i32>>16	because brN/NZ only looks at [15:0]
                push	0x4000				; h 0x4000
                shl							; h 0x8000		// fixme: use fixed push that uses not insn
                and.r						; i32<0? [15]

abs32:
				dup							; i32 i32
				sgn32						; i32 i32<0?
				brZ		abs32_1				; i32
				neg32						; -i32
abs32_1:		ret

// rdBit a 16-bit word
rdBit16:
				push	0
rdBit16z:
				REPEAT	16
				 rdBit
				ENDR
				ret


; ============================================================================
; support
; ============================================================================

CmdCtrlSet:		rdReg	HOST_RX
ctrl_set:
				push	ctrl
				fetch16
				or
				br		ctrl_cont

CmdCtrlClr:		rdReg	HOST_RX
ctrl_clr:
				not
				push	ctrl
				fetch16
				and
ctrl_cont:
				dup
				push	ctrl
				store16
				drop
				wrReg2	SET_CTRL
				ret
			
ctrl:			u16		0

CmdCPUCtrClr:	wrEvt2	CPU_CTR_CLR
            	ret

CmdGetCPUCtr:
				wrEvt	HOST_RST
				rdReg	GET_CPU_CTR0
				wrReg	HOST_TX
				rdReg	GET_CPU_CTR1
				wrReg	HOST_TX
				rdReg	GET_CPU_CTR2
				wrReg	HOST_TX
				rdReg	GET_CPU_CTR3
				wrReg	HOST_TX
				ret


; ============================================================================
; receiver
; ============================================================================

				STRUCT	RX_CHAN
				 u16	rx_enabled			1	; 
				 u16	rx_wcount			1	; counts are number of iq pairs in buffer
				ENDS

RX_channels:	REPEAT	RX_CHANS
				 RX_CHAN	
				ENDR

GetRXchanPtr:	push	sizeof RX_CHAN			; rx# sizeof
				mult							; offset
				push	RX_channels				; baseaddr
				add.r							; ba + offset

CmdEnableRX:
				rdReg	HOST_RX				; rx#
				dup
				wrReg2	SET_RX_CHAN
				call	GetRXchanPtr
				to_r
				
				RdReg32	HOST_RX
				r
				addi	rx_enabled
                store16
                drop
                
                // reset buffer & count
                push	0					; bit
                br		rx_flip

				// NB: this requires that the main loop cycles at audio rate or better
RX_Buffer:									; bit this rx#
				wrReg2	SET_RX_CHAN			; bit this
                to_r						; bit

                r
                addi	rx_enabled
                fetch16
                brZ		rx_buf_exit			; bit
                
                r
                addi	rx_wcount			; bit &rx_wcount
                fetch16						; bit rx_wcount
                addi	1					; bit rx_wcount++
                dup							; bit rx_wcount++ rx_wcount++
                
                r
                addi	rx_wcount			; bit rx_wcount++ rx_wcount++ &rx_wcount
                store16						; bit rx_wcount++ &rx_wcount
				drop						; bit rx_wcount++
				
				push	NRX_SAMPS			; bit rx_wcount++ NRX_SAMPS
				sub							; bit (rx_wcount == NRX_SAMPS)?
				brNZ	rx_buf_exit			; bit
				
				// flip rx buffers
rx_flip:
				wrEvt2	RX_FLIP_BUFS		; bit
				
				// reset count
				push	0
				r
				addi	rx_wcount
				store16
				drop						; bit
				
				// signal host				; bit
				call	ctrl_set			;
				push	0
				
rx_buf_exit:								; bit
				drop						;
                r_from						; this
                drop.r						;

CmdGetRX:
				rdReg	HOST_RX				; bit
				rdReg	HOST_RX				; bit rx#
				wrReg2	SET_RX_CHAN			; bit
				call	ctrl_clr			;
				wrEvt	HOST_RST
				push	NRX_SAMPS			; cnt
				
rx_more:									; cnt
				wrEvt2	GET_RX_SAMP			; move i
				wrEvt2	GET_RX_SAMP			; move q
				wrEvt2	GET_RX_SAMP			; move iq3
				push	1					; cnt 1
				sub							; cnt--
				dup
				brNZ	rx_more
				drop.r

CmdSetRXFreq:	rdReg	HOST_RX				; rx#
				wrReg2	SET_RX_CHAN			;
                RdReg32	HOST_RX				; freq
                wrReg2	SET_RX_FREQ			;
                ret

CmdClrRXOvfl:	// assume currently selected rx
				wrEvt	CLR_RX_OVFL
				ret

CmdSetGen:
#if	USE_GEN
				rdReg	HOST_RX				; wparam
                RdReg32	HOST_RX				; wparam lparam
                wrReg2	SET_GEN				; wparam
                drop.r
#else
                ret
#endif

CmdSetGenAttn:
#if	USE_GEN
				rdReg	HOST_RX				; wparam
                RdReg32	HOST_RX				; wparam lparam
                wrReg2	SET_GEN_ATTN		; wparam
                drop.r
#else
                ret
#endif


; ============================================================================
; waterfall
; ============================================================================

CmdWFSample:	wrEvt2	WF_SAMPLER_RST
            	ret

CmdGetWFSamples:
				wrEvt	HOST_RST
				push	NWF_SAMPS_LOOP
wf_more:
				REPEAT	NWF_SAMPS_RPT
#if	USE_WF_PUSH24
				 wrEvt2	GET_WF_SAMP_3
#endif
				 wrEvt2	GET_WF_SAMP_I
				 wrEvt2	GET_WF_SAMP_Q
				ENDR
				
				push	1
				sub
				dup
				brNZ	wf_more
				drop.r

CmdSetWFFreq:	rdReg	HOST_RX				; wparam
				wrReg2	SET_WF_CHAN			;
                RdReg32	HOST_RX				; lparam
                wrReg2	SET_WF_FREQ			;
				ret

CmdSetWFDecim:	rdReg	HOST_RX
				wrReg2	SET_WF_DECIM
				ret


; ============================================================================
; GPS
; ============================================================================

#if GPS_CHANS
				DEF		MAX_BITS	64
	
				STRUCT	GPS_CHAN
				 u16	ch_NAV_MS		1						; Milliseconds 0 ... 19
				 u16	ch_NAV_BITS		1						; Bit count
				 u16	ch_NAV_GLITCH	1						; Glitch count
				 u16	ch_NAV_PREV		1						; Last data bit = ip[15]
				 u16	ch_NAV_BUF		MAX_BITS / 16			; NAV data buffer
				 u64	ch_CA_FREQ		1						; Loop integrator
				 u64	ch_LO_FREQ		1						; Loop integrator
				 u16	ch_IQ			2						; Last IP, QP
				 u16	ch_CA_GAIN		2						; KI, KP-KI = 20, 27-20
				 u16	ch_LO_GAIN		2						; KI, KP-KI = 21, 28-21
				 u16	ch_unlocked		1
				ENDS

GPS_channels:	REPEAT	GPS_CHANS
				 GPS_CHAN	
				ENDR

GetGPSchanPtr:	push	sizeof GPS_CHAN			; chan# sizeof
				mult							; offset
				push	GPS_channels			; baseaddr
				add.r							; ba + offset

; ============================================================================
;	err64 = ext64(pe-pl:32)
;	ki.e64 = err64 << (ki=ch_CA_gain[0]:16)
;	new64 = (ch_CA_FREQ:64) + ki.e64
;	(ch_CA_FREQ:64) = new64
;	kp.e64 = ki.e64 << ("ki-kp"=ch_CA_gain[1]:16)
;	nco64 = new64 + kp.e64
;	wrReg nco64:32
	
;	fetch64: H L(tos)

				MACRO	CloseLoop freq gain nco
										; err32, i.e. pe-pl for CA loop, ip*qp for LO loop
				 extend					; err64                         9
				 r						; err64	this	                1
				 addi	gain			; err64 &this.gain[0]           1
				 fetch16				; err64 ki                      1
				 shl64_n				; ki.e64                     ki+8
				 over					; ki.e64 ki.e64                 1
				 over					; ki.e64 ki.e64                 1
				 r						; ki.e64 ki.e64 this			1
				 addi	freq			; ki.e64 ki.e64 &this.freq		1
				 fetch64				; ki.e64 ki.e64 old64          19
				 add64					; ki.e64 new64                  7
				 over					; ki.e64 new64 new64            1
				 over					; ki.e64 new64 new64            1
				 r						; ki.e64 new64 new64 this		2
				 addi	freq			; ki.e64 new64 new64 &this.freq 2
				 store64				; ki.e64 new64 &this.freq	   17
				 pop					; ki.e64 new64					1
				 swap64					; new64 ki.e64                  6
				 r						; new64 ki.e64 this				1
				 addi	gain + 2		; new64 ki.e64 &this.gain[1]	1
				 fetch16				; new64 ki.e64 kp-ki            1
				 shl64_n				; new64 kp.e64            kp-ki+8
				 add64					; nco64                         7
				 pop					; nco32                         1
				 wrReg	nco				;                               1
				ENDM					;                 TOTAL = kp + 98
			
; ============================================================================

// get 15-bit I/Q data [15:1] arranged into 16 bits as: [15][14:1]0
GetCount:		push	0						; 0								20
				rdBit							; [15]
GetCount2:
				REPEAT 14
				 rdBit							; [15:1]
				ENDR
				shl.r							; [15:0]

GetPower:		call	GetCount				; i								48
				dup
				mult							; i^2
				call	GetCount				; i^2 q
				dup
				mult							; i^2 q^2
				add.r							; p

; ============================================================================

GPS_Method:									; this ch#
				wrReg	SET_CHAN			; this
                to_r						;

                rdReg	GET_CHAN_IQ			; 0
                rdBit						; bit			keep msb of I as nav data
                dup							; bit bit
                call	GetCount2			; bit ip
                call	GetCount   		    ; bit ip qp

                over
                over						; bit ip qp ip qp
                r							; bit ip qp ip qp this
                addi	ch_IQ				; bit ip qp ip qp &q
                store16
                addi	2					; bit ip qp ip &i
                store16
                drop						; bit ip qp

				over						; bit ip qp ip
				over						; bit ip qp ip qp
                mult						; bit ip qp ip*qp

                CloseLoop ch_LO_FREQ ch_LO_GAIN SET_LO_NCO

                dup							; bit ip qp qp
                mult						; bit ip qp^2
                swap						; bit qp^2 ip
                dup							; bit qp^2 ip ip
                mult						; bit qp^2 ip^2
                add							; bit pp
                call	GetPower			; bit pp pe
                over						; bit pp pe pp
                over						; bit pp pe pp pe
                sub							; bit pp pe pp-pe
                sgn32						; bit pp pe (pp-pe)<0[15]
                rot							; bit pe (pp-pe)<0 pp
                call	GetPower			; bit pe (pp-pe)<0 pp pl
                swap						; bit pe (pp-pe)<0 pl pp
                over						; bit pe (pp-pe)<0 pl pp pl
                sub							; bit pe (pp-pe)<0 pl pp-pl
                sgn32						; bit pe (pp-pe)<0 pl (pp-pl)<0[15]
                rot							; bit pe pl (pp-pl)<0 (pp-pe)<0
				or							; bit pe pl unlocked:15
                r							; bit pe pl unlocked this
                addi	ch_unlocked			; bit pe pl unlocked &ch_unlocked
                store16						; bit pe pl &ch_unlocked
                pop							; bit pe pl
                sub							; bit pe-pl

                CloseLoop ch_CA_FREQ ch_CA_GAIN SET_CA_NCO


				// process NAV data (20 msec per bit @ 50 bps)
				
				//	if (bit == ch_NAV_PREV) {
				//		// NavSame
				//		if (ch_NAV_MS != 19) ch_NAV_MS++, return
				//		
				//		ch_NAV_MS = 0
				//		ch_NAV_BITS = (ch_NAV_BITS + 1) & (MAX_BITS - 1)
				//		ch_NAV_BUF[] <<= |= bit
				//		return
				//	}
				//	
				//	ch_NAV_PREV = bit
				//	
				//	if (ch_NAV_MS != 0) ch_NAV_GLITCH++
				//	
				//	// NavEdge
				//	ch_NAV_MS = 1
				//	return


				// if bit == ch_NAV_PREV goto NavSame
                r							; bit this
                addi	ch_NAV_PREV			; bit &prev
                fetch16						; bit prev
                over						; bit prev bit
                sub							; bit diff
                brZ		NavSame				; bit

                r							; bit this
                addi	ch_NAV_PREV			; bit &prev
                store16
                drop						;
                
				// if ch_NAV_MS == 0 then goto NavEdge
                r							; this
                addi	ch_NAV_MS			; &ms
                fetch16						; ms
                brZ		NavEdge

				// ch_NAV_GLITCH++
                r							; this
                addi	ch_NAV_GLITCH		; &g
                fetch16						; g
                addi	1					; g+1
                r							; g+1 this
                addi	ch_NAV_GLITCH		; g+1 &g
                store16
                drop						;

NavEdge:        // ch_NAV_MS = 1
				push	1					; 1
                r_from						; 1 this
                addi	ch_NAV_MS			; 1 &ms
                store16
                drop.r						;

NavSame:        // if ch_NAV_MS == 19 then goto NavSave
				r							; bit this
                addi	ch_NAV_MS			; bit &ms
                dup							; bit &ms &ms
                fetch16						; bit &ms ms
                push	19
                sub							; bit &ms ms-19
                brZ		NavSave				; bit &ms

				// else ch_NAV_MS++
                fetch16						; bit ms
                addi	1					; bit ms+1
                r_from						; bit ms+1 this
                addi	ch_NAV_MS			; bit ms+1 &ms
                store16
                drop						; bit
                drop.r						;

NavSave:        // ch_NAV_MS = 0
				push	0
				swap						; bit 0 &ms
                store16
                drop						; bit

				// ch_NAV_BITS = (ch_NAV_BITS + 1) & (MAX_BITS - 1)
                r							; bit this
                addi	ch_NAV_BITS			; bit &cnt
                fetch16						; bit cnt
                dup							; bit cnt cnt
                addi	1					; bit cnt cnt+1
                push	MAX_BITS - 1
                and							; bit cnt wrapped
                r							; bit cnt wrapped this
                addi	ch_NAV_BITS			; bit cnt wrapped &cnt
                store16
                drop						; bit cnt

				// ch_NAV_BUF[] <<= |= bit
				REPEAT	4
				 shr						; bit cnt/16
				ENDR
                shl							; bit offset
                r_from						; bit offset this
                addi	ch_NAV_BUF			; bit offset buf
                add							; bit ptr
                dup							; bit ptr ptr
                to_r						; bit ptr
                fetch16						; bit old
                shl							; bit old<<1
                add							; new
                r_from						; new ptr
                store16
                drop.r						;
			
; ============================================================================

UploadSamples:	REPEAT	16
				 wrEvt	GET_GPS_SAMPLES
				ENDR
				ret

UploadChan:										; &GPS_channels[n]
				REPEAT	sizeof GPS_CHAN / 2
				 wrEvt	GET_MEMORY
				ENDR
				ret

// "wrEvt GET_MEMORY" side-effect: auto mem ptr incr, i.e. 2x tos += 2 (explains "-4" below)
UploadClock:									; &GPS_channels
				wrEvt	GET_MEMORY				; GPS_channels++->ch_NAV_MS
				wrEvt	GET_MEMORY				; GPS_channels++->ch_NAV_BITS
				rdBit16
				wrReg	HOST_TX
				addi.r	sizeof GPS_CHAN - 4		; &GPS_channels++

UploadGlitches:									; &GPS_channels + ch_NAV_GLITCH
				wrEvt	GET_MEMORY
				addi.r	sizeof GPS_CHAN - 2

; ============================================================================

				MACRO	SetRate member nco		;
				 rdReg	HOST_RX					; chan
				 RdReg32 HOST_RX				; chan freq32
				 swap							; freq32 chan
				 over							; freq32 chan freq32
				 over							; freq32 chan freq32 chan
				 call	GetGPSchanPtr			; freq32 chan freq32 this
				 addi	member					; freq32 chan freq32 &freq
				 push	0						; freq32 chan freq32 &freq 0
				 swap							; freq32 chan freq64 &freq
				 store64						; freq chan &freq
				 drop							; freq chan
				 wrReg	SET_CHAN				; freq
				 wrReg	nco						;
				ENDM

				MACRO	SetGain member			;
				 rdReg	HOST_RX					; chan
				 RdReg32 HOST_RX				; chan kp,ki
				 swap							; kp,ki chan
				 call	GetGPSchanPtr			; kp,ki this
				 addi	member					; kp,ki &gain
				 store32						; &gain
				 drop							;
				ENDM

; ============================================================================

CmdSample:		wrEvt	GPS_SAMPLER_RST
            	ret

CmdSetMask:     SetReg	SET_MASK
                ret

CmdSetRateCA:   SetRate	ch_CA_FREQ SET_CA_NCO
                ret

CmdSetRateLO:   SetRate	ch_LO_FREQ SET_LO_NCO
                ret

CmdSetGainCA:   SetGain ch_CA_GAIN
                ret

CmdSetGainLO:   SetGain ch_LO_GAIN
                ret

CmdSetSV:       SetReg	SET_CHAN
                SetReg	SET_SV
                ret

CmdPause:       SetReg	SET_CHAN
                SetReg	SET_PAUSE
                ret

CmdGetGPSSamples:
				wrEvt	HOST_RST			; 16*2 = 32 bytes per SPI transfer?
				REPEAT	16					; 16 here * 16 in UploadSamples * 2 B/W = 512 read length in search.cpp
				 call	UploadSamples
				ENDR
				ret

CmdGetChan:     rdReg	HOST_RX				; wparam
                wrEvt	HOST_RST			; chan
                call	GetGPSchanPtr		; this
                call	UploadChan			; this++
                drop.r

CmdGetClocks:   wrEvt	HOST_RST
                rdReg	GET_SNAPSHOT		; side-effect: push 0
                rdBit16z
                wrReg	HOST_TX
                rdBit16
                wrReg	HOST_TX
                rdBit16
                wrReg	HOST_TX
                REPEAT	GPS_CHANS
                 rdBit						; chan srq
                ENDR
                wrReg	HOST_TX
                push	GPS_channels
                REPEAT	GPS_CHANS
                 call	UploadClock
                ENDR
                drop.r

CmdGetGlitches: wrEvt	HOST_RST
                push	GPS_channels + ch_NAV_GLITCH
                REPEAT	GPS_CHANS
                 call	UploadGlitches
                ENDR
                drop.r
#endif

; ============================================================================

// at end to check if firmware-assisted load of code worked
CmdPing2:		wrEvt	HOST_RST
			#if 1
				REPEAT	512
				 nop						; fixme REMOVE eventually
				ENDR
			#endif
                push	0xbabe
                wrReg	HOST_TX
                ret
