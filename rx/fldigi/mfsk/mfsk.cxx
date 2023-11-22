// ----------------------------------------------------------------------------
// mfsk.cxx  --  mfsk modem
//
// Copyright (C) 2006-2009
//		Dave Freese, W1HKJ
//
// This file is part of fldigi.  Adapted from code contained in gmfsk source code
// distribution.
//  gmfsk Copyright (C) 2001, 2002, 2003
//  Tomi Manninen (oh2bns@sral.fi)
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <config.h>

#include <cstdlib>
#include <cstring>
#include <libgen.h>

#include <FL/Fl.H>

#include "mfsk.h"
#include "modem.h"

#include "misc.h"
#include "main.h"
#include "fl_digi.h"
#include "configuration.h"
#include "status.h"
#include "trx.h"

#include "ascii.h"

#include "fileselect.h"

#include "qrunner.h"

#include "debug.h"

#define SOFTPROFILE false

using namespace std;

// MFSKpic receive start delay value based on a viterbi length of 45
//   44 nulls at 8 samples per pixel
//   88 nulls at 4 samples per pixel
//   176 nulls at 2 samples per pixel
struct TRACEPAIR {
	int trace;
	int delay;
	TRACEPAIR( int a, int b) { trace = a; delay = b;}
};

TRACEPAIR tracepair(45, 352);

// enable to limit within band signal sidebands
// not really needed
static bool xmt_filter = false;

//=============================================================================
char mfskmsg[80];
//=============================================================================

#include "mfsk-pic.cxx"

void  mfsk::tx_init()
{
	txstate = TX_STATE_PREAMBLE;
	bitstate = 0;

	double factor = 1.5;
	double bw2 = factor*(numtones + 1) * samplerate / symlen / 2.0;
	double flo = (get_txfreq_woffset() - bw2);// / samplerate;
	if (flo <= 100) flo = 100;
	double fhi = (get_txfreq_woffset() + bw2);// / samplerate;
	if (fhi >= samplerate/2 - 100) fhi = samplerate/2 - 100;

	xmtfilt->init_bandpass (255, 1, flo/samplerate, fhi/samplerate);

	videoText();
}

void  mfsk::rx_init()
{
	rxstate = RX_STATE_DATA;
	synccounter = 0;
	symcounter = 0;
	met1 = 0.0;
	met2 = 0.0;
	counter = 0;
	RXspp = 8;

	for (int i = 0; i < 2 * symlen; i++) {
		for (int j = 0; j < 32; j++)
			pipe[i].vector[j] = cmplx(0,0);
	}
	reset_afc();
	s2n = 0.0;
	memset(picheader, ' ', PICHEADER - 1);
	picheader[PICHEADER -1] = 0;
	put_MODEstatus(mode);
	syncfilter->reset();
	staticburst = false;

	s2n_valid = false;
}


void mfsk::init()
{
	modem::init();
	rx_init();
	set_scope_mode(Digiscope::SCOPE);
// picture mode init
	setpicture_link(this);
	TXspp = txSPP;
	RXspp = 8;

	if (progdefaults.StartAtSweetSpot)
		set_freq(progdefaults.PSKsweetspot);
	else if (progStatus.carrier != 0) {
		set_freq(progStatus.carrier);
#if !BENCHMARK_MODE
		progStatus.carrier = 0;
#endif
	} else
		set_freq(wf->Carrier());

}

void mfsk::shutdown()
{
}

mfsk::~mfsk()
{
	stopflag = true;
	int msecs = 200;
	while(--msecs && txstate != TX_STATE_PREAMBLE) MilliSleep(1);
// do not destroy picTxWin or picRxWin as there may be pending updates
// in the UI request queue
	if (picTxWin) picTxWin->hide();
	activate_mfsk_image_item(false);

	if (bpfilt) delete bpfilt;
	if (xmtfilt) delete xmtfilt;
	if (rxinlv) delete rxinlv;
	if (txinlv) delete txinlv;
	if (dec2) delete dec2;
	if (dec1) delete dec1;
	if (enc) delete enc;
	if (pipe) delete [] pipe;
	if (hbfilt) delete hbfilt;
	if (binsfft) delete binsfft;
	for (int i = 0; i < SCOPESIZE; i++) {
		if (vidfilter[i]) delete vidfilter[i];
	}
	if (syncfilter) delete syncfilter;
}

