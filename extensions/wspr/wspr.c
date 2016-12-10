/*
 * k9an-wspr is a detector/demodulator/decoder for K1JT's 
 * Weak Signal Propagation Reporter (WSPR) mode.
 *
 * Copyright 2014, Steven Franke, K9AN
 */

// FIXME add whatever copyright they're now using

#include "wspr.h"

#ifndef EXT_WSPR
	void wspr_main() {}
#else

#include "fano.h"
#include "kiwi.h"
#include "misc.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define WSPR_DEBUG_MSG	true
#define WSPR_DEBUG_MSG	false

#define WSPR_DATA		0

#define NPK 256
#define MAX_NPK 8

//wspr_status
#define	NONE		0
#define	IDLE		1
#define	SYNC		2
#define	RUNNING		3
#define	DECODING	4

#define NTASK 64
#define	NT() NextTask("wspr")

struct wspr_t {
	int rx_chan;
	bool create_tasks;
	int ping_pong, fft_ping_pong, decode_ping_pong;
	int capture, demo;
	int status, status_resume;
	bool send_error, abort_decode;
	int WSPR_FFTtask_id, WSPR_DecodeTask_id;
	
	// sampler
	bool reset, tsync;
	int last_sec;
	int decim, didx, group;
	int demo_sn, demo_rem;
	double fi;
	
	// FFT task
	FFTW_COMPLEX *fftin, *fftout;
	FFTW_PLAN fftplan;
	int FFTtask_group;
	
	// computed by sampler or FFT task, processed by decode task
	time_t utc[2];
	CPX_t i_data[2][TPOINTS], q_data[2][TPOINTS];
	float pwr_samp[2][NFFT][FPG*GROUPS];
	float pwr_sampavg[2][NFFT];

	// decode task
	float dialfreq;
	u1_t *symbols, *decdata;
	char *callsign, *grid;
	float allfreqs[NPK];
	char allcalls[NPK][7];
};

static wspr_t wspr[RX_CHANS];

// defined constants
static int nffts = FPG*floor(GROUPS-1) - 1;
static int nbins_411 = ceilf(NFFT * BW_MAX / FSRATE)+1;
static int hbins_205 = (nbins_411-1)/2;
static unsigned int nbits = HSYM_81;

// computed constants
static int decimate;
static double fdecimate;
static int verbose=0, quickmode=0, medium_effort=0;

// loaded from admin configuration
int bfo;


// what		m0				m1				m2
// f1		i				i				i
//			o=fbest			o=fbest			x
// shift1	i (indir)		i				i
//			o=best_shift	o=best_shift	x
// drift1	i				i				i
// sync		o=syncmax		o=syncmax		x

