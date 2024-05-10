// Copyright (c) 2017-2024 John Seamons, ZL4VO/KF6VO

//
// TODO
//    parity checking
//

var msf = {
   AMPL_NOISE_THRESHOLD: 3,
   arm: 0,
   data: 0,

   cur: 0,
   sec: 0,
   msec: 0,
   dcnt: 0,
   line: 0,
   phase: 0,
   
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
   // bits are what the minute _will be_ at the approaching minute boundary
   
   var min  = tc_bcd(bits, 51, 7, -1);
   var hour = tc_bcd(bits, 44, 6, -1);
   var day  = tc_bcd(bits, 35, 6, -1);
   var wday = tc_bcd(bits, 38, 3, -1);
   var mo   = tc_bcd(bits, 29, 5, -1) - 1;
   var yr   = tc_bcd(bits, 24, 8, -1) + 2000;

   var s = day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' UTC';
   tc_dmsg('  '+ s +'<br>');
   tc_stat('lime', 'Time: '+ s);
}

function msf_clr()
{
   var m = msf;
   
   m.cur = m.cnt = m.one_width = m.zero_width = 0;
   m.phase ^= 1;
   m.arm = m.no_modulation = m.dcnt = m.modct = m.line = m.sec = m.msec = 0;
}

// called at 100 Hz (10 msec)
function msf_ampl(ampl)
{
	var i;
	var m = msf;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.5)? 1:0;
	ampl = ampl ^ tc.force_rev ^ tc.data_ainv;
	
	// de-noise signal
   if (ampl == m.cur) {
   	m.cnt = 0;
   } else {
   	m.cnt++;
   	if (m.cnt > m.AMPL_NOISE_THRESHOLD) {
   		m.cur = ampl;
   		m.cnt = 0;
   		//if (tc.state == tc.ACQ_SYNC)
   		//   tc_dmsg((m.data? '1':'0') +':'+ (m.data? m.one_width : m.zero_width) +' ');
   		//if (tc.state == tc.ACQ_SYNC && !m.data)
   		//   tc_dmsg(m.zero_width +' ');
	      if (!tc.ref) { m.data = ampl; tc.ref = 1; } else { m.data ^= 1; }
   		if (m.data) m.one_width = 0; else m.zero_width = 0;
   	}
   }

   if (m.data) m.one_width++; else m.zero_width++;
	
	if (tc.state == tc.ACQ_SYNC && m.arm == 0 && m.zero_width >= 40) { m.arm = 1; m.zero_width = 0; }
	if (m.arm == 1 && m.one_width >= 40 && m.data) { m.arm = 2; m.one_width = 0; }
	
	if (m.arm == 2 && m.data == 0) {
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
	   var b = (tc.state == tc.MIN_MARK || m.zero_width > 15)? 1:0;     // inverted
      tc.raw[m.sec] = b;

      if (tc.state == tc.MIN_MARK) {
         m.msec = 990;
	      tc.state = tc.ACQ_DATA;
	   }

      if (1) {
         //tc_dmsg(m.sec.toFixed(0) + b.toFixed(0) +'|');
         tc_dmsg(b);
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

   tc.data = m.data;
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
