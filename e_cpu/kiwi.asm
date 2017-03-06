
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

; Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#include ../kiwi.config

				MACRO	FreezeTOS
				 wrEvt2	FREEZE_TOS
				 nop								; enough time for latch and sync
				 nop
				ENDM

				MACRO	StackCheck	which expected
#if STACK_CHECK
				 push	which
				 push	expected
				 call	stack_check
#endif
				ENDM

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
			wrEvt	CPU_CTR_ENA
				 push	chan * sizeof GPS_CHAN + GPS_channels
				 push	chan
				 call	GPS_Method					; this ch#
			wrEvt	CPU_CTR_DIS
_svcGPS # chan # :									; ...
				ENDM

; ============================================================================

Entry:
				nop									; FIXME: due to pipelining of insn BRAM
				nop									; first two insns are not executed
				
#if STACK_CHECK
				push	sp_reenter
				incr16
				pop
#endif

				push	0							; disable any channel srqs
				wrReg	SET_MASK
				rdReg	GET_SRQ
				pop

Ready:
			wrEvt	CPU_CTR_DIS
				wrEvt	HOST_RDY
#if USE_HBEAT
				call	heartbeat
#endif
				StackCheck	sp_ready 0

NoCmd:
				// poll for srqs
				rdReg	GET_SRQ						; 0
				rdBit								; host_srq

#if GPS_CHANS
				REPEAT	GPS_CHANS
				 push	0
				 rdBit
				ENDR								; host_srq gps_srq(GPS_CHANS-1) ... (0)
#endif

#if RX_CHANS
				rdReg	GET_RX_SRQ					; host_srq gps_srq(GPS_CHANS-1) ... (0) 0
				rdBit2								; host_srq gps_srq(GPS_CHANS-1) ... (0) rx_srq
				
				brZ		no_rx_svc
			wrEvt	CPU_CTR_ENA
				call	RX_Buffer
			wrEvt	CPU_CTR_DIS

				StackCheck	sp_rx (GPS_CHANS + 1)
no_rx_svc:											; host_srq gps_srq(GPS_CHANS-1) ... (0)
#endif