mfsk::mfsk(trx_mode mfsk_mode) : modem()
{
	cap |= CAP_AFC | CAP_REV;

	double bw, cf, flo, fhi;
	mode = mfsk_mode;
	depth = 10;

// CAP_IMG is set in cap iff image transfer supported
	switch (mode) {

	case MODE_MFSK4:
		samplerate = 8000;
		symlen = 2048;
		symbits = 5;
		depth = 5;
		basetone = 256;
		numtones = 32;
		preamble = 107; // original mfsk modes
		break;
	case MODE_MFSK8:
		samplerate = 8000;
		symlen =  1024;
		symbits =    5;
		depth = 5;
		basetone = 128;
		numtones = 32;
		preamble = 107; // original mfsk modes
		break;
	case MODE_MFSK31:
		samplerate = 8000;
		symlen =  256;
		symbits =   3;
		depth = 10;
		basetone = 32;
		numtones = 8;
		preamble = 107; // original mfsk modes
		break;
	case MODE_MFSK32:
		samplerate = 8000;
		symlen =  256;
		symbits =   4;
		depth = 10;
		basetone = 32;
		numtones = 16;
		preamble = 107; // original mfsk modes
		cap |= CAP_IMG;
		break;
	case MODE_MFSK64:
		samplerate = 8000;
		symlen =  128;
		symbits =    4;
		depth = 10;
		basetone = 16;
		numtones = 16;
		preamble = 180;
		cap |= CAP_IMG;
		break;
	case MODE_MFSK128:
		samplerate = 8000;
		symlen =  64;
		symbits =   4;
		depth = 20;
		basetone = 8;
		numtones = 16;
		cap |= CAP_IMG;
		preamble = 214;
		break;

	case MODE_MFSK64L:
		samplerate = 8000;
		symlen =  128;
		symbits =    4;
		depth = 400;
		preamble = 2500;
		basetone = 16;
		numtones = 16;
		break;
	case MODE_MFSK128L:
		samplerate = 8000;
		symlen =  64;
		symbits =   4;
		depth = 800;
		preamble = 5000;
		basetone = 8;
		numtones = 16;
		break;

	case MODE_MFSK11:
		samplerate = 11025;
		symlen =  1024;
		symbits =   4;
		depth = 10;
		basetone = 93;
		numtones = 16;
		preamble = 107;
		break;
	case MODE_MFSK22:
		samplerate = 11025;
		symlen =  512;
		symbits =    4;
		depth = 10;
		basetone = 46;
		numtones = 16;
		preamble = 107;
		break;

	case MODE_MFSK16:
	default:
		samplerate = 8000;
		symlen =  512;
		symbits =   4;
		depth = 10;
		basetone = 64;
		numtones = 16;
		preamble = 107;
		cap |= CAP_IMG;
		break;
	}

	tonespacing = (double) samplerate / symlen;
	basefreq = 1.0 * samplerate * basetone / symlen;

	binsfft		= new sfft (symlen, basetone, basetone + numtones );
	hbfilt		= new C_FIR_filter();
	hbfilt->init_hilbert(37, 1);

	syncfilter = new Cmovavg(8);

	for (int i = 0; i < SCOPESIZE; i++)
		vidfilter[i] = new Cmovavg(16);

	pipe		= new rxpipe[ 2 * symlen ];

	enc			= new encoder (NASA_K, POLY1, POLY2);
	dec1		= new viterbi (NASA_K, POLY1, POLY2);
	dec2		= new viterbi (NASA_K, POLY1, POLY2);

	dec1->settraceback (tracepair.trace);
	dec2->settraceback (tracepair.trace);
	dec1->setchunksize (1);
	dec2->setchunksize (1);

	txinlv = new interleave (symbits, depth, INTERLEAVE_FWD);
	rxinlv = new interleave (symbits, depth, INTERLEAVE_REV);

	bw = (numtones - 1) * tonespacing;
	cf = basefreq + bw / 2.0;

	flo = (cf - bw/2 - 2 * tonespacing) / samplerate;
	fhi = (cf + bw/2 + 2 * tonespacing) / samplerate;
	bpfilt = new C_FIR_filter();
	bpfilt->init_bandpass (127, 1, flo, fhi);

	xmtfilt = new C_FIR_filter();

	scopedata.alloc(symlen * 2);

	fragmentsize = symlen;
	bandwidth = (numtones - 1) * tonespacing;

	startpic = false;
	abortxmt = false;
	stopflag = false;

	bitshreg = 0;
	bitstate = 0;
	phaseacc = 0;
	pipeptr = 0;
	metric = 0;
	prev1symbol = prev2symbol = 0;
	symbolpair[0] = symbolpair[1] = 0;

// picTxWin and picRxWin are created once to support all instances of mfsk
	if (!picTxWin) createTxViewer();
	if (!picRxWin)
		createRxViewer();
	activate_mfsk_image_item(true);
	afcmetric = 0.0;
	datashreg = 1;

	for (int i = 0; i < 128; i++) prepost[i] = 0;
}


