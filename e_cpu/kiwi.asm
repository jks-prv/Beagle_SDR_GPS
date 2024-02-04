
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

; Copyright (c) 2014-2016 John Seamons, ZL4VO/KF6VO

#include ../kiwi.config

				MACRO	loop_ct count
				 push   count - 1
                 to_loop
				ENDM

				MACRO	loop2_ct count
				 push   count - 1
                 to_loop2
				ENDM

				MACRO	FreezeTOS
				 wrEvt2	FREEZE_TOS
				 nop								; enough time for latch and sync
				 nop
				ENDM

				MACRO	B2B_FreezeTOS
				 nop								; enough time between two consecutive FreezeTOS
				 nop
				 nop
				 nop
				ENDM

				MACRO	StackCheck	which expected
#if STACK_CHECK
				 push	which
				 push	expected
				 call	stack_check
#endif
				ENDM

				MACRO	push32 hi lo
				 push	lo							; 0,l
				 push	hi							; 0,l 0,h
				 swap16								; 0,l h,0
				 add								; h,l
				ENDM

                MACRO   dup64                       ; h l
                 over                               ; h l h
                 over                               ; h l h l
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
				wrReg	SET_GPS_MASK
				rdReg	GET_SRQ
				pop

Ready:
			wrEvt2	CPU_CTR_DIS
				wrEvt	HOST_RDY
				push	CTRL_CMD_READY
				call	ctrl_set			        ; signal cmd finished

				StackCheck	sp_ready 0

NoCmd:
				// poll for srqs
				rdReg	GET_SRQ						; 0
				rdBit0								; host_srq

#if USE_GPS
                loop_gps_chans
gps_srq_loop:   push	0
				rdBit0
				loop    gps_srq_loop
				                                    ; host_srq gps_srq(gps_chans-1) ... (0)
#endif

#if USE_SDR
				rdReg	GET_RX_SRQ					; host_srq gps_srq(gps_chans-1) ... (0) 0
				rdBit1								; host_srq gps_srq(gps_chans-1) ... (0) rx_srq
				
				brZ		no_rx_svc
			wrEvt2	CPU_CTR_ENA
				call	RX_Buffer
			wrEvt2	CPU_CTR_DIS

#if STACK_CHECK
                // CAUTION: untested replacement code below for GPS_CHANS => gps_chans transition
				//StackCheck	sp_rx (GPS_CHANS + 1)
				
				push	sp_rx                       ; which
                call    push_gps_chans_m1           ; which #chans_m1
				addi    2                           ; which expected(gps_chans_m1+2 = gps_chans+1)
				call	stack_check
#endif

no_rx_svc:											; host_srq gps_srq(gps_chans-1) ... (0)

#endif

#if USE_GPS
                call    loop2_gps_chans             ; host_srq gps_srq(gps_chans-1) ... (0)
gps_svc_loop:                                       ; gps_srq
                brZ     no_gps_svc                  ; 
			wrEvt2	CPU_CTR_ENA
                call    push_gps_chans_m1           ; #chans_m1
                loop2_from                          ; #chans_m1 loop_ch#(11 10 ... 0)
                sub                                 ; #ch(11-11=0 11-10=1 ... 11-0=11)
                dup                                 ; ch# ch#
                call    GetGPSchanPtr               ; ch# this
                swap                                ; this ch#
				call	GPS_Method					;
			wrEvt2	CPU_CTR_DIS
no_gps_svc:     loop2   gps_svc_loop                ; NB: loop2 because GPS_Method() uses shl64_n()::to_loop

#endif
				
				brZ		NoCmd                       ; no host_srq pending

			wrEvt2	CPU_CTR_ENA
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
				push	CTRL_CMD_READY
				call	ctrl_clr			        ; signal cmd busy

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

// CmdPing and CmdSetMem must be in first 1K of CPU RAM (i.e. loaded by boot)
CmdPing:
				wrEvt	HOST_RST
                push	0xcafe
                wrReg	HOST_TX

#if SPI_PUMP_CHECK
				push	0
				push	0
				push	0
				push	0                           ; 0 0 0 0
				// sp should be +4 from entry at this point
				sp_rp                               ; 0 0 rp sp
                wrReg	HOST_TX                     ; 0 0 rp
                pop                                 ; 0 0
                pop                                 ; 0
                pop                                 ;
                
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

CmdSetMem:		rdReg	HOST_RX						; data
				rdReg	HOST_RX						; data addr
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
				rdReg	HOST_RX				; addr
				dup							; addr addr
				wrEvt	HOST_RST
				fetch16						; addr data
                wrReg	HOST_TX
                wrReg	HOST_TX
                ret
