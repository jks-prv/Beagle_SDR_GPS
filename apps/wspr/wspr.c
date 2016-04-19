/*
 * k9an-wspr is a detector/demodulator/decoder for K1JT's 
 * Weak Signal Propagation Reporter (WSPR) mode.
 *
 * Copyright 2014, Steven Franke, K9AN
*/

#ifdef APP_WSPR

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include "fano.h"
#include "wspr.h"

#define NTASK 64
#define	NT() NextTask("wspr")

static void sync_and_demodulate(
	CPX_t *id,
	CPX_t *qd,
	long np,
	unsigned char *symbols,
	float *f1,
	float fstep,
	int *shift1,
	int lagmin, int lagmax, int lagstep,
	float *drift1,
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
					fp = f0 + (*drift1/2.0)*(i-HSYM_81)/FHSYM_81;
					
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
            
            fp=f0+(*drift1/2.0)*(i-FHSYM_81)/FHSYM_81;
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

static int verbose=0, quickmode=0, medium_effort=0, decimate, decim=0, didx=0, group=0;
static float dialfreq=0.0;
static float freqmin=-110, freqmax=110;
static int WSPR_FFTtask_id, WSPR_DecodeTask_id;

static int ping_pong, fft_ping_pong, decode_ping_pong;
static CPX_t i_data[2][TPOINTS], q_data[2][TPOINTS];
static time_t utc[2];

static conn_t *wspr_wf;
static bool tsync = FALSE;

void wspr_data(int run, u4_t freq, int nsamps, TYPECPX *samps)
{
	int i;
	time_t t;
	
	if (!run) {
		ping_pong = decim = didx = group = 0;
		tsync = FALSE;
		return;
	}
	
	dialfreq = ((float) freq) / MHz;

	time(&t); struct tm tm; gmtime_r(&t, &tm);
	if ((tsync == FALSE) && !(tm.tm_min&1) && (tm.tm_sec == 0) && (run != 2)) {
		printf("WSPR SYNC %s", ctime(&t));
		ping_pong ^= 1;
		didx = group = 0;
		send_meta(wspr_wf, META_WSPR_SYNC, 0, 0);
		tsync = TRUE;
	}
	
	if (group == 0) utc[ping_pong] = t;
	
	// decimate
	CPX_t *idat = i_data[ping_pong], *qdat = q_data[ping_pong];
	//double scale = 1000.0;
	double scale = 1.0;

    for (i=0; i<nsamps; i++) {

		#if WSPR_NSAMPS == 45000
			if ((run != 2) && (decim++ < (decimate-1))) continue;	// don't decimate in demo mode
		#else
			if (decim++ < (decimate-1)) continue;
		#endif

		decim = 0;
    	if (didx >= TPOINTS) {
			#if WSPR_NSAMPS == 45000
				ping_pong ^= 1;
				didx = group = 0;
			#endif
    		return;
    	}
    	if (group == 3) tsync = FALSE;

		CPX_t re = (CPX_t) samps[i].re/scale;
		CPX_t im = (CPX_t) samps[i].im/scale;
		idat[didx] = re;
		qdat[didx] = im;

		if ((didx % NFFT) == (NFFT-1)) {
			fft_ping_pong = ping_pong;
			if (group) TaskWakeup(WSPR_FFTtask_id, TRUE, group-1);	// skip first to pipeline
			group++;
		}
		didx++;
    }
}

static FFTW_COMPLEX *fftin, *fftout;
static FFTW_PLAN fftplan;
static float pwr_samp[2][NFFT][FPG*GROUPS];
static float pwr_sampavg[2][NFFT];
static int nffts = FPG*floor(GROUPS-1) - 1;
static int nbins_411 = ceilf(NFFT*BW_MAX/FSRATE)+1;
static int hbins_205 = (nbins_411-1)/2;

#define NPK 256
#define MAX_NPK 8
static unsigned int nbits = HSYM_81;
static u1_t *symbols, *decdata;
static char *callsign,*grid;
static float allfreqs[NPK];
static char allcalls[NPK][7];

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
    qsort(tmpsort, nbins_411, sizeof(float), floatcomp);
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

void WSPR_FFTtask()
{
	int i,j,k;
	
	while (1) {
	
	//printf("WSPR_FFTtask sleep..\n");
	int grp = TaskSleep(0);
	int first = grp*FPG, last = first+FPG;

// Do ffts over 2 symbols, stepped by half symbols
	CPX_t *id = i_data[fft_ping_pong], *qd = q_data[fft_ping_pong];

	float maxiq = 1e-66, maxpwr = 1e-66;
	int maxi=0;
	float savg[NFFT];
	memset(savg, 0, sizeof(savg));
	
    for (i=first; i<last; i++) {
        for (j=0; j<NFFT; j++ ){
            k = i*HSPS+j;
            fftin[j][0] = id[k];
            fftin[j][1] = qd[k];
            //if (i==0) printf("IN %d %fi %fq\n", j, fftin[j][0], fftin[j][1]);
        }
        
		//u4_t start = timer_us();
		NT();
		FFTW_EXECUTE(fftplan);
		NT();
		//if (i==0) printf("512 FFT %.1f us\n", (float)(timer_us()-start));
		
        for (j=0; j<NFFT; j++) {
            k = j+SPS;
            if (k > (NFFT-1))
                k = k-NFFT;
            //if (i==0) printf("OUT %d %fi %fq\n", j, fftout[k][0], fftout[k][1]);
            float pwr = fftout[k][0]*fftout[k][0] + fftout[k][1]*fftout[k][1];
            if (fftout[k][0] > maxiq) { maxiq = fftout[k][0]; }
            if (pwr > maxpwr) { maxpwr = pwr; maxi = k; }
            pwr_samp[fft_ping_pong][j][i] = pwr;
            pwr_sampavg[fft_ping_pong][j] += pwr;
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
	ws[0] = ((grp == 0) && tsync)? 1:0;
	send_meta_bytes(wspr_wf, META_WSPR_DATA, ws, nbins_411+1);

	//printf("WSPR_FFTtask group %d:%d %d-%d(%d) %f %f(%d)\n",
	//	fft_ping_pong, grp, first, last, nffts, maxiq, maxpwr, maxi);
	printf("  %d-%d(%d)\r", first, last, nffts); fflush(stdout);
    
    if (last < nffts) continue;

    decode_ping_pong = fft_ping_pong;
    //printf("---DECODE -> %d\n", decode_ping_pong);
    printf("\n");
    TaskWakeup(WSPR_DecodeTask_id, TRUE, 0);
    
    }
}

typedef struct {
	float freq0, snr0, drift0, sync0;
	int shift0, bin0;
} pk_t;

int snr_comp(const void *elem1, const void *elem2)
{
	const pk_t *e1 = (const pk_t*) elem1, *e2 = (const pk_t*) elem2;
	int r = (e1->snr0 < e2->snr0)? 1 : ((e1->snr0 > e2->snr0)? -1:0);	// NB: comparison reversed to sort in descending order
	return r;
}

int freq_comp(const void *elem1, const void *elem2)
{
	const pk_t *e1 = (const pk_t*) elem1, *e2 = (const pk_t*) elem2;
	int r = (e1->freq0 < e2->freq0)? -1 : ((e1->freq0 > e2->freq0)? 1:0);
	return r;
}

void print_proctime(u4_t start)
{

}

void WSPR_DecodeTask()
{
    long int i,j,k;
    int delta, shift1, lagmin, lagmax, lagstep, worth_a_try, decoded;
    u4_t n28b, m22b;
    unsigned long metric, maxcycles, cycles;
    float df = FSRATE/FSPS/2;
    float dt = 1.0/FSRATE;
    float f1, fstep, snr1, sync1, drift1;
    pk_t pk[NPK];
    //double mi, mq, mi2, mq2, miq;
    u4_t start=0;
	CPX_t *idat, *qdat;

#include "./mettab.c"

	// fixme: revisit in light of new priority task system
	// this was intended to allow a minimum percentage of runtime
	TaskParams(wspr? wspr : 4000);

	while (1) {
	
    memset(&pwr_sampavg[decode_ping_pong][0], 0, sizeof(pwr_sampavg[0]));
    memset(symbols, 0, sizeof(char)*nbits*2);
    memset(allfreqs, 0, sizeof(float)*NPK);
    memset(allcalls, 0, sizeof(char)*NPK*7);
    memset(grid, 0, sizeof(char)*5);
    memset(callsign, 0, sizeof(char)*7);
    
	send_meta(wspr_wf, META_WSPR_DECODING, 0, 0);
	//printf("WSPR_DecodeTask sleep..\n");
	if (start) printf("WSPR_DecodeTask %.1f sec\n", (float)(timer_ms()-start)/1000.0);

	TaskSleep(0);

	start = timer_ms();
	//printf("WSPR_DecodeTask wakeup\n");
	idat = i_data[decode_ping_pong];
	qdat = q_data[decode_ping_pong];
	send_meta(wspr_wf, META_WSPR_DECODING, 1, 0);

	#if 0
    if (verbose) {
    	getStats(idat, qdat, TPOINTS, &mi, &mq, &mi2, &mq2, &miq);
        printf("total power: %4.1f dB\n", 10*log10(mi2+mq2));
		NT();
    }
    #endif
    
    float smspec[nbins_411];
    renormalize(pwr_sampavg[decode_ping_pong], smspec);

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
printf("initial npk %d/%d\n", npk, NPK);
	NT();

// let's not waste time on signals outside of the range [freqmin,freqmax].
    i=0;
    for( j=0; j<npk; j++) {
        if( pk[j].freq0 >= freqmin && pk[j].freq0 <= freqmax ) {
            pk[i].bin0 = pk[j].bin0;
            pk[i].freq0 = pk[j].freq0;
            pk[i].snr0 = pk[j].snr0;
            i++;
        }
    }
    npk=i;
printf("freq range limited npk %d\n", npk);
	NT();
	
	// only look at a limited number of strong peaks
    qsort(pk, npk, sizeof(pk_t), snr_comp);	// sort in decreasing snr order
    if (npk > MAX_NPK) npk = MAX_NPK;
printf("MAX_NPK limited npk %d/%d\n", npk, MAX_NPK);

	// keep in snr order so strong sigs are processed first in case we run out of decode time
    //qsort(pk, npk, sizeof(pk_t), freq_comp);	// re-sort in increasing frequency order
    
    // send peak info to client
    u1_t peaks[2+NPK*2];
    int isnr;
    peaks[0] = nbins_411>>8; peaks[1] = nbins_411&0xff;
    for (j=0; j<npk; j++) {
    	peaks[2+j*2] = pk[j].bin0/2;
    	isnr = (pk[j].snr0 + 33)*7;
    	if (isnr<0) isnr=0;
    	if (isnr>255) isnr=255;
    	peaks[2+j*2+1] = isnr;
    }
	send_meta_bytes(wspr_wf, META_WSPR_PEAKS, peaks, 2+npk*2);
	
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
							p0=pwr_samp[decode_ping_pong][ifd-3][kindex];
							p1=pwr_samp[decode_ping_pong][ifd-1][kindex];
							p2=pwr_samp[decode_ping_pong][ifd+1][kindex];
							p3=pwr_samp[decode_ping_pong][ifd+3][kindex];
							ss=ss+(2*pr3[k]-1)*(p3+p1-p0-p2);
							pow=pow+p0+p1+p2+p3;
							sync1=ss/pow;
						}
						NT();
					}
					if( sync1 > smax ) {
						smax=sync1;
						pk[j].shift0=HSPS*(k0+1);
						pk[j].drift0=idrift;
						pk[j].freq0=(ifr-SPS)*df;
						pk[j].sync0=sync1;
					}
					//printf("drift %d  k0 %d  sync %f\n",idrift,k0,smax);
					NT();
				}
			}
        }
        if ( verbose ) {
        	print_proctime(start);
			printf("npeak %3ld: %6.1f snr  %9.6f (%6.2f) freq  %5d shift  %4.1f drift  %6.3f sync\n",
                j, pk[j].snr0, dialfreq+(BFO+pk[j].freq0)/1e6, pk[j].freq0, pk[j].shift0, pk[j].drift0, pk[j].sync0); }
    }

    //FILE *fall_wspr, *fwsprd;
    //fall_wspr=fopen("ALL_WSPR.TXT","a");
    //fwsprd=fopen("wsprd.out","w");
    
    int uniques=0, valid=0;
    
    int pki;
    for (pki=0; pki<npk; pki++) {
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
		
        f1 = pk[pki].freq0;
        snr1 = pk[pki].snr0;
        drift1 = pk[pki].drift0;
        shift1 = pk[pki].shift0;
        sync1 = pk[pki].sync0;
        if( verbose ) {
        	print_proctime(start);
			printf("coarse   : %6.1f snr  %9.6f (%6.2f) freq  %5d shift  %4.1f drift  %6.3f sync\n",
            	snr1, dialfreq+(BFO+f1)/1e6, f1, shift1, drift1, sync1); }

		// fine search for best sync lag (mode 0)
        fstep=0.0;
        lagmin=shift1-144;
        lagmax=shift1+144;
        lagstep=8;
        sync_and_demodulate(idat, qdat, TPOINTS, symbols, &f1, fstep, &shift1, lagmin, lagmax, lagstep, &drift1, &sync1, 0);
        if( verbose ) {
        	print_proctime(start);
			printf("fine sync: %6.1f snr  %9.6f (%6.2f) freq  %5d shift  %4.1f drift  %6.3f sync\n",
            	snr1, dialfreq+(BFO+f1)/1e6, f1, shift1, drift1, sync1); }
		NT();

		// fine search for frequency peak (mode 1)
        fstep=0.1;
        sync_and_demodulate(idat, qdat, TPOINTS, symbols, &f1, fstep, &shift1, lagmin, lagmax, lagstep, &drift1, &sync1, 1);
        if( verbose ) {
        	print_proctime(start);
			printf("fine freq: %6.1f snr  %9.6f (%6.2f) freq  %5d shift  %4.1f drift  %6.3f sync\n",
            	snr1, dialfreq+(BFO+f1)/1e6, f1, shift1, drift1, sync1); }
		NT();

		worth_a_try = (sync1 > 0.2)? 1:0;

        int idt=0, ii, jiggered_shift;
        delta = 50;
        //maxcycles = 10000;
        //maxcycles = 200;
        maxcycles = 10;
        decoded = 0;

        while (worth_a_try && !decoded && (idt <= (medium_effort? 32:128))) {
            ii = (idt+1)/2;
            if (idt%2 == 1) ii = -ii;
            jiggered_shift = shift1+ii;
        
			// use mode 2 to get soft-decision symbols
            sync_and_demodulate(idat, qdat, TPOINTS, symbols, &f1, fstep, &jiggered_shift, lagmin, lagmax, lagstep, &drift1, &sync1, 2);
			NT();
        
            deinterleave(symbols);
			NT();

            decoded = fano(&metric, &cycles, decdata, symbols, nbits, mettab, delta, maxcycles);
			if (verbose) {
        		print_proctime(start);
				printf("jig <>%3d: %6.1f snr  %9.6f (%6.2f) freq  %5d shift  %4.1f drift  %6.3f sync  %4ld metric  %3ld cycles\n",
					idt, snr1, dialfreq+(BFO+f1)/1e6, f1, jiggered_shift, drift1, sync1, metric, cycles);
			}
			NT();
            
            idt++;

            if (quickmode) {
                break;
            }
        }
        
        if (worth_a_try && decoded)
        {
        	valid=0;
            unpack50(decdata, &n28b, &m22b);
            unpackcall(n28b, callsign);
            int ntype = (m22b & 127) - 64;
            u4_t ngrid = m22b >> 7;
            unpackgrid(ngrid, grid);

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
            for (i=0; i<npk; i++) {
                if ( !strcmp(callsign, allcalls[i]) && (fabs( f1-allfreqs[i] ) < 1.0) )
                    dupe=1;
            }
            
            if ((verbose || !dupe) && valid) {
            	struct tm tm;
            	gmtime_r(&utc[decode_ping_pong], &tm);
            
                uniques++;
                strcpy(allcalls[uniques], callsign);
                allfreqs[uniques]=f1;
                
            	printf("%02d%02d %3.0f %4.1f %10.6f %2d  %-s %4s %2d in %.3f secs --------------------------------------------------------------------\n",
                   tm.tm_hour, tm.tm_min, snr1, (shift1*dt-2.0), dialfreq+(BFO+f1)/1e6,
                   (int)drift1, callsign, grid, ntype, (float)(timer_ms()-decode_start)/1000.0);
                
                char s[128];
                sprintf(s, "%02d%02d %3.0f %4.1f %9.6f %2d  %-6s %4s  %2d",
                   tm.tm_hour, tm.tm_min, snr1, (shift1*dt-2.0), dialfreq+(BFO+f1)/1e6, (int) drift1, callsign, grid, ntype);
				send_meta_bytes(wspr_wf, META_WSPR_DECODED, (u1_t*) s, strlen(s));
				
				#if 0
				fprintf(fall_wspr,"%6s %4s %3.0f %3.0f %4.1f %10.6f  %-s %4s %2d          %2.0f     %lu    %4d\n",
						   date, uttime, sync1*10, snr1,
						   shift1*dt-2.0, dialfreq+(BFO+f1)/1e6,
						   callsign, grid, ntype, drift1, cycles/HSYM_81, ii);
				fprintf(fwsprd,"%6s %4s %3d %3.0f %4.1f %10.6f  %-s %4s %2d          %2d     %lu\n",
							date, uttime, (int)(sync1*10), snr1,
							shift1*dt-2.0, dialfreq+(BFO+f1)/1e6,
							callsign, grid, ntype, (int)drift1, cycles/HSYM_81);
				#endif
            }
        }
    }
    //fclose(fall_wspr);
    //fclose(fwsprd);
	NT();
    
    }
}