//=====================================================================
// receive processing
//=====================================================================

void mfsk::s2nreport(void)
{
	modem::s2nreport();
	s2n_valid = false;
}

bool mfsk::check_picture_header(char c)
{
	char *p;

	if (c >= ' ' && c <= 'z') {
		memmove(picheader, picheader + 1, PICHEADER - 1);
		picheader[PICHEADER - 2] = c;
	}
	picW = 0;
	picH = 0;
	color = false;

	p = strstr(picheader, "Pic:");
	if (p == NULL)
		return false;

	p += 4;

	if (*p == 0) return false;

	while ( *p && isdigit(*p))
		picW = (picW * 10) + (*p++ - '0');

	if (*p++ != 'x')
		return false;

	while ( *p && isdigit(*p))
		picH = (picH * 10) + (*p++ - '0');

	if (*p == 'C') {
		color = true;
		p++;
	}
	if (*p == ';') {
		if (picW == 0 || picH == 0 || picW > 4095 || picH > 4095)
			return false;
		RXspp = 8;
		return true;
	}
	if (*p == 'p')
		p++;
	else
		return false;
	if (!*p)
		return false;
	RXspp = 8;
	if (*p == '4') RXspp = 4;
	if (*p == '2') RXspp = 2;
	p++;
	if (!*p)
		return false;
	if (*p != ';')
		return false;
	if (picW == 0 || picH == 0 || picW > 4095 || picH > 4095)
		return false;
	return true;
}

void mfsk::recvpic(cmplx z)
{
	int byte;
	picf += arg( conj(prevz) * z) * samplerate / TWOPI;
	prevz = z;

	if ((counter % RXspp) == 0) {
		picf = 256 * (picf / RXspp - basefreq) / bandwidth;
		byte = (int)CLAMP(picf, 0.0, 255.0);
		if (reverse)
			byte = 255 - byte;

		if (color) {
			pixelnbr = rgb + row + 3*col;
			REQ(updateRxPic, byte, pixelnbr);
			if (++col == picW) {
				col = 0;
				if (++rgb == 3) {
					rgb = 0;
					row += 3 * picW;
				}
			}
		} else {
			for (int i = 0; i < 3; i++)
				REQ(updateRxPic, byte, pixelnbr++);
		}
		picf = 0.0;

		int n = picW * picH * 3;
		if (pixelnbr % (picW * 3) == 0) {
			snprintf(mfskmsg, sizeof(mfskmsg),
					 "Rx pic: %3.1f%%",
					 (100.0f * pixelnbr) / n);
			put_status(mfskmsg);
		}
	}
}

