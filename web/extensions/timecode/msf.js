// Copyright (c) 2017-2019 John Seamons, ZL/KF6VO

//
// TODO
//    inverted amplitude
//    parity checking
//

var msf = {
   arm: 0,
   NOISE_THRESHOLD: 3,
   cur: 0,
   sec: 0,
   msec: 0,
   dcnt: 0,
   line: 0,
   
   end: null
};

function msf_dmsg(s)
{
   w3_innerHTML('id-tc-msf', s);
}

// see: en.wikipedia.org/wiki/Time_from_NPL_(MSF)
function msf_legend()
{
   if ((msf.line++ & 3) == 0)
      tc_dmsg('1 uuuuuuuuuuuuuuuu yyyyyyyy mmmmm dddddd www hhhhhh mmmmmmm 01111110<br>');
}

function msf_decode(bits)
{
   /*
      tc_dmsg('<br>');
      for (var i=0; i < bits.length; i++) {
         tc_dmsg(bits[i].toFixed(0));
         if ([0,16,24,29,35,38,44,51].includes(i)) tc_dmsg(' ');
      }
      tc_dmsg(' #'+ bits.length +'<br>');
   */
   
   var min  = tc_bcd(bits, 51, 7, -1);    // what the minute _will be_ at the approaching minute boundary
   var hour = tc_bcd(bits, 44, 6, -1);
   var day  = tc_bcd(bits, 35, 6, -1);
   var wday = tc_bcd(bits, 38, 3, -1);
   var mo   = tc_bcd(bits, 29, 5, -1) - 1;
   var yr   = tc_bcd(bits, 24, 8, -1) + 2000;

   tc_dmsg('  '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' UTC<br>');
   tc_stat('lime', 'Time decoded: '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' UTC');
}

function msf_clr()
{
   var m = msf;
   
   m.cur = m.cnt = m.one_width = m.zero_width = 0;
   m.arm = m.no_modulation = m.dcnt = m.modct = m.line = m.sec = m.msec = 0;
}

// called at 100 Hz (10 msec)
function msf_ampl(ampl)
{
	var i;
	var m = msf;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.5)? 1:0;
	if (!tc.ref) { tc.data = ampl; tc.ref = 1; }
	
	// de-noise signal
   if (ampl == m.cur) {
   	m.cnt = 0;
   } else {
   	m.cnt++;
   	if (m.cnt > m.NOISE_THRESHOLD) {
   		m.cur = ampl;
   		m.cnt = 0;
   		//if (tc.state == tc.ACQ_SYNC)
   		//   tc_dmsg((tc.data? '1':'0') +':'+ (tc.data? m.one_width : m.zero_width) +' ');
   		//if (tc.state == tc.ACQ_SYNC && !tc.data)
   		//   tc_dmsg(m.zero_width +' ');
   		tc.data ^= 1;
   		if (tc.data) m.one_width = 0; else m.zero_width = 0;
   	}
   }

   if (tc.data) m.one_width++; else m.zero_width++;
	
	if (tc.state == tc.ACQ_SYNC && m.arm == 0 && m.zero_width >= 40) { m.arm = 1; m.zero_width = 0; }
	if (m.arm == 1 && m.one_width >= 40 && tc.data) { m.arm = 2; m.one_width = 0; }
	
	if (m.arm == 2 && tc.data == 0) {
	   tc.trig = 0;
      tc.sample_point = 25;
      m.sec = 0;
      m.msec = 0;
      if (tc.state == tc.ACQ_SYNC) {
         tc_stat('cyan', 'Found sync');
         //tc_dmsg('SYNC<br>');
      }
      msf_legend();
      tc.raw = [];
      tc.state = tc.MIN_MARK;
	   m.arm = 0;
	}
	
	if (tc.state == tc.MIN_MARK || (tc.state == tc.ACQ_DATA && tc.sample_point == tc.trig)) {
	   var a_bit = (tc.state == tc.MIN_MARK || m.zero_width > 15)? 1:0;     // inverted
      tc.raw[m.sec] = a_bit;

      if (tc.state == tc.MIN_MARK) {
         m.msec = 990;
	      tc.state = tc.ACQ_DATA;
	   }

      if (1) {
         //tc_dmsg(m.sec.toFixed(0) + a_bit.toFixed(0) +'|');
         tc_dmsg(a_bit);
         if ([0,16,24,29,35,38,44,51].includes(m.dcnt)) tc_dmsg(' ');
      } else {
         tc_dmsg(m.zero_width +' ');
      }

      m.dcnt++;
      if (m.dcnt == 60) {
         msf_decode(tc.raw);   // for the minute just reached
         msf_legend();
         tc.raw = [];
         m.sec = -1;
         m.dcnt = 0;
      }
   }

   m.msec += 10;

   if (m.msec == 1000) {
      m.sec++;
      m.msec = 0;
   }
}

function msf_focus()
{
}


function msf_blur()
{
   var el;
	el = w3_el('id-tc-bits');
	if (el) el.innerHTML = '';
	el = w3_el('id-tc-msf');
	if (el) el.innerHTML = '';
}


function msf_init()
{
   w3_el('id-tc-addon').innerHTML += w3_div('id-tc-bits') + w3_div('id-tc-msf');
}
