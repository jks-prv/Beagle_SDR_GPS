// -*- C++ -*-

#ifndef SUPPORT_SIMD_H
#define SUPPORT_SIMD_H

#include <stdint.h>
#include <fftw3.h>

// c = conj(a)*b
extern void simd_multiply_conjugate(int len,
                                    const fftwf_complex* a,
                                    const fftwf_complex* b,
                                    fftwf_complex* c);
// fv = float(2*(cv>0)-1)
extern void simd_bit2float(int len, const int8_t* cv, float* fv);

#endif // SUPPORT_SIMD_H