void mfsk::recvchar(int c)
{
	if (c == -1 || c == 0)
		return;

	put_rx_char(c);

	if (check_picture_header(c) == true) {
		counter = tracepair.delay;
		switch (mode) {
			case MODE_MFSK16:
				if (symbolbit == symbits) counter += symlen;
				break;
			case MODE_MFSK32:
				if (symbolbit == symbits) counter += symlen;
				break;
			case MODE_MFSK64:
				counter = 4956;
				if (symbolbit % 2 == 0) counter += symlen;
				break;
			case MODE_MFSK128:
				counter = 1824;
				if (symbolbit % 2 == 0) counter += symlen;
				break;
			case MODE_MFSK31:
				counter = 5216;
				if (symbolbit == symbits) counter += symlen;
				break;
			case MODE_MFSK4:
			case MODE_MFSK8:
			case MODE_MFSK11:
			case MODE_MFSK22:
			default: break;
		};

		rxstate = RX_STATE_PICTURE_START;
		picturesize = RXspp * picW * picH * (color ? 3 : 1);
		pixelnbr = 0;
		col = 0;
		row = 0;
		rgb = 0;
		memset(picheader, ' ', PICHEADER - 1);
		picheader[PICHEADER -1] = 0;
		return;
	} else counter = 0;

	if (progdefaults.Pskmails2nreport && (mailserver || mailclient)) {
		if ((c == SOH) && !s2n_valid) {
			// starts collecting s2n from first SOH in stream (since start of RX)
			s2n_valid = true;
			s2n_sum = s2n_sum2 = s2n_ncount = 0.0;
		}
		if (s2n_valid) {
			s2n_sum += s2n_metric;
			s2n_sum2 += (s2n_metric * s2n_metric);
			s2n_ncount++;
			if (c == EOT)
				s2nreport();
		}
	}
	return;
}

void mfsk::recvbit(int bit)
{
	int c;

	datashreg = (datashreg << 1) | !!bit;
	if ((datashreg & 7) == 1) {
		c = varidec(datashreg >> 1);
		recvchar(c);
		datashreg = 1;
	}
}

void mfsk::decodesymbol(unsigned char symbol)
{
	int c, met;

	symbolpair[0] = symbolpair[1];
	symbolpair[1] = symbol;

	symcounter = symcounter ? 0 : 1;

// only modes with odd number of symbits need a vote
	if (symbits == 5 || symbits == 3) { // could use symbits % 2 == 0
		if (symcounter) {
			if ((c = dec1->decode(symbolpair, &met)) == -1)
				return;
			met1 = decayavg(met1, met, 50);//32);
			if (met1 < met2)
				return;
			metric = met1;
		} else {
			if ((c = dec2->decode(symbolpair, &met)) == -1)
				return;
			met2 = decayavg(met2, met, 50);//32);
			if (met2 < met1)
				return;
			metric = met2;
		}
	} else {
		if (symcounter) return;
		if ((c = dec2->decode(symbolpair, &met)) == -1)
			return;
		met2 = decayavg(met2, met, 50);//32);
		metric = met2;
	}

	if (progdefaults.Pskmails2nreport && (mailserver || mailclient)) {
		// s2n reporting: re-calibrate
		s2n_metric = metric * 4.5 - 42;
		s2n_metric = CLAMP(s2n_metric, 0.0, 100.0);
	}

	// Re-scale the metric and update main window
	metric -= 60.0;
	metric *= 0.5;

	metric = CLAMP(metric, 0.0, 100.0);
	display_metric(metric);

	if (progStatus.sqlonoff && metric < progStatus.sldrSquelchValue)
		return;

	recvbit(c);

}

