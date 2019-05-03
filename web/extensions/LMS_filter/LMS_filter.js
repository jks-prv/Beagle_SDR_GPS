// Copyright (c) 2017 John Seamons, ZL/KF6VO

var lms_ext_name = 'LMS_filter';		// NB: must match lms_filter.c:lms_ext.name

var lms_first_time = true;

function LMS_filter_main()
{
	ext_switch_to_client(lms_ext_name, lms_first_time, lms_recv);		// tell server to use us (again)
	if (!lms_first_time)
		lms_controls_setup();
	lms_first_time = false;
}

function lms_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('lms_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('lms_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('lms_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				lms_controls_setup();
				break;

			default:
				console.log('lms_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function lms_controls_setup()
{
	var controls_html =
		w3_div('id-lms-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_div('w3-medium w3-text-aqua', '<b>LMS filter controls</b>'),
				w3_inline('w3-margin-between-16',
               w3_checkbox('w3-label-inline w3-text-css-orange/', 'Denoiser', 'lms.denoise2', lms.denoise, 'lms_denoise2_cb'),
               w3_button('w3-padding-smaller', 'SSB #1', 'lms_de_presets_cb', 0),
               w3_button('w3-padding-smaller', 'SSB #2', 'lms_de_presets_cb', 1)
            ),
            w3_div('w3-section',
               w3_slider('', 'Delay line', 'lms.de_delay', lms.de_delay, 1, 200, 1, 'lms_delay_cb'),
               w3_slider('', 'Beta', 'lms.de_beta', lms.de_beta, 0.0001, 0.150, 0.0001, 'lms_beta_cb'),
               w3_slider('', 'Decay', 'lms.de_decay', lms.de_decay, 0.90, 1.0, 0.0001, 'lms_decay_cb')
            ),
				w3_inline('w3-margin-between-16',
               w3_checkbox('w3-label-inline w3-text-css-orange/', 'Autonotch', 'lms.autonotch2', lms.autonotch, 'lms_autonotch2_cb'),
               w3_button('w3-padding-smaller', 'Voice', 'lms_an_presets_cb', 0),
               w3_button('w3-padding-smaller', 'Slow CW', 'lms_an_presets_cb', 1),
               w3_button('w3-padding-smaller', 'Fast CW', 'lms_an_presets_cb', 2)
            ),
            w3_div('w3-section',
               w3_slider('', 'Delay line', 'lms.an_delay', lms.an_delay, 1, 200, 1, 'lms_delay_cb'),
               w3_slider('', 'Beta', 'lms.an_beta', lms.an_beta, 0.0001, 0.150, 0.0001, 'lms_beta_cb'),
               w3_slider('', 'Decay', 'lms.an_decay', lms.an_decay, 0.90, 1.0, 0.0001, 'lms_decay_cb')
            )
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(400, 400);
   //w3_menu('id-lms-presets-menu', 'lms_presets_menu_cb');
}

var lms = {
   denoise: 0,
   denoise2: 0,
   de_delay: 1,
   //de_beta: 0.0058,
   de_beta: 0.05,
   de_decay: 0.98,
   
   autonotch: 0,
   autonotch2: 0,
   an_delay: 48,
   an_beta: 0.125,
   an_decay: 0.99915
};

function lms_presets_menu_cb()
{

}

var lms_de_presets = [
   1,    0.05,    0.98,
   100,  0.07,    0.985
];

function lms_de_presets_cb(path, idx, first)
{
   var p = lms_de_presets;
   w3_slider_set('lms.de_delay', p[idx*3], 'lms_delay_cb');
   w3_slider_set('lms.de_beta', p[idx*3+1], 'lms_beta_cb');
   w3_slider_set('lms.de_decay', p[idx*3+2], 'lms_decay_cb');
}

var lms_an_presets = [
   48,   0.125,   0.99915,
   48,   0.002,   0.9998,
   48,   0.001,   0.9980
];

function lms_an_presets_cb(path, idx, first)
{
   var p = lms_an_presets;
   w3_slider_set('lms.an_delay', p[idx*3], 'lms_delay_cb');
   w3_slider_set('lms.an_beta', p[idx*3+1], 'lms_beta_cb');
   w3_slider_set('lms.an_decay', p[idx*3+2], 'lms_decay_cb');
   /*
   w3_menu_items('id-right-click-menu',
      'defaults',
      'CW signals'
   );
   w3_menu_popup('id-lms-presets-menu', x, y);
   */
}

function lms_delay_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Delay line: '+ (val +' samp'+ ((val == 1)? '':'s') +', '+ (val * 1/12000 * 1e3).toFixed(3) +' msec'), path);
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET '+ path +'='+ val);
	}
}

function lms_beta_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Beta: '+ val.toFixed(4), path);
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET '+ path +'='+ val);
	}
}

function lms_decay_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Decay: '+ val.toFixed(4), path);
	if (complete) {
	   //console.log(path +' val='+ val);
      snd_send('SET '+ path +'='+ val);
	}
}

function lms_denoise_cb(path, checked, first)
{
   checked = checked? 1:0;
   lms.denoise = checked;
   //console.log('lms_denoise_cb '+ checked);
   w3_checkbox_set(path, checked);
   w3_checkbox_set('lms.denoise2', checked);
	snd_send('SET lms_denoise='+ checked);
}

function lms_denoise2_cb(path, checked, first)
{
   checked = checked? 1:0;
   lms.denoise = lms.denoise2 = checked;
   //console.log('lms_denoise2_cb '+ checked);
   w3_checkbox_set(path, checked);
   w3_checkbox_set('lms.denoise', checked);
	snd_send('SET lms_denoise='+ checked);
}

function lms_autonotch_cb(path, checked, first)
{
   checked = checked? 1:0;
   lms.autonotch = checked;
   //console.log('lms_autonotch_cb '+ checked);
   w3_checkbox_set(path, checked);
   w3_checkbox_set('lms.autonotch2', checked);
	snd_send('SET lms_autonotch='+ checked);
}

function lms_autonotch2_cb(path, checked, first)
{
   checked = checked? 1:0;
   lms.autonotch = lms.autonotch2 = checked;
   //console.log('lms_autonotch2_cb '+ checked);
   w3_checkbox_set(path, checked);
   w3_checkbox_set('lms.autonotch', checked);
	snd_send('SET lms_autonotch='+ checked);
}
