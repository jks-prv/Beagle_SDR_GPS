#pragma once

// In case the C11 CMPLX macro is not present, try our best to provide a substitute

#if !defined CMPLX
	#if defined HAS_BUILTIN_COMPLEX
		#define CMPLX(re,im) __builtin_complex((double)(re), (double)(im))
	#elif defined __clang__
		#define CMPLX(re,im) (__extension__ (_Complex double){ (double)(re), (double)(im) })
	#elif defined _Imaginary_I
		#define CMPLX(re,im) (_Complex double)((double)(re) + _Imaginary_I * (double)(im))
	#else
		#define CMPLX(re,im) (_Complex double)((double)(re) + _Complex_I * (double)(im))
	#endif
#else
#endif

// same for CMPLXF

#if !defined CMPLXF
	#if defined HAS_BUILTIN_COMPLEX
		#define CMPLXF(re,im) __builtin_complex((float)(re), (float)(im))
	#elif defined __clang__
		#define CMPLXF(re,im) (__extension__ (_Complex float){ (float)(re), (float)(im) })
	#elif defined _Imaginary_I
		#define CMPLXF(re,im) (_Complex float)((float)(re) + _Imaginary_I * (float)(im))
	#else
		#define CMPLXF(re,im) (_Complex float)((float)(re) + _Complex_I * (float)(im))
	#endif
#endif
