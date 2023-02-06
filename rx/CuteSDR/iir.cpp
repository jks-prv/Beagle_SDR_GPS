//////////////////////////////////////////////////////////////////////
// iir.cpp: implementation of the CIir class.
//
//  This class implements a biquad IIR filter.
//
//  Implements a second order IIR filter stage.
//  The transfer function of the filter stage implemented is in
//  the direct 2 form :
//
//                          -1       -2
//                 B0 + B1 z   + B2 z
//          H(z) = --------------------
//                         -1       -2
//                 1 + A1 z   + A2 z
//
//  The block diagram used in the implementation is given below:
//
//          input               w(n)   B0         output
//            -----> + ----------+-----[>--> + ----->
//                   |           |           |
//                   |        +--+--+        |
//                   |        |Delay|        |
//                   |        +--+--+        |
//                   |   -A1     |w(n-1) B1  |
//                   +----<]-----+-------[>--+
//                   |           |           |
//                   |        +--+--+        |
//                   |        |Delay|        |
//                   |        +--+--+        |
//                   |   -A2     |w(n-2) B2  |
//                   +----<]-----+-------[>--+
//		w(n) = in - A1*w(n-1) - A2*w(n-2)
//		out = B0*w(n) + B1*w(n-1) + B2*w(n-2)
//		w(n-2) = w(n-1)       w(n-1) = w(n)
//*=========================================================================================
//  The filter design equations came from a paper by Robert Bristow-Johnson
//		"Cookbook formulae for audio EQ biquad filter coefficients"
//
// History:
//	2011-02-05  Initial creation MSW
//	2011-03-27  Initial release
//	2013-07-28  Added single/double precision math macros
//////////////////////////////////////////////////////////////////////

//==========================================================================================
// + + +   This Software is released under the "Simplified BSD License"  + + +
//Copyright 2010 Moe Wheatley. All rights reserved.
//
//Redistribution and use in source and binary forms, with or without modification, are
//permitted provided that the following conditions are met:
//
//   1. Redistributions of source code must retain the above copyright notice, this list of
//	  conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright notice, this list
//	  of conditions and the following disclaimer in the documentation and/or other materials
//	  provided with the distribution.
//
//THIS SOFTWARE IS PROVIDED BY Moe Wheatley ``AS IS'' AND ANY EXPRESS OR IMPLIED
//WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
//FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Moe Wheatley OR
//CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
//ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
//ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//The views and conclusions contained in the software and documentation are those of the
//authors and should not be interpreted as representing official policies, either expressed
//or implied, of Moe Wheatley.
//==========================================================================================
#include "iir.h"

/////////////////////////////////////////////////////////////////////////////////
//	Construct CIir object
/////////////////////////////////////////////////////////////////////////////////
CIir::CIir()
{
	InitBR( 25000, 1000.0, 100000);
}


void CIir::InitFilterCoef(TYPEREAL A0, TYPEREAL A1, TYPEREAL A2, TYPEREAL B0, TYPEREAL B1, TYPEREAL B2)
{
	m_B0 = B0/A0;
	m_B1 = B1/A0;
	m_B2 = B2/A0;
	m_A1 = A1/A0;
	m_A2 = A2/A0;

	m_w1a = 0.0;
	m_w2a = 0.0;
	m_w1b = 0.0;
	m_w2b = 0.0;
}


/////////////////////////////////////////////////////////////////////////////////
//	Initialize IIR variables for Low Pass IIR filter.
// analog prototype == H(s) = 1 / (s^2 + s/Q + 1)
/////////////////////////////////////////////////////////////////////////////////
void CIir::InitLP( TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate)
{
	TYPEREAL w0 = K_2PI * F0Freq/SampleRate;	//normalized corner frequency
	TYPEREAL alpha = MSIN(w0)/(2.0*FilterQ);
	TYPEREAL A = 1.0/(1.0 + alpha);	//scale everything by 1/A0 for direct form 2
	m_B0 = A*( (1.0 - MCOS(w0))/2.0);
	m_B1 = A*( 1.0 - MCOS(w0));
	m_B2 = A*( (1.0 - MCOS(w0))/2.0);
	m_A1 = A*( -2.0*MCOS(w0));
	m_A2 = A*( 1.0 - alpha);

	m_w1a = 0.0;
	m_w2a = 0.0;
	m_w1b = 0.0;
	m_w2b = 0.0;
}


