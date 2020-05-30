#pragma once

#define NOISE_TYPES 4
#define NOISE_PARAMS 8

typedef enum { NB_OFF = 0, NB_STD = 1, NB_WILD = 2 } nb_algo_e;
typedef enum { NB_BLANKER = 0, NB_WF = 1, NB_CLICK = 2 } nb_type_e;

typedef enum { NR_OFF_ = 0, NR_WDSP = 1, NR_ORIG = 2, NR_SPECTRAL = 3 } nr_algo_e;
typedef enum { NR_DENOISE = 0, NR_AUTONOTCH = 1 } nr_type_e;
