#include "biquad.h"

CBiquad m_de_emp_Biquad[MAX_RX_CHANS];

CBiquad::CBiquad()
{
}

// REAL version
void CBiquad::ProcessFilter(int InLength, TYPEREAL *InBuf, TYPEREAL *OutBuf)
{
    TYPEREAL x, y;
    
	for (int i = 0; i < InLength; i++) {
	    x = InBuf[i];
	    OutBuf[i] = y = m_a0*x + m_a1*m_x1 + m_a2*m_x2 - m_b1*m_y1 - m_b2*m_y2;
        m_x2 = m_x1; m_x1 = x;
        m_y2 = m_y1; m_y1 = y;
	}
}

// MONO16 version
void CBiquad::ProcessFilter(int InLength, TYPEMONO16 *InBuf, TYPEMONO16 *OutBuf)
{
    TYPEREAL x, y;
    
	for (int i = 0; i < InLength; i++) {
	    x = InBuf[i];
	    OutBuf[i] = y = m_a0*x + m_a1*m_x1 + m_a2*m_x2 - m_b1*m_y1 - m_b2*m_y2;
        m_x2 = m_x1; m_x1 = x;
        m_y2 = m_y1; m_y1 = y;
	}
}

void CBiquad::InitFilterCoef(TYPEREAL a0, TYPEREAL a1, TYPEREAL a2, TYPEREAL b0, TYPEREAL b1, TYPEREAL b2)
{
    m_a0 = a0 / b0;     // normalize filter constants
    m_a1 = a1 / b0;
    m_a2 = a2 / b0;
    m_b1 = b1 / b0;
    m_b2 = b2 / b0;
    m_x1 = m_x2 = m_y1 = m_y2 = 0;
}