/////////////////////////////////////////////////////////////////////////////////
//	Initialize IIR variables for High Pass IIR filter.
// analog prototype == H(s) = s^2 / (s^2 + s/Q + 1)
/////////////////////////////////////////////////////////////////////////////////
void CIir::InitHP( TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate)
{
	TYPEREAL w0 = K_2PI * F0Freq/SampleRate;	//normalized corner frequency
	TYPEREAL alpha = MSIN(w0)/(2.0*FilterQ);
	TYPEREAL A = 1.0/(1.0 + alpha);	//scale everything by 1/A0 for direct form 2
	m_B0 = A*( (1.0 + MCOS(w0))/2.0);
	m_B1 = -A*( 1.0 + MCOS(w0));
	m_B2 = A*( (1.0 + MCOS(w0))/2.0);
	m_A1 = A*( -2.0*MCOS(w0));
	m_A2 = A*( 1.0 - alpha);

	m_w1a = 0.0;
	m_w2a = 0.0;
	m_w1b = 0.0;
	m_w2b = 0.0;
}

/////////////////////////////////////////////////////////////////////////////////
//	Initialize IIR variables for Band Pass IIR filter.
// analog prototype == H(s) = (s/Q) / (s^2 + s/Q + 1)
/////////////////////////////////////////////////////////////////////////////////
void CIir::InitBP( TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate)
{
	TYPEREAL w0 = K_2PI * F0Freq/SampleRate;	//normalized corner frequency
	TYPEREAL alpha = MSIN(w0)/(2.0*FilterQ);
	TYPEREAL A = 1.0/(1.0 + alpha);	//scale everything by 1/A0 for direct form 2
	m_B0 = A * alpha;
	m_B1 = 0.0;
	m_B2 = A * -alpha;
	m_A1 = A*( -2.0*MCOS(w0));
	m_A2 = A*( 1.0 - alpha);

	m_w1a = 0.0;
	m_w2a = 0.0;
	m_w1b = 0.0;
	m_w2b = 0.0;
}

/////////////////////////////////////////////////////////////////////////////////
//	Initialize IIR variables for Band Reject(Notch) IIR filter.
// analog prototype == H(s) = (s^2 + 1) / (s^2 + s/Q + 1)
/////////////////////////////////////////////////////////////////////////////////
void CIir::InitBR( TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate)
{
	TYPEREAL w0 = K_2PI * F0Freq/SampleRate;	//normalized corner frequency
	TYPEREAL alpha = MSIN(w0)/(2.0*FilterQ);
	TYPEREAL A = 1.0/(1.0 + alpha);	//scale everything by 1/A0 for direct form 2
	m_B0 = A*1.0;
	m_B1 = A*( -2.0*MCOS(w0));
	m_B2 = A*1.0;
	m_A1 = A*( -2.0*MCOS(w0));
	m_A2 = A*( 1.0 - alpha);

	m_w1a = 0.0;
	m_w2a = 0.0;
	m_w1b = 0.0;
	m_w2b = 0.0;
}

/////////////////////////////////////////////////////////////////////////////////
//	Process InLength InBuf[] samples and place in OutBuf[]
//REAL version
/////////////////////////////////////////////////////////////////////////////////
void CIir::ProcessFilter(int InLength, TYPEREAL* InBuf, TYPEREAL* OutBuf)
{
	for (int i=0; i<InLength; i++)
	{
		TYPEREAL w0 = InBuf[i] - m_A1*m_w1a - m_A2*m_w2a;
		OutBuf[i] = m_B0*w0 + m_B1*m_w1a + m_B2*m_w2a;
		m_w2a = m_w1a;
		m_w1a = w0;
	}
}

/////////////////////////////////////////////////////////////////////////////////
//	Process InLength InBuf[] samples and place in OutBuf[]
//Complex version
/////////////////////////////////////////////////////////////////////////////////
void CIir::ProcessFilter(int InLength, TYPECPX* InBuf, TYPECPX* OutBuf)
{
	for (int i=0; i<InLength; i++)
	{
		TYPEREAL w0a = InBuf[i].re - m_A1*m_w1a - m_A2*m_w2a;
		OutBuf[i].re = m_B0*w0a + m_B1*m_w1a + m_B2*m_w2a;
		m_w2a = m_w1a;
		m_w1a = w0a;

		TYPEREAL w0b = InBuf[i].im - m_A1*m_w1b - m_A2*m_w2b;
		OutBuf[i].im = m_B0*w0b + m_B1*m_w1b + m_B2*m_w2b;
		m_w2b = m_w1b;
		m_w1b = w0b;

	}
}

/////////////////////////////////////////////////////////////////////////////////
//	Process InLength InBuf[] samples and place in OutBuf[]
//Complex version
/////////////////////////////////////////////////////////////////////////////////
void CIir::ProcessFilter(int InLength, TYPEMONO16* InBuf, TYPEMONO16* OutBuf)
{
	for (int i=0; i<InLength; i++)
	{
		TYPEREAL w0 = InBuf[i] - m_A1*m_w1a - m_A2*m_w2a;
		OutBuf[i] = (TYPEMONO16) (m_B0*w0 + m_B1*m_w1a + m_B2*m_w2a);
		m_w2a = m_w1a;
		m_w1a = w0;
	}
}
