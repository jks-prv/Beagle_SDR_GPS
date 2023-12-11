/*
 This file is part of program wsprd, a detector/demodulator/decoder
 for the Weak Signal Propagation Reporter (WSPR) mode.
 
 Copyright 2001-2015, Joe Taylor, K1JT
 
 Much of the present code is based on work by Steven Franke, K9AN,
 which in turn was based on earlier work by K1JT.
 
 Copyright 2014-2015, Steven Franke, K9AN
 
 License: GNU GPL v3
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "wspr.h"
#include "mem.h"

static const unsigned char pr3[NSYM_162]=
{1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,
    0,1,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,1,
    0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
    1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,
    0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,0,0,1,0,
    0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,
    0,1,0,0,0,1,1,1,0,0,0,0,0,1,0,1,0,0,1,1,
    0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
    0,0};

static const float DT = 1.0/FSRATE, DF = FSRATE/FSPS;
static const float TWOPIDT = 2*K_PI*DT;

static void sync_and_demodulate(wspr_t *w,
	WSPR_CPX_t *id, WSPR_CPX_t *qd, long np,
	unsigned char *symbols, float *f1, int ifmin, int ifmax, float fstep,
	int *shift1,
	int lagmin, int lagmax, int lagstep,
	float drift1, int symfac, float *sync, wspr_mode_e mode)
{
    /***********************************************************************
     * mode = FIND_BEST_TIME_LAG:   no frequency or drift search. find best time lag.
     *        FIND_BEST_FREQ:       no time lag or drift search. find best frequency.
     *        CALC_SOFT_SYMS:       no frequency or time lag search. calculate soft-decision
     *                              symbols using passed frequency and shift.
     ************************************************************************/
    
    float const DF15=DF*1.5, DF05=DF*0.5;

    int i, j, k, lag;
    float
		i0[NSYM_162], q0[NSYM_162],
		i1[NSYM_162], q1[NSYM_162],
		i2[NSYM_162], q2[NSYM_162],
		i3[NSYM_162], q3[NSYM_162];
    double p0, p1, p2, p3, cmet, totp, syncmax, fac;
    double
		c0[SPS], s0[SPS],
		c1[SPS], s1[SPS],
		c2[SPS], s2[SPS],
		c3[SPS], s3[SPS];
    double
		dphi0, cdphi0, sdphi0,
		dphi1, cdphi1, sdphi1,
		dphi2, cdphi2, sdphi2,
		dphi3, cdphi3, sdphi3;
    float f0=0.0, fp, ss, fbest=0.0, fsum=0.0, f2sum=0.0, fsymb[NSYM_162];
    int best_shift = 0, ifreq;
    
    syncmax=-1e30;

    if (mode == FIND_BEST_TIME_LAG) { ifmin=0; ifmax=0; fstep=0.0; }
    if (mode == FIND_BEST_FREQ)     { lagmin=*shift1; lagmax=*shift1; }
    if (mode == CALC_SOFT_SYMS)     { lagmin=*shift1; lagmax=*shift1; ifmin=0; ifmax=0; }

    for(ifreq=ifmin; ifreq<=ifmax; ifreq++) {
        f0=*f1+ifreq*fstep;
        for(lag=lagmin; lag<=lagmax; lag=lag+lagstep) {
            ss=0.0;
            totp=0.0;
            for (i=0; i<NSYM_162; i++) {
                fp = f0 + (drift1/2.0)*((float)i-FHSYM_81)/FHSYM_81;
                float fplast;   // init first time through by i == 0
                if( i==0 || (fp != fplast) ) {  // only calculate sin/cos if necessary
                    dphi0=TWOPIDT*(fp-DF15);
                    cdphi0=cos(dphi0);
                    sdphi0=sin(dphi0);
                    
                    dphi1=TWOPIDT*(fp-DF05);
                    cdphi1=cos(dphi1);
                    sdphi1=sin(dphi1);
                    
                    dphi2=TWOPIDT*(fp+DF05);
                    cdphi2=cos(dphi2);
                    sdphi2=sin(dphi2);
                    
                    dphi3=TWOPIDT*(fp+DF15);
                    cdphi3=cos(dphi3);
                    sdphi3=sin(dphi3);
                    
                    c0[0]=1; s0[0]=0;
                    c1[0]=1; s1[0]=0;
                    c2[0]=1; s2[0]=0;
                    c3[0]=1; s3[0]=0;
                    
                    for (j=1; j<SPS; j++) {
                        c0[j]=c0[j-1]*cdphi0 - s0[j-1]*sdphi0;
                        s0[j]=c0[j-1]*sdphi0 + s0[j-1]*cdphi0;
                        c1[j]=c1[j-1]*cdphi1 - s1[j-1]*sdphi1;
                        s1[j]=c1[j-1]*sdphi1 + s1[j-1]*cdphi1;
                        c2[j]=c2[j-1]*cdphi2 - s2[j-1]*sdphi2;
                        s2[j]=c2[j-1]*sdphi2 + s2[j-1]*cdphi2;
                        c3[j]=c3[j-1]*cdphi3 - s3[j-1]*sdphi3;
                        s3[j]=c3[j-1]*sdphi3 + s3[j-1]*cdphi3;
						
						if ((j%YIELD_EVERY_N_TIMES)==(YIELD_EVERY_N_TIMES-1)) WSPR_SHMEM_YIELD;
                    }
                    fplast = fp;
                }
                
                i0[i]=0.0; q0[i]=0.0;
                i1[i]=0.0; q1[i]=0.0;
                i2[i]=0.0; q2[i]=0.0;
                i3[i]=0.0; q3[i]=0.0;
                
                for (j=0; j<SPS; j++) {
                    k=lag+i*SPS+j;
                    wspr_array_dim(i, TPOINTS);
                    wspr_array_dim(j, SPS);
                    if( (k>0) && (k<np) ) {
                        wspr_array_dim(k, TPOINTS);
                        i0[i]=i0[i] + id[k]*c0[j] + qd[k]*s0[j];
                        q0[i]=q0[i] - id[k]*s0[j] + qd[k]*c0[j];
                        i1[i]=i1[i] + id[k]*c1[j] + qd[k]*s1[j];
                        q1[i]=q1[i] - id[k]*s1[j] + qd[k]*c1[j];
                        i2[i]=i2[i] + id[k]*c2[j] + qd[k]*s2[j];
                        q2[i]=q2[i] - id[k]*s2[j] + qd[k]*c2[j];
                        i3[i]=i3[i] + id[k]*c3[j] + qd[k]*s3[j];
                        q3[i]=q3[i] - id[k]*s3[j] + qd[k]*c3[j];
						
						if ((j%YIELD_EVERY_N_TIMES)==(YIELD_EVERY_N_TIMES-1)) WSPR_SHMEM_YIELD;
                    }
                }
                p0=i0[i]*i0[i] + q0[i]*q0[i];
                p1=i1[i]*i1[i] + q1[i]*q1[i];
                p2=i2[i]*i2[i] + q2[i]*q2[i];
                p3=i3[i]*i3[i] + q3[i]*q3[i];

                p0=sqrt(p0);
                p1=sqrt(p1);
                p2=sqrt(p2);
                p3=sqrt(p3);
                
                totp=totp+p0+p1+p2+p3;
                cmet=(p1+p3)-(p0+p2);
                ss = (pr3[i] == 1) ? ss+cmet : ss-cmet;
                if (mode == CALC_SOFT_SYMS) {                 //Compute soft symbols
                    if(pr3[i]==1) {
                        fsymb[i]=p3-p1;
                    } else {
                        fsymb[i]=p2-p0;
                    }
                }
            }
            ss=ss/totp;
            if( ss > syncmax ) {          //Save best parameters
                syncmax=ss;
                best_shift=lag;
                fbest=f0;
            }
        } // lag loop
    } //freq loop
    
    if (mode == FIND_BEST_TIME_LAG || mode == FIND_BEST_FREQ) {                       //Send best params back to caller
        *sync=syncmax;
        *shift1=best_shift;
        *f1=fbest;
        return;
    }
    
    if (mode == CALC_SOFT_SYMS) {
        *sync=syncmax;
        for (i=0; i<NSYM_162; i++) {              //Normalize the soft symbols
            fsum=fsum+fsymb[i]/FNSYM_162;
            f2sum=f2sum+fsymb[i]*fsymb[i]/FNSYM_162;
        }
        fac=sqrt(f2sum-fsum*fsum);
        WSPR_SHMEM_YIELD;

        for (i=0; i<NSYM_162; i++) {
            fsymb[i]=symfac*fsymb[i]/fac;
            if( fsymb[i] > 127) fsymb[i]=127.0;
            if( fsymb[i] < -128 ) fsymb[i]=-128.0;
            symbols[i] = fsymb[i] + 128;
        }
        WSPR_SHMEM_YIELD;
        return;
    }
    WSPR_SHMEM_YIELD;
    return;
}

