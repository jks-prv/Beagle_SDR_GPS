
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


; ============================================================================
; GPS
; ============================================================================

				// UploadClock requires ch_NAV_MS and ch_NAV_BITS to be in the order shown
				
				STRUCT	GPS_CHAN
				 u16	ch_NAV_MS		1						; Milliseconds C/A: 0-19, E1B: 0
				 u16	ch_NAV_BITS		1						; Bit count
				 u16	ch_NAV_GLITCH	1						; Glitch count
				 u16	ch_NAV_PREV		1						; Last data bit = ip[15]
				 u16	ch_NAV_BUF		MAX_NAV_BITS / 16		; NAV data buffer
				 u64	ch_CG_FREQ		1						; Loop integrator
				 u64	ch_LO_FREQ		1						; Loop integrator
				 u32	ch_IQ			2 * 3                   ; Last IQP, IQE, IQL
				 u16	ch_CG_GAIN		2						; KI, KP (stored as KP-KI)
				 u16	ch_LO_GAIN		2						; KI, KP (stored as KP-KI)
				 u16	ch_unlocked		1
				 u16    ch_E1B_mode     1
				 u16    ch_LO_polarity  1
				ENDS

GPS_channels:	REPEAT	GPS_MAX_CHANS
				 GPS_CHAN	
				ENDR

GetGPSchanPtr:									; chan#
				push	sizeof GPS_CHAN			; chan# sizeof
				mult							; offset
				push	GPS_channels			; offset baseaddr
				add.r							; offset + baseaddr

; ============================================================================
;   e64 = ext64(LO:ip*qp or CG:pe-pl)
;   ki = ch_CG/LO_gain[0]
;   eki64 = e64 << ki
;   newF64 = *curF64 + eki64
;   *curF64 = newF64
;   kp_m_ki = ch_CG/LO_gain[1]      // stored as kp-ki to simplify code
;   ekp64 = eki64 << kp_m_ki        // i.e. e64 << kp, assumes kp >= ki
;   nco64 = newF64 + ekp64
;   nco32 = nco64 >> 32             // use most significant 32-bits
;
;   effectively:
;   newF = curF + (err << ki)
;   curF = newF
;   nco = (newF + (err << kp)) >> 32

				MACRO	CloseLoop freq gain nco
										; errH L, i.e. pe-pl for C/A CG loop, ip*qp for LO loop
				 r						; errH L this                       1
				 addi	gain			; errH L &gain[0]                   1
				 fetch16				; errH L ki                         1
				 shl64_n				; err<<ki:H L                       ki+8
				 dup64					; err<<ki:H L H L                   2
				 r						; err<<ki:H L H L this              1
				 addi	freq			; err<<ki:H L H L &freq             1
				 fetch64				; err<<ki:H L H L curFH L           19
				 add64					; err<<ki:H L newFH L               7
				 dup64					; err<<ki:H L newFH L H L           2
				 r						; err<<ki:H L newFH L H L this      1
				 addi	freq			; err<<ki:H L newFH L H L &freq     1
				 store64				; err<<ki:H L newFH L &freq         17
				 pop					; err<<ki:H L newFH L               1
				 swap64					; newFH L err<<ki:H L               6
				 r						; newFH L err<<ki:H L this          1
				 addi	gain + 2		; newFH L err<<ki:H L &gain[1]      1
				 fetch16				; newFH L err<<ki:H L kp-ki         1
				 shl64_n				; newFH L err<<kp:H L               kp-ki+8
				 add64					; nco64H L                          7
				 pop					; nco32                             1
				 wrReg	nco				;                                   1
				ENDM					;                                   TOTAL = kp + 98
			
; ============================================================================

#if GPS_INTEG_BITS == 16
                error   // GPS_INTEG_BITS = 16 no longer supported in this code; see prior versions
#endif

#if GPS_INTEG_BITS == 18
// get 18-bit I/Q data
GetCount:		push	0						; 0
				rdBit0							; [17]
GetCount2:
                rdBit0_16z
				rdBit0.r                        ; [17:0]

GetPower:		call	GetCount				; i[17:0]
				dup                             ; i[17:0] i[17:0]
				mult20							; i^2:H L
				call	GetCount				; i^2:H L q[17:0]
				dup                             ; i^2:H L q[17:0] q[17:0]
				mult20							; i^2:H L q^2:H L
				add64							; pH L
				ret                             ; pH L
#endif

