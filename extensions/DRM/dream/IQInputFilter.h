/* Automatically generated file with GNU Octave */

/* File name: "IQInputFilter.octave" */
/* Filter taps in time-domain */

#ifndef _IQINPUTFILTER_H_
#define _IQINPUTFILTER_H_

#define NUM_TAPS_IQ_INPUT_FILT        101
#define IQ_INP_HIL_FILT_DELAY         50

/* Low pass prototype for Hilbert-filter */
extern const float fHilFiltIQ[NUM_TAPS_IQ_INPUT_FILT];

#define NUM_TAPS_IQ_INPUT_FILT_HQ        169
#define IQ_INP_HIL_FILT_DELAY_HQ         84

/* Low pass prototype for Hilbert-filter */
extern const float fHilFiltIQ_HQ[NUM_TAPS_IQ_INPUT_FILT_HQ];

#endif /* _IQINPUTFILTER_H_ */
