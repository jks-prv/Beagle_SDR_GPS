/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "sstv.h"


#if 0

// Return the FFT bin index matching the given frequency
u4_t GET_BIN(SSTV_REAL Freq, u4_t FFTLen)
{
    return (Freq / sstv.nom_rate * FFTLen);
}

// Sinusoid power from complex DFT coefficients
SSTV_REAL POWER(SSTV_FFTW_COMPLEX coeff)
{
    return SSTV_MPOW(coeff[0],2) + SSTV_MPOW(coeff[1],2);
}

#endif

// Clip to [0..255]
u1_t clip(SSTV_REAL a)
{
    if      (a < 0)   return 0;
    else if (a > 255) return 255;
    return  (u1_t)    SSTV_MROUND(a);
}

// Convert degrees -> radians
SSTV_REAL deg2rad(SSTV_REAL Deg)
{
    return (Deg / 180) * M_PI;
}

// Convert radians -> degrees
SSTV_REAL rad2deg(SSTV_REAL rad)
{
    return (180 / M_PI) * rad;
}
