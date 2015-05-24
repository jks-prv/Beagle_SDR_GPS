#include "wspr.h"

#if WSPR_NSAMPS == 45000
	TYPECPX wspr_samps[WSPR_NSAMPS] = {
		#include "apps/wspr/wav.h"
	};
#endif