void renormalize(wspr_t *w, float psavg[], float smspec[], float tmpsort[])
{
	int i,j,k;

	// smooth with 7-point window and limit the spectrum to +/-150 Hz
    int window[7] = {1,1,1,1,1,1,1};
    
    for (i=0; i<nbins_411; i++) {
        smspec[i] = 0.0;
        for (j=-3; j<=3; j++) {
            k = SPS-hbins_205+i+j;
            if (k < NFFT)
                smspec[i] += window[j+3]*psavg[k];
            WSPR_CHECK(else lprintf("WSPR WARNING! renormalize: k=%d i=%d j=%d\n", k, i, j);)
        }
    }
	WSPR_SHMEM_YIELD;

	// Sort spectrum values, then pick off noise level as a percentile
    for (j=0; j<nbins_411; j++)
        tmpsort[j] = smspec[j];
    qsort(tmpsort, nbins_411, sizeof(float), qsort_floatcomp);
	WSPR_SHMEM_YIELD;

	// Noise level of spectrum is estimated as 123/411= 30'th percentile
    float noise_level = tmpsort[122];
    
	/* Renormalize spectrum so that (large) peaks represent an estimate of snr.
	 * We know from experience that threshold snr is near -7dB in wspr bandwidth,
	 * corresponding to -7-26.3=-33.3dB in 2500 Hz bandwidth.
	 * The corresponding threshold is -42.3 dB in 2500 Hz bandwidth for WSPR-15.
	 */
	w->min_snr = pow(10.0,-7.0/10.0); //this is min snr in wspr bw
	w->snr_scaling_factor = (w->wspr_type == WSPR_TYPE_2MIN)? 26.3 : 35.3;
	for (j=0; j<411; j++) {
		smspec[j] = smspec[j]/noise_level - 1.0;
		if (smspec[j] < w->min_snr) smspec[j]=0.1*w->min_snr;
	}
	WSPR_SHMEM_YIELD;
}

/***************************************************************************
 symbol-by-symbol signal subtraction
 ****************************************************************************/
