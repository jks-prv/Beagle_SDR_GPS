// Copyright (c) 2016 John Seamons, ZL4VO/KF6VO

var gen = {
   ext_name: 'sig_gen',    // NB: must match sig_gen.c:sig_gen.name
   first_time: true,

	freq: 10000,
	freq_stop: 10006,
	step: 0.1,
	dwell: 300,
	attn_dB: 80,
	attn_dB_max: 100,
	attn_ampl: 0,
	filter: 0,

   mode: 1,
   OFF: 0,
   RF: 1,
   AF: 2,
   SELF_TEST: 3,
   SELF_TEST_EN: 1,
   mode_s: [ 'off', 'RF tone', 'AF noise', 'Self test' ],
   
   RF_TONE: 0x01,
   AF_NOISE: 0x02,
   STEST: 0x04,
   
	rf_enable: true,
	sweeping: 0,

	attn_offset_val: 11.6,
	//attn_offset: 1,
	//attn_offset_s: [ 'no offset', 'S-meter' ]
};

function sig_gen_main()
{
	ext_switch_to_client(gen.ext_name, gen.first_time, gen_recv);		// tell server to use us (again)
	if (!gen.first_time)
		gen_auth();
	gen.first_time = false;
}

function gen_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('gen_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('gen_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('gen_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				gen_auth();
				break;

			default:
				console.log('gen_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function gen_auth()
{
	admin_pwd_query(function() { gen_controls_setup(); });
}

function gen_controls_setup()
{
	//gen.attn_offset_s[1] = 'S-meter cal '+ cfg.S_meter_cal +' dB';
   gen.save_freq = gen.freq;
   var do_sweep = 0, do_help = false;
   
	gen.url_params = ext_param();
	var p = gen.url_params;
   if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         var r, v;
         //console.log('i='+ i +' a='+ a);
         if (w3_ext_param('help', a).match) {
            do_help = true;
         } else
         if ((r = w3_ext_param('mode', a)).match) {
            if (isNumber(r.num)) {
               gen.mode = w3_clamp(r.num, 0, gen.mode_s.length-1, 0);
               if (kiwi.ext_clk && gen.mode == gen.SELF_TEST) gen.mode = gen.OFF;
            }
         } else
         if ((r = w3_ext_param('freq', a)).match || (i == 0 && (r = w3_ext_param(null, a)).match)) {
            if (r.has_value) {
               v = r.string_case.parseFloatWithUnits('kM', 1e-3);
               if (isNumber(v)) gen.freq = v;
            }
         } else
         if ((r = w3_ext_param('attn', a)).match) {
            if (isNumber(r.num)) {
               gen.attn_dB = w3_clamp(Math.abs(r.num), 0, gen.attn_dB_max, 80);
            }
         } else
         if ((r = w3_ext_param('stop', a)).match) {
            if (r.has_value) {
               v = r.string_case.parseFloatWithUnits('kM', 1e-3);
               if (isNumber(v)) gen.freq_stop = v;
            }
         } else
         if ((r = w3_ext_param('step', a)).match) {
            if (r.has_value) {
               v = r.string_case.parseFloatWithUnits('kM', 1e-3);
               if (isNumber(v)) gen.step = v;
            }
         } else
         if ((r = w3_ext_param('dwell', a)).match) {
            if (isNumber(r.num)) {
               gen.dwell = r.num;
            }
         } else
         if ((r = w3_ext_param('sweep', a)).match) {
            do_sweep = 1;
         }
      });
   }
   
   gen.attn_offset_val = +kiwi_storeInit('sig_gen_offset', 11.6);
   
	var controls_html =
		w3_div('id-test-controls w3-text-white',
			w3_divs('/w3-tspace-8',
				w3_div('w3-medium w3-text-aqua', '<b>Signal generator</b>'),
				w3_div('', 'All frequencies in kHz'),
            w3_inline('',
               w3_input('w3-padding-small w3-width-90 w3-margin-right', 'Freq', 'gen.freq', gen.freq, 'gen_freq_cb'),
               w3_input('w3-padding-small w3-width-90 w3-margin-right', 'Stop', 'gen.freq_stop', gen.freq_stop, 'gen_stop_cb'),
               w3_input('w3-padding-small w3-width-90 w3-margin-right', 'Step', 'gen.step', gen.step, 'gen_step_cb'),
               w3_input('w3-padding-small w3-width-90', 'Dwell (ms)', 'gen.dwell', gen.dwell, 'w3_num_cb')
            ),
				w3_inline('w3-margin-top/w3-margin-between-16 w3-valign',
               w3_select('id-gen-mode w3-text-red', 'Mode', '', 'gen.mode', gen.mode, gen.mode_s, 'gen_mode_cb'),
				   w3_button('w3-red', '-Step', 'gen_step_up_down_cb', -1),
				   w3_button('w3-green', '+Step', 'gen_step_up_down_cb', +1),
				   w3_button('id-gen-sweep w3-css-yellow', 'Sweep', 'gen_sweep_cb')
				),
				w3_col_percent('w3-margin-top',
               w3_slider('id-gen-attn//', 'Attenuation', 'gen.attn_dB', gen.attn_dB, 0, gen.attn_dB_max, 5, 'gen_attn_cb'), 35,
               w3_div(''), 10,
               w3_div('',
				      //w3_div('', 'Offset attenuation by:'),
                  //w3_select('w3-text-red w3-width-auto', '', '', 'gen.attn_offset', gen.attn_offset, gen.attn_offset_s, 'gen_attn_offset_cb')
                  w3_input('id-gen-offset w3-label-bold//w3-padding-small w3-width-64', 'Offset (dB)', 'gen.attn_offset_val', gen.attn_offset_val, 'gen_attn_val_cb')
               ), 55
            )
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(dbgUs? 575 : 450, 250);
	gen_freq_cb('gen.freq', gen.freq);

	// if no URL "f=" param set freq so signal appears on screen
	// (in case off screen at current zoom level)
	if (kiwi_url_param('f', null, null) == null)
      ext_tune(gen.freq, null, ext_zoom.CUR);

	toggle_or_set_spec(toggle_e.SET, spec.RF);
	spec.saved_audio_comp = ext_get_audio_comp();
	if (spec.saved_audio_comp) ext_set_audio_comp(false, /* NO_WRITE_COOKIE */ true);
	ext_send('SET wf_comp=0');
	if (kiwi.ext_clk) w3_select_set_disabled('id-gen-mode', gen.SELF_TEST, true, 'no self-test available when ext clk used');
	if (do_sweep) gen_sweep_cb();
   if (do_help) ext_help_click();
}

function gen_set(freq, ampl, always)
{
   console.log('gen_set f='+ freq +' ampl='+ ampl +' attn='+ gen.attn_dB +' self_test='+ (gen.SELF_TEST? 1:0));
   //kiwi_trace();
   if (always == true || gen.rf_enable) set_gen((gen.mode == gen.SELF_TEST)? -freq : freq, ampl);
}

function gen_run()
{
   var run = (gen.mode == gen.RF)? gen.RF_TONE : ((gen.mode == gen.AF)? gen.AF_NOISE: ((gen.mode == gen.SELF_TEST)? gen.STEST :0));
   ext_send('SET run='+ run);
}

function gen_mode_cb(path, idx, first)
{
	gen.mode = +idx;
	gen.rf_enable = (gen.mode == gen.RF || gen.mode == gen.SELF_TEST);
	console.log('gen_mode_cb mode='+ gen.mode +' f='+ (gen.rf_enable? gen.freq : 0) +' attn='+ gen.attn_dB);
	if (!gen.rf_enable && gen.sweeping) gen_sweep_cancel();
	gen_attn_cb('gen.attn_dB', gen.attn_dB, true);     // add/remove gen.attn_offset_val depending on mode setting
	gen_set(gen.rf_enable? gen.freq : 0, gen.attn_ampl, true);

   // force CW mode in self-test so S-meter measures self-test carrier!
	if (gen.mode == gen.SELF_TEST)
	   ext_set_mode('cw');

   colormap_update();
   gen_run();
}

function gen_freq_cb(path, val)
{
   // might not be a string if not called from w3_input()
   //console.log('gen_freq_cb: val='+ val +' '+ typeof(val));
	gen.freq = val.toString().parseFloatWithUnits('kM', 1e-3, 3);
	w3_num_cb(path, gen.freq);
	w3_set_value(path, gen.freq);
	
	// to minimize glitch in waterfall peak mode
	// don't switch NCO frequency unless fully attenuated
	if (gen.old_freq) gen_set(gen.old_freq, 0);
	gen_set(gen.freq, 0);
	gen_set(gen.freq, gen.attn_ampl);
	gen.old_freq = gen.freq;
}

function gen_stop_cb(path, val, first)
{
   gen.freq_stop = val.parseFloatWithUnits('kM', 1e-3, 3);
	w3_num_cb(path, gen.freq_stop);
	w3_set_value(path, gen.freq_stop);
}

function gen_step_cb(path, val, first)
{
   console.log('gen_step_cb: val='+ val +' '+ typeof(val));
   gen.step = val.parseFloatWithUnits('kM', 1e-3, 3);
	w3_num_cb(path, gen.step);
	w3_set_value(path, gen.step);
}

function gen_step_up_down_cb(path, sign, first)
{
   var step = parseInt(sign) * gen.step;
   console.log('gen_step_up_down_cb: step='+ step +' '+ typeof(step));
   gen.freq += step;
   gen.freq = w3_clamp(gen.freq, 0, cfg.max_freq? 32e3 : 30e3);
	gen_freq_cb('gen.freq', gen.freq);
}

function gen_sweep_cancel()
{
   kiwi_clearInterval(gen.sweep_interval);
   gen_freq_cb('gen.freq', gen.save_freq);
   w3_button_text('id-gen-sweep', 'Sweep', 'w3-css-yellow', 'w3-red');
   gen.sweeping = 0;
}

function gen_sweep_cb(path, val, first)
{
   if (first) return;
   
   if (!gen.sweeping && gen.rf_enable) {
      gen.save_freq = gen.freq;
      w3_button_text('id-gen-sweep', 'Stop', 'w3-red', 'w3-css-yellow');
      console.log('$ gen_sweep_cb');
      gen.sweep_interval = setInterval(function() {
         var prev = gen.freq;
         gen.freq += gen.step;
	      console.log('gen_sweep_cb '+ prev.toFixed(3) +'|'+ gen.step.toFixed(3) +'|'+ gen.freq.toFixed(3));
	      var stop = ((gen.step > 0 && gen.freq > gen.freq_stop) || (gen.step < 0 && gen.freq < gen.freq_stop));
         if (stop || gen.freq < 0.01 || gen.freq > (cfg.max_freq? 32e3 : 30e3))
            gen_sweep_cancel();
         gen_freq_cb('gen.freq', gen.freq);
      }, gen.dwell);
      gen.sweeping = 1;
   } else {
      gen_sweep_cancel();
   }
}

function gen_attn_cb(path, val, complete)
{
   gen.attn_dB = +val;
	//var dB = gen.attn_dB + ((gen.mode == gen.RF || gen.mode == gen.SELF_TEST)? gen.attn_offset_val : 0);
	var dB = gen.attn_dB - ((gen.mode == gen.RF || gen.mode == gen.SELF_TEST)? gen.attn_offset_val : 0);
	if (dB < 0) dB = 0;
	var attn_ampl = Math.pow(10, -dB/20);     // use the amplitude form since we are multipling a signal
	gen.attn_ampl = Math.round(0x1ffff * attn_ampl);   // hardware gen_attn is 18-bit signed so max pos is 0x1ffff
	console.log('gen_attn gen.attn_dB='+ gen.attn_dB +' attn_offset_val='+ gen.attn_offset_val +' dB='+ dB +' attn_ampl='+ gen.attn_ampl +' / '+ gen.attn_ampl.toHex());
	w3_num_cb(path, gen.attn_dB);
	w3_set_label('Attenuation '+ (-gen.attn_dB).toString() +' dB', path);
	w3_hide2('id-gen-attn', gen.mode == gen.SELF_TEST);
	w3_hide2('id-gen-offset', gen.mode == gen.SELF_TEST);
	
	if (complete) {
		gen_set(gen.freq, gen.attn_ampl);
      ext_send('SET attn='+ gen.attn_ampl.toFixed(0));
		colormap_update();
	}
}

/*
function gen_attn_offset_cb(path, idx, first)
{
   idx = +idx;
   gen.attn_offset_val = (idx == 0)? 0 : cfg.S_meter_cal;
   gen_attn_cb('gen.attn_dB', gen.attn_dB, true);
}
*/

function gen_attn_val_cb(path, val, first)
{
   gen.attn_offset_val = +val;
   gen_attn_cb('gen.attn_dB', gen.attn_dB, true);
   kiwi_storeWrite('sig_gen_offset', gen.attn_offset_val);
}

// hook that is called when controls panel is closed
function sig_gen_blur()
{
	//console.log('### sig_gen_blur');
	gen_set(0, 0, true);
	ext_send('SET run=0');
	if (spec.saved_audio_comp) ext_set_audio_comp(true);
	ext_send('SET wf_comp=1');
   toggle_or_set_spec(toggle_e.SET, spec.NONE);
   setTimeout(function() { wf_autoscale_cb(); }, 1500);
}

function sig_gen_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Signal generator help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               'The signal generator has three modes:' +
               '<ul><li><x1>RF tone</x1> mode replaces the data from the Kiwi ADC with ' +
               'an RF tone from a digital oscillator. This oscillator has limited SFDR, so some distortion products ' +
               'will be noted especially at large attenuation levels.</li>' +
               
               '<li><x1>AF noise</x1> replaces the audio channel with broadband noise with variable attenuation. ' +
               'It is useful for testing some of the internal Kiwi features such as de-emphasis.' +
               
               '<li><x1>Self test</x1> (KiwiSDR 2 and later) The digital oscillator described above is routed as an ' +
               'output to the <x1>EXT CLK &amp; TEST</x1> SMA connector. It is intended to be looped-back to the RF antenna input SMA ' +
               'via a short SMA-to-SMA cable. In this way the Kiwi\'s RF front end and ADC prior to the FPGA can be tested ' +
               'against the known signal source. ' +
               '<a href="http://kiwisdr.com/info#id-self-test" target="_blank">Test result images</a></li></ul>' +
                        
               'URL parameters: <br>' +
               w3_text('|color:orange', '<i>kHz</i> or freq:<i>kHz</i>&nbsp; mode:[<i>0123</i>] &nbsp; attn:<i>dB</i> &nbsp; stop:<i>kHz</i> &nbsp; ' +
               'step:<i>kHz</i> &nbsp; dwell:<i>msecs</i> &nbsp; sweep') +
               
               '<br><br>Frequencies are in kHz and can use the <x1>k</x1> and <x1>M</x1> suffix notation (e.g. 7.1M). ' +
               'The mode numbers 0-3 correspond to the four Mode menu entries.'
            )
         );

      confirmation_show_content(s, 610, 375);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}