#if GPS_CHANS
				REPEAT	GPS_CHANS
				 SvcGPS	<iter>
				 StackCheck	(sp_gps # <iter>) (GPS_CHANS - <iter>)
				ENDR								; host_srq
#endif
				
				brZ		NoCmd
			wrEvt	CPU_CTR_ENA
				wrEvt	HOST_RST
				rdReg	HOST_RX						; cmd
				dup
				push	NUM_CMDS
				swap
				sub									; cmd NUM_CMDS-cmd
				sgn32_16
				brZ		cmd_ok						; cmd
				pop
				br		Ready						; just ignore a bad cmd
cmd_ok:
#if STACK_CHECK
				dup
				push	0xff
				call	ctrl_clr
				push	0xff
				and
				call	ctrl_set					; remember last cmd received
				
				shl									; cmd*2
				dup
				shl									; cmd*2 cmd*4
				push	sp_cmds_save				; don't leave stack mis-aligned during command run
				store16
				pop									; cmd*2
				push	Commands
				add									; &Commands[cmd*2]
				fetch16
				push	sp_return
				to_r
				to_r
				ret
sp_return:
				push	sp_cmds_save
				fetch16
				push	sp_cmds
				add
				push	0							; sp_cmds+cmd*4 0
				call	stack_check
				br		Ready

sp_cmds_save:	u16		0
#else
				shl
				push	Commands
				add									; &Commands[cmd*2]
				fetch16
				push	Ready
				to_r
				to_r
				ret
#endif

; ============================================================================

// CmdPing and CmdLoad must be in first 1K of CPU RAM (i.e. loaded by boot)
CmdPing:
				wrEvt	HOST_RST
                push	0xcafe
                wrReg	HOST_TX

#if SPI_PUMP_CHECK
				push	0
				push	0
				push	0
				push	0
				sp						// sp should be +4 from entry at this point
                wrReg	HOST_TX
                pop
                pop
                pop
                
                push	sp2_seq
                fetch16
                wrReg	HOST_TX

#if STACK_CHECK
                push	sp_reenter
                fetch16
#else
				push	0
#endif
                wrReg	HOST_TX
                
                push	0xbabe
                wrReg	HOST_TX

                push	sp2_seq
                incr16
                drop.r

sp2_seq:		u16		0xc00c
#endif
                ret

CmdLoad:		rdReg	HOST_RX						; insn
				rdReg	HOST_RX						; insn addr
				store16								; addr
                drop.r

CmdGetStatus:
				wrEvt	HOST_RST
                rdReg	GET_STATUS
                push	STAT_FW_ID ~
                and
                push	FW_ID
                or
                wrReg	HOST_TX
                ret

CmdGetMem:
#if USE_DEBUG
				rdReg	HOST_RX						; addr
				dup									; addr addr
				wrEvt	HOST_RST
				fetch16								; addr data
                wrReg	HOST_TX
                wrReg	HOST_TX
                ret
#else
				ret
#endif

CmdFlush:
				ret

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
				push	HEARTBEAT_IND
				push	hb
				fetch16
				push	hb_val
				fetch16
				and
				brZ		hb_flip
				call	ctrl_set
				br		hb_cont

hb:				u16		0
hb_val:			u16		1

hb_flip:
				call	ctrl_clr
hb_cont:
				push	hb
				incr16
				drop.r
#endif

#if STACK_CHECK
stack_check:										; addr #sp
				addi	3							; addr #sp+3			caller did 2 pushes, we're just about to do 1
				push	0							; addr #sp+3 0
				sp									; addr #sp+3 sp			'sp' just overwrites TOS
				dup									; addr #sp+3 sp sp
				rot									; addr sp sp #sp+3
				sub									; addr sp (#sp+3 == sp)?
				brNZ	stack_bad					; addr sp
				pop									; addr
				pop.r
stack_bad:											; addr sp
				push	3							; correct by 3 from above
				sub
				swap								; sp-3 addr				the last bad sp value
				store16								; addr
				addi	2							; addr++				how many times it has happened
				incr16								; *addr++
				pop.r

CmdUploadStackCheck:
				// upload stack check info				
				wrEvt	HOST_RST
                push	0xbeef
                wrReg	HOST_TX

                push	sp_counters
sp_more:
				dup									; a a
                fetch16
                wrReg	HOST_TX						; a
                addi	2
                dup									; a+2 a+2
                push	sp_counters_end
                sub
                brNZ	sp_more						; a+2
                pop
                
                push	sp_seq
                fetch16
                wrReg	HOST_TX

                push	0x8bad
                wrReg	HOST_TX

                push	sp_reenter
                fetch16
                wrReg	HOST_TX

                push	0xfeed
                wrReg	HOST_TX

                push	sp_seq
                incr16
                drop.r

sp_seq:			u16		0xc11c
sp_reenter:		u16		0

sp_counters:
sp_ready:		u16		0			// the last bad sp value
				u16		0			// how many times it has happened
sp_rx:			u16		0
				u16		0
				REPEAT	GPS_CHANS
sp_gps # <iter> # :
				 u16		0
				 u16		0
				ENDR
sp_cmds:
                REPEAT	NUM_CMDS
				 u16	0
				 u16	0
				ENDR
sp_counters_end:

#else
CmdUploadStackCheck:
				ret
#endif

; ============================================================================

				// order must match that in spi.h
Commands:
				u16		CmdSetRXFreq
				u16		CmdSetRXNsamps
				u16		CmdSetGen
				u16		CmdSetGenAttn
				u16		CmdPing
				u16		CmdLoad
				u16		CmdPing2
				u16		CmdGetRX
				u16		CmdClrRXOvfl
				u16		CmdSetWFFreq
				u16		CmdSetWFDecim
				u16		CmdWFReset
				u16		CmdGetWFSamples
				u16		CmdGetWFContSamps
				u16		CmdCPUCtrClr
				u16		CmdGetCPUCtr
				u16		CmdCtrlSet
				u16		CmdCtrlClr
				u16		CmdCtrlGet
				u16		CmdGetMem
				u16		CmdGetStatus
				u16		CmdFlush
				u16		CmdTestRead
				u16		CmdUploadStackCheck
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

// Called inline by asm to generate push constants with bit-15 set (using only a single insn).
// Must be in first half of BRAM in case called by code that loads the second half.
or_0x8000_assist:
				push	0x4000
				shl
				or.r

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

add64nc:									; ah al bh bl                   7
                rot							; ah bh bl al
                add							; ah bh sl
                to_r						; ah bh             ; sl
                add							; sh                ; sl
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
sgn32_16:		swap16						; i32>>16	because brN/NZ only looks at [15:0]
                push	0x8000				; h 0x8000
                and.r						; i32<0? [15]

abs32:
				dup							; i32 i32
				sgn32_16					; i32 i32<0?
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

incr16:										; addr
				dup							; addr addr
				fetch16						; addr data
				addi	1					; addr data+1
				dup							; addr data+1 data+1
				rot							; data+1 data+1 addr
				store16						; data+1 addr
				drop.r						; data+1


; ============================================================================
; support
; ============================================================================

CmdCtrlSet:		rdReg	HOST_RX
ctrl_set:
				push	ctrl
				fetch16
				or
				br		ctrl_update

CmdCtrlClr:		rdReg	HOST_RX
ctrl_clr:
				not
				push	ctrl
				fetch16
				and
ctrl_update:
				dup
				wrReg2	SET_CTRL
				push	ctrl
				store16
				drop
				ret

CmdCtrlGet:		wrEvt	HOST_RST
				push	ctrl
				fetch16
				wrReg	HOST_TX
				ret
			
ctrl:			u16		0

CmdCPUCtrClr:	wrEvt	CPU_CTR_CLR
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

nrx_samps:		u16		0

#if SND_SEQ_CHECK
rx_seq:			u16		0
#endif
				// called at audio buffer flip rate (i.e. every NRX_SAMPS audio samples)
RX_Buffer:
				// if nrx_samps has not been set yet hardware will not interrupt at correct rate
				push	nrx_samps
				fetch16
				brZ		not_init
				
				push	CTRL_INTERRUPT
				call	ctrl_set			; signal the interrupt
				
#if SND_SEQ_CHECK
				// increment seq
                push	rx_seq
                incr16
				pop
#endif
not_init:		ret

CmdGetRX:
				wrEvt	HOST_RST

				push	CTRL_INTERRUPT
				call	ctrl_clr			; clear the interrupt as a side-effect

#if SND_SEQ_CHECK
				push	rx_seq				; &rx_seq
				fetch16						; rx_seq
				push	0x0ff0
				wrReg	HOST_TX
				wrReg	HOST_TX
#endif
				
				push	NRX_SAMPS_LOOP		; cnt
				
rx_more:									; cnt
				REPEAT	NRX_SAMPS_RPT
				 wrEvt2	GET_RX_SAMP			; move i
				 wrEvt2	GET_RX_SAMP			; move q
				 wrEvt2	GET_RX_SAMP			; move iq3
				ENDR
				push	1					; cnt 1
				sub							; cnt--
				dup
				brNZ	rx_more
				pop

				wrEvt2	GET_RX_SAMP			; move ticks[3]
				wrEvt2	GET_RX_SAMP
				wrEvt2	GET_RX_SAMP
				ret

CmdSetRXNsamps:	rdReg	HOST_RX				; nsamps
				dup
				push	nrx_samps
				store16
				pop							; nsamps
				
				FreezeTOS
                wrReg2	SET_RX_NSAMPS		;
                ret

CmdSetRXFreq:	rdReg	HOST_RX				; rx#
				wrReg2	SET_RX_CHAN			;
                RdReg32	HOST_RX				; freq
				FreezeTOS
                wrReg2	SET_RX_FREQ			;
                ret

CmdClrRXOvfl:
				wrEvt2	CLR_RX_OVFL
				ret

CmdSetGen:
#if	USE_GEN
				rdReg	HOST_RX				; wparam
                RdReg32	HOST_RX				; wparam lparam
				FreezeTOS
                wrReg2	SET_GEN				; wparam
                drop.r
#else
                ret
#endif

CmdSetGenAttn:
#if	USE_GEN
				rdReg	HOST_RX				; wparam
                RdReg32	HOST_RX				; wparam lparam
				FreezeTOS
                wrReg2	SET_GEN_ATTN		; wparam
                drop.r
#else
                ret
#endif


; ============================================================================
; waterfall
; ============================================================================

CmdWFReset:	
				rdReg	HOST_RX				; wparam
				wrReg2	SET_WF_CHAN			;
				rdReg	HOST_RX
				FreezeTOS
				wrReg2	WF_SAMPLER_RST
            	ret

CmdGetWFSamples:
				rdReg	HOST_RX				; wparam
				wrReg2	SET_WF_CHAN			;
getWFSamples2:
				wrEvt	HOST_RST
				push	NWF_SAMPS_LOOP
wf_more:
				REPEAT	NWF_SAMPS_RPT
				 wrEvt2	GET_WF_SAMP_I
				 wrEvt2	GET_WF_SAMP_Q
				ENDR
				
				push	1
				sub
				dup
				brNZ	wf_more

				REPEAT	NWF_SAMPS_REM
				 wrEvt2	GET_WF_SAMP_I
				 wrEvt2	GET_WF_SAMP_Q
				ENDR
				drop.r

CmdGetWFContSamps:
				rdReg	HOST_RX				; wparam
				wrReg2	SET_WF_CHAN			;
				push	WF_SAMP_SYNC | WF_SAMP_CONTIN
				FreezeTOS
				wrReg2	WF_SAMPLER_RST
				br		getWFSamples2

CmdSetWFFreq:	rdReg	HOST_RX				; wparam
				wrReg2	SET_WF_CHAN			;
                RdReg32	HOST_RX				; lparam
				FreezeTOS
                wrReg2	SET_WF_FREQ			;
				ret

CmdSetWFDecim:	
				rdReg	HOST_RX				; wparam
				wrReg2	SET_WF_CHAN			;
                RdReg32	HOST_RX				; lparam
				FreezeTOS
				wrReg2	SET_WF_DECIM		;
				ret


; ============================================================================
; GPS
; ============================================================================

#if GPS_CHANS
				// UploadClock requires ch_NAV_MS and ch_NAV_BITS to be sequential
				
				STRUCT	GPS_CHAN
				 //u16	ch_MAGIC		1
				 u16	ch_NAV_MS		1						; Milliseconds 0 ... 19
				 u16	ch_NAV_BITS		1						; Bit count
				 u16	ch_NAV_GLITCH	1						; Glitch count
				 u16	ch_NAV_PREV		1						; Last data bit = ip[15]
				 u16	ch_NAV_BUF		MAX_NAV_BITS / 16		; NAV data buffer
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

GetGPSchanPtr:									; chan#
				push	sizeof GPS_CHAN			; chan# sizeof
				mult							; offset
				push	GPS_channels			; baseaddr
				add.r							; baseaddr + offset

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

// get 16-bit I/Q data
GetCount:		push	0						; 0								20
				rdBit							; [15]
GetCount2:
				REPEAT 15
				 rdBit							; 
				ENDR
				ret								; [15:0]

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
                rdBit						; Inav			keep msb of I as nav data
                dup							; Inav bit
                call	GetCount2			; Inav ip
                call	GetCount   		    ; Inav ip qp

				// save last I/Q values
				over						; Inav ip qp ip
				over						; Inav ip qp ip qp
				swap						; Inav ip ip qp ip
                r							; Inav ip qp qp ip this
                addi	ch_IQ 				; Inav ip qp qp ip &i
                store16						; Inav ip qp qp &i
                addi	2					; Inav ip qp qp &q
                store16						; Inav ip qp &q
                drop						; Inav ip qp

				// close the LO loop
				over						; Inav ip qp ip
				over						; Inav ip qp ip qp
                mult						; Inav ip qp ip*qp
                CloseLoop ch_LO_FREQ ch_LO_GAIN SET_LO_NCO

				// close the CA loop
                dup							; Inav ip qp qp
                mult						; Inav ip qp^2
                swap						; Inav qp^2 ip
                dup							; Inav qp^2 ip ip
                mult						; Inav qp^2 ip^2
                add							; Inav pp
                call	GetPower			; Inav pp pe
                over						; Inav pp pe pp
                over						; Inav pp pe pp pe
                sub							; Inav pp pe pp-pe
                sgn32_16					; Inav pp pe (pp-pe)<0[15]
                rot							; Inav pe (pp-pe)<0 pp
                call	GetPower			; Inav pe (pp-pe)<0 pp pl
                swap						; Inav pe (pp-pe)<0 pl pp
                over						; Inav pe (pp-pe)<0 pl pp pl
                sub							; Inav pe (pp-pe)<0 pl pp-pl
                sgn32_16					; Inav pe (pp-pe)<0 pl (pp-pl)<0[15]
                rot							; Inav pe pl (pp-pl)<0 (pp-pe)<0
				or							; Inav pe pl unlocked:15
                r							; Inav pe pl unlocked this
                addi	ch_unlocked			; Inav pe pl unlocked &ch_unlocked
                store16						; Inav pe pl &ch_unlocked
                pop							; Inav pe pl
                sub							; Inav pe-pl
                CloseLoop ch_CA_FREQ ch_CA_GAIN SET_CA_NCO


				// process NAV data (20 msec per bit @ 50 bps)
				// remember: loop is running at 1 kHz (1 msec), so 20 samples per bit
				
				//	if (Inav == ch_NAV_PREV) {
				//		// NavSame
				//		if (ch_NAV_MS != 19) ch_NAV_MS++, return
				//		
				//		stayed same for all 20 samples
				//		ch_NAV_MS = 0
				//		ch_NAV_BITS = (ch_NAV_BITS + 1) & (MAX_NAV_BITS - 1)
				//		ch_NAV_BUF[] <<= |= Inav
				//		return
				//	}
				//	
				//	ch_NAV_PREV = Inav
				//	
				//	if (ch_NAV_MS != 0) ch_NAV_GLITCH++		// changed in the middle of sampling
				//	
				//	// NavEdge
				//	ch_NAV_MS = 1
				//	return


				// if Inav == ch_NAV_PREV goto NavSame
                r							; Inav this
                addi	ch_NAV_PREV			; Inav &prev
                fetch16						; Inav prev
                over						; Inav prev Inav
                sub							; Inav diff
                brZ		NavSame				; Inav

                r							; Inav this
                addi	ch_NAV_PREV			; Inav &prev
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
				r							; Inav this
                addi	ch_NAV_MS			; Inav &ms
                dup							; Inav &ms &ms
                fetch16						; Inav &ms ms
                push	19
                sub							; Inav &ms ms-19
                brZ		NavSave				; Inav &ms

				// else ch_NAV_MS++
                fetch16						; Inav ms
                addi	1					; Inav ms+1
                r_from						; Inav ms+1 this
                addi	ch_NAV_MS			; Inav ms+1 &ms
                store16
                drop						; Inav
                drop.r						;

NavSave:        // ch_NAV_MS = 0
				push	0
				swap						; Inav 0 &ms
                store16
                drop						; Inav
                
				// ch_NAV_BITS = (ch_NAV_BITS + 1) & (MAX_NAV_BITS - 1)
                r							; Inav this
                addi	ch_NAV_BITS			; Inav &cnt
                fetch16						; Inav cnt
                dup							; Inav cnt cnt
                addi	1					; Inav cnt cnt+1
                push	MAX_NAV_BITS - 1
                and							; Inav cnt wrapped
                r							; Inav cnt wrapped this
                addi	ch_NAV_BITS			; Inav cnt wrapped &cnt
                store16
                drop						; Inav cnt

				// ch_NAV_BUF[] <<= |= Inav
				REPEAT	4
				 shr						; Inav cnt/16
				ENDR
                shl							; Inav offset
                r_from						; Inav offset this
                addi	ch_NAV_BUF			; Inav offset buf
                add							; Inav ptr
                dup							; Inav ptr ptr
                to_r						; Inav ptr
                fetch16						; Inav old
                shl							; Inav old<<1
                add							; new
                r_from						; new ptr
                store16
                drop.r						;
			
; ============================================================================

UploadChan:										; &GPS_channels[n]
				REPEAT	sizeof GPS_CHAN / 2
				 wrEvt	GET_MEMORY
				ENDR
				ret

// "wrEvt GET_MEMORY" side-effect: auto mem ptr incr, i.e. 2x tos += 2 (explains "-4" below)
UploadClock:									; &GPS_channels
				wrEvt	GET_MEMORY				; GPS_channels++ -> ch_NAV_MS
				wrEvt	GET_MEMORY				; GPS_channels++ -> ch_NAV_BITS
				rdBit16							; clock replica
				wrReg	HOST_TX
				addi.r	sizeof GPS_CHAN - 4		; &GPS_channels++

UploadGlitches:									; &GPS_channels + ch_NAV_GLITCH
				wrEvt	GET_MEMORY
				addi.r	sizeof GPS_CHAN - 2		; &(GPS_channels+1) + ch_NAV_GLITCH

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
				wrEvt	HOST_RST
				push	GPS_SAMPS
up_more:
				wrEvt	GET_GPS_SAMPLES
				push	1
				sub
				dup
				brNZ	up_more
				drop.r

CmdGetChan:     rdReg	HOST_RX				; chan#
                wrEvt	HOST_RST			; chan#
                call	GetGPSchanPtr		; this
                call	UploadChan			; this++
                drop.r

CmdGetClocks:   wrEvt	HOST_RST
                rdReg	GET_SNAPSHOT		; 0
                rdBit16z					; 48-bit system clock
                wrReg	HOST_TX
                rdBit16						; "
                wrReg	HOST_TX
                rdBit16						; "
                wrReg	HOST_TX
                
                push	0
                REPEAT	GPS_CHANS
                 rdBit						; chan srq
                ENDR
                wrReg	HOST_TX
                
                push	GPS_channels + ch_NAV_MS
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
                push	0xbabe
                wrReg	HOST_TX
                ret
