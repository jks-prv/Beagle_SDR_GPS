// Copyright (c) 2017 John Seamons, ZL/KF6VO

var noise_filter = {
   ext_name: 'noise_filter',     // NB: must match noise_filter.cpp:noise_filter_ext.name
   first_time: true,

   algo: 0,
   algo_s: [ '(none selected)', 'wdsp variable-leak LMS', 'original LMS', 'spectral noise reduction' ],
   width: 400,
   height: [ 100, 475, 400, 200 ],
   
   NR_OFF: 0,
   denoise: 0,
   autonotch: 0,
   enable: [0, 0],
   
   // type
   NR_DENOISE: 0,
   NR_AUTONOTCH: 1,

   NR_WDSP: 1,
   wdsp_de_taps: 64,
   wdsp_de_delay: 16,
   wdsp_de_gain: 10,    // non-linear: 1..20 => 8.192e-2..1.5625e-7, 10 => 0.00008
   wdsp_de_leakage: 7,  // non-linear: 1..23 => , 7 => 0.125
   wdsp_an_taps: 64,
   wdsp_an_delay: 16,
   wdsp_an_gain: 10,    // non-linear: 1..20 => 8.192e-2..1.5625e-7, 10 => 0.00008
   wdsp_an_leakage: 7,  // non-linear: 1..23 => , 7 => 0.125
   
   NR_ORIG: 2,
   de_delay: 1,
   de_beta: 0.05,
   de_decay: 0.98,
   an_delay: 48,
   an_beta: 0.125,
   an_decay: 0.99915,

   NR_SPECTRAL: 3,
   spec_gain: 0,
   spec_alpha: 0.95,
   active_snr: 30,
};

function noise_filter_main()
{
	ext_switch_to_client(noise_filter.ext_name, noise_filter.first_time, noise_filter_recv);		// tell server to use us (again)
	if (!noise_filter.first_time)
		noise_filter_controls_setup();
	noise_filter.first_time = false;
}

