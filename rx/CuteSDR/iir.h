//////////////////////////////////////////////////////////////////////
// iir.h: interface for the CIir class.
//
//  This class implements an IIR  filter
//
// History:
//	2011-02-05  Initial creation MSW
//	2011-03-27  Initial release
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
//=============================================================================
#ifndef IIR_H
#define IIR_H

#include "datatypes.h"
#include "kiwi.h"


class CIir
{
public:
	CIir();

	void InitFilterCoef(TYPEREAL A0, TYPEREAL A1, TYPEREAL A2, TYPEREAL B0, TYPEREAL B1, TYPEREAL B2);
	void InitLP(TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate);	//create Low Pass
	void InitHP(TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate);	//create High Pass
	void InitBP(TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate);	//create Band Pass
	void InitBR(TYPEREAL F0Freq, TYPEREAL FilterQ, TYPEREAL SampleRate);	//create Band Reject
	void ProcessFilter(int InLength, TYPEREAL* InBuf, TYPEREAL* OutBuf);
	void ProcessFilter(int InLength, TYPECPX* InBuf, TYPECPX* OutBuf);
	void ProcessFilter(int InLength, TYPEMONO16* InBuf, TYPEMONO16* OutBuf);

private:
	TYPEREAL m_A1;		//direct form 2 coefficients
	TYPEREAL m_A2;
	TYPEREAL m_B0;
	TYPEREAL m_B1;
	TYPEREAL m_B2;

	TYPEREAL m_w1a;		//biquad delay storage
	TYPEREAL m_w2a;
	TYPEREAL m_w1b;		//biquad delay storage
	TYPEREAL m_w2b;
};

#endif // IIR_H
