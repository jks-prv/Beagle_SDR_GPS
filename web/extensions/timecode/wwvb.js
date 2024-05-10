// Copyright (c) 2017-2024 John Seamons, ZL4VO/KF6VO

//
// TODO
//    parity checking
//

var wwvb = {
   // ampl
   AMPL_NOISE_THRESHOLD: 3,
   arm: 0,
   data: 0,
   data_last: 0,

   // phase
   PHASE_NOISE_THRESHOLD: 15,
   prev_one_width: 0,
   ph_sync:    [ 0,0,0,1,1,1,0,1,1,0,1,0,0,0 ],    // sync[13..0] starting at :59 (double ampl marker pulses)
   phase_inv: 0,
   sixmin_mode: 0,
   
   cur: 0,
   cnt: 0,
   width: 0,
   dcnt: 0,
   tick: 0,
   min: 0,
   time0: 0,
   time0_copy: 0,
   line: 0,
   
   end: null
};

function wwvb_dmsg(s)
{
   w3_innerHTML('id-tc-wwvb', s);
}

// see: en.wikipedia.org/wiki/WWVB
function wwvb_legend(phase)
{
   if ((wwvb.line++ & 3) == 0) {
      if (phase)
         tc_dmsg('sync           ppppp m d mmmmmmmmm r mmmmmmmmm r mmmmmmm LL N LLL dddddd<br>');
      else
         tc_dmsg('M mmm0mmmm M00 hh0hhhh M00 dd0ddddMdddd 00 +-+ M DDDD 0 yyyyMyyyy 0 YSDD M<br>');
   }
}

function wwvb_ampl_decode(bits)
{
   // bits are what the minute _was_ at the previous minute boundary
   
   // all given in UTC
   var min  = (tc_gap_bcd(bits, 8,  8, -1) + 1) % 60;
   var hour = tc_gap_bcd(bits, 18,  7, -1);
   var doy  = tc_gap_bcd(bits, 33, 12, -1);
   var yr   = tc_gap_bcd(bits, 53,  9, -1) + 2000;

   var d = kiwi_UTCdoyToDate(doy, yr, hour, min, 0);
   var day = d.getUTCDate().fieldWidth(2);
   var mo = tc.mo[d.getUTCMonth()];

   var s = day +' '+ mo +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' UTC';
   tc_dmsg('  ' + 'day #'+ doy +' '+ s +'<br>');
   tc_stat('lime', 'Time: '+ s);
}

function wwvb_clr()
{
   var w = wwvb;
   
   w.data = w.data_last = w.cur = 0;
   w.arm = 0;
   w.cnt = w.dcnt = 0;
   w.prev_one_width = w.one_width = w.zero_width = 0;
   w.line = 0;
}

// called at 100 Hz (10 msec)
function wwvb_ampl(ampl)
{
	var w = wwvb;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.95)? 1:0;
	ampl = ampl ^ tc.force_rev ^ tc.data_ainv;
	
	// de-noise signal
   if (ampl == w.cur) {
   	w.cnt = 0;
   } else {
   	w.cnt++;
   	if (w.cnt > w.AMPL_NOISE_THRESHOLD) {
   		w.cur = ampl;
   		w.cnt = 0;
	      if (!tc.ref) { w.data = ampl; tc.ref = 1; } else { w.data ^= 1; }
   		if (!w.data) {
   		   //tc_dmsg(w.one_width +' ');
   		   //tc_dmsg((w.prev_one_width +'|'+ w.one_width +' ');
   		   //tc_dmsg('S'+ tc.ACQ_SYNC +' ');
   		   if (tc.state == tc.ACQ_SYNC && w.prev_one_width > 0 && w.prev_one_width < 25 && w.one_width < 25) {
   		      //tc_dmsg('[SYNC] ');
               tc_stat('cyan', 'Found sync');
               tc.trig = -20;
               tc.sample_point = 80;
               w.sec = 0;
               w.msec = 0;
               wwvb_legend(0);
               tc.raw = [];
   		      tc.state = tc.MIN_MARK;
   		   }
   		   w.prev_one_width = w.one_width;
   		}
         if (w.data) w.one_width = 0; else w.zero_width = 0;
   	}
   }

   if (w.data) w.one_width++; else w.zero_width++;

	if (tc.state == tc.MIN_MARK || (tc.state == tc.ACQ_DATA && tc.sample_point == tc.trig)) {
	   var b = (tc.state == tc.MIN_MARK || w.one_width <= 60)? 1:0;
      //tc_dmsg(b.toFixed(0) + w.one_width.toFixed(0) +'|');
      tc_dmsg(b);
      tc.raw[w.sec] = b;
      if ([0,8,11,18,21,33,35,38,39,43,44,53,54,58].includes(w.dcnt)) tc_dmsg(' ');

      if (tc.state == tc.MIN_MARK) {
         w.msec = 990;
	      tc.state = tc.ACQ_DATA;
	   }

      w.dcnt++;
      if (w.dcnt == 60) {
         wwvb_ampl_decode(tc.raw);   // for the minute just reached
         wwvb_legend(0);
         tc.raw = [];
         w.sec = -1;
         w.dcnt = 0;
      }
   }

   w.msec += 10;

   if (w.msec == 1000) {
      w.sec++;
      w.msec = 0;
   }

   tc.data = w.data;
}