function noise_filter_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('noise_filter_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('noise_filter_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('noise_filter_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				noise_filter_controls_setup();
				break;

			default:
				console.log('noise_filter_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function noise_filter_controls_html()
{
   var s = '';
   
   switch (noise_filter.algo) {
   
   case noise_filter.NR_WDSP:
      s =
         w3_inline('w3-margin-between-16',
            w3_checkbox('w3-label-inline w3-text-css-orange/', 'Denoiser', 'noise_filter.denoise', noise_filter.denoise, 'noise_filter_cb')
         ) +
         w3_div('w3-section',
            w3_slider('', 'Taps', 'noise_filter.wdsp_de_taps', noise_filter.wdsp_de_taps, 16, 128, 1, 'nf_wdsp_taps_cb'),
            w3_slider('', 'Delay', 'noise_filter.wdsp_de_delay', noise_filter.wdsp_de_delay, 2, 128, 1, 'nf_wdsp_delay_cb'),
            w3_slider('', 'Gain', 'noise_filter.wdsp_de_gain', noise_filter.wdsp_de_gain, 1, 20, 1, 'nf_wdsp_gain_cb'),
            w3_slider('', 'Leakage', 'noise_filter.wdsp_de_leakage', noise_filter.wdsp_de_leakage, 1, 23, 1, 'nf_wdsp_leakage_cb')
         ) +
         w3_inline('w3-margin-between-16',
            w3_checkbox('w3-label-inline w3-text-css-orange/', 'Autonotch', 'noise_filter.autonotch', noise_filter.autonotch, 'noise_filter_cb')
         ) +
         w3_div('w3-section',
            w3_slider('', 'Taps', 'noise_filter.wdsp_an_taps', noise_filter.wdsp_an_taps, 16, 128, 1, 'nf_wdsp_taps_cb'),
            w3_slider('', 'Delay', 'noise_filter.wdsp_an_delay', noise_filter.wdsp_an_delay, 2, 128, 1, 'nf_wdsp_delay_cb'),
            w3_slider('', 'Gain', 'noise_filter.wdsp_an_gain', noise_filter.wdsp_an_gain, 1, 20, 1, 'nf_wdsp_gain_cb'),
            w3_slider('', 'Leakage', 'noise_filter.wdsp_an_leakage', noise_filter.wdsp_an_leakage, 1, 23, 1, 'nf_wdsp_leakage_cb')
         );
      break;
   
   case noise_filter.NR_ORIG:
      s =
         w3_inline('w3-margin-between-16',
            w3_checkbox('w3-label-inline w3-text-css-orange/', 'Denoiser', 'noise_filter.denoise', noise_filter.denoise, 'noise_filter_cb'),
            w3_button('w3-padding-tiny', 'SSB #1', 'noise_filter_de_presets_cb', 0),
            w3_button('w3-padding-tiny', 'SSB #2', 'noise_filter_de_presets_cb', 1)
         ) +
         w3_div('w3-section',
            w3_slider('', 'Delay line', 'noise_filter.de_delay', noise_filter.de_delay, 1, 200, 1, 'noise_filter_delay_cb'),
            w3_slider('', 'Beta', 'noise_filter.de_beta', noise_filter.de_beta, 0.0001, 0.150, 0.0001, 'noise_filter_beta_cb'),
            w3_slider('', 'Decay', 'noise_filter.de_decay', noise_filter.de_decay, 0.90, 1.0, 0.0001, 'noise_filter_decay_cb')
         ) +
         w3_inline('w3-margin-between-16',
            w3_checkbox('w3-label-inline w3-text-css-orange/', 'Autonotch', 'noise_filter.autonotch', noise_filter.autonotch, 'noise_filter_cb'),
            w3_button('w3-padding-tiny', 'Voice', 'noise_filter_an_presets_cb', 0),
            w3_button('w3-padding-tiny', 'Slow CW', 'noise_filter_an_presets_cb', 1),
            w3_button('w3-padding-tiny', 'Fast CW', 'noise_filter_an_presets_cb', 2)
         ) +
         w3_div('w3-section',
            w3_slider('', 'Delay line', 'noise_filter.an_delay', noise_filter.an_delay, 1, 200, 1, 'noise_filter_delay_cb'),
            w3_slider('', 'Beta', 'noise_filter.an_beta', noise_filter.an_beta, 0.0001, 0.150, 0.0001, 'noise_filter_beta_cb'),
            w3_slider('', 'Decay', 'noise_filter.an_decay', noise_filter.an_decay, 0.90, 1.0, 0.0001, 'noise_filter_decay_cb')
         );
      break;

   case noise_filter.NR_SPECTRAL:
      s =
         w3_div('w3-section',
            w3_slider('', 'Gain', 'noise_filter.spec_gain', noise_filter.spec_gain, -30, 30, 1, 'nf_spectral_gain_cb'),
            w3_slider('', 'Alpha', 'noise_filter.spec_alpha', noise_filter.spec_alpha, 0.90, 0.99, 0.01, 'nf_spectral_alpha_cb'),
            w3_slider('', 'Active SNR', 'noise_filter.active_snr', noise_filter.active_snr, 2, 30, 1, 'nf_spectral_asnr_cb')
         );
      break;
   }
   
   if (ext_is_IQ_or_stereo_mode()) {
      s = 'No noise filtering in IQ or stereo modes';
   }
   
	var controls_html =
		w3_div('id-noise-filter-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_inline('w3-margin-between-8',
				   w3_div('w3-medium w3-text-aqua', '<b>Noise filter: </b>'),
				   w3_div('w3-text-white', noise_filter.algo_s[noise_filter.algo])
				),
            w3_div('w3-section', s)
         )
		);
	
	return controls_html;
}

function noise_filter_controls_setup()
{
	ext_panel_show(noise_filter_controls_html(), null, null);
	ext_set_controls_width_height(noise_filter.width, noise_filter.height[noise_filter.algo]);
}

function noise_filter_controls_refresh()
{
	if (ext_panel_displayed('noise_filter')) {
	   ext_panel_redisplay(noise_filter_controls_html());
	   ext_set_controls_width_height(noise_filter.width, noise_filter.height[noise_filter.algo]);
	}
}

function noise_filter_environment_changed(changed)
{
   if (changed.mode) {
      var is_IQ_or_stereo_mode = ext_is_IQ_or_stereo_mode();
      if (is_IQ_or_stereo_mode != noise_filter.is_IQ_or_stereo_mode) {
         noise_filter_controls_refresh();
         noise_filter.is_IQ_or_stereo_mode = is_IQ_or_stereo_mode;
      }
   }
}

// called from openwebrx.js
function noise_filter_init()
{
	noise_filter.algo = readCookie('last_nr_algo', 0);
	nr_algo_cb('nr_algo', noise_filter.algo, false, true);
   noise_filter_send(noise_filter.NR_DENOISE);
   noise_filter_send(noise_filter.NR_AUTONOTCH);
}

function noise_filter_send(type)
{
   var p0, p1, p2, p3;

   if (noise_filter.algo == noise_filter.NR_OFF) return;
   
   if (noise_filter.algo == noise_filter.NR_WDSP) {
      if (type == noise_filter.NR_DENOISE) {
         p0 = noise_filter.wdsp_de_taps;
         p1 = noise_filter.wdsp_de_delay;
         p2 = noise_filter.wdsp_de_gain;
         p2 = 8.192e-2 / Math.pow(2, 20 - p2);
         p3 = noise_filter.wdsp_de_leakage;
         p3 = 8192 / Math.pow(2, 23 - p3);
      } else
      if (type == noise_filter.NR_AUTONOTCH) {
         p0 = noise_filter.wdsp_an_taps;
         p1 = noise_filter.wdsp_an_delay;
         p2 = noise_filter.wdsp_an_gain;
         p2 = 8.192e-2 / Math.pow(2, 20 - p2);
         p3 = noise_filter.wdsp_an_leakage;
         p3 = 8192 / Math.pow(2, 23 - p3);
      }
   } else
   if (noise_filter.algo == noise_filter.NR_ORIG) {
      if (type == noise_filter.NR_DENOISE) {
         p0 = noise_filter.de_delay;
         p1 = noise_filter.de_beta;
         p2 = noise_filter.de_decay;
         p3 = 0;
      } else
      if (type == noise_filter.NR_AUTONOTCH) {
         p0 = noise_filter.an_delay;
         p1 = noise_filter.an_beta;
         p2 = noise_filter.an_decay;
         p3 = 0;
      }
   } else
   if (noise_filter.algo == noise_filter.NR_SPECTRAL) {
      p0 = Math.pow(10, noise_filter.spec_gain/20);
      p1 = noise_filter.spec_alpha;
      p2 = Math.pow(10, noise_filter.active_snr/10);
      p3 = 0;
   }

   snd_send('SET nr type='+ type +' param=0 pval='+ p0);
   snd_send('SET nr type='+ type +' param=1 pval='+ p1);
   snd_send('SET nr type='+ type +' param=2 pval='+ p2);
   snd_send('SET nr type='+ type +' param=3 pval='+ p3);
   snd_send('SET nr type='+ type +' en='+ noise_filter.enable[type]);
}

function nr_algo_cb(path, idx, first, init)
{
   console.log('nr_algo_cb idx='+ idx +' first='+ first +' init='+ init);
   if (first) return;      // because call via openwebrx has zero, not restored value
   idx = +idx;
   w3_select_value(path, idx);
   noise_filter.algo = idx;
   writeCookie('last_nr_algo', idx.toString());
   snd_send('SET nr algo='+ noise_filter.algo);

   if (init != true) {     // redundant when called from noise_filter_init()
      noise_filter_send(noise_filter.NR_DENOISE);
      noise_filter_send(noise_filter.NR_AUTONOTCH);
   }

   noise_filter_controls_refresh();
}

function noise_filter_cb(path, checked, first)
{
   checked = checked? 1:0;
   console.log('noise_filter_cb '+ checked +' path='+ path);
   setVarFromString(path, checked);
   w3_checkbox_set(path, checked);
   var type = path.includes('denoise')? 0:1;
   noise_filter.enable[type] = checked;
   noise_filter_send(type);
}


// NR_WDSP

function nf_wdsp_taps_cb(path, val, complete, first)
{
	var type = path.includes('de_')? 0:1;
	var delay = type? noise_filter.wdsp_an_delay : noise_filter.wdsp_de_delay;
   val = +val;
   if (val < delay) val = delay;
	w3_num_cb(path, val);
	w3_set_label('Taps: '+ val, path);
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET nr type='+ type +' param=0 pval='+ val);
	}
}