void mfsk::softdecode(cmplx *bins)
{
	double binmag, sum=0, avg=0, b[symbits];
	unsigned char symbols[symbits];
	int i, j, k, CWIsymbol;

	static int CWIcounter[MAX_SYMBOLS] = {0};
	static const int CWI_MAXCOUNT=6; // this is the maximum number of repeated tones which is valid for the modem ( 0 excluded )

	for (i = 0; i < symbits; i++)
		b[i] = 0.0;

	// Calculate the average signal, ignoring CWI tones
	for (i = 0; i < numtones; i++) {
	  	if ( CWIcounter[i] < CWI_MAXCOUNT )
			sum += abs(bins[i]);
	}
	avg = sum / numtones;

	// avoid divide by zero later
	if ( sum < 1e-10 ) sum = 1e-10;

	// dynamic CWI avoidance: use harddecode() result (currsymbol) for CWI detection
	if (reverse)
	          CWIsymbol = (numtones - 1) - currsymbol;
	else
	          CWIsymbol = currsymbol;

	// Add or subtract the CWI counters based on harddecode result
	// avoiding tone #0 by starting at 1
	for (i = 1; i <  numtones ; i++) {
	          if (reverse)
		      k = (numtones - 1) - i;
	          else
		      k = i;

	          if ( k == CWIsymbol)
		       CWIcounter[k]++;
	          else
		       CWIcounter[k]--;

		  // bounds-check the counts to keep the values sane
	          if (CWIcounter[k] < 0) CWIcounter[k] = 0;
	          if (CWIcounter[k] > CWI_MAXCOUNT) CWIcounter[k] = CWI_MAXCOUNT + 1;
	}

// Grey decode and form soft decision samples
	for (i = 0; i < numtones; i++) {
		j = graydecode(i);

		if (reverse)
			k = (numtones - 1) - i;
		else
			k = i;

		// Avoid CWI. This never affects tone #0
		if ( CWIcounter[k] > CWI_MAXCOUNT ) {
			binmag = avg; // soft-puncture to the average signal-level
		} else if ( CWIsymbol == k )
			binmag = 2.0f * abs(bins[k]); // give harddecode() a vote in softdecode's decision.
		else
			binmag = abs(bins[k]);

		for (k = 0; k < symbits; k++)
			b[k] += (j & (1 << (symbits - k - 1))) ? binmag : -binmag;
	}
#if SOFTPROFILE
	LOG_INFO("harddecode() symbol = %d", CWIsymbol );
#endif

// shift to range 0...255
	for (i = 0; i < symbits; i++) {
		unsigned char softbits;
		if (staticburst)
			softbits = 128;  // puncturing
		else
			softbits = (unsigned char)clamp(128.0 + (b[i] / (sum) * 256.0), 0, 255);

		symbols[i] = softbits;
#if SOFTPROFILE
		LOG_INFO("softbits = %3u", softbits);
#endif
	}

	rxinlv->symbols(symbols);

	for (i = 0; i < symbits; i++) {
		symbolbit = i + 1;
		decodesymbol(symbols[i]);
		if (counter) return;
	}
}

cmplx mfsk::mixer(cmplx in, double f)
{
	cmplx z;

// Basetone is a nominal 1000 Hz
	f -= tonespacing * basetone + bandwidth / 2;

	z = in * cmplx( cos(phaseacc), sin(phaseacc) );

	phaseacc -= TWOPI * f / samplerate;
	if (phaseacc < 0) phaseacc += TWOPI;

	return z;
}

// finds the tone bin with the largest signal level
// assumes that will be the present tone received
// with NO CW inteference

int mfsk::harddecode(cmplx *in)
{
	double x, max = 0.0, avg = 0.0;
	int i, symbol = 0;
	int burstcount = 0;

	for (int i = 0; i < numtones; i++)
		avg += abs(in[i]);
	avg /= numtones;

	if (avg < 1e-20) avg = 1e-20;

	for (i = 0; i < numtones; i++) {
		x = abs(in[i]);
		if ( x > max) {
			max = x;
			symbol = i;
		}
		if (x > 2.0 * avg) burstcount++;
	}

	staticburst = (burstcount == numtones);

	if (!staticburst)
		afcmetric = 0.95*afcmetric + 0.05 * (2 * max / avg);
	else
		afcmetric = 0.0;

	return symbol;
}

void mfsk::update_syncscope()
{
	int j;
	int pipelen = 2 * symlen;
	memset(scopedata, 0, 2 * symlen * sizeof(double));
	if (!progStatus.sqlonoff || metric >= progStatus.sldrSquelchValue)
		for (unsigned int i = 0; i < SCOPESIZE; i++) {
			j = (pipeptr + i * pipelen / SCOPESIZE + 1) % (pipelen);
			scopedata[i] = vidfilter[i]->run(abs(pipe[j].vector[prev1symbol]));
		}
	set_scope(scopedata, SCOPESIZE);

	scopedata.next(); // change buffers
	snprintf(mfskmsg, sizeof(mfskmsg), "s/n %3.0f dB", 20.0 * log10(s2n));
	put_Status1(mfskmsg);
}