#if GPS_INTEG_BITS == 20
// get 20-bit I/Q data
GetCount:		push	0						; 0
				rdBit0							; [19]
GetCount2:
                rdBit0_16z
				rdBit0							; 
				rdBit0							; 
				rdBit0.r                        ; [19:0]

GetPower:		call	GetCount				; i[19:0]
				dup                             ; i[19:0] i[19:0]
				mult20							; i^2:H L
				call	GetCount				; i^2:H L q[19:0]
				dup                             ; i^2:H L q[19:0] q[19:0]
				mult20							; i^2:H L q^2:H L
				add64							; pH L
				ret                             ; pH L
#endif

; ============================================================================

                MACRO   GPS_sext_32
#if GPS_INTEG_BITS == 18
                 sext18_32
#endif
#if GPS_INTEG_BITS == 20
                 sext20_32
#endif
                ENDM

GPS_Method:									; this ch#
#if USE_LOGGER
                swap                        ; ch# this
                over                        ; ch# this ch#
				wrReg	SET_GPS_CHAN        ; ch# this
                to_r						; ch#
                push    iq_ch               ; ch# &iq_ch
                fetch16                     ; ch# iq_ch
                sub                         ; (ch# == iq_ch)?
                brNZ    g_method_reg        ;               log only iq_ch channel

                rdReg	GET_CHAN_IQ			; 0
                rdBit0						; Inav			keep msb of I as nav data
                dup							; Inav ip[MSB]

                call	GetCount2			; Inav ip
                dup                         ; Inav ip ip
                GPS_sext_32                 ; Inav ip ip
                swap16
                wrEvt   PUT_LOG             ;               save ip[31:16]
                swap16
                wrEvt   PUT_LOG             ;               save ip[15:0]
                pop

                call	GetCount   		    ; Inav ip qp
                dup                         ; Inav ip qp qp
                GPS_sext_32                 ; Inav ip ip qp
                swap16
                wrEvt   PUT_LOG             ;               save qp[31:16]
                swap16
                wrEvt   PUT_LOG             ;               save qp[15:0]
                pop
                
                br      g_continue          ; Inav ip qp
#endif
				wrReg	SET_GPS_CHAN        ; this
                to_r						;

g_method_reg:
                rdReg	GET_CHAN_IQ			; 0
                rdBit0						; Inav			keep msb of I as nav data
                dup							; Inav bit
                call	GetCount2			; Inav ip
                call	GetCount   		    ; Inav ip qp

g_continue:
				// save last prompt I/Q values
				dup64						; Inav ip qp ip qp
				swap						; Inav ip ip qp ip
                GPS_sext_32                 ; Inav ip ip qp ip
                r							; Inav ip qp qp ip this
                addi	ch_IQ 				; Inav ip qp qp ip &i
                store32						; Inav ip qp qp &i
                addi	4					; Inav ip qp qp &q
                swap                        ; Inav ip qp &q qp
                GPS_sext_32                 ; Inav ip ip &q qp
                swap                        ; Inav ip qp qp &q
                store32						; Inav ip qp &q
                drop						; Inav ip qp


                // for C/A and E1B
				// close the LO loop based on error term ip*qp
				
				dup64						; Inav ip qp ip qp
                mult20						; Inav ip qp ip*qp:H L
                CloseLoop ch_LO_FREQ ch_LO_GAIN SET_LO_NCO

                // compute prompt power (pp)
                dup							; Inav ip qp qp
                mult20						; Inav ip qp^2:H L
                rot                         ; Inav qp^2:H L ip
                dup							; Inav qp^2:H L ip ip
                mult20						; Inav qp^2:H L ip^:2H L
                add64                       ; Inav ppH L


				// for C/A and E1B begin with computing early-minus-late-power (EMLP)
				// i.e. pe-pl, (ie*ie+qe*qe) - (il*il+ql*ql)

                // get power early (pe) and save
                call	GetPower			; Inav ppH L peH L
                dup64                       ; Inav ppH L peH L peH L
                r							; Inav ppH L peH L peH L this
                addi	ch_IQ + 8           ; Inav ppH L peH L peH L &pei
                store64						; Inav ppH L peH L &pei+4
                drop                        ; Inav ppH L peH L

                // check pe < pp for unlock indicator
                over64						; Inav ppH L peH L ppH L
                over64						; Inav ppH L peH L ppH L peH L
                sub64						; Inav ppH L peH L pp-pe:H L
                sgn64_16					; Inav ppH L peH L (pp-pe)<0[15]                ; r stack:
                r                           ; Inav ppH L peH L (pp-pe)<0[15] this           ; this
                swap                        ; Inav ppH L peH L this (pp-pe)<0[15]
                to_r                        ; Inav ppH L peH L this                         ; this (pp-pe)<0[15]
                to_r                        ; Inav ppH L peH L                              ; this (pp-pe)<0[15] this
                swap64						; Inav peH L ppH L

                // get power late (pl) and save
                call	GetPower			; Inav peH L ppH L plH L
                dup64                       ; Inav ppH L ppH L plH L plH L                  ; this (pp-pe)<0[15] this
                r_from                      ; Inav ppH L ppH L plH L plH L this             ; this (pp-pe)<0[15]
                addi	ch_IQ + 16          ; Inav ppH L ppH L plH L plH L &pli
                store64						; Inav ppH L ppH L plH L &pli+4
                drop                        ; Inav ppH L ppH L plH L

                // check pl < pp for unlock indicator
                swap64						; Inav peH L plH L ppH L
                over64						; Inav peH L plH L ppH L plH L
                sub64						; Inav peH L plH L pp-pl:H L
                sgn64_16					; Inav peH L plH L (pp-pl)<0[15]                ; this (pp-pe)<0[15]
                r_from                      ; Inav peH L plH L (pp-pl)<0[15] (pp-pe)<0[15]  ; this

                // set unlock indicator
				or							; Inav peH L plH L unlocked[15]
                r							; Inav peH L plH L unlocked this                ; this
                addi	ch_unlocked			; Inav peH L plH L unlocked &ch_unlocked
                store16						; Inav peH L plH L &ch_unlocked
                pop							; Inav peH L plH L

                // error term is pe-pl
                sub64						; Inav pe-pl:H L

                // C/A or E1B?
                r                           ; Inav pe-pl:H L this
                addi    ch_E1B_mode         ; Inav pe-pl:H L &ch_E1B_mode
                fetch16                     ; Inav pe-pl:H L e1b_mode
                brNZ    E1B_CG_loop         ; Inav pe-pl:H L

                // C/A BPSK
				// close the CG loop based on error term pe-pl
                br      close_CG_loop       ; Inav pe-pl:H L


				// E1B BOC(1,1)
				// close the CG loop based on ACF +/- AACF after determining locked LO polarity
E1B_CG_loop:                                ; Inav pe-pl:H L
                r                           ; Inav pe-pl:H L this
                addi    ch_LO_polarity      ; Inav pe-pl:H L &ch_LO_polarity
                fetch16                     ; Inav pe-pl:H L pol
                brZ     close_CG_loop       ; Inav pe-pl:H L                // pol == 0: begin by using EMLP
                dup64                       ; Inav ACF:H L ACF:H L          // ACF = pe-pl error term
                abs64                       ; Inav ACF:H L AACF:H L
                r                           ; Inav ACF:H L AACF:H L this
                addi    ch_LO_polarity      ; Inav ACF:H L AACF:H L &ch_LO_polarity
                fetch16                     ; Inav ACF:H L AACF:H L pol
                push    1                   ; Inav ACF:H L AACF:H L pol 1
                sub                         ; Inav ACF:H L AACF:H L (pol==1)?
                brNZ    E1B_AACF_sub        ; Inav ACF:H L AACF:H L
E1B_AACF_add:                               ;                               // pol == 1: ACF+AACF
                add64                       ; Inav (ACF+AACF)H L
                br      E1B_AACF_end
E1B_AACF_sub:                               ;                               // pol == 2: ACF-AACF
                sub64                       ; Inav (ACF-AACF)H L
E1B_AACF_end:
                // instead of dividing by 2 here the AGC is simply reduced by 2 when ACF mode is in effect
                // fall through ...

close_CG_loop:                              ; Inav errH L

                CloseLoop ch_CG_FREQ ch_CG_GAIN SET_CG_NCO
                
                r                           ; Inav this
                dup                         ; Inav this this
                addi    ch_E1B_mode         ; Inav this &ch_E1B_mode
                fetch16                     ; Inav this e1b_mode
                brNZ    NavSave             ; Inav this                     // E1B nav case
                // fall through ...                                         // L1 C/A nav case

                //
                // L1 C/A nav
                //
				// process NAV data (20 msec per bit @ 50 bps)
				// remember: loop is running at 1 kHz (1 msec), so 20 samples per bit
				//
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
				//  // NavNotSame
				//	ch_NAV_PREV = Inav
				//	
				//	if (ch_NAV_MS != 0) ch_NAV_GLITCH++		// changed in the middle of sampling
				//	
				//	// NavEdge
				//	ch_NAV_MS = 1
				//	return


				// if Inav == ch_NAV_PREV goto NavSame
                							; Inav this
                addi	ch_NAV_PREV			; Inav &prev
                fetch16						; Inav prev
                over						; Inav prev Inav
                sub							; Inav diff
                brZ		NavSame				; Inav

                // NavNotSame
                r							; Inav this
                addi	ch_NAV_PREV			; Inav &prev
                store16
                drop						;
                
				// if ch_NAV_MS == 0 then goto NavEdge
                r							; this(&ms)
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
                r_from						; 1 this(&ms)               NB: pops final this from r stack
                store16
                drop.r						;

NavSame:        // if ch_NAV_MS == 19 then goto NavSave
				r							; Inav this(&ms)
                dup							; Inav &ms &ms
                fetch16						; Inav &ms ms
                push	19
                sub							; Inav &ms ms-19
                brZ		NavSave				; Inav &ms

				// else ch_NAV_MS++
                fetch16						; Inav ms
                addi	1					; Inav ms+1
                r_from						; Inav ms+1 this(&ms)       NB: pops final this from r stack
                store16
                drop						; Inav
                drop.r						;

NavSave:                                    ; Inav &ms
                // ch_NAV_MS = 0
				push	0                   ; Inav &ms 0
				swap						; Inav 0 &ms
                store16                     ; Inav &ms
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
                shl							; Inav offset               offset = cnt/16
                r_from						; Inav offset this          NB: pops final this from r stack
                addi	ch_NAV_BUF			; Inav offset buf
                add							; Inav ptr
                dup							; Inav ptr ptr
                to_r						; Inav ptr
                fetch16						; Inav old
                shl							; Inav old<<1
                add							; new
                r_from						; new ptr
                store16                     ; ptr
                drop.r						;

                //
                // E1B nav
                //
				// process NAV data (4 msec per bit @ 250 bps)
				// remember: loop is running at 250 Hz (4 msec), so 1 sample per bit spanning 1 epoch.
                //
				//  ch_NAV_MS = 0
				//  ch_NAV_BITS = (ch_NAV_BITS + 1) & (MAX_NAV_BITS - 1)
				//  ch_NAV_BUF[] <<= |= Inav
				//  return
			
; ============================================================================

UploadChan:										; &GPS_channels[n]
				loop_ct (sizeof GPS_CHAN / 2)
upload_ch_loop: wrEvt	GET_MEMORY
				loop    upload_ch_loop
				ret

// "wrEvt GET_MEMORY" side-effect: auto mem ptr incr, i.e. 2x tos += 2 (explains "-4" below)
UploadClock:									; &GPS_channels + ch_NAV_MS
				wrEvt	GET_MEMORY				; GPS_channels++ -> ch_NAV_MS
				wrEvt	GET_MEMORY				; GPS_channels++ -> ch_NAV_BITS

#if GPS_REPL_BITS == 16
				rdBit0_16						; 16-bit clock replica
				wrReg	HOST_TX
				push    0                       ; to simplify code always return 2 words
				wrReg	HOST_TX
#endif

#if GPS_REPL_BITS == 18
				rdBit0_16						; 18-bit clock replica sent across 2 words
				wrReg	HOST_TX
				push    0
				rdBit0
				rdBit0
				wrReg	HOST_TX
#endif
				addi.r	sizeof GPS_CHAN - 4		; &GPS_channels++

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
				 swap							; freq32 chan freq64H L=0 &freq
				 store64						; freq32 chan &freq
				 drop							; freq32 chan
				 wrReg	SET_GPS_CHAN            ; freq32
				 wrReg	nco						;
				ENDM

; ============================================================================

CmdSample:		wrEvt	GPS_SAMPLER_RST
            	ret

gps_chans_m1:   u16		GPS_MAX_CHANS - 1   ; NB: -1 due to how tp_loop[2] insn works

CmdSetChans:    rdReg	HOST_RX             ; #chans
                push    gps_chans_m1        ; #chans &gps_chans_m1
                store16                     ; &gps_chans_m1
                pop.r                       ;

push_gps_chans_m1:
                push    gps_chans_m1        ; &gps_chans_m1
                fetch16.r                   ; #chans_m1

loop_gps_chans:
                call    push_gps_chans_m1   ; #chans_m1
                to_loop.r                   ;

loop2_gps_chans:
                call    push_gps_chans_m1   ; #chans_m1
                to_loop2.r                  ;

CmdSetMask:     SetReg SET_GPS_MASK
                ret

CmdSetRateCG:
                SetRate ch_CG_FREQ SET_CG_NCO
                ret

CmdSetRateLO:
                SetRate ch_LO_FREQ SET_LO_NCO
                ret

set_gain:                                   ; gain
                rdReg	HOST_RX				; gain chan
                call	GetGPSchanPtr		; gain this
                add             			; &gain
                RdReg32 HOST_RX				; &gain kp,ki
                swap						; kp,ki &gain
                store32						; &gain
                drop.r

CmdSetGainCG:
                push    ch_CG_GAIN
                set_gain
                ret

CmdSetGainLO:
                push    ch_LO_GAIN
                set_gain
                ret

CmdSetSat:      rdReg	HOST_RX             ; chan#
                dup                         ; chan# chan#
                wrReg   SET_GPS_CHAN        ; chan#
                call    GetGPSchanPtr       ; this
                rdReg	HOST_RX             ; this sat#
                dup                         ; this sat# sat#
                wrReg   SET_SAT             ; this sat#
                push    E1B_MODE            ; this sat# E1B_MODE
                and                         ; this e1b_mode
                swap                        ; eb1_mode this
                addi    ch_E1B_mode         ; e1b_mode &ch_E1B_mode
                store16                     ; &ch_E1B_mode
                pop.r                       ;

CmdSetE1Bcode:
                push    E1B_CODE_LOOP2
e1b_more:
				loop_ct	E1B_CODE_RPT
e1b_loop1:      SetReg	SET_E1B_CODE
				loop    e1b_loop1
				
				push	1
				sub
				dup
				brNZ	e1b_more

				loop_ct	E1B_CODE_REM
e1b_loop2:      SetReg	SET_E1B_CODE
				loop    e1b_loop2
				drop.r

CmdSetPolarity: rdReg	HOST_RX             ; chan#
                call    GetGPSchanPtr       ; this
                rdReg	HOST_RX             ; this pol
                swap                        ; pol this
                addi    ch_LO_polarity      ; pol &ch_LO_polarity
                store16                     ; &ch_LO_polarity
                pop.r                       ;

CmdPause:       SetReg	SET_GPS_CHAN
                SetReg	SET_PAUSE
                ret

CmdGetGPSSamples:
				wrEvt	HOST_RST
				push	GPS_SAMPS_LOOP
up_more:
				loop_ct	GPS_SAMPS_RPT
up_loop:        wrEvt	GET_GPS_SAMPLES
				loop    up_loop

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
                rdBit0_16z					; 48-bit system clock
                wrReg	HOST_TX
                rdBit0_16					; "
                wrReg	HOST_TX
                rdBit0_16					; "
                wrReg	HOST_TX
                
                push	0
                loop_gps_chans
gps_chan_loop:  rdBit0						; chan srq
                loop    gps_chan_loop
                wrReg	HOST_TX
                
                push	GPS_channels + ch_NAV_MS
                loop2_gps_chans
upload_loop:    call	UploadClock
                loop2   upload_loop         ; NB: loop2 because UploadClock() eventually uses loop insn
                drop.r

CmdGetGlitches: wrEvt	HOST_RST
                push	GPS_channels + ch_NAV_GLITCH
                loop_gps_chans
glitch_loop:                                ; &GPS_channels + ch_NAV_GLITCH
				wrEvt	GET_MEMORY
				addi	sizeof GPS_CHAN - 2 ; &(GPS_channels+1) + ch_NAV_GLITCH
                loop    glitch_loop
                drop.r

iq_ch:			u16		0

CmdIQLogReset:  rdReg	HOST_RX             ; ch#
                push    iq_ch               ; ch# &iq_ch
                store16                     ; &iq_ch
				wrEvt	LOG_RST             ; &iq_ch
                pop.r                       ;

CmdIQLogGet:    wrEvt	HOST_RST
				push	GPS_IQ_SAMPS
up_more_log:
				wrEvt	GET_LOG             ; IH
				wrEvt	GET_LOG             ; IL
				wrEvt	GET_LOG             ; QH
				wrEvt	GET_LOG             ; QL
				push	1
				sub
				dup
				brNZ	up_more_log
				drop.r
