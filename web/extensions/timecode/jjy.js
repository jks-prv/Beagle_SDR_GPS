// Copyright (c) 2017-2024 John Seamons, ZL4VO/KF6VO

//
// TODO
//    parity checking
//

var jjy = {
   // ampl
   AMPL_NOISE_THRESHOLD: 3,
   arm: 0,
   prev_zero_width: 0,
   
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

function jjy_dmsg(s)
{
   w3_innerHTML('id-tc-jjy', s);
}

// see: en.wikipedia.org/wiki/JJY
function jjy_legend()
{
   if ((jjy.line++ & 3) == 0) {
      tc_dmsg('M mmm0mmmm M00 hh0hhhh M00 dd0ddddMdddd 00 ppU MU yyyyyyyy M www LS 0000 M<br>');
   }
}

function jjy_ampl_decode(bits)
{
   // bits are what the minute _was_ at the previous minute boundary
   
   // all given in JST (UTC + 9)
   var min  = (tc_gap_bcd(bits, 8,  8, -1) + 1) % 60;
   var hour = tc_gap_bcd(bits, 18,  7, -1);
   var doy  = tc_gap_bcd(bits, 33, 12, -1);
   var yr   = tc_bcd(bits, 48,  8, -1) + 2000;
   
   // find correct day and month from JST day-of-year
   var d = kiwi_UTCdoyToDate(doy, yr, hour, min, 0);
   d = new Date(d.getTime() - 9*60*60*1000);  // convert JST to UTC (-9 hours)
   var st = d.toLocaleString("en-US", {timeZone:"Japan"});
   st = st.split('/');
   var mo = tc.mo[+st[0]-1];
   var day = st[1].fieldWidth(2);

   var s = day +' '+ mo +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' JST';
   tc_dmsg('  ' + 'day #'+ doy +' '+ s +'<br>');
   tc_stat('lime', 'Time: '+ s);
}

function jjy_clr()
{
   var w = jjy;
   
   w.data = w.cur = 0;
   w.arm = 0;
   w.cnt = w.dcnt = 0;
   w.prev_zero_width = w.one_width = w.zero_width = 0;
   w.line = 0;
}

// called at 100 Hz (10 msec)
function jjy_ampl(ampl)
{
	var w = jjy;
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
   		if (w.data) {
   		   //if (tc.state == tc.ACQ_SYNC) tc_dmsg(w.zero_width +' ');
   		   if (tc.state == tc.ACQ_SYNC && w.prev_zero_width > 0 && w.prev_zero_width > 75 && w.zero_width > 55) {
   		      //tc_dmsg('[SYNC]<br>');
               tc_stat('cyan', 'Found sync');
               tc.trig = -20;
               tc.sample_point = 80;
               w.sec = 0;
               w.msec = 0;
               jjy_legend();
               tc.raw = [];
   		      tc.state = tc.MIN_MARK;
   		   }
   		   w.prev_zero_width = w.zero_width;
   		}
   		if (w.data) w.one_width = 0; else w.zero_width = 0;
   	}
   }

   if (w.data) w.one_width++; else w.zero_width++;

	if (tc.state == tc.MIN_MARK || (tc.state == tc.ACQ_DATA && tc.sample_point == tc.trig)) {
	   var b = (tc.state == tc.MIN_MARK || w.one_width <= 60)? 1:0;
      //tc_dmsg(b.toFixed(0) + w.one_width.toFixed(0) +'|');
      tc_dmsg(b);
      tc.raw[w.sec] = b;    // raw, not inversion-corrected data
      if ([0,8,11,18,21,33,35,38,40,48,49,52,54,58].includes(w.dcnt)) tc_dmsg(' ');

      if (tc.state == tc.MIN_MARK) {
         w.msec = 990;
	      tc.state = tc.ACQ_DATA;
	   }

      w.dcnt++;
      if (w.dcnt == 60) {
         jjy_ampl_decode(tc.raw);   // for the minute just reached
         jjy_legend();
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

function jjy_focus()
{
}


function jjy_blur()
{
   var el;
	el = w3_el('id-tc-bits');
	if (el) el.innerHTML = '';
	el = w3_el('id-tc-jjy');
	if (el) el.innerHTML = '';
}


function jjy_init()
{
   w3_el('id-tc-addon').innerHTML += w3_div('id-tc-bits') + w3_div('id-tc-jjy');
}