void mfsk::synchronize()
{
	int i, j;
	double syn = -1;
	double val, max = 0.0;

	if (currsymbol == prev1symbol)
		return;
	if (prev1symbol == prev2symbol)
		return;

	j = pipeptr;

	for (i = 0; i < 2 * symlen; i++) {
		val = abs(pipe[j].vector[prev1symbol]);

		if (val > max) {
			max = val;
			syn = i;
		}

		j = (j + 1) % (2 * symlen);
	}

	syn = syncfilter->run(syn);

	synccounter += (int) floor((syn - symlen) / numtones + 0.5);

	update_syncscope();
}

void mfsk::reset_afc() {
	freqerr = 0.0;
	syncfilter->reset();
	return;
}

void mfsk::afc()
{
	cmplx z;
	cmplx prevvector;
	double f, f1;
	double ts = tonespacing / 4;

	if (sigsearch) {
		reset_afc();
		sigsearch = 0;
	}

	if (staticburst || !progStatus.afconoff)
		return;
	if (metric < progStatus.sldrSquelchValue)
		return;
	if (afcmetric < 3.0)
		return;
	if (currsymbol != prev1symbol)
		return;

	if (pipeptr == 0)
		prevvector = pipe[2*symlen - 1].vector[currsymbol];
	else
		prevvector = pipe[pipeptr - 1].vector[currsymbol];
	z = conj(prevvector) * currvector;

	f = arg(z) * samplerate / TWOPI;

	f1 = tonespacing * (basetone + currsymbol);

	if ( fabs(f1 - f) < ts) {
		freqerr = decayavg(freqerr, (f1 - f), 32);
		set_freq(frequency - freqerr);
	}

}

void mfsk::eval_s2n()
{
	sig = abs(pipe[pipeptr].vector[currsymbol]);
	noise = (numtones -1) * abs(pipe[pipeptr].vector[prev2symbol]);
	if (noise > 0)
		s2n = decayavg ( s2n, sig / noise, 64 );

}

int mfsk::rx_process(const double *buf, int len)
{
	cmplx z;
	cmplx* bins = 0;

	while (len-- > 0) {
// create analytic signal...
		z = cmplx( *buf, *buf );
		buf++;
		hbfilt->run ( z, z );
// shift in frequency to the base freq
		z = mixer(z, frequency);
// bandpass filter around the shifted center frequency
// with required bandwidth
		bpfilt->run ( z, z );

// copy current vector to the pipe
		binsfft->run (z, pipe[pipeptr].vector, 1);
		bins = pipe[pipeptr].vector;

		if (rxstate == RX_STATE_PICTURE_START) {
			if (--counter == 0) {
				counter = picturesize;
				rxstate = RX_STATE_PICTURE;
				REQ( showRxViewer, picW, picH );
			}
		}
		if (rxstate == RX_STATE_PICTURE) {
			if (--counter == 0) {
				rxstate = RX_STATE_DATA;
				put_status("");

				string autosave_dir = PicsDir;
				picRx->save_png(autosave_dir.c_str());
				rx_init();
			} else
				recvpic(z);
			continue;
		}

// copy current vector to the pipe
//		binsfft->run (z, pipe[pipeptr].vector, 1);
//		bins = pipe[pipeptr].vector;

		if (--synccounter <= 0) {

			synccounter = symlen;

			currsymbol = harddecode(bins);
			currvector = bins[currsymbol];
			softdecode(bins);

// symbol sync
			synchronize();

// frequency tracking
			afc();
			eval_s2n();

			prev2symbol = prev1symbol;
			prev2vector = prev1vector;
			prev1symbol = currsymbol;
			prev1vector = currvector;
		}
		pipeptr = (pipeptr + 1) % (2 * symlen);
	}

	return 0;
}