function self_test_cb()
{
   kiwi_open_or_reload_page({ qs:'f=10Mcwz0&ext=sig,mode:3', tab:true });
}

// called to display HTML for configuration parameters in admin interface
function sig_gen_config_html()
{
   var s =
      (kiwi.model != kiwi.KiwiSDR_1)?
         w3_divs('w3-container',
            w3_inline('/w3-margin-bottom w3-valign',
               w3_text('w3-medium w3-bold w3-text-teal', 'Self-test function'),
               w3_button('w3-aqua w3-margin-left', 'start self-test', 'self_test_cb'),
               w3_text('w3-medium w3-margin-left w3-text-teal', 'Note: only works with first receiver channel (rx0)')
            ),
            w3_divs('w3-container w3-width-half',
               'To use the self-test function attach the short cable that came with your Kiwi ' +
               'between the <x1>EXT CLK</x1> and <x1>RF IN</x1> connectors. ' +
               'Make sure there are no user connections to your Kiwi (especially on the first channel rx0). ' +
               'Then press the <x1>Self-test function</x1> button above to start the test.<br><br>' +
               
               'A user connection will be made and the signal generator extension opened in self-test mode ' +
               '(this will only work on the first channel rx0). ' +
               'Compare against these sample ' +
               w3_link('w3-link-darker-color', 'kiwisdr.com/info#id-self-test', 'test result images') +
               '. Check at the different zoom levels shown.<br><br>' +
               
               'The generated 10 MHz test signal (plus harmonics and sidebands) is not precise. ' +
               'So the exact levels and waveform characteristics will vary a bit. ' +
               'The goal is to see if the RF front end is responding as it should. ' +
               'The digital attenuator on the main control panel, RF tab, should accurately attenuate the signals ' +
               'shown in the spectrum display and on the S-meter. There may be some lag in the spectrum values ' +
               'adjusting depending on the filtering method in effect (especially for IIR filtering).' +
               ''
            )
         )
      : '';
   
   ext_config_html(gen, 'sig_gen', 'Sig Gen', 'Signal Generator configuration', s);
}
