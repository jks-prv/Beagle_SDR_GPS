// Copyright (c) 2017 John Seamons, ZL/KF6VO

var noise_blank = {
   ext_name: 'noise_blank',     // NB: must match noise_blank.cpp:noise_blank_ext.name
   first_time: true,
   
   algo: 0,
   algo_s: [ '(none)', 'standard', 'Wild algo' ],
   width: 400,
   height: [ 185, 310, 350 ],
   
   NB_OFF: 0,
   blanker: 0,
   test: 0,
   test_s: [ 'test off', 'test on: pre filter (std)', 'test on: post filter (Wild)' ],
   test_gain: 0,
   test_width: 1,
   wf: 1,
   
   // type
   NB_BLANKER: 0,
   NB_WF: 1,
   NB_CLICK: 2,

   NB_STD: 1,
   gate: 100,
   threshold: 50,
   
   NB_WILD: 2,
   thresh: 0.95,
   taps: 10,
   impulse_samples: 7,     // must be odd
};

function noise_blank_main()
{
	ext_switch_to_client(noise_blank.ext_name, noise_blank.first_time, noise_blank_recv);		// tell server to use us (again)
	if (!noise_blank.first_time)
		noise_blank_controls_setup();
	noise_blank.first_time = false;
}

function noise_blank_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('noise_blank_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('noise_blank_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('noise_blank_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				noise_blank_controls_setup();
				break;

			default:
				console.log('noise_blank_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function noise_blank_controls_html()
{
   var s = '';
   
   switch (noise_blank.algo) {
   
   case noise_blank.NB_STD:
      s =
         w3_slider('', 'Gate', 'noise_blank.gate', noise_blank.gate, 100, 5000, 100, 'noise_blank_gate_cb') +
         w3_slider('', 'Threshold', 'noise_blank.threshold', noise_blank.threshold, 0, 100, 1, 'noise_blank_threshold_cb');
      break;
   
   case noise_blank.NB_WILD:
      if (ext_is_IQ_or_stereo_curmode())
         s = 'No Wild algorithm blanking in IQ or stereo modes';
      else
         s =
            w3_slider('', 'Threshold', 'noise_blank.thresh', noise_blank.thresh, 0.05, 3, 0.05, 'noise_blank_thresh_cb') +
            w3_slider('', 'Taps', 'noise_blank.taps', noise_blank.taps, 6, 40, 1, 'noise_blank_taps_cb') +
            w3_slider('', 'Samples', 'noise_blank.impulse_samples', noise_blank.impulse_samples, 3, 41, 2, 'noise_blank_impulse_samples_cb');
      break;
   }
   
	var controls_html =
		w3_div('id-noise-blanker-controls w3-text-white',
			w3_divs('/w3-tspace-8',
				w3_inline('w3-halign-space-between|width:75%/',
				   w3_div('w3-medium w3-text-aqua', '<b>Noise blanker: </b>'),
				   w3_div('w3-text-white', noise_blank.algo_s[noise_blank.algo]),
				   w3_button('w3-padding-tiny w3-aqua', 'Defaults', 'noise_blank_load_defaults')
				),
				w3_inline('w3-margin-between-32',
               w3_select('w3-text-red', '', 'test mode', 'noise_blank.test', noise_blank.test, noise_blank.test_s, 'noise_blank_test_cb'),
               w3_checkbox('w3-label-inline w3-text-css-orange/', 'Also blank WF', 'noise_blank.wf', noise_blank.wf, 'noise_blank_wf_cb')
            ),
            w3_slider('', 'Test pulse gain', 'noise_blank.test_gain', noise_blank.test_gain, -90, 0, 1, 'noise_blank_test_gain_cb'),
            w3_slider('', 'Test pulse width', 'noise_blank.test_width', noise_blank.test_width, 1, 30, 1, 'noise_blank_test_width_cb'),
            ((noise_blank.algo != noise_blank.NB_OFF)? '<hr>' : ''),
            w3_div('w3-section', s)
			)
		);
	
	return controls_html;
}

function noise_blank_controls_refresh()
{
	if (ext_panel_displayed('noise_blank')) {
	   ext_panel_redisplay(noise_blank_controls_html());
	   ext_set_controls_width_height(noise_blank.width, noise_blank.height[noise_blank.algo]);
	}
}

function noise_blank_controls_setup()
{
	ext_panel_show(noise_blank_controls_html(), null, null);
	ext_set_controls_width_height(noise_blank.width, noise_blank.height[noise_blank.algo]);
}

function noise_blank_environment_changed(changed)
{
   if (changed.mode) {
      var is_IQ_or_stereo_mode = ext_is_IQ_or_stereo_curmode();
      if (is_IQ_or_stereo_mode != noise_blank.is_IQ_or_stereo_mode) {
         noise_blank_controls_refresh();
         noise_blank.is_IQ_or_stereo_mode = is_IQ_or_stereo_mode;
      }
   }
}

// called from main ui, not ext
function noise_blank_init()
{
	// NB_STD
	noise_blank.gate = +kiwi_storeGet('last_noiseGate', cfg.nb_gate);
	noise_blank.threshold = +kiwi_storeGet('last_noiseThresh', cfg.nb_thresh);
	
	// NB_WILD
	noise_blank.thresh = +kiwi_storeGet('last_noiseThresh2', cfg.nb_thresh2);
	noise_blank.taps = +kiwi_storeGet('last_noiseTaps', cfg.nb_taps);
	noise_blank.impulse_samples = +kiwi_storeGet('last_noiseSamps', cfg.nb_samps);
	
	noise_blank.wf = +kiwi_storeGet('last_nb_wf', cfg.nb_wf);
	noise_blank.algo = +kiwi_storeGet('last_nb_algo', cfg.nb_algo);
	nb_algo_cb('nb_algo', noise_blank.algo, false, 'i');
}

function noise_blank_load_defaults()
{
	// NB_STD
   noise_blank.gate = cfg.nb_gate;
   noise_blank.threshold = cfg.nb_thresh;

	// NB_WILD
   noise_blank.thresh = cfg.nb_thresh2;
   noise_blank.taps = cfg.nb_taps;
   noise_blank.impulse_samples = cfg.nb_samps;

   noise_blank.wf = cfg.nb_wf;
   noise_blank.algo = cfg.nb_algo;
   nb_algo_cb('nb_algo', noise_blank.algo, false, 'd');
}

// called from right-click menu
function noise_blank_save_defaults()
{
	// NB_STD
   cfg.nb_gate = noise_blank.gate;
   cfg.nb_thresh = noise_blank.threshold;

	// NB_WILD
   cfg.nb_thresh2 = noise_blank.thresh;
   cfg.nb_taps = noise_blank.taps;
   cfg.nb_samps = noise_blank.impulse_samples;

   cfg.nb_wf = noise_blank.wf;
   ext_set_cfg_param('cfg.nb_algo', noise_blank.algo, EXT_SAVE);
}

// Don't have multiple simultaneous types, like the noise filter, to handle.
// Called even for parameter changes as they also need to be passed to WF.
function noise_blank_send()
{
   snd_send('SET nb algo='+ noise_blank.algo);

   if (noise_blank.algo == noise_blank.NB_STD) {
      snd_send('SET nb type=0 param=0 pval='+ noise_blank.gate);
      snd_send('SET nb type=0 param=1 pval='+ noise_blank.threshold);
   } else
   if (noise_blank.algo == noise_blank.NB_WILD) {
      snd_send('SET nb type=0 param=0 pval='+ noise_blank.thresh);
      snd_send('SET nb type=0 param=1 pval='+ noise_blank.taps);
      snd_send('SET nb type=0 param=2 pval='+ noise_blank.impulse_samples);
   }
   snd_send('SET nb type=0 en='+ (noise_blank.algo? 1:0));

   snd_send('SET nb type=1 en='+ noise_blank.wf);

   if (noise_blank.test) {
      snd_send('SET nb type=2 param=0 pval='+ Math.pow(10, noise_blank.test_gain/20));
      snd_send('SET nb type=2 param=1 pval='+ noise_blank.test_width);
   }
   snd_send('SET nb type=2 en='+ noise_blank.test);
}

function nb_algo_cb(path, idx, first, from)
{
   //console.log('nb_algo_cb idx='+ idx +' first='+ first +' from='+ from);
   if (first) return;      // because call via main ui has zero, not restored value
   idx = +idx;
   w3_select_value(path, idx);
   noise_blank.algo = idx;
   kiwi_storeSet('last_nb_algo', idx.toString());
   noise_blank_send();
   noise_blank_controls_refresh();
}

function noise_blank_test_cb(path, idx, first)
{
   if (first) return;
   noise_blank.test = +idx;
   noise_blank_send();
}

function noise_blank_test_gain_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Test pulse gain: '+ val.toFixed(0) +' dB', path);
	if (complete && !first) noise_blank_send();
}

function noise_blank_test_width_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Test pulse width: '+ val.toFixed(0) +' samples', path);
	if (complete && !first) noise_blank_send();
}

function noise_blank_wf_cb(path, checked, first)
{
   if (first) return;
   noise_blank.wf = checked? 1:0;
   kiwi_storeSet('last_nb_wf', noise_blank.wf.toString());
   noise_blank_send();
}


// NB_STD

function noise_blank_gate_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Gate: '+ val.toFixed(0) +' usec', path);

	if (complete) {
	   kiwi_storeSet('last_noiseGate', val.toString());
      if (!first) noise_blank_send();
	}
}

function noise_blank_threshold_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Threshold: '+ val.toFixed(0) +'%', path);
	
	if (complete) {
	   kiwi_storeSet('last_noiseThresh', val.toString());
      if (!first) noise_blank_send();
	}
}


// NB_WILD

function noise_blank_thresh_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Threshold: '+ val.toFixed(2), path);
	if (complete) {
	   kiwi_storeSet('last_noiseThresh2', val.toString());
	   if (!first) noise_blank_send();
	}
}

function noise_blank_taps_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Taps: '+ val.toFixed(0), path);
	if (complete) {
	   kiwi_storeSet('last_noiseTaps', val.toString());
	   if (!first) noise_blank_send();
	}
}

function noise_blank_impulse_samples_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Impulse samples: '+ val.toFixed(0), path);
	if (complete) {
	   kiwi_storeSet('last_noiseSamps', val.toString());
	   if (!first) noise_blank_send();
	}
}