//=====================================================================
// transmit processing
//=====================================================================

void mfsk::transmit(double *buf, int len)
{
	if (xmt_filter)
		for (int i = 0; i < len; i++) xmtfilt->Irun(buf[i], buf[i]);

	ModulateXmtr(buf, len);
}


void mfsk::sendsymbol(int sym)
{
	double f, phaseincr;

	f = get_txfreq_woffset() - bandwidth / 2;

	sym = grayencode(sym & (numtones - 1));
	if (reverse)
		sym = (numtones - 1) - sym;

	phaseincr = TWOPI * (f + sym*tonespacing) / samplerate;

	for (int i = 0; i < symlen; i++) {
		outbuf[i] = cos(phaseacc);
		phaseacc -= phaseincr;
		if (phaseacc < 0) phaseacc += TWOPI;
	}
	transmit (outbuf, symlen);
}

void mfsk::sendbit(int bit)
{
	int data = enc->encode(bit);
	for (int i = 0; i < 2; i++) {
		bitshreg = (bitshreg << 1) | ((data >> i) & 1);
		bitstate++;

		if (bitstate == symbits) {
			txinlv->bits(&bitshreg);
			sendsymbol(bitshreg);
			bitstate = 0;
			bitshreg = 0;
		}
	}
}

void mfsk::sendchar(unsigned char c)
{
	const char *code = varienc(c);
	while (*code)
		sendbit(*code++ - '0');
	put_echo_char(c);
}

void mfsk::sendidle()
{
	sendchar(0);
	sendbit(1);

// extended zero bit stream
	for (int i = 0; i < 32; i++)
		sendbit(0);
}

void mfsk::flushtx(int nbits)
{
// flush the varicode decoder at the other end
	sendbit(1);

// flush the convolutional encoder and interleaver
//VK2ETA high speed modes	for (int i = 0; i < 107; i++)
//W1HKJ	for (int i = 0; i < preamble; i++)
	for (int i = 0; i < nbits; i++)
		sendbit(0);

	bitstate = 0;
}


void mfsk::sendpic(unsigned char *data, int len)
{
	double *ptr;
	double f;
	int i, j;

	ptr = outbuf;

	for (i = 0; i < len; i++) {
		if (txstate == TX_STATE_PICTURE)
		    REQ(updateTxPic, data[i]);
		if (reverse)
			f = get_txfreq_woffset() - bandwidth * (data[i] - 128) / 256.0;
		else
			f = get_txfreq_woffset() + bandwidth * (data[i] - 128) / 256.0;

		for (j = 0; j < TXspp; j++) {
			*ptr++ = cos(phaseacc);
			phaseacc += TWOPI * f / samplerate;
			if (phaseacc > TWOPI) phaseacc -= TWOPI;
		}
	}

	transmit (outbuf, TXspp * len);
}

// -----------------------------------------------------------------------------
// send prologue consisting of tracepair.delay 0's
void mfsk::flush_xmt_filter(int n)
{
	double f1 = get_txfreq_woffset() - bandwidth / 2.0;
	double f2 = get_txfreq_woffset() + bandwidth / 2.0;
	for (int i = 0; i < n; i++) {
		outbuf[i] = cos(phaseacc);
		phaseacc += TWOPI * (reverse ? f2 : f1) / samplerate;
		if (phaseacc > TWOPI) phaseacc -= TWOPI;
	}
	transmit (outbuf, tracepair.delay);
}

void mfsk::send_prologue()
{
	flush_xmt_filter(tracepair.delay);
}

void mfsk::send_epilogue()
{
	flush_xmt_filter(64);
}

static bool close_after_transmit = false;

void mfsk::clearbits()
{
	int data = enc->encode(0);
	for (int k = 0; k < preamble; k++) {
		for (int i = 0; i < 2; i++) {
			bitshreg = (bitshreg << 1) | ((data >> i) & 1);
			bitstate++;

			if (bitstate == symbits) {
				txinlv->bits(&bitshreg);
				bitstate = 0;
				bitshreg = 0;
			}
		}
	}
}