static void sync_and_demodulate(
	CPX_t *id,
	CPX_t *qd,
	long np,
	unsigned char *symbols,
	float fstep,
	int lagmin, int lagmax, int lagstep,
	float drift1,
	float *f1,
	int *shift1,
	float *sync,
	int mode)
{
    float dt=1.0/FSRATE, df=FSRATE/FSPS, fbest;
    long int i, j, k;
    double pi=4.*atan(1.0);
    float f0=0.0,fp,ss;
    int lag;
    
    double
    i0[NSYM_162],q0[NSYM_162],
    i1[NSYM_162],q1[NSYM_162],
    i2[NSYM_162],q2[NSYM_162],
    i3[NSYM_162],q3[NSYM_162];
    
    double p0,p1,p2,p3,cmet,totp,syncmax, phase, fac;
    double
    c0[SPS],s0[SPS],
    c1[SPS],s1[SPS],
    c2[SPS],s2[SPS],
    c3[SPS],s3[SPS];
    double
    dphi0, cdphi0, sdphi0,
    dphi1, cdphi1, sdphi1,
    dphi2, cdphi2, sdphi2,
    dphi3, cdphi3, sdphi3;
    float fsymb[NSYM_162];
    int best_shift = 0, ifreq;
    int ifmin=0, ifmax=0;
    
    syncmax=-1e30;


// mode is the last argument:
// 0 no frequency or drift search. find best time lag.
// 1 no time lag or drift search. find best frequency.
//      2 no frequency or time lag search. calculate soft-decision symbols
//        using passed frequency and shift.
    if( mode == 0 ) {
        ifmin=0;
        ifmax=0;
        fstep=0.0;
        f0=*f1;
    }
    if( mode == 1 ) {
        lagmin=*shift1;
        lagmax=*shift1;
        ifmin=-5;
        ifmax=5;
        f0=*f1;

    }
    if( mode == 2 ) {
        best_shift = *shift1;
        f0=*f1;
    }

    if( mode != 2 ) {
		for(ifreq=ifmin; ifreq<=ifmax; ifreq++)
		{
			f0=*f1+ifreq*fstep;
	// search lag range
			
			for(lag=lagmin; lag<=lagmax; lag=lag+lagstep)
			{
				ss=0.0;
				totp=0.0;
				for (i=0; i<NSYM_162; i++)
				{
					fp = f0 + (drift1/2.0)*(i-HSYM_81)/FHSYM_81;
					
					dphi0=2*pi*(fp-1.5*df)*dt;
					cdphi0=cos(dphi0);
					sdphi0=sin(dphi0);
					dphi1=2*pi*(fp-0.5*df)*dt;
					cdphi1=cos(dphi1);
					sdphi1=sin(dphi1);
					dphi2=2*pi*(fp+0.5*df)*dt;
					cdphi2=cos(dphi2);
					sdphi2=sin(dphi2);
					dphi3=2*pi*(fp+1.5*df)*dt;
					cdphi3=cos(dphi3);
					sdphi3=sin(dphi3);
					
					c0[0]=1;
					s0[0]=0;
					c1[0]=1;
					s1[0]=0;
					c2[0]=1;
					s2[0]=0;
					c3[0]=1;
					s3[0]=0;
					
					for (j=1; j<SPS; j++) {
						c0[j]=c0[j-1]*cdphi0-s0[j-1]*sdphi0;
						s0[j]=c0[j-1]*sdphi0+s0[j-1]*cdphi0;
						c1[j]=c1[j-1]*cdphi1-s1[j-1]*sdphi1;
						s1[j]=c1[j-1]*sdphi1+s1[j-1]*cdphi1;
						c2[j]=c2[j-1]*cdphi2-s2[j-1]*sdphi2;
						s2[j]=c2[j-1]*sdphi2+s2[j-1]*cdphi2;
						c3[j]=c3[j-1]*cdphi3-s3[j-1]*sdphi3;
						s3[j]=c3[j-1]*sdphi3+s3[j-1]*cdphi3;
						
						if ((j%NTASK)==(NTASK-1)) NT();
					}
					
					i0[i]=0.0;
					q0[i]=0.0;
					i1[i]=0.0;
					q1[i]=0.0;
					i2[i]=0.0;
					q2[i]=0.0;
					i3[i]=0.0;
					q3[i]=0.0;
	 
					for (j=0; j<SPS; j++)
					{
						k=lag+i*SPS+j;
						if( (k>0) & (k<np) ) {
						i0[i]=i0[i]+id[k]*c0[j]+qd[k]*s0[j];
						q0[i]=q0[i]-id[k]*s0[j]+qd[k]*c0[j];
						i1[i]=i1[i]+id[k]*c1[j]+qd[k]*s1[j];
						q1[i]=q1[i]-id[k]*s1[j]+qd[k]*c1[j];
						i2[i]=i2[i]+id[k]*c2[j]+qd[k]*s2[j];
						q2[i]=q2[i]-id[k]*s2[j]+qd[k]*c2[j];
						i3[i]=i3[i]+id[k]*c3[j]+qd[k]*s3[j];
						q3[i]=q3[i]-id[k]*s3[j]+qd[k]*c3[j];
						}
						
						if ((j%NTASK)==(NTASK-1)) NT();
					}
					
					p0=i0[i]*i0[i]+q0[i]*q0[i];
					p1=i1[i]*i1[i]+q1[i]*q1[i];
					p2=i2[i]*i2[i]+q2[i]*q2[i];
					p3=i3[i]*i3[i]+q3[i]*q3[i];
				
					totp=totp+p0+p1+p2+p3;
					cmet=(p1+p3)-(p0+p2);
					ss=ss+cmet*(2*pr3[i]-1);
				}
				
				if( ss/totp > syncmax ) {
					syncmax=ss/totp;
	 
					best_shift=lag;
					
					fbest=f0;
				}
			} // lag loop
		} //freq loop
		
		*sync=syncmax;
		*shift1=best_shift;
		*f1=fbest;
		return;
    } //if not mode 2
    
    if( mode == 2 )
    {
        for (i=0; i<NSYM_162; i++)
        {
            i0[i]=0.0;
            q0[i]=0.0;
            i1[i]=0.0;
            q1[i]=0.0;
            i2[i]=0.0;
            q2[i]=0.0;
            i3[i]=0.0;
            q3[i]=0.0;
            
            fp=f0+(drift1/2.0)*(i-FHSYM_81)/FHSYM_81;
            for (j=0; j<SPS; j++)
            {
                k=best_shift+i*SPS+j;
                if( (k>0) & (k<np) ) {
                phase=2*pi*(fp-1.5*df)*k*dt;
                i0[i]=i0[i]+id[k]*cos(phase)+qd[k]*sin(phase);
                q0[i]=q0[i]-id[k]*sin(phase)+qd[k]*cos(phase);
                phase=2*pi*(fp-0.5*df)*k*dt;
                i1[i]=i1[i]+id[k]*cos(phase)+qd[k]*sin(phase);
                q1[i]=q1[i]-id[k]*sin(phase)+qd[k]*cos(phase);
                phase=2*pi*(fp+0.5*df)*k*dt;
                i2[i]=i2[i]+id[k]*cos(phase)+qd[k]*sin(phase);
                q2[i]=q2[i]-id[k]*sin(phase)+qd[k]*cos(phase);
                phase=2*pi*(fp+1.5*df)*k*dt;
                i3[i]=i3[i]+id[k]*cos(phase)+qd[k]*sin(phase);
                q3[i]=q3[i]-id[k]*sin(phase)+qd[k]*cos(phase);
                }
                
                if ((j%NTASK)==(NTASK-1)) NT();
            }
            
            p0=i0[i]*i0[i]+q0[i]*q0[i];
            p1=i1[i]*i1[i]+q1[i]*q1[i];
            p2=i2[i]*i2[i]+q2[i]*q2[i];
            p3=i3[i]*i3[i]+q3[i]*q3[i];

            
            if( pr3[i] == 1 )
            {
                fsymb[i]=(p3-p1);
            } else {
                fsymb[i]=(p2-p0);
            }
        }
        float fsum=0.0, f2sum=0.0;
        for (i=0; i<NSYM_162; i++) {
            fsum=fsum+fsymb[i]/FNSYM_162;
            f2sum=f2sum+fsymb[i]*fsymb[i]/FNSYM_162;
//            printf("%d %f\n",i,fsymb[i]);
        }
        NT();
        
        fac=sqrt(f2sum-fsum*fsum);
        for (i=0; i<NSYM_162; i++) {
            fsymb[i]=128*fsymb[i]/fac;
            if( fsymb[i] > 127)
                fsymb[i]=127.0;
            if( fsymb[i] < -128 )
                fsymb[i]=-128.0;
            symbols[i]=fsymb[i]+128;
//            printf("symb: %lu %d\n",i, symbols[i]);
        }
        NT();
    }
}