static void subtract_signal(float *id, float *qd, long np,
                     float f0, int shift0, float drift0, unsigned char* channel_symbols)
{
    int i, j, k;
    float fp;
    
    float i0,q0;
    float c0[SPS],s0[SPS];
    float dphi, cdphi, sdphi;
    
    for (i=0; i<NSYM_162; i++) {
        fp = f0 + ((float)drift0/2.0)*((float)i-FHSYM_81)/FHSYM_81;
        
        dphi=TWOPIDT*(fp+((float)channel_symbols[i]-1.5)*DF);
        cdphi=cos(dphi);
        sdphi=sin(dphi);
        
        c0[0]=1; s0[0]=0;
        
        for (j=1; j<SPS; j++) {
            c0[j]=c0[j-1]*cdphi - s0[j-1]*sdphi;
            s0[j]=c0[j-1]*sdphi + s0[j-1]*cdphi;
        }
        
        i0=0.0; q0=0.0;
        
        for (j=0; j<SPS; j++) {
            k=shift0+i*SPS+j;
            if( (k>0) & (k<np) ) {
                i0=i0 + id[k]*c0[j] + qd[k]*s0[j];
                q0=q0 - id[k]*s0[j] + qd[k]*c0[j];
            }
        }
        
        
        // subtract the signal here.
        
        i0=i0/FSPS; //will be wrong for partial symbols at the edges...
        q0=q0/FSPS;
        
        for (j=0; j<SPS; j++) {
            k=shift0+i*SPS+j;
            if( (k>0) & (k<np) ) {
                id[k]=id[k]- (i0*c0[j] - q0*s0[j]);
                qd[k]=qd[k]- (q0*c0[j] + i0*s0[j]);
            }
        }
    }
    return;
}

/******************************************************************************
 Fully coherent signal subtraction
 *******************************************************************************/
static void subtract_signal2(float *id, float *qd, long np,
                      float f0, int shift0, float drift0, unsigned char* channel_symbols)
{
    float phi=0, dphi, cs;
    int i, j, k, ii, nsym=NSYM_162, nspersym=SPS,  nfilt=SPS; //nfilt must be even number.
    int nsig=nsym*nspersym;
    int nc2=TPOINTS;
    
    float *refi, *refq, *ci, *cq, *cfi, *cfq;

    refi = (float *) kiwi_imalloc("subtract_signal2", sizeof(float)*nc2);
    refq = (float *) kiwi_imalloc("subtract_signal2", sizeof(float)*nc2);
    ci = (float *) kiwi_imalloc("subtract_signal2", sizeof(float)*nc2);
    cq = (float *) kiwi_imalloc("subtract_signal2", sizeof(float)*nc2);
    cfi = (float *) kiwi_imalloc("subtract_signal2", sizeof(float)*nc2);
    cfq = (float *) kiwi_imalloc("subtract_signal2", sizeof(float)*nc2);
    
    memset(refi,0,sizeof(float)*nc2);
    memset(refq,0,sizeof(float)*nc2);
    memset(ci,0,sizeof(float)*nc2);
    memset(cq,0,sizeof(float)*nc2);
    memset(cfi,0,sizeof(float)*nc2);
    memset(cfq,0,sizeof(float)*nc2);
    
    /******************************************************************************
     Measured signal:                    s(t)=a(t)*exp( j*theta(t) )
     Reference is:                       r(t) = exp( j*phi(t) )
     Complex amplitude is estimated as:  c(t)=LPF[s(t)*conjugate(r(t))]
     so c(t) has phase angle theta-phi
     Multiply r(t) by c(t) and subtract from s(t), i.e. s'(t)=s(t)-c(t)r(t)
     *******************************************************************************/
    
    // create reference wspr signal vector, centered on f0.
    //
    for (i=0; i<nsym; i++) {
        
        cs=(float)channel_symbols[i];
        
        dphi=TWOPIDT*
        (
         f0 + (drift0/2.0)*((float)i-(float)nsym/2.0)/((float)nsym/2.0)
         + (cs-1.5)*DF
         );
        
        for ( j=0; j<nspersym; j++ ) {
            ii=nspersym*i+j;
            refi[ii]=cos(phi); //cannot precompute sin/cos because dphi is changing
            refq[ii]=sin(phi);
            phi=phi+dphi;
        }
    }
    
    // s(t) * conjugate(r(t))
    // beginning of first symbol in reference signal is at i=0
    // beginning of first symbol in received data is at shift0.
    // filter transient lasts nfilt samples
    // leave nfilt zeros as a pad at the beginning of the unfiltered reference signal
    for (i=0; i<nsym*nspersym; i++) {
        k=shift0+i;
        if( (k>0) && (k<np) ) {
            ci[i+nfilt] = id[k]*refi[i] + qd[k]*refq[i];
            cq[i+nfilt] = qd[k]*refi[i] - id[k]*refq[i];
        }
    }
    
    //lowpass filter and remove startup transient
    float w[nfilt], norm=0, partialsum[nfilt];
    memset(partialsum,0,sizeof(float)*nfilt);
    for (i=0; i<nfilt; i++) {
        w[i]=sin(K_PI*(float)i/(float)(nfilt-1));
        norm=norm+w[i];
    }
    for (i=0; i<nfilt; i++) {
        w[i]=w[i]/norm;
    }
    for (i=1; i<nfilt; i++) {
        partialsum[i]=partialsum[i-1]+w[i];
    }
    
    // LPF
    for (i=nfilt/2; i<TPOINTS-nfilt/2; i++) {
        cfi[i]=0.0; cfq[i]=0.0;
        for (j=0; j<nfilt; j++) {
            cfi[i]=cfi[i]+w[j]*ci[i-nfilt/2+j];
            cfq[i]=cfq[i]+w[j]*cq[i-nfilt/2+j];
        }
    }
    
    // subtract c(t)*r(t) here
    // (ci+j*cq)(refi+j*refq)=(ci*refi-cq*refq)+j(ci*refq)+cq*refi)
    // beginning of first symbol in reference signal is at i=nfilt
    // beginning of first symbol in received data is at shift0.
    for (i=0; i<nsig; i++) {
        if( i<nfilt/2 ) {        // take care of the end effect (LPF step response) here
            norm=partialsum[nfilt/2+i];
        } else if( i>(nsig-1-nfilt/2) ) {
            norm=partialsum[nfilt/2+nsig-1-i];
        } else {
            norm=1.0;
        }
        k=shift0+i;
        j=i+nfilt;
        if( (k>0) && (k<np) ) {
            id[k]=id[k] - (cfi[j]*refi[i]-cfq[j]*refq[i])/norm;
            qd[k]=qd[k] - (cfi[j]*refq[i]+cfq[j]*refi[i])/norm;
        }
    }
    
    kiwi_ifree(refi, "subtract_signal2");
    kiwi_ifree(refq, "subtract_signal2");
    kiwi_ifree(ci, "subtract_signal2");
    kiwi_ifree(cq, "subtract_signal2");
    kiwi_ifree(cfi, "subtract_signal2");
    kiwi_ifree(cfq, "subtract_signal2");

    return;
}

