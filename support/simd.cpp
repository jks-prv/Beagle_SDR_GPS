// -*- C++ -*-

#include <complex>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#include "simd.h"

// c = conj(a) * b
void simd_multiply_conjugate_ccc(int len,
                                 const fftwf_complex* a,
                                 const fftwf_complex* b,
                                 fftwf_complex* c)
{
    const std::complex<float>* pa = reinterpret_cast<const std::complex<float>*>(a);
    const std::complex<float>* pb = reinterpret_cast<const std::complex<float>*>(b);
    std::complex<float>*       pc = reinterpret_cast<std::complex<float>*>(c);

    int counter=0;
#ifdef __ARM_NEON
    float32x4x2_t u, v, w;
    for (counter=0; counter<len/4; ++counter) {
        __builtin_prefetch(pa+256);
        __builtin_prefetch(pb+256);
        u = vld2q_f32((const float32_t*)pa); // [r1, i1]
        v = vld2q_f32((const float32_t*)pb); // [r2, i2]
        w.val[0] = vmulq_f32(u.val[0], v.val[0]);           // rw  = r1*r1
        w.val[1] = vmulq_f32(u.val[0], v.val[1]);           // iw  = r1*i2
        w.val[0] = vmlaq_f32(w.val[0], u.val[1], v.val[1]); // rw += i1*i2
        w.val[1] = vmlsq_f32(w.val[1], u.val[1], v.val[0]); // iw -= i1*r2
        vst2q_f32((float32_t*)pc, w);
        pa+=4, pb+=4, pc+=4;
    }
    counter *= 4;
#endif
    for (; counter<len; ++counter)
        *pc++  = std::conj(*pa++)*(*pb++);
}

// c = a * b
void simd_multiply_ccc(int len,
                       const fftwf_complex* a,
                       const fftwf_complex* b,
                       fftwf_complex* c)
{
    const std::complex<float>* pa = reinterpret_cast<const std::complex<float>*>(a);
    const std::complex<float>* pb = reinterpret_cast<const std::complex<float>*>(b);
    std::complex<float>*       pc = reinterpret_cast<std::complex<float>*>(c);

    int counter=0;
#ifdef __ARM_NEON
    float32x4x2_t u, v, w;
    for (counter=0; counter<len/4; ++counter) {
        __builtin_prefetch(pa+4);
        __builtin_prefetch(pb+4);
        u = vld2q_f32((const float32_t*)pa); // [r1, i1]
        v = vld2q_f32((const float32_t*)pb); // [r2, i2]
        w.val[0] = vmulq_f32(u.val[0], v.val[0]);           // rw  = r1*r1
        w.val[1] = vmulq_f32(u.val[0], v.val[1]);           // iw  = r1*i2
        w.val[0] = vmlsq_f32(w.val[0], u.val[1], v.val[1]); // rw -= i1*i2
        w.val[1] = vmlaq_f32(w.val[1], u.val[1], v.val[0]); // iw += i1*r2
        vst2q_f32((float32_t*)pc, w);
        pa+=4, pb+=4, pc+=4;
    }
    counter *= 4;
#endif
    for (; counter<len; ++counter)
        *pc++  = (*pa++)*(*pb++);
}

// c = a * b
void simd_multiply_cfc(int len,
                       const fftwf_complex* a,
                       const float *b,
                       fftwf_complex* c)
{
    const std::complex<float>* pa = reinterpret_cast<const std::complex<float>*>(a);
    const float*               pb = b;
    std::complex<float>*       pc = reinterpret_cast<std::complex<float>*>(c);

    int counter=0;
#ifdef __ARM_NEON
    float32x4x2_t u, w;
    float32x4_t v;
    for (counter=0; counter<len/4; ++counter) {
        __builtin_prefetch(pa+4);
        __builtin_prefetch(pb+4);
        __builtin_prefetch(pc+4);
        u = vld2q_f32((const float32_t*)pa); // [ru, iu]
        v = vld1q_f32(pb);                   // scale
        w.val[0] = vmulq_f32(u.val[0], v);   // rw  = ru*scale
        w.val[1] = vmulq_f32(u.val[1], v);   // iw  = iu*scale
        vst2q_f32((float32_t*)pc, w);
        pa+=4, pb+=4, pc+=4;
    }
    counter *= 4;
#endif
    for (; counter<len; ++counter)
        *pc++ = (*pa++)*(*pb++);
}

// f = (c>0 ? 1.0 : -1.0)
void simd_bit2float(int len, const int8_t* cv, float* fv)
{
    int counter=0;
#ifdef __ARM_NEON
    const int8x8_t tbl[4] = { { 0,-1,-1,-1, 2,-1,-1,-1 },
                              { 4,-1,-1,-1, 6,-1,-1,-1 },
                              { 1,-1,-1,-1, 3,-1,-1,-1 },
                              { 5,-1,-1,-1, 7,-1,-1,-1 }};
    const float32x4_t minus_one = { -1,-1,-1,-1 };
	const float32x4_t zero      = {  0, 0, 0, 0 };
	const float32x4_t plus_one  = {  1, 1, 1, 1 };

    for (; counter<len/8; ++counter) {
        __builtin_prefetch(cv+256,0,0);
        int8x8_t p = vld1_s8(cv);
        int8x8x2_t    t0 = { vtbl1_s8(p, tbl[0]),
							 vtbl1_s8(p, tbl[1]) };
        int8x8x2_t    t1 = { vtbl1_s8(p, tbl[2]),
							 vtbl1_s8(p, tbl[3]) };        
        float32x4x2_t ff = { vcvtq_f32_s32(vreinterpretq_s32_s8(vcombine_s8(t0.val[0], t0.val[1]))),
                             vcvtq_f32_s32(vreinterpretq_s32_s8(vcombine_s8(t1.val[0], t1.val[1]))) };
		uint32x4_t b0 = vcgtq_f32(ff.val[0], zero);
		ff.val[0] = vreinterpretq_f32_u32(vorrq_u32(vandq_u32(vreinterpretq_u32_f32(plus_one),  b0),
													vbicq_u32(vreinterpretq_u32_f32(minus_one), b0)));
		uint32x4_t b1 = vcgtq_f32(ff.val[1], zero);
		ff.val[1] = vreinterpretq_f32_u32(vorrq_u32(vandq_u32(vreinterpretq_u32_f32(plus_one),  b1),
													vbicq_u32(vreinterpretq_u32_f32(minus_one), b1)));		
        vst2q_f32(fv, ff);
        fv += 8;
        cv += 8;
    }
    counter *= 8;
#endif
    for (; counter<len; ++counter, ++cv) {
        *fv++ = float(2*(*cv>0) - 1);
    }
}