// END f1 drift1 shift1 sync

const char *status_str[] = { "none", "idle", "sync", "running", "decoding" };

static void wspr_status(wspr_t *w, int status, int resume)
{
	printf("WSPR RX%d wspr_status: set status %d-%s\n", w->rx_chan, status, status_str[status]);
	ext_send_msg(w->rx_chan, WSPR_DEBUG_MSG, "EXT WSPR_STATUS=%d", status);
	w->status = status;
	if (resume != NONE) {
		printf("WSPR RX%d wspr_status: will resume to %d-%s\n", w->rx_chan, resume, status_str[resume]);
		w->status_resume = resume;
	}
}

static void renormalize(float psavg[], float smspec[])
{
	int i,j,k;

// smooth with 7-point window and limit the spectrum to +/-150 Hz
    int window[7] = {1,1,1,1,1,1,1};
    
    for (i=0; i<nbins_411; i++) {
        smspec[i] = 0.0;
        for (j=-3; j<=3; j++) {
            k = SPS-hbins_205+i+j;
            smspec[i] += window[j+3]*psavg[k];
        }
    }
	NT();

// sort spectrum values so that we can pick off noise level as a percentile
    float tmpsort[nbins_411];
    for (j=0; j<nbins_411; j++)
        tmpsort[j] = smspec[j];
    qsort(tmpsort, nbins_411, sizeof(float), qsort_floatcomp);
	NT();

// noise level of spectrum is estimated as 123/nbins_411= 30'th percentile
// of the smoothed spectrum. Matched filter sidelobes from very strong signals
// will cause the estimated noise level to be biased high, causing estimated
// snrs to be biased low.
// my SNRs differ from wspr0/wspr-x when there are strong signals in the band.
// This suggests that K1JT's approach and mine have different biases.
    
    float noise_level = tmpsort[122];
    
// renormalize spectrum so that (large) peaks represent an estimate of snr
    float min_snr_neg33db = pow(10.0, (-33+26.5)/10.0);
    for (j=0; j<nbins_411; j++) {
        smspec[j] = smspec[j]/noise_level - 1.0;
        if (smspec[j] < min_snr_neg33db)
            smspec[j] = 0.1;
            continue;
    }
	NT();
}

void WSPR_FFT(void *param)
{
	int i,j,k;
	
	while (1) {
	
	//printf("WSPR_FFTtask sleep..\n");
	int rx_chan = TaskSleep(0);

	wspr_t *w = &wspr[rx_chan];
	int grp = w->FFTtask_group;
	//printf("WSPR_FFTtask wakeup\n");
	int first = grp*FPG, last = first+FPG;

// Do ffts over 2 symbols, stepped by half symbols
	CPX_t *id = w->i_data[w->fft_ping_pong], *qd = w->q_data[w->fft_ping_pong];

	float maxiq = 1e-66, maxpwr = 1e-66;
	int maxi=0;
	float savg[NFFT];
	memset(savg, 0, sizeof(savg));
	
    for (i=first; i<last; i++) {
        for (j=0; j<NFFT; j++ ){
            k = i*HSPS+j;
            w->fftin[j][0] = id[k];
            w->fftin[j][1] = qd[k];
            //if (i==0) printf("IN %d %fi %fq\n", j, w->fftin[j][0], w->fftin[j][1]);
        }
        
		//u4_t start = timer_us();
		NT();
		FFTW_EXECUTE(w->fftplan);
		NT();
		//if (i==0) printf("512 FFT %.1f us\n", (float)(timer_us()-start));
		
        for (j=0; j<NFFT; j++) {
            k = j+SPS;
            if (k > (NFFT-1))
                k = k-NFFT;
            //if (i==0) printf("OUT %d %fi %fq\n", j, w->fftout[k][0], w->fftout[k][1]);
            float pwr = w->fftout[k][0]*w->fftout[k][0] + w->fftout[k][1]*w->fftout[k][1];
            if (w->fftout[k][0] > maxiq) { maxiq = w->fftout[k][0]; }
            if (pwr > maxpwr) { maxpwr = pwr; maxi = k; }
            w->pwr_samp[w->fft_ping_pong][j][i] = pwr;
            w->pwr_sampavg[w->fft_ping_pong][j] += pwr;
            savg[j] += pwr;
        }
		NT();
    }

	// send spectrum data to client
    float smspec[nbins_411], dB, max_dB=-99, min_dB=99;
    renormalize(savg, smspec);
    
    for (j=0; j<nbins_411; j++) {
		smspec[j] = dB = 10*log10(smspec[j])-26.5;
		if (dB > max_dB) max_dB = dB;
		if (dB < min_dB) min_dB = dB;
	}

	float y, range_dB, pix_per_dB;
	#if 1
		max_dB = -10;
		min_dB = -40;
	#endif
	range_dB = max_dB - min_dB;
	pix_per_dB = 255.0 / range_dB;
	u1_t ws[nbins_411+1];
	
	for (j=0; j<nbins_411; j++) {
		#if 0
			if (j<50) smspec[j] = -15;
			if (j<40) smspec[j] = -20;
			if (j<30) smspec[j] = -23;
			if (j<20) smspec[j] = -26;
			if (j<10) smspec[j] = -29;
		#endif
		y = pix_per_dB * (smspec[j] - min_dB);
		if (y > 255.0) y = 255.0;
		if (y < 0) y = 0;
		ws[j+1] = (u1_t) y;
	}
	ws[0] = ((grp == 0) && w->tsync)? 1:0;

	if (ext_send_msg_data(w->rx_chan, WSPR_DEBUG_MSG, WSPR_DATA, ws, nbins_411+1) < 0) {
		w->send_error = true;
	}

	//printf("WSPR_FFTtask group %d:%d %d-%d(%d) %f %f(%d)\n", w->fft_ping_pong, grp, first, last, nffts, maxiq, maxpwr, maxi);
	//printf("  %d-%d(%d)\r", first, last, nffts); fflush(stdout);
    
    if (last < nffts) continue;

    w->decode_ping_pong = w->fft_ping_pong;
    printf("WSPR ---DECODE -> %d\n", w->decode_ping_pong);
    printf("\n");
    TaskWakeup(w->WSPR_DecodeTask_id, TRUE, w->rx_chan);
    
    }
}