#include "./metric_tables.h"
static int mettab[2][256];

void wspr_init()
{
    float bias = 0.45;						//Fano metric bias (used for both Fano and stack algorithms)

    // setup metric table
    for (int i=0; i < SPS; i++) {
        mettab[0][i] = round(10 * (metric_tables[2][i] - bias));
        mettab[1][i] = round(10 * (metric_tables[2][SPS-1-i] - bias));
    }
    
    wspr_update_vars_from_config(false);
}

#ifdef WSPR_SHMEM_DISABLE
    #define send_peaks_all(w, npk) w->npk = npk; if (!w->autorun) wspr_send_peaks(w, 0, npk);
    #define send_peak_single(w, pki) if (!w->autorun) wspr_send_peaks(w, pki, pki+1);
    #define send_decode(w, seq) wspr_send_decode(w, seq-1);
#else
    #define send_peaks_all(w, npk) \
        if (!w->autorun) { \
            if (w->send_peaks_seq < MAX_NPKQ) { \
                w->npk = npk; \
                wb->send_peaks_q[w->send_peaks_seq].start = 0; \
                wb->send_peaks_q[w->send_peaks_seq].stop = npk; \
                w->send_peaks_seq++; \
                /* printf("WSPR ALL send_peaks_seq=%d\n", w->send_peaks_seq); */ \
            } else { \
                lprintf("WSPR WARNING! send_peaks_seq(%d) >= MAX_NPKQ(%d)\n", w->send_peaks_seq, MAX_NPKQ); \
            } \
        }
    #define send_peak_single(w, pki) \
        if (!w->autorun) { \
            if (w->send_peaks_seq < MAX_NPKQ) { \
                wb->send_peaks_q[w->send_peaks_seq].start = pki; \
                wb->send_peaks_q[w->send_peaks_seq].stop = pki+1; \
                w->send_peaks_seq++; \
                /* printf("WSPR SINGLE send_peaks_seq=%d\n", w->send_peaks_seq); */ \
            } else { \
                lprintf("WSPR WARNING! send_peaks_seq(%d) >= MAX_NPKQ(%d)\n", w->send_peaks_seq, MAX_NPKQ); \
            } \
        }
    #define send_decode(w, seq) w->send_decode_seq = seq
#endif

