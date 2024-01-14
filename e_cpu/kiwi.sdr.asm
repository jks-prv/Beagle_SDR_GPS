
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
				
				push	CTRL_SND_INTR
				call	ctrl_set			; signal the interrupt
				
#if SND_SEQ_CHECK
				// increment seq
                push	rx_seq
                incr16
				pop
#endif
not_init:		ret

CmdGetRX:
                rdReg	HOST_RX				; nrx_samps_rem
                rdReg	HOST_RX				; nrx_samps_rem nrx_samps_loop
				wrEvt	HOST_RST

				push	CTRL_SND_INTR
				call	ctrl_clr			; clear the interrupt as a side-effect

#if SND_SEQ_CHECK
				push	rx_seq				; &rx_seq
				fetch16						; rx_seq
				push	0x0ff0
				wrReg	HOST_TX
				wrReg	HOST_TX
#endif
				                            ; cnt = nrx_samps_loop
				
rx_loop:									; cnt
				loop_ct	NRX_SAMPS_RPT
rx_loop2:       wrEvt2	GET_RX_SAMP			; move i
				wrEvt2	GET_RX_SAMP			; move q
				wrEvt2	GET_RX_SAMP			; move iq3
				loop    rx_loop2

				push	1					; cnt 1
				sub							; cnt--
				dup                         ; cnt cnt
				brNZ	rx_loop             ; cnt
				pop                         ; cnt = nrx_samps_rem
				
				// NB: nrx_samps_rem can be zero on entry -- that is why test is at top of loop
rx_tail:                                    ; cnt
				dup                         ; cnt cnt
				brNZ	rx_tail2            ; cnt

				wrEvt2	GET_RX_SAMP			; move ticks[3]
				wrEvt2	GET_RX_SAMP
				wrEvt2	GET_RX_SAMP
				
				wrEvt2	GET_RX_SAMP         ; move stored buffer counter
				wrEvt2  RX_GET_BUF_CTR      ; move current buffer counter
				drop.r                      ;

rx_tail2:                                   ; cnt
				wrEvt2	GET_RX_SAMP			; move i
				wrEvt2	GET_RX_SAMP			; move q
				wrEvt2	GET_RX_SAMP			; move iq3
				push	1					; cnt 1
				sub							; cnt--
				br      rx_tail

CmdSetRXNsamps:	rdReg	HOST_RX				; nsamps
				dup
				push	nrx_samps
				store16
				pop							; nsamps
				
				FreezeTOS
                wrReg2	SET_RX_NSAMPS		;
                
                wrEvt2  RX_BUFFER_RST       ; reset read/write pointers, buffer counter
                ret

CmdSetRXFreq:	rdReg	HOST_RX				; rx#
				wrReg2	SET_RX_CHAN			;
                RdReg32	HOST_RX				; freqH
				FreezeTOS
                wrReg2	SET_RX_FREQ			;
                B2B_FreezeTOS               ; delay so back-to-back FreezeTOS works
                rdReg	HOST_RX				; freqL
				FreezeTOS
                wrReg2	SET_RX_FREQ | FREQ_L
                ret

CmdClrRXOvfl:
				wrEvt2	CLR_RX_OVFL
				ret

CmdSetGenFreq:
#if	USE_GEN
				rdReg	HOST_RX				; rx#   ignored: gen is applied in place of ADC data
                RdReg32	HOST_RX				; rx# genH
				FreezeTOS
                wrReg2	SET_GEN_FREQ        ; rx#
                B2B_FreezeTOS               ; delay so back-to-back FreezeTOS works
                rdReg	HOST_RX				; genL
				FreezeTOS
                wrReg2	SET_RX_FREQ | FREQ_L ; rx#
                drop.r                      ;
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

CmdSetOVMask:
				rdReg	HOST_RX				; wparam
                RdReg32	HOST_RX				; wparam lparam
				FreezeTOS
                wrReg   SET_CNT_MASK		; wparam
                drop.r

CmdGetADCCtr:
				wrEvt	HOST_RST
				rdReg2	GET_ADC_CTR0        ; adc_count[15:0]
				wrReg	HOST_TX
				rdReg2	GET_ADC_CTR1        ; adc_count[31:16]
				wrReg	HOST_TX
                ret

CmdSetADCLvl:
                rdReg	HOST_RX				; adc_level[13:0]
				FreezeTOS
				wrReg	SET_ADC_LVL
                ret


; ============================================================================
; waterfall
; ============================================================================

CmdWFReset:	
				rdReg	HOST_RX				; wf_chan
				wrReg2	SET_WF_CHAN			;
				rdReg	HOST_RX             ; WF_SAMP_*
				FreezeTOS
				wrReg2	WF_SAMPLER_RST
            	ret

CmdGetWFSamples:
				rdReg	HOST_RX				; wf_chan
				wrReg2	SET_WF_CHAN			;
getWFSamples2:
				wrEvt	HOST_RST
				push	NWF_SAMPS_LOOP      ; cnt
wf_more:                                    ; cnt
				loop_ct NWF_SAMPS_RPT       ; cnt
wf_loop1:
				wrEvt2	GET_WF_SAMP_I
				wrEvt2	GET_WF_SAMP_Q
			    loop    wf_loop1
				
				push	1                   ; cnt 1
				sub                         ; cnt
				dup                         ; cnt cnt
				brNZ	wf_more             ; cnt

				loop_ct NWF_SAMPS_REM       ; cnt
wf_loop2:
				wrEvt2	GET_WF_SAMP_I
				wrEvt2	GET_WF_SAMP_Q
			    loop    wf_loop2
				drop.r                      ;

CmdGetWFContSamps:
				rdReg	HOST_RX				; wf_chan
				wrReg2	SET_WF_CHAN			;
				push	WF_SAMP_SYNC | WF_SAMP_CONTIN
				FreezeTOS
				wrReg2	WF_SAMPLER_RST
				br		getWFSamples2

CmdSetWFFreq:	rdReg	HOST_RX				; wf_chan
				wrReg2	SET_WF_CHAN			;
                RdReg32	HOST_RX				; freqH
				FreezeTOS
                wrReg2	SET_WF_FREQ			;
                B2B_FreezeTOS               ; delay so back-to-back FreezeTOS works
                rdReg	HOST_RX				; freqL
				FreezeTOS
                wrReg2	SET_WF_FREQ | FREQ_L
				ret

CmdSetWFDecim:	
				rdReg	HOST_RX				; wf_chan
				wrReg2	SET_WF_CHAN			;
                RdReg32	HOST_RX				; lparam
				FreezeTOS
				wrReg2	SET_WF_DECIM		;
				ret
