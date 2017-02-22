#include "wspr.h"
#include "fano.h"
#include "jelinek.h"

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

void sync_and_demodulate_old(
	CPX_t *id, CPX_t *qd, long np,
	unsigned char *symbols, float *f1, float fstep,
	int *shift1,
	int lagmin, int lagmax, int lagstep,
	float drift1, float *sync, int mode)
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
// 2 no frequency or time lag search. calculate soft-decision symbols
//	using passed frequency and shift.
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
//            wprintf("%d %f\n",i,fsymb[i]);
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
//            wprintf("symb: %lu %d\n",i, symbols[i]);
        }
        NT();
    }
}

// END f1 drift1 shift1 sync

void wspr_decode_old(wspr_t *w)
{
    long int i,j,k;
    int delta, lagmin, lagmax, lagstep;
    u4_t call_28b, grid_pwr_22b, grid_15b, pwr_7b;
    unsigned int metric, maxcycles, cycles, maxnp;
    float df = FSRATE/FSPS/2;
    float dt = 1.0/FSRATE;

	int pki;
    pk_t pk[NPK];

	int shift;
    float f, fstep, snr, drift;
    float syncC, sync0, sync1, sync2;

	CPX_t *idat = w->i_data[w->decode_ping_pong];
	CPX_t *qdat = w->q_data[w->decode_ping_pong];

#include "./mettab.old.h"

    int uniques = 0;
	memset(w->allfreqs, 0, sizeof(w->allfreqs));
	memset(w->allcalls, 0, sizeof(w->allcalls));
		
    float smspec[nbins_411];
    renormalize(w, w->pwr_sampavg[w->decode_ping_pong], smspec);

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
	//wprintf("initial npk %d/%d\n", npk, NPK);
	NT();

	// let's not waste time on signals outside of the range [FMIN, FMAX].
    i=0;
    for (pki=0; pki < npk; pki++) {
        if( pk[pki].freq0 >= FMIN && pk[pki].freq0 <= FMAX ) {
            pk[i].bin0 = pk[pki].bin0;
            pk[i].freq0 = pk[pki].freq0;
            pk[i].snr0 = pk[pki].snr0;
            i++;
        }
    }
    npk = i;
	//wprintf("freq range limited npk %d\n", npk);
	NT();
	
	// only look at a limited number of strong peaks due to the long decode time
    qsort(pk, npk, sizeof(pk_t), snr_comp);		// sort in decreasing snr order
    if (npk > MAX_NPK_OLD) npk = MAX_NPK_OLD;
	//wprintf("MAX_NPK_OLD limited npk %d/%d\n", npk, MAX_NPK_OLD);

	// remember our frequency-sorted index
    qsort(pk, npk, sizeof(pk_t), freq_comp);
    for (pki=0; pki < npk; pki++) {
    	pk[pki].freq_idx = pki;
    }

    // send peak info to client in increasing frequency order
	pk_t pk_freq[NPK];
	memcpy(pk_freq, pk, sizeof(pk));

    for (pki=0; pki < npk; pki++) {
    	sprintf(pk_freq[pki].snr_call, "%d", (int) roundf(pk_freq[pki].snr0));
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
    float smax,ss,power,p0,p1,p2,p3;
    
    for (pki=0; pki < npk; pki++) {
        smax = -1e30;
        if0 = pk[pki].freq0/df+SPS;
        
        for (ifr=if0-1; ifr<=if0+1; ifr++) {
			for (k0=-10; k0<22; k0++)
			{
				for (idrift=-4; idrift<=4; idrift++)
				{
					ss=0.0;
					power=0.0;
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
							power=power+p0+p1+p2+p3;
							syncC=ss/power;
						}
						NT();
					}
					if( syncC > smax ) {
						smax=syncC;
						pk[pki].shift0=HSPS*(k0+1);
						pk[pki].drift0=idrift;
						pk[pki].freq0=(ifr-SPS)*df;
						pk[pki].sync0=syncC;
					}
					//wprintf("drift %d  k0 %d  sync %f\n",idrift,k0,smax);
					NT();
				}
			}
        }
		wprintf("npeak     #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync  %3d bin\n",
			pki, pk[pki].snr0, w->dialfreq+(bfo+pk[pki].freq0)/1e6, pk[pki].freq0, pk[pki].shift0, pk[pki].drift0, pk[pki].sync0, pk[pki].bin0);
    }

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

		wprintf("coarse    #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync\n",
			pki, snr, w->dialfreq+(bfo+f)/1e6, f, shift, drift, syncC);

		// fine search for best sync lag (mode 0)
        fstep=0.0;
        lagmin=shift-144;
        lagmax=shift+144;
        lagstep=8;
        
        // f: in = yes
        // shift: in = via lagmin/max
        sync_and_demodulate_old(idat, qdat, TPOINTS, w->symbols, &f, fstep, &shift, lagmin, lagmax, lagstep, drift, &sync0, 0);
        // f: out = fbest always
        // shift: out = best_shift always
        // sync0: out = syncmax always

		wprintf("fine sync #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync%s\n",
			pki, snr, w->dialfreq+(bfo+f)/1e6, f, shift, drift, sync0,
			(sync0 < syncC)? ", sync worse?":"");
		NT();

		// using refinements from mode 0 search above, fine search for frequency peak (mode 1)
        fstep=0.1;

        // f: in = yes
        // shift: in = yes
        sync_and_demodulate_old(idat, qdat, TPOINTS, w->symbols, &f, fstep, &shift, lagmin, lagmax, lagstep, drift, &sync1, 1);
        // f: out = fbest always
        // shift: out = best_shift always
        // sync1: out = syncmax always

		wprintf("fine freq #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync%s\n",
			pki, snr, w->dialfreq+(bfo+f)/1e6, f, shift, drift, sync1,
			(sync1 < sync0)? ", sync worse?":"");
		NT();

		int worth_a_try = (sync1 > 0.2)? 1:0;
		if (!worth_a_try) wprintf("NO TRY    #%ld\n", pki);

        int idt=0, ii, jiggered_shift;
        delta = 50;
        //maxcycles = 10000;
        //maxcycles = 200;
        maxcycles = 10;
        int decoded = 0;

        while (!w->abort_decode && worth_a_try && !decoded && (idt <= (w->medium_effort? 32:128))) {
            ii = (idt+1)/2;
            if (idt%2 == 1) ii = -ii;
            jiggered_shift = shift+ii;
        
			// use mode 2 to get soft-decision symbols
			// f: in = yes
			// shift: in = yes
            sync_and_demodulate_old(idat, qdat, TPOINTS, w->symbols, &f, fstep, &jiggered_shift, lagmin, lagmax, lagstep, drift, &sync2, 2);
			// f: out = NO
			// shift: out = NO
			// sync2: out = NO
			NT();
        
            deinterleave(w->symbols);
			NT();

            decoded = fano(&metric, &cycles, &maxnp, w->decdata, w->symbols, NBITS, mettab, delta, maxcycles);
			wprintf("jig <>%3d #%ld %6.1f snr  %9.6f (%7.2f) freq  %5d shift  %4.1f drift  %6.3f sync  %4ld metric  %3ld cycles\n",
				idt, pki, snr, w->dialfreq+(bfo+f)/1e6, f, jiggered_shift, drift, sync2, metric, cycles);
			NT();
            
            idt++;

            if (w->quickmode) {
                break;
            }
        }

		int valid = 0;
        if (decoded) {
            unpack50(w->decdata, &call_28b, &grid_pwr_22b, &grid_15b, &pwr_7b);
            unpackcall(call_28b, w->callsign);
            unpackgrid(grid_15b, w->grid);
            int ntype = pwr_7b - 64;

			// this is where the extended messages would be taken care of
            if ((ntype >= 0) && (ntype <= 62)) {
                int nu = ntype%10;
                if (nu == 0 || nu == 3 || nu == 7) {
                    valid=1;
                } else {
            		wprintf("ntype non-std %d ===============================================================\n", ntype);
                }
            } else if ( ntype < 0 ) {
				wprintf("ntype <0 %d ===============================================================\n", ntype);
            } else {
            	wprintf("ntype=63 %d ===============================================================\n", ntype);
            }
            
			// de-dupe using callsign and freq (only a dupe if freqs are within 1 Hz)
            int dupe=0;
            for (i=0; i < uniques; i++) {
                if ( !strcmp(w->callsign, w->allcalls[i]) && (fabs( f-w->allfreqs[i] ) <= 1.0) )
                    dupe=1;
            }
            
            if (valid && !dupe) {
            	struct tm tm;
            	gmtime_r(&w->utc[w->decode_ping_pong], &tm);
            
                strcpy(w->allcalls[uniques], w->callsign);
                w->allfreqs[uniques]=f;
                uniques++;
                
            	wprintf("%02d%02d %3.0f %4.1f %10.6f %2d  %-s %4s %2d in %.3f secs --------------------------------------------------------------------\n",
                   tm.tm_hour, tm.tm_min, snr, (shift*dt-2.0), w->dialfreq+(bfo+f)/1e6,
                   (int)drift, w->callsign, w->grid, ntype, (float)(timer_ms()-decode_start)/1000.0);
                
                int dBm = ntype;
                double watts, factor;
                char *W_s;
                const char *units;

                watts = pow(10.0, (dBm - 30)/10.0);
                if (watts >= 1.0) {
                	factor = 1.0;
                	units = "W";
                } else
                if (watts >= 1e-3) {
                	factor = 1e3;
                	units = "mW";
                } else
                if (watts >= 1e-6) {
                	factor = 1e6;
                	units = "uW";
                } else
                if (watts >= 1e-9) {
                	factor = 1e9;
                	units = "nW";
                } else {
                	factor = 1e12;
                	units = "pW";
                }
                
                watts *= factor;
                if (watts < 10.0)
                	asprintf(&W_s, "%.1f %s", watts, units);
                else
                	asprintf(&W_s, "%.0f %s", watts, units);
                
				ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                	"%02d%02d %3.0f %4.1f %9.6f %2d  "
                	"<a href='https://www.qrz.com/lookup/%s' target='_blank'>%-6s</a> "
                	"<a href='http://www.levinecentral.com/ham/grid_square.php?Grid=%s' target='_blank'>%s</a> "
                	"%5d  %3d (%s)",
					tm.tm_hour, tm.tm_min, snr, (shift*dt-2.0), w->dialfreq+(bfo+f)/1e6, (int) drift,
					w->callsign, w->callsign, w->grid, w->grid,
					(int) grid_to_distance_km(w->grid), dBm, W_s);
				free(W_s);
				
				ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_UPLOAD",
                	"%02d%02d %3.0f %4.1f %9.6f %2d %-6s %s %3d",
					tm.tm_hour, tm.tm_min, snr, (shift*dt-2.0), w->dialfreq+(bfo+f)/1e6, (int) drift,
					w->callsign, w->grid, dBm);

				strcpy(pk_freq[pk[pki].freq_idx].snr_call, w->callsign);
            } else {
            	valid = 0;
			}
        } else {
			if (worth_a_try)
				wprintf("GIVE UP  #%ld %.3f secs\n", pki, (float)(timer_ms()-decode_start)/1000.0);
        }

		if (!valid)
			pk_freq[pk[pki].freq_idx].flags |= WSPR_F_DELETE;

		wspr_send_peaks(w, pk_freq, npk);
    }
	NT();
}