void wspr_send_peaks(wspr_t *w, pk_t *pk, int npk)
{
    char peaks_s[NPK*(6+1+LEN_CALL+1) + 16];
    char *s = peaks_s;
    int j, n;
    for (j=0; j < npk; j++) {
    	int bin_flags = pk[j].bin0 | pk[j].flags;
    	n = sprintf(s, "%d:%s:", bin_flags, pk[j].snr_call); s += n;
    }
	ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_PEAKS", "%s", peaks_s);
}

void WSPR_Deco(void *param)
{
    long int i,j,k;
    int delta, lagmin, lagmax, lagstep;
    u4_t n28b, m22b;
    unsigned long metric, maxcycles, cycles;
    float df = FSRATE/FSPS/2;
    float dt = 1.0/FSRATE;
    pk_t pk[NPK];
    //double mi, mq, mi2, mq2, miq;
    u4_t start=0;
	CPX_t *idat, *qdat;

	int shift;
    float f, fstep, snr, drift;
    float syncC, sync0, sync1, sync2;

#include "./mettab.c"

	// fixme: revisit in light of new priority task system
	// this was intended to allow a minimum percentage of runtime
	TaskParams(4000);		//FIXME does this really still work?

	while (1) {
	
	printf("WSPR_DecodeTask sleep..\n");
	if (start) printf("WSPR_DecodeTask %.1f sec\n", (float)(timer_ms()-start)/1000.0);

	int rx_chan = TaskSleep(0);
	wspr_t *w = &wspr[rx_chan];

	w->abort_decode = false;
	start = timer_ms();
	printf("WSPR_DecodeTask wakeup\n");

    memset(w->symbols, 0, sizeof(char)*nbits*2);
    memset(w->allfreqs, 0, sizeof(float)*NPK);
    memset(w->allcalls, 0, sizeof(char)*NPK*7);
    memset(w->grid, 0, sizeof(char)*5);
    memset(w->callsign, 0, sizeof(char)*7);
    
	idat = w->i_data[w->decode_ping_pong];
	qdat = w->q_data[w->decode_ping_pong];
    wspr_status(w, DECODING, NONE);

	#if 0
    if (verbose) {
    	getStats(idat, qdat, TPOINTS, &mi, &mq, &mi2, &mq2, &miq);
        printf("total power: %4.1f dB\n", 10*log10(mi2+mq2));
		NT();
    }
    #endif
    
    float smspec[nbins_411];
    renormalize(w->pwr_sampavg[w->decode_ping_pong], smspec);

// find all local maxima in smoothed spectrum.
    memset(pk, 0, sizeof(pk));
    
    int npk=0;
    for (j=1; j<(nbins_411-1); j++) {
        if ((smspec[j] > smspec[j-1]) && (smspec[j] > smspec[j+1]) && (npk < NPK)) {
        	pk[npk].bin0 = j;
            pk[npk].freq0 = (j-hbins_205)*df;
            pk[npk].snr0 = 10*log10(smspec[j])-26.5;
            npk++;
        }
    }
	//printf("initial npk %d/%d\n", npk, NPK);
	NT();

// let's not waste time on signals outside of the range [FMIN, FMAX].
    i=0;
    for( j=0; j<npk; j++) {
        if( pk[j].freq0 >= FMIN && pk[j].freq0 <= FMAX ) {
            pk[i].bin0 = pk[j].bin0;
            pk[i].freq0 = pk[j].freq0;
            pk[i].snr0 = pk[j].snr0;
            i++;
        }
    }
    npk = i;
	//printf("freq range limited npk %d\n", npk);
	NT();
	
	// only look at a limited number of strong peaks
    qsort(pk, npk, sizeof(pk_t), snr_comp);		// sort in decreasing snr order
    if (npk > MAX_NPK) npk = MAX_NPK;
	//printf("MAX_NPK limited npk %d/%d\n", npk, MAX_NPK);

	// remember our frequency-sorted index
    qsort(pk, npk, sizeof(pk_t), freq_comp);
    for (j=0; j<npk; j++) {
    	pk[j].freq_idx = j;
    }

    // send peak info to client in increasing frequency order
	pk_t pk_freq[NPK];
	memcpy(pk_freq, pk, sizeof(pk));

    for (j=0; j < npk; j++) {
    	sprintf(pk_freq[j].snr_call, "%d", (int) roundf(pk_freq[j].snr0));
    }

	if (npk)
		wspr_send_peaks(w, pk_freq, npk);
	
	// keep 'pk' in snr order so strong sigs are processed first in case we run out of decode time
    qsort(pk, npk, sizeof(pk_t), snr_comp);		// sort in decreasing snr order
    
	/* do coarse estimates of freq, drift and shift using k1jt's basic approach, 
	more or less.
	
	- look for time offsets of up to +/- 8 symbols relative to the nominal start
	 time, which is 2 seconds into the file
	- this program calculates shift relative to the beginning of the file
	- negative shifts mean that the signal started before the beginning of the 
	 file
	- The program prints shift-2 seconds to give values consistent with K1JT's
	definition
	- shifts that cause the sync vector to fall off of an end of the data vector 
	 are accommodated by "partial decoding", such that symbols that cannot be 
	 decoded due to missing data produce a soft-decision symbol value of 128 
	 for the fano decoder.
	- the frequency drift model is linear, deviation of +/- drift/2 over the
	  span of 162 symbols, with deviation equal to 0 at the center of the signal
	  vector (should be consistent with K1JT def). 
	*/

    int idrift,ifr,if0,ifd,k0;
    long int kindex;
    float smax,ss,pow,p0,p1,p2,p3;
    
    for (j=0; j<npk; j++) {
        smax = -1e30;
        if0 = pk[j].freq0/df+SPS;
        
        for (ifr=if0-1; ifr<=if0+1; ifr++) {
			for (k0=-10; k0<22; k0++)
			{
				for (idrift=-4; idrift<=4; idrift++)
				{
					ss=0.0;
					pow=0.0;
					for (k=0; k<NSYM_162; k++)
					{
						ifd=ifr+((float)k-FHSYM_81)/FHSYM_81*( (float)idrift )/(2.0*df);
						kindex=k0+2*k;
						if( kindex < nffts ) {
							p0=w->pwr_samp[w->decode_ping_pong][ifd-3][kindex];
							p1=w->pwr_samp[w->decode_ping_pong][ifd-1][kindex];
							p2=w->pwr_samp[w->decode_ping_pong][ifd+1][kindex];
							p3=w->pwr_samp[w->decode_ping_pong][ifd+3][kindex];
							ss=ss+(2*pr3[k]-1)*(p3+p1-p0-p2);
							pow=pow+p0+p1+p2+p3;
							syncC=ss/pow;
						}
						NT();
					}
					if( syncC > smax ) {
						smax=syncC;
						pk[j].shift0=HSPS*(k0+1);
						pk[j].drift0=idrift;
						pk[j].freq0=(ifr-SPS)*df;
						pk[j].sync0=syncC;
					}
					//printf("drift %d  k0 %d  sync %f\n",idrift,k0,smax);
					NT();
				}
			}
        }
        if ( verbose ) {
			printf("npeak     #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync  %3d bin\n",
                j, pk[j].snr0, w->dialfreq+(bfo+pk[j].freq0)/1e6, pk[j].freq0, pk[j].shift0, pk[j].drift0, pk[j].sync0, pk[j].bin0); }
    }

    //FILE *fall_wspr, *fwsprd;
    //fall_wspr=fopen("ALL_WSPR.TXT","a");
    //fwsprd=fopen("wsprd.out","w");
    
    int uniques=0;
    
    int pki;
    for (pki=0; pki<npk && !w->abort_decode; pki++) {
		u4_t decode_start = timer_ms();

		// now refine the estimates of freq, shift using sync as a metric.
		// sync is calculated such that it is a float taking values in the range
		// [0.0,1.0].
				
		// function sync_and_demodulate has three modes of operation
		// mode is the last argument:
		//      0 no frequency or drift search. find best time lag.
		//      1 no time lag or drift search. find best frequency.
		//      2 no frequency or time lag search. calculate soft-decision symbols
		//        using passed frequency and shift.
		
		f = pk[pki].freq0;
        snr = pk[pki].snr0;
        drift = pk[pki].drift0;
        shift = pk[pki].shift0;
        syncC = pk[pki].sync0;
        
		pk_freq[pk[pki].freq_idx].flags |= WSPR_F_DECODING;
		wspr_send_peaks(w, pk_freq, npk);

        if( verbose ) {
			printf("coarse    #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync\n",
            	pki, snr, w->dialfreq+(bfo+f)/1e6, f, shift, drift, syncC); }

		// fine search for best sync lag (mode 0)
        fstep=0.0;
        lagmin=shift-144;
        lagmax=shift+144;
        lagstep=8;
        
        // f: in = yes
        // shift: in = via lagmin/max
        sync_and_demodulate(idat, qdat, TPOINTS, w->symbols, fstep, lagmin, lagmax, lagstep, drift, &f, &shift, &sync0, 0);
        // f: out = fbest always
        // shift: out = best_shift always
        // sync0: out = syncmax always

        if (verbose) {
			printf("fine sync #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync%s\n",
            	pki, snr, w->dialfreq+(bfo+f)/1e6, f, shift, drift, sync0,
            	(sync0 < syncC)? ", sync worse?":"");
        }
		NT();

		// using refinements from mode 0 search above, fine search for frequency peak (mode 1)
        fstep=0.1;

        // f: in = yes
        // shift: in = yes
        sync_and_demodulate(idat, qdat, TPOINTS, w->symbols, fstep, lagmin, lagmax, lagstep, drift, &f, &shift, &sync1, 1);
        // f: out = fbest always
        // shift: out = best_shift always
        // sync1: out = syncmax always

        if (verbose) {
			printf("fine freq #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync%s\n",
            	pki, snr, w->dialfreq+(bfo+f)/1e6, f, shift, drift, sync1,
            	(sync1 < sync0)? ", sync worse?":"");
        }
		NT();

		int worth_a_try = (sync1 > 0.2)? 1:0;
        if (verbose) {
			printf("try?      #%ld %s\n", pki, worth_a_try? "yes":"no");
        }

        int idt=0, ii, jiggered_shift;
        delta = 50;
        //maxcycles = 10000;
        //maxcycles = 200;
        maxcycles = 10;
        int decoded = 0;

        while (!w->abort_decode && worth_a_try && !decoded && (idt <= (medium_effort? 32:128))) {
            ii = (idt+1)/2;
            if (idt%2 == 1) ii = -ii;
            jiggered_shift = shift+ii;
        
			// use mode 2 to get soft-decision symbols
			// f: in = yes
			// shift: in = yes
            sync_and_demodulate(idat, qdat, TPOINTS, w->symbols, fstep, lagmin, lagmax, lagstep, drift, &f, &jiggered_shift, &sync2, 2);
			// f: out = NO
			// shift: out = NO
			// sync2: out = NO
			NT();
        
            deinterleave(w->symbols);
			NT();

            decoded = fano(&metric, &cycles, w->decdata, w->symbols, nbits, mettab, delta, maxcycles);
			if (verbose) {
				printf("jig <>%3d #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync  %4ld metric  %3ld cycles\n",
					idt, pki, snr, w->dialfreq+(bfo+f)/1e6, f, jiggered_shift, drift, sync2, metric, cycles);
			}
			NT();
            
            idt++;

            if (quickmode) {
                break;
            }
        }

		int valid = 0;
        if (decoded) {
            unpack50(w->decdata, &n28b, &m22b);
            unpackcall(n28b, w->callsign);
            int ntype = (m22b & 127) - 64;
            u4_t ngrid = m22b >> 7;
            unpackgrid(ngrid, w->grid);

			// this is where the extended messages would be taken care of
            if ((ntype >= 0) && (ntype <= 62)) {
                int nu = ntype%10;
                if (nu == 0 || nu == 3 || nu == 7) {
                    valid=1;
                } else {
            		printf("ntype non-std %d ===============================================================\n", ntype);
                }
            } else if ( ntype < 0 ) {
				printf("ntype <0 %d ===============================================================\n", ntype);
            } else {
            	printf("ntype=63 %d ===============================================================\n", ntype);
            }
            
			// de-dupe using callsign and freq (only a dupe if freqs are within 1 Hz)
            int dupe=0;
            for (i=0; i < uniques; i++) {
                if ( !strcmp(w->callsign, w->allcalls[i]) && (fabs( f-w->allfreqs[i] ) <= 1.0) )
                    dupe=1;
            }
            
            if ((verbose || !dupe) && valid) {
            	struct tm tm;
            	gmtime_r(&w->utc[w->decode_ping_pong], &tm);
            
                strcpy(w->allcalls[uniques], w->callsign);
                w->allfreqs[uniques]=f;
                uniques++;
                
            	printf("%02d%02d %3.0f %4.1f %10.6f %2d  %-s %4s %2d in %.3f secs --------------------------------------------------------------------\n",
                   tm.tm_hour, tm.tm_min, snr, (shift*dt-2.0), w->dialfreq+(bfo+f)/1e6,
                   (int)drift, w->callsign, w->grid, ntype, (float)(timer_ms()-decode_start)/1000.0);
                
				ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                	"%02d%02d %3.0f %4.1f %9.6f %2d  %-6s %4s  %2d",
					tm.tm_hour, tm.tm_min, snr, (shift*dt-2.0), w->dialfreq+(bfo+f)/1e6, (int) drift, w->callsign, w->grid, ntype);
				
				strcpy(pk_freq[pk[pki].freq_idx].snr_call, w->callsign);

				#if 0
				fprintf(fall_wspr,"%6s %4s %3.0f %3.0f %4.1f %10.6f  %-s %4s %2d          %2.0f     %lu    %4d\n",
						   date, uttime, sync1*10, snr,
						   shift*dt-2.0, w->dialfreq+(bfo+f)/1e6,
						   w->callsign, w->grid, ntype, drift, cycles/HSYM_81, ii);
				fprintf(fwsprd,"%6s %4s %3d %3.0f %4.1f %10.6f  %-s %4s %2d          %2d     %lu\n",
							date, uttime, (int)(sync1*10), snr,
							shift*dt-2.0, w->dialfreq+(bfo+f)/1e6,
							w->callsign, w->grid, ntype, (int)drift, cycles/HSYM_81);
				#endif
            } else {
            	valid = 0;
			}
        } else {
			if (verbose)
				printf("give up   #%ld %.3f secs\n", pki, (float)(timer_ms()-decode_start)/1000.0);
        }

		if (!valid)
			pk_freq[pk[pki].freq_idx].flags |= WSPR_F_DELETE;

		wspr_send_peaks(w, pk_freq, npk);
    }

    //fclose(fall_wspr);
    //fclose(fwsprd);
	NT();

	if (w->abort_decode)
		printf("decoder aborted\n");

    wspr_status(w, w->status_resume, NONE);

    }	// while(1)
}

