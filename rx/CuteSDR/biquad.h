#pragma once

#include "datatypes.h"
#include "kiwi.h"

class CBiquad
{
public:
    CBiquad();

	void InitFilterCoef(TYPEREAL a0, TYPEREAL a1, TYPEREAL a2, TYPEREAL b0, TYPEREAL b1, TYPEREAL b2);
	void ProcessFilter(int InLength, TYPEREAL *InBuf, TYPEREAL *OutBuf);
	void ProcessFilter(int InLength, TYPEMONO16 *InBuf, TYPEMONO16 *OutBuf);

private:
	TYPEREAL m_a0, m_a1, m_a2, m_b0, m_b1, m_b2;
	TYPEREAL m_x1, m_x2, m_y1, m_y2;
};

extern CBiquad m_de_emp_Biquad[MAX_RX_CHANS];