#else
				ret
#endif

CmdGetSPRP:
#if USE_DEBUG
				wrEvt	HOST_RST
				push    0                   ; 0
				push    0                   ; 0 0
				sp_rp                       ; rp sp
                wrReg	HOST_TX             ; rp
                wrReg	HOST_TX             ;
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
				dup                         ; ct ct
                wrReg	HOST_TX             ; ct
				push	tr_id               ; ct @tr_id
				fetch16                     ; ct tr_id
                wrReg	HOST_TX             ; ct
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
				push	1                   ; ct 1
				sub                         ; ct-1
				dup                         ; nct ct-1
				brNZ	tr_more             ; nct
#if SPI_32
				push	tr_id               ; nct @tr_id
				fetch16                     ; nct tr_id
                addi	1                   ; nct tr_id+1
				push	tr_id               ; nct tr_id+1 @tr_id
				store16                     ; nct @tr_id
				pop                         ; nct
				drop.r                      ; nct

tr_id:			u16		0
#else
				drop.r
#endif

#if STACK_CHECK
stack_check:                                ; addr #sp
				addi	3                   ; addr #sp+4			StackCheck macro did 2 pushes, we're just about to do 2 more
				push	0                   ; addr #sp+4 0
				push	0                   ; addr #sp+4 0 0
				sp_rp                       ; addr #sp+4 rp sp
				swap                        ; addr #sp+4 sp rp
				pop                         ; addr #sp+4 sp
				dup                         ; addr #sp+4 sp sp
				rot                         ; addr sp sp #sp+4
				sub                         ; addr sp (#sp+4 == sp)?
				brNZ	stack_bad           ; addr sp
				pop                         ; addr
				pop.r                       ;
stack_bad:                                  ; addr sp
				push	4                   ; correct by 4 from above
				sub
				swap                        ; sp-4 addr				the last bad sp value
				store16                     ; addr
				addi	2                   ; addr++				how many times it has happened
				incr16                      ; *addr++
				pop.r

CmdUploadStackCheck:
				// upload stack check info				
				wrEvt	HOST_RST
                push	0xbeef
                wrReg	HOST_TX

                push	sp_counters
sp_more:
				dup                         ; a a
                fetch16
                wrReg	HOST_TX             ; a
                addi	2
                dup                         ; a+2 a+2
                push	sp_counters_end
                sub
                brNZ	sp_more             ; a+2
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
sp_ready:		u16		0			        ; the last bad sp value
				u16		0			        ; how many times it has happened
sp_rx:			u16		0
				u16		0
				REPEAT	GPS_MAX_CHANS
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
                // general
				u16		CmdPing
				u16		CmdSetMem
				u16		CmdPing2
				u16		CmdGetCPUCtr
				u16		CmdCtrlClrSet
				u16     CmdCtrlPulse
				u16		CmdCtrlGet
				u16		CmdGetMem
				u16		CmdGetStatus
				u16		CmdFlush
				u16		CmdTestRead
				u16		CmdUploadStackCheck
				u16     CmdGetSPRP

                // SDR
#if USE_SDR
				u16		CmdSetRXFreq
				u16		CmdSetRXNsamps
				u16		CmdSetGenFreq
				u16		CmdSetGenAttn
				u16		CmdGetRX
				u16		CmdClrRXOvfl
				u16		CmdSetWFFreq
				u16		CmdSetWFDecim
				u16		CmdWFReset
				u16		CmdGetWFSamples
				u16		CmdGetWFContSamps
				u16     CmdSetOVMask
				u16     CmdGetADCCtr
				u16     CmdSetADCLvl
#endif

                // GPS
#if USE_GPS
				u16		CmdSample
				u16		CmdSetChans
				u16		CmdSetMask
				u16		CmdSetRateCG
				u16		CmdSetRateLO
				u16		CmdSetGainCG
				u16		CmdSetGainLO
				u16		CmdSetSat
				u16     CmdSetE1Bcode
				u16     CmdSetPolarity
				u16		CmdPause
				u16		CmdGetGPSSamples
				u16		CmdGetChan
				u16		CmdGetClocks
				u16		CmdGetGlitches
				u16		CmdIQLogReset
				u16		CmdIQLogGet
#endif


#include_other other.cmds.asm


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
// in mem: a: [31:0] a+4: [63:32] or a: [15:0] a+2: [31:16] a+4: [47:32] a+6: [63:48]
// on stk: nos: [63:32] tos: [31:0]
store64:									; [63:32] [31:0] a             17
                store32						; [63:32] a
                addi	4					; [63:32] a+4	NB: means store64 returns a+4, not a!
                // fall through ...

// NB
// in mem: a: [15:0] a+2: [31:16]
store32:									; [31:0] a                      8
                over						; [31:0] a [31:0]
                swap16						; x|[15:0] a x|[31:16]
                over						; x|[15:0] a x|[31:16] a
                addi	2					; x|[15:0] a x|[31:16] a+2
                store16						; x|[15:0] a a+2
                drop						; x|[15:0] a
                store16.r					; a

swap64:										; ah al bh bl                   6
                rot							; ah bh bl al
                to_r						; ah bh bl          ; al
                rot							; bh bl ah          ; al
                r_from						; bh bl ah al
                ret

over64:                                     ; ah al bh bl
                to_r                        ; ah al bh                  ; bl
                to_r                        ; ah al                     ; bl bh
                dup64                       ; ah al ah al
                r_from                      ; ah al ah al bh            ; bl
                r_from                      ; ah al ah al bh bl         ;
                swap64                      ; ah al bh bl ah al
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
dec:
                push    1
                sub.r

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

shl64_n:									; i64H i64L n
                push    1                   ; i64H i64L n 1
                sub                         ; i64H i64L n-1
                to_loop                     ; i64H i64L
shl64_loop:     shl64
                loop    shl64_loop
                ret					    	; i64H<<n i64L<<n

neg32:			push	0					; i32 0
				swap						; 0 i32
				sub.r						; -i32

neg64:			push	0					; i64H L 0
                push	0					; i64H L 0 0
                swap64                      ; 0 0 i64H L
				sub64						; -i64H L
				ret

conv32_64:                                  ; i32                           9
				push	0					; i32 0
				over						; i32 0 i32
				shl64						; i32 [31] i32<<1
				pop							; i32 [31]
				neg32						; i32 -[31]
				swap.r						; i64H i64L

                // NB: assumes [31:18] = 0, FIXME?
sext16_32:                                  ; [15:0]
                dup                         ; [15:0] [15:0]
                push    0x8000              ; [15:0] [15:0] 0x00008000
                and                         ; [15:0] [15]
                brZ     sext16_32_1         ; [15:0]
                push    0xffff              ; [15:0] 0x0000ffff
                swap16                      ; [15:0] 0xffff0000
                or                          ; 0xffff|[15:0]                 ; [15] = 1
sext16_32_1:    ret                         ; 0x0000|[15:0]                 ; [15] = 0

                // NB: assumes [31:18] = 0, FIXME?
sext18_32:                                  ; [17:0]
                dup                         ; [17:0] [17:0]
                swap16                      ; [17:0] [15:0]|x[17:16]
                shr                         ; [17:0] [15:0]>>1|x[17]
                push    1                   ; [17:0] [15:0]>>1|x[17] 1      ; zero [15:0]>>1|x part
                and                         ; [17:0] [17]
                neg32                       ; [17:0] -[17]                  ; 0 => 0, 1 => -1
                push32  0xfffe 0x0000
                and                         ; [17:0] {15{-[17]},17'b0}
                or.r                        ; {14{[17]},[17:0]}

                // NB: assumes [31:20] = 0, FIXME?
sext20_32:                                  ; [19:0]
                dup                         ; [19:0] [19:0]
                swap16                      ; [19:0] [15:0]|12'x,[19:16]
                shr
                shr
                shr                         ; [19:0] [15:0]>>3|15'x,[19]
                push    1                   ; [19:0] [15:0]>>3|15'x,[19] 1  ; zero [15:0]>>3|15'x part
                and                         ; [19:0] [19]
                neg32                       ; [19:0] -[19]                  ; 0 => 0, 1 => -1
                push32  0xfff8 0x0000
                and                         ; [19:0] {13{-[19]},19'b0}
                or.r                        ; {12{[19]},[19:0]}

                // NB: assumes [63:36] = 0, FIXME?
sext36_64:                                  ; [35:32] [31:0]
                swap                        ; [31:0] [35:32]
                dup                         ; [31:0] [35:32] [35:32]
                shr
                shr
                shr                         ; [31:0] [35:32] [35]
                push    1                   ; 
                and
                neg32                       ; [31:0] [35:32] -[35]          ; 0 => 0, 1 => -1
                push32  0xffff 0xfff8
                and                         ; [31:0] [35:32] {29{-[35]},3'b0}
                or                          ; [31:0] {28{[35]},[35:32]}
                swap.r                      ; {28{[35]},[35:32]} [31:0]

// returns [31] in [15] i.e. if tos is a negative signed value
sgn32_16:		swap16						; i32>>16	because brN/NZ only looks at [15:0]
                push	0x8000				; h 0x8000
                and.r						; i32<0? [15]

// returns [63] in [15] i.e. if tos is a negative signed value
sgn64_16:		                            ; H L
                pop					    	; H
                sgn32_16                    ; i32<0? [15]
                ret

abs32:
				dup							; i32 i32
				sgn32_16					; i32 i32<0?
				brZ		abs32_1				; i32
				neg32						; -i32
abs32_1:		ret

abs64:
				dup64                       ; i64H L i64H L
				sgn64_16					; i64H L i64<0?
				brZ		abs64_1				; i64H L
				neg64						; -i64H L
abs64_1:		ret

// rdBit0 a 16-bit word
rdBit0_16:
				push	0
rdBit0_16z:
                loop_ct 16
rdBit0_loop:    rdBit0
                loop    rdBit0_loop
				ret

// not used currently
#if 0
// rdBit1 a 16-bit word
rdBit1_16:
				push	0
rdBit1_16z:
                loop_ct 16
rdBit1_loop:    rdBit1
                loop    rdBit1_loop
				ret

// rdBit2 a 16-bit word
rdBit2_16:
				push	0
rdBit2_16z:
                loop_ct 16
rdBit2_loop:    rdBit2
                loop    rdBit2_loop
				ret
#endif

// increment a u16 memory location and keep value on stack
incr16:										; addr
				dup							; addr addr
				fetch16						; addr data
				addi	1					; addr data+1
				dup							; addr data+1 data+1
				rot							; data+1 data+1 addr
				store16						; data+1 addr
				drop.r						; data+1

// decrement a u16 memory location and keep value on stack
decr16:										; addr
				dup							; addr addr
				fetch16						; addr data
				dec                         ; addr data-1
				dup							; addr data-1 data-1
				rot							; data-1 data-1 addr
				store16						; data-1 addr
				drop.r						; data-1


; ============================================================================
; support
; ============================================================================

ctrl_clr:
				not
				push	ctrl
				fetch16
				and
ctrl_update:
				dup
				wrReg   SET_CTRL
				push	ctrl
				store16
				drop.r

ctrl_set:
				push	ctrl
				fetch16
				or
				br		ctrl_update

CmdCtrlClrSet:
                rdReg	HOST_RX             ; clr
                rdReg	HOST_RX             ; clr set
                swap                        ; set clr
				not                         ; set ~clr
				push	ctrl                ; set ~clr &ctrl_old
				fetch16                     ; set ~clr ctrl_old
				and                         ; set ctrl_clr
				or                          ; ctrl_clr_set
				dup
				wrReg   SET_CTRL
				push	ctrl
				store16
				drop.r

CmdCtrlPulse:
                rdReg	HOST_RX             ; bits
                dup                         ; bits bits
				not                         ; bits ~bits
				push	ctrl                ; bits ~bits &ctrl_old
				fetch16                     ; bits ~bits ctrl_old

                // clear first (if not already)
				and                         ; bits ctrl_clr
				dup                         ; bits ctrl_clr ctrl_clr
				wrReg   SET_CTRL            ; bits ctrl_clr
				
				// save cleared value
				dup                         ; bits ctrl_clr ctrl_clr
				push	ctrl                ; bits ctrl_clr ctrl_clr &ctrl
				store16                     ; bits ctrl_clr &ctrl
				drop                        ; bits ctrl_clr

				dup                         ; bits ctrl_clr ctrl_clr
				rot                         ; ctrl_clr ctrl_clr bits
				or                          ; ctrl_clr ctrl_set

                // set & clear in one clock pulse
				wrReg   SET_CTRL            ; ctrl_clr
				wrReg   SET_CTRL            ;
				ret

CmdCtrlGet:		wrEvt	HOST_RST
				push	ctrl
				fetch16
				wrReg	HOST_TX
				ret
			
ctrl:			u16		0

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
				wrEvt2	CPU_CTR_CLR
				ret


; ============================================================================
; SDR
; ============================================================================

#if USE_SDR
#include kiwi.sdr.asm
#endif


; ============================================================================
; GPS
; ============================================================================

#if USE_GPS
#include kiwi.gps.asm
#endif


; ============================================================================
; OTHER
; ============================================================================

#include_other other.asm


; ============================================================================

// at end to check if firmware-assisted load of code worked
CmdPing2:		wrEvt	HOST_RST
                push	0xbabe
                wrReg	HOST_TX
                ret