void wspr_data(int rx_chan, int ch, int nsamps, TYPECPX *samps)
{
	wspr_t *w = &wspr[rx_chan];
	int i;

	//printf("WD%d didx %d send_error %d reset %d\n", w->capture, w->didx, w->send_error, w->reset);
	if (w->send_error || (w->demo && w->capture && w->didx >= TPOINTS)) {
		printf("RX%d STOP send_error %d\n", w->rx_chan, w->send_error);
		ext_unregister_receive_iq_samps(w->rx_chan);
		w->send_error = FALSE;
		w->capture = FALSE;
		w->reset = TRUE;
	}

	if (w->reset) {
		w->ping_pong = w->decim = w->didx = w->group = w->demo_sn = 0;
		w->fi = 0;
		w->demo_rem = WSPR_DEMO_NSAMPS;
		w->tsync = FALSE;
		w->status_resume = IDLE;	// decoder finishes after we stop capturing
		w->reset = FALSE;
	}

	if (!w->capture) {
		return;
	}
	
	time_t t;
	time(&t); struct tm tm; gmtime_r(&t, &tm);
	if (tm.tm_sec != w->last_sec) {
		if (tm.tm_min&1 && tm.tm_sec == 50 && !w->demo)
			w->abort_decode = true;
		
		w->last_sec = tm.tm_sec;
	}
	
	if (w->demo) {
		// readout of the wspr_demo_samps is paced by the audio rate
		// but takes less than CTIME (120s) because it's already decimated
		samps = &wspr_demo_samps[w->demo_sn];
		nsamps = MIN(nsamps, w->demo_rem);
		w->demo_sn += nsamps;
		w->demo_rem -= nsamps;
		#if 0
		if (w->demo_rem == 0) {
			w->demo_sn = 0;
			w->demo_rem = WSPR_DEMO_NSAMPS;
		}
		#endif
	} else {
		if (w->tsync == FALSE) {		// sync to even minute boundary if not in demo mode
			if (!(tm.tm_min&1) && (tm.tm_sec == 0)) {
				printf("WSPR SYNC %s", ctime(&t));
				w->ping_pong ^= 1;
				w->decim = w->didx = w->group = 0;
				w->fi = 0;
				if (w->status != DECODING)
					wspr_status(w, RUNNING, RUNNING);
				w->tsync = TRUE;
			}
		}
	}
	
	if (w->didx == 0) {
    	memset(&w->pwr_sampavg[w->ping_pong][0], 0, sizeof(w->pwr_sampavg[0]));
	}
	
	if (w->group == 0) w->utc[w->ping_pong] = t;
	
	CPX_t *idat = w->i_data[w->ping_pong], *qdat = w->q_data[w->ping_pong];
	//double scale = 1000.0;
	double scale = 1.0;
	
	#define NON_INTEGER_DECIMATION
	#ifdef NON_INTEGER_DECIMATION

		for (i = (w->demo? 0 : trunc(w->fi)); i < nsamps;) {
	
			if (w->didx >= TPOINTS)
				return;

    		if (w->group == 3) w->tsync = FALSE;	// arm re-sync
			
			CPX_t re = (CPX_t) samps[i].re/scale;
			CPX_t im = (CPX_t) samps[i].im/scale;
			idat[w->didx] = re;
			qdat[w->didx] = im;
	
			if ((w->didx % NFFT) == (NFFT-1)) {
				w->fft_ping_pong = w->ping_pong;
				w->FFTtask_group = w->group-1;
				if (w->group) TaskWakeup(w->WSPR_FFTtask_id, TRUE, w->rx_chan);	// skip first to pipeline
				w->group++;
			}
			w->didx++;
	
			w->fi += fdecimate;
			i = w->demo? i+1 : trunc(w->fi);
		}
		w->fi -= nsamps;	// keep bounded

	#else

		for (i=0; i<nsamps; i++) {
	
			// decimate
			// demo mode samples are pre-decimated
			if (!w->demo && (w->decim++ < (decimate-1)))
				continue;
			w->decim = 0;
			
			if (w->didx >= TPOINTS)
				return;

    		if (w->group == 3) w->tsync = FALSE;	// arm re-sync
			
			CPX_t re = (CPX_t) samps[i].re/scale;
			CPX_t im = (CPX_t) samps[i].im/scale;
			idat[w->didx] = re;
			qdat[w->didx] = im;
	
			if ((w->didx % NFFT) == (NFFT-1)) {
				w->fft_ping_pong = w->ping_pong;
				w->FFTtask_group = w->group-1;
				if (w->group) TaskWakeup(w->WSPR_FFTtask_id, TRUE, w->rx_chan);	// skip first to pipeline
				w->group++;
			}
			w->didx++;
		}

    #endif
}