void wspr_init(conn_t *cw, double frate)
{
    assert(FSPS == round(SYMTIME * SRATE));
    assert(SPS == (int) FSPS);
    assert(HSPS == (SPS/2));

	// options:
    //quickmode = 1;
    //medium_effort = 1;
    verbose = 1;
    //freqmin=-150.0;
    //freqmax=150.0;
    
    decimate = round(frate/FSRATE);
    //printf("WSPR frate=%.1f decim=%d sps=%d NFFT=%d nbins_411=%d\n", frate, decimate, SPS, NFFT, nbins_411);
	wspr_wf = cw;
	send_meta(wspr_wf, META_WSPR_INIT, nbins_411, 0);

    fftin = (FFTW_COMPLEX*) FFTW_MALLOC(sizeof(FFTW_COMPLEX)*NFFT);
    fftout = (FFTW_COMPLEX*) FFTW_MALLOC(sizeof(FFTW_COMPLEX)*NFFT);
    fftplan = FFTW_PLAN_DFT_1D(NFFT, fftin, fftout, FFTW_FORWARD, FFTW_ESTIMATE);

    symbols = (unsigned char*) malloc(sizeof(char)*nbits*2);
    decdata = (u1_t*) malloc((nbits+7)/8);
    grid = (char *) malloc(sizeof(char)*5);
    callsign = (char *) malloc(sizeof(char)*7);

    WSPR_FFTtask_id = CreateTask(WSPR_FFTtask, APPS_PRIORITY);
    WSPR_DecodeTask_id = CreateTask(WSPR_DecodeTask, APPS_PRIORITY);
}

#endif