function wwvb_phase_sync()
{
	var rl = tc.raw.length;
	var sync = 1;
	for (i = 0; i < 14; i++) {
		if (tc.raw[rl-(14-i)] != wwvb.ph_sync[i]) {
			sync = 0;
			break;
		}
	}
	if (!sync) {
			sync = 2;
			for (i = 0; i < 14; i++) {
				if (tc.raw[rl-(14-i)] != (wwvb.ph_sync[i] ^ 1)) {
					sync = 0;
					break;
				}
			}
	}
	
	if (sync != 0)
      console.log('SYNC'+ ((sync == 2)? '-I':'') +' dlen='+ tc.raw.length);
	
	return sync;
}

// called at 100 Hz (10 msec)
function wwvb_phase(ampl_sgn)
{
	var i, sync, data;
	var w = wwvb;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	if (!tc.ref) { w.data = ampl_sgn; tc.ref = 1; }
	
	// de-noise signal
   if (ampl_sgn == w.cur) {
   	w.cnt = 0;
   } else {
   	w.cnt++;
   	if (w.cnt > w.PHASE_NOISE_THRESHOLD) {
   		w.cur = ampl_sgn;
   		w.cnt = 0;
   		w.data ^= 1;
   	}
   }

	if (w.data) {
		w.width++;
	}
	
	// if we need it, measure width on falling edge w.data
	if (tc.sample_point == -1 && w.data_last != w.data) {
		//tc_dmsg(w.data_last.toFixed(0) + w.data.toFixed(0) +':'+ w.width.toFixed(0) +' ');
		if (w.width > 50 && w.width < 110) {
			//tc.sample_point = tc.trig - Math.round(w.width/2);
			tc.sample_point = tc.trig - 1;
			if (tc.sample_point < 0) tc.sample_point += 100;
			//tc_dmsg('T'+ tc.sample_point.toString() +' ');
			tc.start_point = 1;
		}
		w.width = 0;
	   w.data_last = w.data;
	}
	
	//wwvb_dmsg('sp='+ tc.sample_point +' trig='+ tc.trig);

	if (tc.sample_point == tc.trig) {
		if (tc.state == tc.ACQ_SYNC) {
			tc.raw.push(w.data);
			tc_dmsg(w.data.toString());
			
			if (tc.raw.length >= 14) {
				sync = wwvb_phase_sync();
				w.phase_inv = 0;
				if (sync == 2) w.phase_inv = 1;
				if (sync) {
					tc_stat('cyan', 'Found sync');
					tc_dmsg(' SYNC'+ (w.phase_inv? '-inv<br>':'<br>'));
					wwvb_legend(1);
					tc_dmsg('00011101101000 ');
					w.dcnt = 13;
					tc.state = tc.ACQ_DATA;
					tc.raw = [];
					w.min = 0;
				}
			}

			w.tick++;
			if (w.tick > (60 + w.ph_sync.length + 5)) {
				tc_dmsg(' RESTART<br>');
				tc.sample_point = -1;
				w.width = 0;
				w.tick = 0;
            tc.raw = [];
            w.min = 0;
			}
		} else
		
		if (tc.state == tc.ACQ_DATA) {
			tc.raw.push(w.data);
			data = w.data ^ w.phase_inv;
			//tc_dmsg(w.phase_inv? (data? 'i':'o') : data.toString());
			tc_dmsg(data.toString());
         if ([12,17,18,19,28,29,38,39,46,48,49,52].includes(w.dcnt)) tc_dmsg(' ');
			
			if (w.dcnt == 18 || (w.dcnt >= 20 && w.dcnt <= 46 && w.dcnt != 29 && w.dcnt != 39)) {
				w.min = (w.min << 1) + data;
				//console.log(w.dcnt +': '+ data +' '+ w.min);
			}

			if (w.dcnt == 19) w.time0_copy = data;
			if (w.dcnt == 46) w.time0 = data;
			
			//tc_info('dlen='+ tc.raw.length +' dcnt='+ w.dcnt);
			if (tc.raw.length >= 14 && w.dcnt == 12) {
				sync = wwvb_phase_sync();
				if (!sync) {
					tc_dmsg(' RESYNC<br>');
					tc.state = tc.ACQ_SYNC;
					tc.sample_point = -1;
					w.width = 0;
					w.tick = 0;
				} else
				if ((sync == 1 && w.phase_inv) || (sync == 2 && !w.phase_inv)) {
					tc_dmsg(' PHASE INVERSION<br>00011101101000 ');
					w.phase_inv ^= 1;
				}
			}
			
			// NB: This only works for the 21st century.
			if (w.dcnt == 58) {
				if (w.min && w.time0_copy != w.time0) {
					tc_dmsg(' TIME BIT-0 MISMATCH');
				} else
				if (w.min) {

// minute of century:
// example 6578970(min)/1440(min/day) = 4568.730, 6578970-(1440*4568) = 1050(min)
// for 2012: 365*12 (2000..2011) + 3(LY: 2000, 2004, 2008) = 4383, 4568-4383=185(days)
// 2012: 1/31 + 2/29(LY) + 3/31 + 4/30 + 5/31 + 6/30 = 182 days (#181 base-0), so day #185 = July 4 2012
// 1050(min) = 60*17(hrs) + 30(min), so timecode is between 17:30..17:31 UTC

					//console.log('start '+ w.min);
					var m = w.min - (tc.MpY + tc.MpD);	// 2000 is LY
					m++;		// minute increments just as we print decode
					//console.log('2000: '+ m);
					var yr = 2001;
					var LY;
					while (true) {
						LY = ((yr & 3) == 0);
						var MpY = tc.MpY + (LY? tc.MpD : 0);
						if (m > MpY) {
							m -= MpY;
							//console.log(yr +': '+ MpY +' '+ m);
							yr++;
						} else {
							break;
						}
					}
					//console.log('yr done: '+ m);
					
					var mo = 0;
					while (true) {
						var days = tc.dim[mo];
						if (mo == 1 && LY) days++;
						var MpM = tc.MpD * days;
						//console.log(tc.mo[mo] +': LY='+ LY +' '+ days);
						if (m > MpM) {
							m -= MpM;
							//console.log(tc.mo[mo] +': '+ MpM +' '+ m);
							mo++;
						} else {
							break;
						}
					}
					//console.log('mo done: '+ m +' '+ (m/24));
					var day = Math.floor(m / tc.MpD);
					m -= day * tc.MpD;
					var hour = Math.floor(m / 60);
					m -= hour * 60;
					tc_dmsg(' '+ (day+1) +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ m.leadingZeros(2) +' UTC');
					tc_stat('lime', 'Time: '+ (day+1) +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ m.leadingZeros(2) +' UTC');
					
					if (m == 10 || m == 40) w.sixmin_mode = 1;
				}
				
				tc_dmsg('<br>');
				wwvb_legend(1);
				tc.raw = [];
				w.min = 0;
			}
			
			w.dcnt++;
			if (w.dcnt == 60) w.dcnt = 0;

			if (w.dcnt == 0 && w.sixmin_mode) {
				w.sixmin_mode = 0;
				tc.state = tc.ACQ_DATA2;
			}
		} else

      // six-minute sequence
		if (tc.state == tc.ACQ_DATA2) {
			if (w.dcnt == 0) tc_dmsg('127b: ');
			if (w.dcnt == 127) tc_dmsg('<br>106b: ');
			if (w.dcnt == 233) tc_dmsg('<br>127b: ');

			data = w.data ^ w.phase_inv;
			tc_dmsg(data.toString());

			w.dcnt++;
			if (w.dcnt == 359) {
				tc_dmsg('<br>');
				w.dcnt = 0;
				tc.state = tc.ACQ_DATA;
				tc.raw = [];
				w.min = 0;
			}
		}
	}
	
	return w.data;
}

function wwvb_focus()
{
}


function wwvb_blur()
{
   var el;
	el = w3_el('id-tc-bits');
	if (el) el.innerHTML = '';
	el = w3_el('id-tc-wwvb');
	if (el) el.innerHTML = '';
}


function wwvb_init()
{
   w3_el('id-tc-addon').innerHTML += w3_div('id-tc-bits') + w3_div('id-tc-wwvb');
}