void wspr_close(int rx_chan)
{
	wspr_t *w = &wspr[rx_chan];
	//printf("WSPR wspr_close RX%d\n", rx_chan);
	if (w->create_tasks) {
		//printf("WSPR wspr_close TaskRemove FFT%d deco%d\n", w->WSPR_FFTtask_id, w->WSPR_DecodeTask_id);
		TaskRemove(w->WSPR_FFTtask_id);
		TaskRemove(w->WSPR_DecodeTask_id);
		w->create_tasks = false;
	}
}

bool wspr_msgs(char *msg, int rx_chan)
{
	wspr_t *w = &wspr[rx_chan];
	int n;
	
	printf("WSPR wspr_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		w->rx_chan = rx_chan;
		time_t t; time(&t);
		ext_send_msg(w->rx_chan, WSPR_DEBUG_MSG, "EXT nbins=%d WSPR_TIME=%d ready", nbins_411, t);
		wspr_status(w, IDLE, IDLE);
		return true;
	}

	n = sscanf(msg, "SET BFO=%d", &bfo);
	if (n == 1) {
		printf("WSPR BFO %d --------------------------------------------------------------\n", bfo);
		return true;
	}

	float f;
	n = sscanf(msg, "SET dialfreq=%f", &f);
	if (n == 1) {
		w->dialfreq = f / kHz;
		printf("WSPR dialfreq %.6f --------------------------------------------------------------\n", w->dialfreq);
		return true;
	}

	n = sscanf(msg, "SET capture=%d demo=%d", &w->capture, &w->demo);
	if (n == 2) {
		if (w->capture) {
			if (!w->create_tasks) {
				w->WSPR_FFTtask_id = CreateTask(WSPR_FFT, 0, EXT_PRIORITY);
				w->WSPR_DecodeTask_id = CreateTask(WSPR_Deco, 0, EXT_PRIORITY);
				w->create_tasks = true;
			}
			
			// send server time to client since that's what sync is based on
			time_t t; time(&t);
			ext_send_msg(w->rx_chan, WSPR_DEBUG_MSG, "EXT WSPR_TIME=%d", t);
			
			w->send_error = false;
			w->reset = TRUE;
			ext_register_receive_iq_samps(wspr_data, rx_chan);
			printf("WSPR CAPTURE --------------------------------------------------------------\n");

			if (w->demo)
				wspr_status(w, RUNNING, IDLE);
			else
				wspr_status(w, SYNC, RUNNING);
		} else {
			w->abort_decode = true;
			ext_unregister_receive_iq_samps(w->rx_chan);
			wspr_close(w->rx_chan);
		}
		return true;
	}
	
	return false;
}