void wspr_decode(int rx_chan)
{
    char cr[] = "(C) 2016, Steven Franke - K9AN";
    (void) cr;

    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    wspr_buf_t *wb = w->buf;
    int i,j,k;

    int ipass, npasses = 1;
    int shift1, lagmin, lagmax, lagstep, ifmin, ifmax;
    unsigned int metric, cycles, maxnp;

    float DF2 = FSRATE/FSPS/2;
    float dt_print;

	int pki, npk=0;

    double freq_print;
    float f1, fstep, sync1, drift1, snr;

    int ndecodes_pass;
    
    //jksd FIXME need way to select:
    //	more_candidates
    //	subtraction
    
    // Parameters used for performance-tuning:
    unsigned int maxcycles=200;				//Decoder timeout limit
    float minsync1=0.10;					//First sync limit
    float minsync2=0.12;					//Second sync limit
    int iifac=2;							//Step size in final DT peakup
    int jig_range=128;
    int symfac=50;							//Soft-symbol normalizing factor
    int maxdrift=4;							//Maximum (+/-) drift
    float minrms=52.0 * (symfac/64.0);		//Final test for plausible decoding
    int delta=60;							//Fano threshold step
    
	wspr_aprintf("DECO-C START decode_ping_pong=%d\n", w->decode_ping_pong);

    wspr_array_dim(w->decode_ping_pong, N_PING_PONG);
	WSPR_CPX_t *idat = wb->i_data[w->decode_ping_pong];
	WSPR_CPX_t *qdat = wb->q_data[w->decode_ping_pong];

    if (!w->init) {
    	w->init = true;
    }

	w->uniques = 0;
	
	// multi-pass strategies
	//#define SUBTRACT_SIGNAL		// FIXME: how to implement spectrum subtraction given our incrementally-computed FFTs?
	#define MORE_EFFORT			// this scheme repeats work as maxcycles is increased, but it's difficult to eliminate that
		
	#if defined(SUBTRACT_SIGNAL)
		npasses = 2;
	#elif defined(MORE_EFFORT)
		npasses = 0;	// unlimited
	#endif

    for (ipass=0; (npasses == 0 || ipass < npasses) && !w->abort_decode; ipass++) {

		#if defined(SUBTRACT_SIGNAL)
        	if (ipass > 0 && ndecodes_pass == 0) break;
        #elif defined(MORE_EFFORT)
        	if (ipass == 0) {
        		maxcycles = 200;
        		iifac = 2;
        	} else {
        		if (maxcycles < 10000) maxcycles *=
        		    //2;  // 200  400  800 1600  3200 6400 12800 repeating 50% of the work each time
        		    //4;  // 200  800 3200 6400 12800            repeating 25% of the work each time
        		      5;  // 200 1000 5000 10000                 repeating 20% of the work each time
        		iifac = 1;
        	}
        #endif
        
		// only build the peak list on the first pass
		if (ipass == 0) {
            wspr_array_dim(w->decode_ping_pong, N_PING_PONG);
			renormalize(w, wb->pwr_sampavg[w->decode_ping_pong], wb->smspec[WSPR_RENORM_DECODE], wb->tmpsort[WSPR_RENORM_DECODE]);
			
			// Find all local maxima in smoothed spectrum.
			memset(wb->pk_snr, 0, sizeof(wb->pk_snr));
			
			npk = 0;
			bool candidate;
			
			if (w->more_candidates) {
				for (j=0; j<nbins_411; j=j+2) {
					candidate = (wb->smspec[WSPR_RENORM_DECODE][j] > w->min_snr) && (npk < NPK);
					if (candidate) {
						wb->pk_snr[npk].bin0 = j;
						wb->pk_snr[npk].freq0 = (j-hbins_205)*DF2;
						wb->pk_snr[npk].snr0 = 10*log10(wb->smspec[WSPR_RENORM_DECODE][j]) - w->snr_scaling_factor;
						npk++;
					}
				}
			} else {
				for (j=1; j<(nbins_411-1); j++) {
					candidate = (wb->smspec[WSPR_RENORM_DECODE][j] > wb->smspec[WSPR_RENORM_DECODE][j-1]) &&
								(wb->smspec[WSPR_RENORM_DECODE][j] > wb->smspec[WSPR_RENORM_DECODE][j+1]) &&
								(npk < NPK);
					if (candidate) {
						wb->pk_snr[npk].bin0 = j;
						wb->pk_snr[npk].freq0 = (j-hbins_205)*DF2;
						wb->pk_snr[npk].snr0 = 10*log10(wb->smspec[WSPR_RENORM_DECODE][j]) - w->snr_scaling_factor;
						npk++;
					}
				}
			}
			wspr_d1printf("initial npk %d/%d\n", npk, NPK);
			WSPR_SHMEM_YIELD;
	
			// Don't waste time on signals outside of the range [fmin,fmax].
			i=0;
			for (pki=0; pki < npk; pki++) {
				if (wb->pk_snr[pki].freq0 >= FMIN && wb->pk_snr[pki].freq0 <= FMAX) {
					wb->pk_snr[i].ignore = false;
					wb->pk_snr[i].bin0 = wb->pk_snr[pki].bin0;
					wb->pk_snr[i].freq0 = wb->pk_snr[pki].freq0;
					wb->pk_snr[i].snr0 = wb->pk_snr[pki].snr0;
					i++;
				}
			}
			npk = i;
			wspr_d1printf("freq range limited npk %d\n", npk);
			WSPR_SHMEM_YIELD;
			
			// only look at a limited number of strong peaks
			qsort(wb->pk_snr, npk, sizeof(wspr_pk_t), snr_comp);		// sort in decreasing snr order
			if (npk > MAX_NPK) npk = MAX_NPK;
			wspr_d1printf("MAX_NPK limited npk %d/%d\n", npk, MAX_NPK);
		
			// remember our frequency-sorted index
			qsort(wb->pk_snr, npk, sizeof(wspr_pk_t), freq_comp);
			for (pki=0; pki < npk; pki++) {
				wb->pk_snr[pki].freq_idx = pki;
			}
		
			// send peak info to client in increasing frequency order
			memcpy(w->pk_freq, wb->pk_snr, sizeof(wspr_pk_t) * npk);
		
			for (pki=0; pki < npk; pki++) {
                kiwi_snprintf_buf(w->pk_freq[pki].snr_call, "%d", (int) roundf(w->pk_freq[pki].snr0));
			}
		
			send_peaks_all(w, npk);
			
			// keep 'wb->pk_snr' in snr order so strong sigs are processed first in case we run out of decode time
			qsort(wb->pk_snr, npk, sizeof(wspr_pk_t), snr_comp);		// sort in decreasing snr order
		} // ipass == 0
        
        int valid_peaks = 0;
        for (pki=0; pki < npk; pki++) {
        	if (wb->pk_snr[pki].ignore) continue;
        	valid_peaks++;
        }
        wspr_d1printf("PASS %d npeaks=%d valid_peaks=%d maxcycles=%d jig_range=%+d..%d jig_step=%d ---------------------------------------------\n",
        	ipass+1, npk, valid_peaks, maxcycles, jig_range/2, -jig_range/2, iifac);

        /* Make coarse estimates of shift (DT), freq, and drift
         
         * Look for time offsets up to +/- 8 symbols (about +/- 5.4 s) relative
         to nominal start time, which is 2 seconds into the file
         
         * Calculates shift relative to the beginning of the file
         
         * Negative shifts mean that signal started before start of file
         
         * The program prints DT = shift-2 s
         
         * Shifts that cause sync vector to fall off of either end of the data
         vector are accommodated by "partial decoding", such that missing
         symbols produce a soft-decision symbol value of 128
         
         * The frequency drift model is linear, deviation of +/- drift/2 over the
         span of 162 symbols, with deviation equal to 0 at the center of the
         signal vector.
         */

        int idrift,ifr,if0,ifd,k0;
        int kindex;
        float smax,ss,power,p0,p1,p2,p3;

        for (pki=0; pki < npk; pki++) {			//For each candidate...
        	wspr_pk_t *p = &wb->pk_snr[pki];
            smax = -1e30;
            if0 = p->freq0/DF2+SPS;

			#if defined(MORE_EFFORT)
				if (ipass != 0 && p->ignore)
					continue;
			#endif
			
            for (ifr=if0-2; ifr<=if0+2; ifr++) {                      //Freq search
                for( k0=-10; k0<22; k0++) {                             //Time search
                    for (idrift=-maxdrift; idrift<=maxdrift; idrift++) {  //Drift search
                        ss=0.0;
                        power=0.0;
                        for (k=0; k<NSYM_162; k++) {				//Sum over symbols
                            ifd=ifr+((float)k-FHSYM_81)/FHSYM_81*( (float)idrift )/(2.0*DF2);
                            kindex=k0+2*k;
                            if( kindex >= 0 && kindex < nffts ) {
                                wspr_array_dim(w->decode_ping_pong, N_PING_PONG);
                                wspr_array_dim(ifd-3, NFFT);
                                wspr_array_dim(ifd+3, NFFT);
                                wspr_array_dim(kindex, FPG*GROUPS);
								p0=wb->pwr_samp[w->decode_ping_pong][ifd-3][kindex];
								p1=wb->pwr_samp[w->decode_ping_pong][ifd-1][kindex];
								p2=wb->pwr_samp[w->decode_ping_pong][ifd+1][kindex];
								p3=wb->pwr_samp[w->decode_ping_pong][ifd+3][kindex];
                                
                                p0=sqrt(p0);
                                p1=sqrt(p1);
                                p2=sqrt(p2);
                                p3=sqrt(p3);
                                
                                ss=ss+(2*pr3[k]-1)*((p1+p3)-(p0+p2));
                                power=power+p0+p1+p2+p3;
                            }
                        }
                        sync1=ss/power;
                        if( sync1 > smax ) {                  //Save coarse parameters
                            smax=sync1;
							p->shift0=HSPS*(k0+1);
							p->drift0=idrift;
							p->freq0=(ifr-SPS)*DF2;
							p->sync0=sync1;
                        }
						//wspr_d1printf("drift %d  k0 %d  sync %f\n", idrift, k0, smax);
                    }
					WSPR_SHMEM_YIELD;
                }
            }
			wspr_d1printf("npeak     #%02ld %6.1f snr  %9.6f (%7.2f) freq  %4.1f drift  %5d shift  %6.3f sync  %3d bin\n",
				pki, p->snr0, w->dialfreq_MHz+(w->bfo+p->freq0)/1e6, w->cf_offset+p->freq0, p->drift0, p->shift0, p->sync0, p->bin0);
        }

        /*
         Refine the estimates of freq, shift using sync as a metric.
         Sync is calculated such that it is a float taking values in the range
         [0.0,1.0].
         
         Function sync_and_demodulate has three modes of operation:
            no frequency or drift search. find best time lag.
            no time lag or drift search. find best frequency.
            no frequency or time lag search. Calculate soft-decision
                symbols using passed frequency and shift.
         
         NB: best possibility for OpenMP may be here: several worker threads
         could each work on one candidate at a time.
         */
		int candidates = 0;
        ndecodes_pass = 0;
		
        for (pki=0; pki < npk && !w->abort_decode; pki++) {
        	bool f_decoded = false, f_delete = false, f_image = false, f_decoding = false;

        	wspr_pk_t *p = &wb->pk_snr[pki];
			u4_t decode_start = timer_ms();

			#if defined(MORE_EFFORT)
				if (ipass != 0 && p->ignore)
					continue;
			#endif
			
			candidates++;
            f1 = p->freq0;
            snr = p->snr0;
            drift1 = p->drift0;
            shift1 = p->shift0;
            sync1 = p->sync0;

			f_decoding = true;
			w->pk_freq[p->freq_idx].flags |= WSPR_F_DECODING;
			send_peak_single(w, p->freq_idx);
	
			wspr_d1printf("start     #%02ld %6.1f snr  %9.6f (%7.2f) freq  %4.1f drift  %5d shift  %6.3f sync\n",
				pki, snr, w->dialfreq_MHz+(w->bfo+f1)/1e6, w->cf_offset+f1, drift1, shift1, sync1);

            // coarse-grid lag and freq search, then if sync > minsync1 continue
            fstep=0.0; ifmin=0; ifmax=0;
            lagmin = shift1-128;
            lagmax = shift1+128;
            lagstep = 64;
            sync_and_demodulate(w, idat, qdat, TPOINTS, w->symbols, &f1, ifmin, ifmax, fstep, &shift1,
                                lagmin, lagmax, lagstep, drift1, symfac, &sync1, FIND_BEST_TIME_LAG);

            fstep = 0.25; ifmin = -2; ifmax = 2;
            sync_and_demodulate(w, idat, qdat, TPOINTS, w->symbols, &f1, ifmin, ifmax, fstep, &shift1,
                                lagmin, lagmax, lagstep, drift1, symfac, &sync1, FIND_BEST_FREQ);

            // refine drift estimate
            fstep=0.0; ifmin=0; ifmax=0;
            float driftp,driftm,syncp,syncm;
            driftp = drift1+0.5;
            sync_and_demodulate(w, idat, qdat, TPOINTS, w->symbols, &f1, ifmin, ifmax, fstep, &shift1,
                                lagmin, lagmax, lagstep, driftp, symfac, &syncp, FIND_BEST_FREQ);
            
            driftm = drift1-0.5;
            sync_and_demodulate(w, idat, qdat, TPOINTS, w->symbols, &f1, ifmin, ifmax, fstep, &shift1,
                                lagmin, lagmax, lagstep, driftm, symfac, &syncm, FIND_BEST_FREQ);
            
            if (syncp > sync1) {
                drift1 = driftp;
                sync1 = syncp;
            } else
            
            if (syncm > sync1) {
                drift1 = driftm;
                sync1 = syncm;
            }

			wspr_d1printf("coarse    #%02ld %6.1f snr  %9.6f (%7.2f) freq  %4.1f drift  %5d shift  %6.3f sync\n",
				pki, snr, w->dialfreq_MHz+(w->bfo+f1)/1e6, w->cf_offset+f1, drift1, shift1, sync1);

            // fine-grid lag and freq search
			bool r_minsync1 = (sync1 > minsync1);

            if (r_minsync1) {
                lagmin = shift1-32; lagmax = shift1+32; lagstep = 16;
                sync_and_demodulate(w, idat, qdat, TPOINTS, w->symbols, &f1, ifmin, ifmax, fstep, &shift1,
                                    lagmin, lagmax, lagstep, drift1, symfac, &sync1, FIND_BEST_TIME_LAG);
            
                // fine search over frequency
                fstep = 0.05; ifmin = -2; ifmax = 2;
                sync_and_demodulate(w, idat, qdat, TPOINTS, w->symbols, &f1, ifmin, ifmax, fstep, &shift1,
                                lagmin, lagmax, lagstep, drift1, symfac, &sync1, FIND_BEST_FREQ);
            } else {
            	p->ignore = true;
				wspr_d1printf("MINSYNC1  #%02ld\n", pki);
				f_delete = true;
            }
            
            int idt=0, ii=0, jiggered_shift;
            float y, sq, rms;
        	int r_decoded = 0;
        	bool r_tooWeak = true;
            
            // ii: 0 +1 -1 +2 -2 +3 -3 ... (*iifac)
            // ii always covers jig_range, stepped by iifac resolution
            while (!w->abort_decode && r_minsync1 && !r_decoded && idt <= (jig_range/iifac)) {
                ii = (idt+1)/2;
                if ((idt&1) == 1) ii = -ii;
                ii = iifac*ii;
                jiggered_shift = shift1+ii;
                
                // Use mode 2 to get soft-decision symbols
                sync_and_demodulate(w, idat, qdat, TPOINTS, w->symbols, &f1, ifmin, ifmax, fstep,
                                    &jiggered_shift, lagmin, lagmax, lagstep, drift1, symfac,
                                    &sync1, CALC_SOFT_SYMS);

                sq = 0.0;
                for (i=0; i<NSYM_162; i++) {
                    y = (float) w->symbols[i] - 128.0;
                    sq += y*y;
                }
                rms = sqrt(sq/FNSYM_162);

				bool weak = true;
                if ((sync1 > minsync2) && (rms > minrms)) {
                    deinterleave(w->symbols);
                    
                    wspr_d2printf("decoder   #%02ld maxcycles %5d %s\n", pki, maxcycles, w->stack_decoder? "Jelinek" : "Fano");
                    if (w->stack_decoder) {
                        if (!w->stack)
			                w->stack = (struct snode *) malloc(WSPR_STACKSIZE * sizeof(struct snode));
                        r_decoded = jelinek(&metric, &cycles, w->decdata, w->symbols, NBITS,
											WSPR_STACKSIZE, w->stack, mettab, maxcycles);
                    } else {
                        r_decoded = fano(&metric, &cycles, &maxnp, w->decdata, w->symbols, NBITS,
										mettab, delta, maxcycles);
                    }

                    r_tooWeak = weak = false;
                }
                
				wspr_d2printf("jig <>%3d #%02ld %6.1f snr  %9.6f (%7.2f) freq  %4.1f drift  %5d(%+4d) shift  %6.3f sync  %4.1f rms",
					idt, pki, snr, w->dialfreq_MHz+(w->bfo+f1)/1e6, w->cf_offset+f1, drift1, jiggered_shift, ii, sync1, rms);
				if (!weak) {
					wspr_d2printf("  %4ld metric  %3ld cycles\n", metric, cycles);
				} else {
					if (sync1 <= minsync2) wspr_d2printf("  SYNC-WEAK");
					if (rms <= minrms) wspr_d2printf("  RMS-WEAK");
					wspr_d2printf("\n");
				}
				
                idt++;
                if (w->quickmode) break;
            }
            
            bool r_timeUp = (!w->abort_decode && r_minsync1 && !r_decoded && idt > (jig_range/iifac));
            int r_valid = 0;
            
            //if (r_timeUp && r_tooWeak && iifac == 1) {
            if (r_timeUp && r_tooWeak) {
				wspr_d1printf("NO CHANGE #%02ld\n", pki);
				p->ignore = true;	// situation not going to get any better
				f_delete = true;
			}
			
			// result priority: abort, minSync1, tooWeak, noChange, timeUp, image, decoded
            
            if (r_decoded) {
                ndecodes_pass++;
                p->ignore = true;
                
                // Unpack the decoded message, update the hashtable, apply
                // sanity checks on grid and power, and return
                // call_loc_pow string and also callsign (for de-duping).
                r_valid = unpk_(w->decdata, w->call_loc_pow, w->callsign, w->grid, &w->dBm);

                // subtract even on last pass
                #ifdef SUBTRACT_SIGNAL
					if (w->subtraction && (ipass < npasses) && r_valid > 0) {
						if (get_wspr_channel_symbols(w->call_loc_pow, w->channel_symbols)) {
							subtract_signal2(idat, qdat, TPOINTS, f1, shift1, drift1, w->channel_symbols);
						} else {
							break;
						}
						
					}
                #endif

                // Remove dupes (same callsign and freq within 3 Hz) and images
                bool r_dupe = false;
                if (r_valid > 0) {
                	bool hash = (strcmp(w->callsign, "...") == 0);
                	for (i=0; i < w->uniques; i++) {
                		wspr_decode_t *dp = &w->deco[i];
						bool match = (strcmp(w->callsign, dp->call) == 0);
						bool close = (fabs(f1 - dp->freq) < 3.0);
						if ((match && close) || (match && !hash)) {
							if (close) {
								f_delete = true;
							} else {
								f_image = true;
							}
							wspr_d1printf("%s     #%02ld  with #%02ld %s, %.3f secs\n", f_image? "IMAGE" : "DUPE ",
								pki, i, dp->call, (float)(timer_ms()-decode_start)/1e3);
							r_dupe = true;
							break;
						}
					}
				}

				if (r_valid <= 0 && !r_dupe) {
					if (r_valid < 0) {
						wspr_d1printf("UNPK ERR! #%02ld  error code %d, %.3f secs\n",
							pki, r_valid, (float)(timer_ms()-decode_start)/1e3);
					} else {
						wspr_d1printf("NOT VALID #%02ld  %.3f secs\n",
							pki, (float)(timer_ms()-decode_start)/1e3);
					}
					f_delete = true;
				}

                if (r_valid > 0 && !r_dupe && w->uniques < NDECO) {
                	f_decoded = true;

                    if (w->wspr_type == WSPR_TYPE_15MIN) {
                        freq_print = w->dialfreq_MHz + (w->bfo+112.5+f1/8.0)/1e6;
                        dt_print = shift1*8*DT-1.0;
                    } else {
                        freq_print = w->dialfreq_MHz + (w->bfo+f1)/1e6;
                        dt_print = shift1*DT-1.0;
                    }

					int hour, min; time_hour_min_sec(w->utc[w->decode_ping_pong], &hour, &min);
            
					wspr_d1printf("TYPE%d %02d%02d %3.0f %4.1f %10.6f %2d %-s %4s %2d [%s] in %.3f secs --------------------------------------------------------------------\n",
					   r_valid, hour, min, snr, dt_print, freq_print, (int) drift1,
					   w->callsign, w->grid, w->dBm, w->call_loc_pow, (float)(timer_ms()-decode_start)/1e3);
					
					wspr_decode_t *dp = &w->deco[w->uniques];
					dp->r_valid = r_valid;
                    strcpy(dp->call, w->callsign);
                    strcpy(dp->grid, w->grid);
                    dp->dBm = w->dBm;
                    kiwi_snprintf_buf(dp->pwr, "%d", w->dBm);
                    dp->freq = f1;
                    dp->hour = hour;
                    dp->min = min;
                    dp->snr = snr;
                    dp->dt_print = dt_print;
                    dp->freq_print = freq_print;
                    dp->drift1 = drift1;
                    strcpy(dp->c_l_p, w->call_loc_pow);
                    w->uniques++;

					send_decode(w, w->uniques);
                }
			} else {
				if (r_timeUp) {
					wspr_d1printf("TIME UP   #%02ld %.3f secs\n", pki, (float)(timer_ms()-decode_start)/1e3);
					#if defined(MORE_EFFORT)
						f_decoding = false;
					#else
						f_delete = true;
					#endif
				}
            }	// decoded

			if (f_decoded) {
				strcpy(w->pk_freq[p->freq_idx].snr_call, w->callsign);
				w->pk_freq[p->freq_idx].flags |= WSPR_F_DECODED;
			} else
			if (f_delete) {
				w->pk_freq[p->freq_idx].flags |= WSPR_F_DELETE;
			} else
			if (f_image) {
				strcpy(w->pk_freq[p->freq_idx].snr_call, "image");
				w->pk_freq[p->freq_idx].flags |= WSPR_F_IMAGE;
			} else
			if (!f_decoding) {
				w->pk_freq[p->freq_idx].flags &= ~WSPR_F_DECODING;
			}
			
			send_peak_single(w, p->freq_idx);
        }	// peak list
        
		if (candidates == 0)
			break;		// nothing left to do
			
    }	// passes

	// when finished delete any unresolved peaks
	for (i=0; i < npk; i++) {
		wspr_pk_t *p = &wb->pk_snr[i];
		if (!(w->pk_freq[p->freq_idx].flags & WSPR_F_DECODED))
			w->pk_freq[p->freq_idx].flags |= WSPR_F_DELETE;
	}
	send_peaks_all(w, npk);

	wspr_aprintf("DECO-C STOP npk=%d\n", npk);
}
