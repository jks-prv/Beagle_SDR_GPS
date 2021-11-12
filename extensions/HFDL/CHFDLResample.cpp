
#ifdef KIWI
#else
    #include "types.h"
    #include <stdlib.h>
    #define STANDALONE_TEST
#endif

// this is here before Kiwi includes to prevent our "#define printf ALT_PRINTF" mechanism being disturbed
#include "CHFDLResample.h"

#ifdef STANDALONE_TEST
#endif

#ifdef KIWI
    #include "types.h"
    #include "printf.h"
#else
    #define real_printf(fmt, ...)
    #define NextTask(s)
#endif

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

CHFDLResample::CHFDLResample() { }

CHFDLResample::~CHFDLResample() { }

int CHFDLResample::setup(double f_srate, int n_samps)
{
    Resample_I = new CAudioResample();
    Resample_Q = new CAudioResample();
    float o_srate = HFDL_MIN_SRATE;
    float ratio = o_srate / f_srate;

    const int fixedInputSize = n_samps;
    Resample_I->Init(fixedInputSize, ratio);
    Resample_Q->Init(fixedInputSize, ratio);
    const int iMaxInputSize = Resample_I->GetMaxInputSize();
    vecTempResBufIn_I.Init(iMaxInputSize, (_REAL) 0.0);
    vecTempResBufIn_Q.Init(iMaxInputSize, (_REAL) 0.0);

    const int iMaxOutputSize = Resample_I->iOutputBlockSize;
    vecTempResBufOut_I.Init(iMaxOutputSize, (_REAL) 0.0);
    vecTempResBufOut_Q.Init(iMaxOutputSize, (_REAL) 0.0);

    Resample_I->Reset();
    Resample_Q->Reset();
    //printf("CHFDLResample: i_srate=%f o_srate=%f ratio=%f n_samps=%d iOutputBlockSize=%d\n",
    //    f_srate, o_srate, ratio, n_samps, Resample_I->iOutputBlockSize);
    
    return Resample_I->iOutputBlockSize;
}

void CHFDLResample::run(float *sample, int length, HFDL_resamp_t *resamp)
{
    int i;

    if (Resample_I == NULL || Resample_Q == NULL) return;
    
    for (i = 0; i < length; i++) {
        vecTempResBufIn_I[i] = sample[i*2];
        vecTempResBufIn_Q[i] = sample[i*2+1];
    }

    Resample_I->Resample(vecTempResBufIn_I, vecTempResBufOut_I);
    Resample_Q->Resample(vecTempResBufIn_Q, vecTempResBufOut_Q);
    //printf("."); fflush(stdout);
    
    resamp->out_I = &vecTempResBufOut_I[0];
    resamp->out_Q = &vecTempResBufOut_Q[0];
}

#ifdef STANDALONE_TEST
    
int main(int argc, char *argv[])
{
    int i;
    //printf("encode_table=%d error=%d\n", ARRAY_LEN(encode_table), ARRAY_LEN(error));
    
    #define ARG(s) (strcmp(argv[ai], s) == 0)
    #define ARGP() argv[++ai]
    #define ARGB(v) (v) = true;
    #define ARGL(v) if (ai+1 < argc) (v) = strtol(argv[++ai], 0, 0);
    
    int display = DX;
    //int display = CMDS;
    //int display = ALL;
    int fileno = 0, repeat = 1;
    //int offset = -1;
    int offset = 0;
    bool use_new_resampler = false;     // there is never a resampler when running standalone (use an 8k file)
    bool compare_mag = false;
    bool compare_dsp_all = false;

    ale::decode_ff_impl decode;
    decode.modem_init(0, use_new_resampler, 0, N_SAMP_BUF, false);   // false = use file realtime instead of current UTC

    for (int ai = 1; ai < argc; ) {
        if (ARG("-f") || ARG("--file")) ARGL(fileno);
        if (ARG("-o") || ARG("--offset")) ARGL(offset);
        if (ARG("-r") || ARG("--repeat")) ARGL(repeat);
        if (ARG("-m") || ARG("--mag")) ARGB(compare_mag);
        if (ARG("-y")) ARGB(compare_dsp_all);

        // display
        if (ARG("-x") || ARG("--dx")) display = DX;
        if (ARG("-c") || ARG("--cmds")) display = CMDS;
        if (ARG("-a") || ARG("--all")) display = ALL;
        if (ARG("-d") || ARG("--debug")) display = DBG;
        ai++;
    }
    
    int ars = ARRAY_LEN(sa_fn);
    if (fileno >= ars) {
        printf("range error: --file 0..%d\n", ars-1);
        exit(-1);
    }
    

    int start = (fileno < 0)? 0 : fileno, stop = (fileno < 0)? (ars-1) : fileno;
    for (int f = start; f <= stop; f++) {
        if (offset >= 0) {
            for (i = 0; i < repeat; i++) {
                decode.set_display(display);

                decode.calc_mag = true;
                decode.run_standalone(f, offset);
                decode.modem_reset();

                if (compare_mag) {
                    decode.calc_mag = false;
                    decode.run_standalone(f, offset);
                    decode.modem_reset();
                }
                
                if (compare_dsp_all) {
                    decode.set_display(ALL);

                    decode.calc_mag = true;
                    decode.run_standalone(f, offset);
                    decode.modem_reset();

                    if (compare_mag) {
                        decode.calc_mag = false;
                        decode.run_standalone(f, offset);
                        decode.modem_reset();
                    }
                }
            }
        } else {
            decode.set_display(display);

            for (offset = 0; offset <= 32; offset += 4) {
                decode.run_standalone(f, offset);
                decode.modem_reset();
            }
        }
    }
}
#endif
