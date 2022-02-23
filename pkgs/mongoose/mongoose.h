#pragma once

#ifdef MONGOOSE_5_6
    #include "mongoose.5.6.h"
#else
    #define MONGOOSE_5_3
    #include "mongoose.5.3.h"
#endif
