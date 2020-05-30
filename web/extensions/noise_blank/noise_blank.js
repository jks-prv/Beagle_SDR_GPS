// Copyright (c) 2017 John Seamons, ZL/KF6VO

var noise_blank = {
   ext_name: 'noise_blank',     // NB: must match noise_blank.cpp:noise_blank_ext.name
   first_time: true,
   
   algo: 0,
   algo_s: [ '(none selected)', 'standard blanker', 'Wild algorithm' ],
   width: 400,
   height: [ 175, 300, 350 ],
   
   NB_OFF: 0,
   blanker: 0,
   test: 0,
   test_s: [ 'test off', 'pre filter (std)', 'post filter (Wild)' ],
   test_gain: 0,
   test_width: 1,
   wf: 1,
   
   // type
   NB_BLANKER: 0,
   NB_WF: 1,
   NB_CLICK: 2,

   NB_STD: 1,
   gate: 0,
   threshold: 0,
   
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
      if (ext_is_IQ_or_stereo_mode())
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
			w3_divs('w3-container/w3-tspace-8',
				w3_inline('w3-margin-between-8',
				   w3_div('w3-medium w3-text-aqua', '<b>Noise blanker: </b>'),
				   w3_div('w3-text-white', noise_blank.algo_s[noise_blank.algo])
				),
				w3_inline('w3-margin-between-32',
               w3_select('|color:red', '', 'test off', 'noise_blank.test', noise_blank.test, noise_blank.test_s, 'noise_blank_test_cb'),
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
      var is_IQ_or_stereo_mode = ext_is_IQ_or_stereo_mode();
      if (is_IQ_or_stereo_mode != noise_blank.is_IQ_or_stereo_mode) {
         noise_blank_controls_refresh();
         noise_blank.is_IQ_or_stereo_mode = is_IQ_or_stereo_mode;
      }
   }
}

// called from openwebrx.js
function noise_blank_init()
{
	noise_blank.algo = readCookie('last_nb_algo', 0);
	nb_algo_cb('nb_algo', noise_blank.algo, false, true);
	
	// NB_STD
	noise_blank.gate = readCookie('last_noiseGate', 100);
	noise_blank.threshold = readCookie('last_noiseThresh', 50);
	
	// NB_WILD
	// ...
	
   noise_blank_send();
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

function nb_algo_cb(path, idx, first, init)
{
   console.log('nb_algo_cb idx='+ idx +' first='+ first +' init='+ init);
   if (first) return;      // because call via openwebrx has zero, not restored value
   idx = +idx;
   w3_select_value(path, idx);
   noise_blank.algo = idx;
   writeCookie('last_nb_algo', idx.toString());

   if (init != true) {     // redundant when called from noise_blank_init()
      noise_blank_send();
   }

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
   noise_blank_send();
}


// NB_STD

function noise_blank_gate_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Gate: '+ val.toFixed(0) +' usec', path);

	if (complete) {
	   writeCookie('last_noiseGate', val.toString());
      if (!first) noise_blank_send();
	}
}

function noise_blank_threshold_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Threshold: '+ val.toFixed(0) +'%', path);
	
	if (complete) {
	   writeCookie('last_noiseThresh', val.toString());
      if (!first) noise_blank_send();
	}
}


// NB_WILD

function noise_blank_thresh_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Threshold: '+ val.toFixed(2), path);
	if (complete && !first) noise_blank_send();
}

function noise_blank_taps_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Taps: '+ val.toFixed(0), path);
	if (complete && !first) noise_blank_send();
}

function noise_blank_impulse_samples_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Impulse samples: '+ val.toFixed(0), path);
	if (complete && !first) noise_blank_send();
}
