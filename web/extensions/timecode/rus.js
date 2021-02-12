// Copyright (c) 2017-2021 John Seamons, ZL/KF6VO

//
// TODO
//    inverted amplitude?
//    parity checking
//

var rus = {
   // ampl
   AMPL_NOISE_THRESHOLD: 5,
   arm: 0,
   prev_zero_width: 0,
   wait: 0,
   
   cur: 0,
   cnt: 0,
   dat0: 0,
   dat1: 0,
   rcnt: 0,
   tick: 0,
   min: 0,
   time0: 0,
   time0_copy: 0,
   line: 0,
   dcnt: 0,
   
   end: null
};

function rus_dmsg(s)
{
   w3_innerHTML('id-tc-rus', s);
}

// see: en.wikipedia.org/wiki/RBU_(radio_station)
// see: en.wikipedia.org/wiki/RTZ_(radio_station)
function rus_legend()
{
   if ((rus.line++ & 3) == 0) {
      tc_dmsg('M 00 UUUUU 000 UUUUU 00 TTTTTT 0 yyyyyyyy mmmmm www dddddd hhhhhh mmmmmmm <br>');
   }
}

function rus_ampl_decode(bits)
{
   var min  = tc_gap_bcd(bits, 59, 7, -1, tc.NO_GAPS) % 60;    // what the minute _will be_ at the approaching minute boundary
   var hour = tc_gap_bcd(bits, 52, 6, -1, tc.NO_GAPS);
   var day  = tc_gap_bcd(bits, 46, 6, -1, tc.NO_GAPS);
   var mo   = tc_gap_bcd(bits, 37, 5, -1, tc.NO_GAPS);
   var yr   = tc_gap_bcd(bits, 32, 8, -1, tc.NO_GAPS) + 2000;

   tc_dmsg('  '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' MSK<br>');
   tc_stat('lime', 'Time decoded: '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' MSK');
}

function rus_clr()
{
   var w = rus;
   
   w.arm = 0;
   w.cnt = w.rcnt = w.dcnt = 0;
   w.prev_zero_width = w.one_width = w.zero_width = 0;
   w.line = 0;
}

// called at 100 Hz (10 msec)
function rus_ampl(ampl)
{
	var w = rus;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.95)? 1:0;
	if (!tc.ref) { tc.data = ampl; tc.ref = 1; }
	w.wait -= 10;
	
	// de-noise signal
   if (ampl == w.cur) {
   	w.cnt = 0;
   } else {
   	w.cnt++;
   	if (w.cnt > w.AMPL_NOISE_THRESHOLD) {
   		w.cur = ampl;
   		w.cnt = 0;
   		tc.data ^= 1;
   		if (tc.data)
            w.dat0 = Math.round(w.zero_width/10);
         else
            w.dat1 = Math.round(w.one_width/10);

   		if (tc.data) {    // 0 to 1 transition, w.dat0 now valid
   		
   		   if (tc.state == tc.ACQ_SYNC) {
   		   /*
   		      var s = w.dat0 +' ';
   		      tc_dmsg(s);
   		      w.dcnt += s.length;
               if (w.dcnt > 80) { tc_dmsg('<br>'); w.dcnt = 0; }
            */

   		      if (w.dat0 == 5) {
                  //tc_dmsg('[SYNC]<br>');
                  tc_stat('cyan', 'Found sync');
                  tc.trig = 30;
                  tc.sample_point = 30;
                  w.sec = 0;
                  w.msec = 0;
                  rus_legend();
                  tc.raw = [];
                  w.rcnt = 0;
                  tc.state = tc.MIN_MARK;
               }
   		   }
   		   
   		   if (tc.state != tc.ACQ_SYNC) {
   		      if (w.wait <= 0) {
                  //if (w.dat0 > 3) w.dat0 = 1;
                  var b = (w.dat0 >= 2)? 1:0;      // only decodes data bit 1
                  tc.raw[w.rcnt] = b;
                  tc_dmsg(b);
                  if ([0,2,7,10,15,17,23,24,32,37,40,46,52].includes(w.rcnt)) tc_dmsg(' ');
                  w.rcnt++;
                  w.wait = 700;
               }
   		   
   		   }
   		   
   		/*
   		   if (tc.state != tc.ACQ_SYNC && w.dat0 == 5) {
   		      tc_dmsg('<br>'); w.dcnt = 0;
   		   }
   		*/
   		   
   		   w.prev_zero_width = w.zero_width;
   		}

   		if (tc.data) w.one_width = 0; else w.zero_width = 0;
   	}
   }

   if (tc.data) w.one_width++; else w.zero_width++;

   if (w.rcnt == 60) {
      rus_ampl_decode(tc.raw);   // for the minute just reached
      rus_legend();
      tc.raw = [];
      w.rcnt = 0;
      w.sec = -1;
   }

   w.msec += 10;

   if (w.msec == 1000) {
      w.sec++;
      w.msec = 0;
   }
}

function rus_focus()
{
}


function rus_blur()
{
   var el;
	el = w3_el('id-tc-bits');
	if (el) el.innerHTML = '';
	el = w3_el('id-tc-rus');
	if (el) el.innerHTML = '';
}


function rus_init()
{
   w3_el('id-tc-addon').innerHTML += w3_div('id-tc-bits') + w3_div('id-tc-rus');
}
