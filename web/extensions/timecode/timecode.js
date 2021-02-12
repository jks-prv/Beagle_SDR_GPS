// Copyright (c) 2017-2019 John Seamons, ZL/KF6VO

var tc = {
   ext_name:   'timecode',    // NB: must match timecode.c:timecode_ext.name
   first_time: true,
   update: false,
   start_point: 0,
   ref: 0,
   NO_GAPS: true,
   
   state:      0,
   ACQ_SYNC:   0,
   MIN_MARK:   1,
   ACQ_DATA:   2,
   ACQ_DATA2:  3,

   dbug:       '',
   test:       0,
   
	srate:		0,
	srate_upd:  0,
	pb_cf:      500,
	interval:	0,
	
	_100Hz:     0,
	_10Hz:      0,
	_1Hz:		   0,
	mkr_per:		0,
	mkr:			0,
	
	trig:       0,
	sample_point: -1,
	ampl_last:  0,
	data:       0,
	data_last:  1,
	
   df:         0,
   phase:      0,
   pll_bw:     0,

   config:     0,
   sig:        { JJY40:0,  RTZ:1,   WWVBa:2, WWVBp:3, JJY60:4, MSF:5,   RBU:6,   BPCa:7,  BPCss:8, DCF77a:9,   DCF77ss:10, TDF:11,  WWV:12 },
   freq:       [ 40,       50.1,    60,      60,      60,      60,      66.766,  68.5,    68.5,    77.5,       77.5,       162,     10000.1, ],
   pb:         [ 5,        30,      5,       5,       5,       5,       30,      5,       5,       5,          5,          5,       5 ],
   pll_bw_i:   [ 100,      100,     100,     5,       100,     100,     100,     100,     100,     100,        100,        5,       5 ],
   pll_off_i:  [ 500,      500,     500,     0,       500,     500,     500,     500,     500,     500,        500,        0,       100 ],
   sigid_s:    [ 'jjy',    'rus',   'wwvb',  'wwvb',  'jjy',   'msf',   'rus',   'bpc',   'bpc',   'dcf77',    'dcf77',    'tdf',   'wwv' ],
   prev_sig:   -1,

   sig_s: [
      //                       ena
      [ '40 kHz JJY-40',         1, 'JJY',      'https://en.wikipedia.org/wiki/JJY' ],
      [ '50 kHz RTZ',            1, 'RTZ',      'https://en.wikipedia.org/wiki/RTZ_(radio_station)' ],
      [ '60 kHz WWVB-ampl',      1, 'WWVB',     'https://en.wikipedia.org/wiki/WWVB' ],
      [ '60 kHz WWVB-phase',     1, 'WWVB',     'https://en.wikipedia.org/wiki/WWVB' ],
      [ '60 kHz JJY-60',         1, 'JJY',      'https://en.wikipedia.org/wiki/JJY' ],
      [ '60 kHz MSF',            1, 'MSF',      'https://en.wikipedia.org/wiki/Time_from_NPL_(MSF)' ],
      [ '66.666 kHz RBU',        1, 'RBU',      'https://en.wikipedia.org/wiki/RBU_(radio_station)' ],
      [ '68.5 kHz BPC-ampl',     1, 'BPC',      'https://en.wikipedia.org/wiki/BPC_(time_signal)' ],
      [ '68.5 kHz BPC-ss',       0, 'BPC',      'https://en.wikipedia.org/wiki/BPC_(time_signal)' ],
      [ '77.5 kHz DCF77-ampl',   1, 'DCF77',    'https://en.wikipedia.org/wiki/DCF77' ],
      [ '77.5 kHz DCF77-ss',     0, 'DCF77',    'https://en.wikipedia.org/wiki/DCF77' ],
      [ '162 kHz TDF',           1, 'TDF',      'https://en.wikipedia.org/wiki/TDF_time_signal' ],
      [ 'WWV/WWVH',              0, 'WWV/WWVH', 'https://en.wikipedia.org/wiki/WWV_(radio_station)' ],
      [ 'EFR Teleswitch (FSK)',  2, 'EFR',      'https://www.efr.de/en/efr-system/#/Technical-Data-forTransmitter-Stations' ],
      [ 'CHU (FSK)',             2, 'CHU',      'https://en.wikipedia.org/wiki/CHU_(radio_station)' ]
   ],
   
   tct: {
	   bs:   [],
	   bp:   [],
	   prev: 0,
	   ct:   0,
   },
   
   //     jan feb mar apr may jun jul aug sep oct nov dec
   dim:  [ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ],
   mo:   [ 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec' ],
   MpD:  24 * 60,
   MpY:  24 * 60 * 365,
   
   // used by individual decoders
   raw: [],
   no_modulation: 0,

   end: null
};

function timecode_main()
{
	ext_switch_to_client(tc.ext_name, tc.first_time, tc_recv);		// tell server to use us (again)
	if (!tc.first_time)
		tc_controls_setup();
	tc.first_time = false;
}

function tc_dmsg(s)
{
	if (s == undefined)
		tc.dbug = '';
	else
		tc.dbug += s;
	w3_el('id-tc-dbug').innerHTML = tc.dbug;
}

function tc_info(s)
{
	w3_el('id-tc-info').innerHTML = s;
}

function tc_stat(color, s)
{
   w3_el('id-tc-status').style.color = color;
   w3_el('id-tc-status').innerHTML = s;
}

function tc_stat2(color, s)
{
   w3_el('id-tc-status2').style.color = color;
   w3_el('id-tc-status2').innerHTML = s;
}

function tc_bcd(bits, offset, n_bits, dir)
{
   var val = 0;
   for (var i=0; i < n_bits; i++) val += bits[offset + (dir*i)]? [1,2,4,8,10,20,40,80][i] : 0;
   return val;
}

function tc_gap_bcd(bits, offset, n_bits, dir, no_gaps)
{
   var val = 0;
   var seq = (no_gaps && no_gaps == tc.NO_GAPS)? [1,2,4,8,10,20,40,80,100,200] : [1,2,4,8,0,10,20,40,80,0,100,200];
   for (var i=0; i < n_bits; i++) val += bits[offset + (dir*i)]? seq[i] : 0;
   return val;
}

// test function
function tc_test(samp, ph, slice)
{
	var tct = tc.tct;
	tct.bs.push(samp);
	tct.bp.push(ph);
	if (tct.bs.length > 500) { tct.bs.shift(); tct.bp.shift(); }
	
	if (tct.prev == 0 && slice == 1) {
		tct.ct++;
		if (tct.ct == 1) {
			scope_clr();
			var l = tct.bs.length;
			console.log('len='+ l);
			var tk=0;
			for (var k=0; k<l; k++) {
				var s = tct.bs[k]/10000;
				var jph = tct.bp[k];
				var mkr = undefined;
				if (tk >= 0) {
					tk -= 19.2;
					mkr = 2;
				}
				tk += 1;
				
				for (var m=0; m<120; m++) scope_draw(s, mkr, undefined, jph);
			}
		}
	}
	tct.prev = slice;
}


function tc_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var nsamps = ba.length;

		if (cmd == 0) {
         for (var i = 1; i < nsamps; i++) {
            var sample = ba[i] - 127;

            var ampl = -sample / 127.0;
            var ampl_abs = Math.abs(ampl);
            var ampl_sgn = (ampl > 0)? 1 : 0;

            if (tc.config == tc.sig.TDF) ampl *= 4;
            var ampl_t = ampl + (tc.ampl_last * 0.99);
            var ampl_offset_removed = ampl_t - tc.ampl_last;
            tc.ampl_last = ampl_t;

            var decision = undefined;
            tc._100Hz += 100 / tc.srate;
            
            // call decoding routines at 100 Hz (10 msec)
            if (tc._100Hz > 1) {
            
               switch (tc.config) {
         
               case tc.sig.WWVBa:
               case tc.sig.WWV:
                  decision = wwvb_ampl(ampl_abs);
                  break;
         
               case tc.sig.WWVBp:
                  decision = wwvb_phase(ampl_sgn);
                  break;
         
               case tc.sig.DCF77a:
                  decision = dcf77_ampl(ampl_abs);
                  break;
         
               case tc.sig.TDF:
                  decision = tdf_phase(ampl_offset_removed);
                  break;
      
               case tc.sig.MSF:
                  decision = msf_ampl(ampl_abs);
                  break;
         
               case tc.sig.BPCa:
                  decision = bpc_ampl(ampl_abs);
                  break;
         
               case tc.sig.JJY40:
               case tc.sig.JJY60:
                  decision = jjy_ampl(ampl_abs);
                  break;
         
               case tc.sig.RBU:
               case tc.sig.RTZ:
                  decision = rus_ampl(ampl_abs);
                  break;
         
               default:
                  break;
         
               }
               
               if (tc._10Hz++ >= 10) {
               /*
                  tc_info(
                     'mix '+ tc.pll.mix_re.toFixed(2).fieldWidth(10) +' '+ tc.pll.mix_im.toFixed(2).fieldWidth(10) +
                     ' lpf '+ tc.pll.lpf_re.toFixed(2).fieldWidth(10) +' '+ tc.pll.lpf_im.toFixed(2).fieldWidth(10) +
                     ' err '+ tc.pll.phase_err.toFixed(2).fieldWidth(6)
                  );
               */
                  tc._10Hz = 0;
               }
               
               if (tc._1Hz++ >= 100 && tc.sig_s[tc.config][1] != 2) {
                  timecode_update_srate();
                  var s = 'cf '+ tc.pb_cf;
                  s += ', pll: '+ tc.df.toFixed(2).withSign() +' Hz ';
                  s += tc.phase.toFixed(2).withSign() +' &phi; ';
                  s += ', srate '+ tc.srate.toFixed(2) +' ('+ tc.srate_upd +'), clock '+ (ext_adc_clock_Hz()/1e6).toFixed(6) +' ('+ ext_adc_gps_clock_corr() +')';
                  tc_stat2('orange', s);
                  tc._1Hz = 0;
               }
            
               tc._100Hz -= 1;
            }
      
            // for now, plot every sample so we can see glitches
            switch (tc.config) {
      
            case tc.sig.WWV:
               scope_draw(
                  /* cyan */ (tc.trig <= tc.sample_point),
                  /* red  */ ampl_abs,
                  /* org  */ undefined,
                  /* blk  */ tc.data? -0.3 : -0.6,
                  /* lime */ 0,
                  /* trig */ 0
               );
               break;
      
            case tc.sig.WWVBa:
               scope_draw(
                  /* cyan */ (tc.trig >= 0 && tc.trig <= tc.sample_point),
                  /* red  */ ampl_abs,
                  /* org  */ undefined,
                  /* blk  */ tc.data? -0.3 : -0.6,
                  /* lime */ 0,
                  /* trig */ 0
               );
               break;
      
            case tc.sig.WWVBp:
               scope_draw(
                  /* cyan */ ampl_sgn,
                  /* red  */ (tc.trig == tc.sample_point)? { num:0.1, color:'blue' } : ampl_abs,
                  /* org  */ ampl >= 0? undefined : ampl,
                  /* blk  */ (tc.trig == tc.sample_point && isDefined(decision))? ((decision > 0)? 0.5:-0.5) : undefined,
                  // /* lime */ tc.start_point? 0 : { num:0, color:'purple' },
                  /* lime */ tc.data? 0 : { num:0, color:'purple' },
                  /* trig */ tc.trig == tc.sample_point
               );
               break;
      
            case tc.sig.DCF77a:
               scope_draw(
                  /* cyan */ (tc.trig <= tc.sample_point),
                  /* red  */ ampl_abs,
                  /* org  */ undefined,
                  /* blk  */ tc.data? -0.3 : -0.6,
                  /* lime */ 0,
                  /* trig */ 0
               );
               break;
      
            case tc.sig.TDF:
               scope_draw(
                  /* cyan */ 0,
                  /* red  */ (Math.abs(ampl_offset_removed) >= 0.3)?
                                 ampl_offset_removed : { num:ampl_offset_removed, color:'orange' },
                  /* org  */ (tc.trig == 1)? { num:1, color:'purple' } : undefined,
                  /* blk  */ (tc.trig > 1)? ((tc.trig == 2)? 0.7 : 0.9) : (tc.no_modulation? -0.9 : -0.7),
                  /* lime */ 0,
                  /* trig */ tc.mkr? ((tc.mkr == 1)? 1 : { num:1, color:'blue' }) : 0
               );
               tc.trig = 0;
               tc.mkr = 0;
               break;
      
            case tc.sig.MSF:
               scope_draw(
                  /* cyan */ (tc.trig <= tc.sample_point),
                  /* red  */ ampl_abs,
                  /* org  */ undefined,
                  /* blk  */ tc.data? -0.6 : -0.3,    // inverted
                  /* lime */ 0,
                  /* trig */ 0
               );
               break;
      
            case tc.sig.BPCa:
               scope_draw(
                  /* cyan */ (tc.trig >= 0 && tc.trig <= tc.sample_point),
                  /* red  */ ampl_abs,
                  /* org  */ undefined,
                  /* blk  */ tc.data? -0.6 : -0.3,    // inverted
                  /* lime */ 0,
                  /* trig */ 0
               );
               break;
      
            case tc.sig.JJY40:
            case tc.sig.JJY60:
               scope_draw(
                  /* cyan */ (tc.trig >= 0 && tc.trig <= tc.sample_point),
                  /* red  */ ampl_abs,
                  /* org  */ undefined,
                  /* blk  */ tc.data? -0.3 : -0.6,
                  /* lime */ 0,
                  /* trig */ 0
               );
               break;
      
            case tc.sig.RBU:
            case tc.sig.RTZ:
               scope_draw(
                  /* cyan */ (tc.trig >= 0 && tc.trig <= tc.sample_point),
                  /* red  */ ampl_abs,
                  /* org  */ undefined,
                  /* blk  */ tc.data? -0.3 : -0.6,
                  /* lime */ 0,
                  /* trig */ 0
               );
               break;
      
            default:
               break;
            }
      
            //if (tc.mkr) tc.mkr--;
         }
		} else {
			console.log('tc_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		}
		
		return;
	}

	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('tc_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('tc_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
            kiwi_load_js_dir('extensions/timecode/',
               ['wwvb.js', 'jjy.js', 'msf.js', 'dcf77.js', 'tdf.js', 'bpc.js', 'rus.js', '../../pkgs/js/scope.js'],
               'tc_controls_setup');
				break;

			case "df":
				tc.df = parseFloat(param[1]);
				break;

			case "phase":
				tc.phase = parseFloat(param[1]);
				break;

			default:
				console.log('tc_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function tc_controls_setup()
{
   var data_html =
      time_display_html('tc') +

		w3_div('id-tc-data|width:1024px; height:200px; background-color:black; position:relative;',
			'<canvas id="id-tc-scope" width="1024" height="200" style="position:absolute"></canvas>'
		);
	
   tc.save_mode = ext_get_mode();
   tc.save_pb = ext_get_passband();
   tc.save_agc_delay = ext_agc_delay(5000);
	tc.config = tc.sig.WWVBp;
   tc.pll_bw = tc.pll_bw_i[tc.config];

	var controls_html =
		w3_div('id-tc-controls w3-text-white|height:100%',
			w3_col_percent('',
				w3_div('w3-medium w3-text-aqua', '<b>Time station decoder</b>'), 25,
				w3_div('id-tc-no-audio w3-text-css-lime w3-hide',
					'No audio will be heard due to zero-IF mode used'
				), 60
			),
			w3_inline('w3-margin-T-8/w3-margin-right',
			   w3_select_conditional('w3-text-red', '', '', 'tc.config', tc.config, tc.sig_s, 'tc_signal_menu_cb'),
            w3_button('w3-padding-small w3-css-yellow', 'Re-sync', 'timecode_resync_cb'),
            w3_button('w3-padding-small w3-aqua', 'Reset PLL', 'timecode_reset_pll_cb'),
            //w3_checkbox('w3-label-inline w3-label-not-bold/', 'update Kiwi<br>date &amp; time', 'tc.update', tc.update, 'w3_bool_cb'),
			   w3_input('w3-padding-tiny w3-label-inline w3-label-not-bold|width:auto|size=3', 'pll bw:', 'tc.pll_bw', tc.pll_bw, 'timecode_pll_bw_cb'),
            w3_button('w3-padding-small w3-aqua', 'Test', 'timecode_test_cb'),
				w3_div('', '<pre id="id-tc-info" style="margin:0"></pre>')
			),
			w3_inline('w3-margin-T-4/w3-margin-right',
				w3_div('id-tc-status w3-show-inline-block'),
				w3_div('id-tc-status2 w3-show-inline-block')
			),
			w3_div('id-tc-addon'),
         w3_div('w3-scroll w3-margin-TB-8 w3-grey-white|height:70%', '<pre id="id-tc-dbug"></pre>')
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('tc');
	tc.sigid_s.forEach(function(sig) { w3_call(sig +'_init'); });
	tc_environment_changed( {resize:1} );
	
	ext_set_controls_width_height(900);
	
	var p = ext_param();
	if (p) {
	   var i;
	   p = p.split(',')[0];
      p = p.toLowerCase();
      for (i = 0; i < tc.sig_s.length; i++) {
         if (tc.sig_s[i][1] && tc.sig_s[i][0].toLowerCase().includes(p))
            break;
      }
      if (i < tc.sig_s.length) {
         tc.config = i;
         w3_select_value('tc.config', i);
      }
   }

	ext_send('SET draw=0');
	ext_send('SET display_mode=0');     // IQ
	ext_send('SET gain=0');
   tc_signal_menu_cb('tc.config', tc.config, false);
	ext_send('SET run=1');
}

function tc_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-tc-data');
	var left = (window.innerWidth - 1024 - time_display_width()) / 2;
	el.style.left = px(left);
}

function tc_signal_menu_cb(path, val, first)
{
   //console.log('tc_signal_menu_cb val='+ val +' first='+ first);
   if (first) return;
	val = +val;
	tc.config = val;
	w3_select_value(path, val);
	tc_info(w3_link('', tc.sig_s[tc.config][3], 'Time station info: '+ tc.sig_s[tc.config][2]));
   w3_call(tc.sigid_s[tc.prev_sig] +'_blur');
   
   if (tc.sig_s[tc.config][1] == 2) {
	   scope_clr();
	   tc_stat('', '');
	   tc_stat2('', '');
      w3_hide('id-tc-no-audio');
      tc_dmsg();
      tc_dmsg('This station can be decoded using the FSK extension (see "Ham/Utility" menu).');
      return;
   }

   tc.prev_sig = val;
   timecode_update_srate();
	var phase_mode = (tc.pll_off_i[tc.config] == 0)? 1:0;
   w3_show_hide('id-tc-no-audio', phase_mode);
	
	// defaults
   ext_tune(tc.freq[val], 'cwn', ext_zoom.ABS, 12);
   var cwo = tc.pll_off_i[tc.config];
   ext_set_passband(cwo-tc.pb[val]/2, cwo+tc.pb[val]/2);
   ext_tune(tc.freq[val], 'cwn', ext_zoom.ABS, 12);      // in cw mode have to set freq again after pb change

   tc.pll_bw = tc.pll_bw_i[tc.config];
   timecode_pll_bw_cb('tc.pll_bw', tc.pll_bw, true, false);
	ext_send('SET pll_offset='+ cwo);
	ext_send('SET pll_mode=1 arg='+ (phase_mode? 2:1));   // PLL on, mode: carrier=1, BPSK=2

	switch (val) {

	case tc.sig.WWVBa:
	case tc.sig.WWVBp:
	case tc.sig.WWV:
		wwvb_focus();
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 10,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;

	case tc.sig.DCF77a:
		dcf77_focus();
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 10,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;

	case tc.sig.MSF:
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 10,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;

	case tc.sig.BPCa:
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 20,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;

	case tc.sig.JJY40:
	case tc.sig.JJY60:
		jjy_focus();
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 10,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;

	case tc.sig.RBU:
	case tc.sig.RTZ:
		rus_focus();
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 10,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;

	case tc.sig.TDF:
		tdf_focus();
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 3,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;
	
	default:
	   scope_init(w3_el('id-tc-scope'), {
         sec_per_sweep: 1,
         srate: tc.srate,
         single_shot: 0,
         background_color: '#f5f5f5'
      });
		break;

	}
	
	timecode_resync_cb();
   var pb = ext_get_passband();
   tc.pb_cf = pb.low + (pb.high - pb.low)/2;
}

function timecode_resync_cb(path, val)
{
	tc_dmsg();
	tc_stat('yellow',
	   w3_inline('',
	      w3_icon('w3-text-aqua', 'fa-cog fa-spin', 20),
	      w3_div('w3-margin-L-8 w3-text-css-yellow', 'Looking for sync')
	   )
	);
	
	tc.raw = [];
	tc.state = tc.ACQ_SYNC;
	tc.sample_point = -1;
	tc.data_last = tc.data = 0;
	tc.ref = 0;
   w3_call(tc.sigid_s[tc.config] +'_clr');
	scope_clr();
	if (tc.test) ext_send('SET test');
}

function timecode_reset_pll_cb(path, val)
{
	ext_send('SET clear');
}

function timecode_pll_bw_cb(path, val, complete, first)
{
   console.log('timecode_pll_bw_cb path='+ path +' val='+ val +' first='+ first);
   if (first) return;
   val = +val;
   tc.pll_bw = val;
	w3_set_value(path, val);
	ext_send('SET pll_bandwidth='+ val);
}

function timecode_test_cb(path, val, first)
{
   if (first) return;
   tc.test = 1;
	ext_send('SET test');
}

function timecode_update_srate()
{
   var srate = ext_sample_rate();
   if (tc.srate != srate) {
      //console.log('timecode_update_srate');
      tc.srate = srate;
      tc.srate_upd++;
   }
}

function timecode_blur()
{
	ext_send('SET run=0');
	kiwi_clearInterval(tc.interval);
   ext_set_passband(tc.save_pb.low, tc.save_pb.high);
   ext_agc_delay(tc.save_agc_delay);
}

// called to display HTML for configuration parameters in admin interface
function timecode_config_html()
{
   ext_config_html(tc, 'timecode', 'Timecode', 'Timecode decoder configuration');
}