function nf_wdsp_delay_cb(path, val, complete, first)
{
	var type = path.includes('de_')? 0:1;
	var taps = type? noise_filter.wdsp_an_taps : noise_filter.wdsp_de_taps;
   val = +val;
   if (val > taps) val = taps;
	w3_num_cb(path, val);
	w3_set_label('Delay: '+ val, path);
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET nr type='+ type +' param=1 pval='+ val);
	}
}

function nf_wdsp_gain_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	var gain = 8.192e-2 / Math.pow(2, 20 - val);
	w3_set_label('Gain: '+ gain.toExponential(2), path);
	var type = path.includes('de_')? 0:1;
	if (complete) {
	   console.log(path +' gain='+ gain);
      snd_send('SET nr type='+ type +' param=2 pval='+ gain);
	}
}

function nf_wdsp_leakage_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	var leakage = 8192 / Math.pow(2, 23 - val);
	w3_set_label('Leakage: '+ leakage.toExponential(2), path);
	var type = path.includes('de_')? 0:1;
	if (complete) {
	   console.log(path +' leakage='+ leakage);
      snd_send('SET nr type='+ type +' param=3 pval='+ leakage);
	}
}


// NR_SPECTRAL

function nf_spectral_gain_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Gain: '+ val +' dB', path);
	if (complete) {
	   console.log(path +' dB='+ val);
      snd_send('SET nr type=0 param=0 pval='+ Math.pow(10, val/20));
	}
}