void wspr_main();

ext_t wspr_ext = {
	"wspr",
	wspr_main,
	wspr_close,
	wspr_msgs,
};

void wspr_main()
{
	int i;
	
    assert(FSPS == round(SYMTIME * FSRATE));
    assert(SPS == (int) FSPS);
    assert(HSPS == (SPS/2));

	// options:
    //quickmode = 1;
    medium_effort = 1;
    verbose = 1;
    
    // FIXME: Someday it's possible samp rate will be different between rx_chans
    // if they have different bandwidths. Not possible with current architecture
    // of data pump.
    double frate = ext_get_sample_rateHz();
    fdecimate = frate / FSRATE;
    //assert (fdecimate >= 1.0);
    decimate = round(fdecimate);
	
	for (i=0; i < RX_CHANS; i++) {
		wspr_t *w = &wspr[i];
		memset(w, 0, sizeof(wspr_t));
		
		w->fftin = (FFTW_COMPLEX*) FFTW_MALLOC(sizeof(FFTW_COMPLEX)*NFFT);
		w->fftout = (FFTW_COMPLEX*) FFTW_MALLOC(sizeof(FFTW_COMPLEX)*NFFT);
		w->fftplan = FFTW_PLAN_DFT_1D(NFFT, w->fftin, w->fftout, FFTW_FORWARD, FFTW_ESTIMATE);
	
		w->symbols = (unsigned char*) malloc(sizeof(char)*nbits*2);
		w->decdata = (u1_t*) malloc((nbits+7)/8);
		w->grid = (char *) malloc(sizeof(char)*5);
		w->callsign = (char *) malloc(LEN_CALL + 1);

		w->status_resume = IDLE;
		w->tsync = FALSE;
		w->capture = 0;
		w->demo = 0;
		w->last_sec = -1;
		w->abort_decode = false;
		w->send_error = false;
		w->demo_rem = WSPR_DEMO_NSAMPS;
	}

	ext_register(&wspr_ext);
    printf("WSPR frate=%.1f decim=%.3f/%d sps=%d NFFT=%d nbins_411=%d\n", frate, fdecimate, decimate, SPS, NFFT, nbins_411);
}

#endif
