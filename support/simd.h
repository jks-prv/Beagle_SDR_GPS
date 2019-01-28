// -*- C++ -*-

#ifndef SUPPORT_SIMD_H
#define SUPPORT_SIMD_H

#include <stdint.h>
#include <fftw3.h>

// c = conj(a)*b
extern void simd_multiply_conjugate_ccc(int len,
                                        const fftwf_complex* a,
                                        const fftwf_complex* b,
                                        fftwf_complex* c);
// c = a*b
extern void simd_multiply_ccc(int len,
                              const fftwf_complex* a,
                              const fftwf_complex* b,
                              fftwf_complex* c);
// c = a*b
extern void simd_multiply_cfc(int len,
                              const fftwf_complex* a,
                              const float* b,
                              fftwf_complex* c);
// fv = float(2*(cv>0)-1)
extern void simd_bit2float(int len, const int8_t* cv, float* fv);

#endif // SUPPORT_SIMD_H