int mfsk::tx_process()
{
	// filter test set to 1
#if 0
	double *ptr;
	double f;
	char msg[100];
	for (int i = 100; i < 3900; i++) {
		ptr = outbuf;
		f = 1.0 * i;
		snprintf(msg, sizeof(msg), "freq: %.0f", f);
		put_status(msg);
		for (int j = 0; j < 32; j++) {
			*ptr++ = cos(phaseacc);
			phaseacc += TWOPI * f / samplerate;
			if (phaseacc > TWOPI) phaseacc -= TWOPI;
		}
		transmit (outbuf, 32);

	}
	return -1;
#endif

	int xmtbyte;

	switch (txstate) {
		case TX_STATE_PREAMBLE:
			clearbits();

			if (mode != MODE_MFSK64L && mode != MODE_MFSK128L )
				for (int i = 0; i < preamble / 3; i++)
					sendbit(0);

			txstate = TX_STATE_START;
			break;
		case TX_STATE_START:
			sendchar('\r');
			sendchar(2);		// STX
			sendchar('\r');
			txstate = TX_STATE_DATA;
			break;

		case TX_STATE_DATA:
			xmtbyte = get_tx_char();

			if(active_modem->XMLRPC_CPS_TEST) {
				if(startpic) startpic = false;
				if(xmtbyte == 0x05) {
					sendchar(0x04); // 0x4 has the same symbol count as 0x5
					break;
				}
			}


			if (xmtbyte == 0x05 || startpic == true) {
				put_status("Tx pic: start");
				int len = (int)strlen(picheader);
				for (int i = 0; i < len; i++)
					sendchar(picheader[i]);
				flushtx(preamble);
				startpic = false;
				txstate = TX_STATE_PICTURE_START;
			}
			else if ( xmtbyte == GET_TX_CHAR_ETX || stopflag)
				txstate = TX_STATE_FLUSH;
			else if (xmtbyte == GET_TX_CHAR_NODATA)
				sendidle();
			else
				sendchar(xmtbyte);
 			break;

		case TX_STATE_FLUSH:
			sendchar('\r');
			sendchar(4);		// EOT
			sendchar('\r');
			flushtx(preamble);
			rxstate = RX_STATE_DATA;
			txstate = TX_STATE_PREAMBLE;
			stopflag = false;
			cwid();
			return -1;

		case TX_STATE_PICTURE_START:
			send_prologue();
			txstate = TX_STATE_PICTURE;
			break;

		case TX_STATE_PICTURE:
			int i = 0;
			int blocklen = 128;
			stop_deadman();
			while (i < xmtbytes) {
				if (stopflag || abortxmt)
					break;
				if (i + blocklen < xmtbytes)
					sendpic( &xmtpicbuff[i], blocklen);
				else
					sendpic( &xmtpicbuff[i], xmtbytes - i);
				if ( (100 * i / xmtbytes) % 2 == 0) {
					snprintf(mfskmsg, sizeof(mfskmsg),
							 "Tx pic: %3.1f%%",
							 (100.0f * i) / xmtbytes);
					put_status(mfskmsg);
				}
				i += blocklen;
			}
			flushtx(preamble);
			start_deadman();

			REQ_FLUSH(GET_THREAD_ID());

			txstate = TX_STATE_DATA;
			put_status("Tx pic: done");
			btnpicTxSendAbort->hide();
			btnpicTxSPP->show();
			btnpicTxSendColor->show();
			btnpicTxSendGrey->show();
			btnpicTxLoad->show();
			btnpicTxClose->show();
			if (close_after_transmit) picTxWin->hide();
			close_after_transmit = false;
			abortxmt = false;
			rxstate = RX_STATE_DATA;
			memset(picheader, ' ', PICHEADER - 1);
			picheader[PICHEADER -1] = 0;
			break;
	}

	return 0;
}

void mfsk::send_color_image(std::string s)
{
	if (load_image(s.c_str())) {
		close_after_transmit = true;
		pic_TxSendColor();
	}
}

void mfsk::send_Grey_image(std::string s)
{
	if (load_image(s.c_str())) {
		close_after_transmit = true;
		pic_TxSendGrey();
	}
}


