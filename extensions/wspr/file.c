#include "wspr.h"

#ifdef EXT_WSPR

TYPECPX wspr_demo_samps[WSPR_DEMO_NSAMPS] = {
	#include "extensions/wspr/wav.h"
};

#endif
