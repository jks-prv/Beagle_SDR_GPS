#include "mongoose.h"

#ifdef MONGOOSE_5_3
    #include "mongoose.5.3.ci"
#elif defined(MONGOOSE_5_6)
    #include "mongoose.5.6.ci"
#else
    #error mongoose version?
#endif
