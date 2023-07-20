#pragma once

#include "AudioResample.h"     // from DRM/dream/resample

#define HFDL_MIN_SRATE  (18000*2)

typedef struct {
    float *out_I, *out_Q;
} HFDL_resamp_t;

class CHFDLResample {

public:
    CHFDLResample();
    ~CHFDLResample();
    int setup(double f_srate, int n_samps);
    void run(float *sample, int length, HFDL_resamp_t *resamp);

protected:
    CAudioResample *Resample_I, *Resample_Q;
    CVector<_REAL> vecTempResBufIn_I, vecTempResBufIn_Q;
    CVector<_REAL> vecTempResBufOut_I, vecTempResBufOut_Q;
};
