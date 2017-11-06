
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
				
				wrEvt2	GET_RX_SAMP         ; move stored buffer counter
				wrEvt2  RX_GET_BUF_CTR      ; move current buffer counter
				ret

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