function nf_spectral_alpha_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Alpha: '+ val.toFixed(2), path);
	if (complete) {
	   console.log(path +'='+ val.toFixed(2));
      snd_send('SET nr type=0 param=1 pval='+ val.toFixed(2));
	}
}

function nf_spectral_asnr_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Active SNR: '+ val +' dB', path);
	if (complete) {
	   console.log(path +' dB='+ val);
      snd_send('SET nr type=0 param=2 pval='+ Math.pow(10, val/10));
	}
}


// NR_ORIG

var noise_filter_de_presets = [
   1,    0.05,    0.98,
   100,  0.07,    0.985
];

function noise_filter_de_presets_cb(path, idx, first)
{
   var p = noise_filter_de_presets;
   w3_slider_set('noise_filter.de_delay', p[idx*3], 'noise_filter_delay_cb');
   w3_slider_set('noise_filter.de_beta', p[idx*3+1], 'noise_filter_beta_cb');
   w3_slider_set('noise_filter.de_decay', p[idx*3+2], 'noise_filter_decay_cb');
}

var noise_filter_an_presets = [
   48,   0.125,   0.99915,
   48,   0.002,   0.9998,
   48,   0.001,   0.9980
];

function noise_filter_an_presets_cb(path, idx, first)
{
   var p = noise_filter_an_presets;
   w3_slider_set('noise_filter.an_delay', p[idx*3], 'noise_filter_delay_cb');
   w3_slider_set('noise_filter.an_beta', p[idx*3+1], 'noise_filter_beta_cb');
   w3_slider_set('noise_filter.an_decay', p[idx*3+2], 'noise_filter_decay_cb');
}

function noise_filter_delay_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Delay line: '+ (val +' samp'+ ((val == 1)? '':'s') +', '+ (val * 1/12000 * 1e3).toFixed(3) +' msec'), path);
	var type = path.includes('de_')? 0:1;
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET nr type='+ type +' param=0 pval='+ val);
	}
}

function noise_filter_beta_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Beta: '+ val.toFixed(4), path);
	var type = path.includes('de_')? 0:1;
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET nr type='+ type +' param=1 pval='+ val);
	}
}

function noise_filter_decay_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Decay: '+ val.toFixed(4), path);
	var type = path.includes('de_')? 0:1;
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET nr type='+ type +' param=2 pval='+ val);
	}
}
