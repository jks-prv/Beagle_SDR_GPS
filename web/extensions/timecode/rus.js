// Copyright (c) 2017-2024 John Seamons, ZL4VO/KF6VO

//
// TODO
//    parity checking
//

var rus = {
   // ampl
   AMPL_NOISE_THRESHOLD: 5,
   arm: 0,
   data: 0,
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
   // bits are what the minute _will be_ at the approaching minute boundary
   
   var min  = tc_bcd(bits, 59, 7, -1) % 60;
   var hour = tc_bcd(bits, 52, 6, -1);
   var day  = tc_bcd(bits, 46, 6, -1);
   var mo   = tc_bcd(bits, 37, 5, -1) - 1;
   var yr   = tc_bcd(bits, 32, 8, -1) + 2000;

   var s = day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' MSK';
   tc_dmsg('  '+ s +'<br>');
   tc_stat('lime', 'Time: '+ s);
}

function rus_clr()
{
   var r = rus;
   
   r.data = r.cur = 0;
   r.arm = 0;
   r.cnt = r.rcnt = r.dcnt = 0;
   r.prev_zero_width = r.one_width = r.zero_width = 0;
   r.line = 0;
}

// called at 100 Hz (10 msec)
function rus_ampl(ampl)
{
	var r = rus;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.95)? 1:0;
	ampl = ampl ^ tc.force_rev ^ tc.data_ainv;
	r.wait -= 10;
	
	// de-noise signal
   if (ampl == r.cur) {
   	r.cnt = 0;
   } else {
   	r.cnt++;
   	if (r.cnt > r.AMPL_NOISE_THRESHOLD) {
   		r.cur = ampl;
   		r.cnt = 0;
	      if (!tc.ref) { r.data = ampl; tc.ref = 1; } else { r.data ^= 1; }
   		if (r.data)
            r.dat0 = Math.round(r.zero_width/10);
         else
            r.dat1 = Math.round(r.one_width/10);

   		if (r.data) {    // 0 to 1 transition, r.dat0 now valid
   		
   		   if (tc.state == tc.ACQ_SYNC) {
   		   /*
   		      var s = r.dat0 +' ';
   		      tc_dmsg(s);
   		      r.dcnt += s.length;
               if (r.dcnt > 80) { tc_dmsg('<br>'); r.dcnt = 0; }
            */

   		      if (r.dat0 == 5) {
                  //tc_dmsg('[SYNC]<br>');
                  tc_stat('cyan', 'Found sync');
                  tc.trig = 30;
                  tc.sample_point = 30;
                  r.sec = 0;
                  r.msec = 0;
                  rus_legend();
                  tc.raw = [];
                  r.rcnt = 0;
                  tc.state = tc.MIN_MARK;
               }
   		   }
   		   
   		   if (tc.state != tc.ACQ_PHASE && tc.state != tc.ACQ_SYNC) {
   		      if (r.wait <= 0) {
                  //if (r.dat0 > 3) r.dat0 = 1;
                  var b = (r.dat0 >= 2)? 1:0;      // only decodes data bit 1
                  tc.raw[r.rcnt] = b;
                  tc_dmsg(b);
                  if ([0,2,7,10,15,17,23,24,32,37,40,46,52].includes(r.rcnt)) tc_dmsg(' ');
                  r.rcnt++;
                  r.wait = 700;
               }
   		   
   		   }
   		   
   		/*
   		   if (tc.state != tc.ACQ_SYNC && r.dat0 == 5) {
   		      tc_dmsg('<br>'); r.dcnt = 0;
   		   }
   		*/
   		   
   		   r.prev_zero_width = r.zero_width;
   		}

   		if (r.data) r.one_width = 0; else r.zero_width = 0;
   	}
   }

   if (r.data) r.one_width++; else r.zero_width++;

   if (r.rcnt == 60) {
      rus_ampl_decode(tc.raw);   // for the minute just reached
      rus_legend();
      tc.raw = [];
      r.rcnt = 0;
      r.sec = -1;
   }

   r.msec += 10;

   if (r.msec == 1000) {
      r.sec++;
      r.msec = 0;
   }

   tc.data = r.data;
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
