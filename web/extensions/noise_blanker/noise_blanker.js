// Copyright (c) 2017 John Seamons, ZL/KF6VO

var noise_blanker = {
   ext_name: 'noise_blanker',     // NB: must match noise_blanker.cpp:noise_blanker_ext.name
   first_time: true,
   
   algo: 0,
   algo_s: [ '(none selected)', 'standard blanker', 'Wild algorithm' ],
   NB_OFF: 0,
   blanker: 0,
   test: 0,
   wf: 1,
   
   // type
   NB_BLANKER: 0,
   NB_WF: 1,
   NB_CLICK: 2,

   NB_STD: 1,
   gate: 0,
   threshold: 0,
   
   NB_WILD: 2,
};

function noise_blanker_main()
{
	ext_switch_to_client(noise_blanker.ext_name, noise_blanker.first_time, noise_blanker_recv);		// tell server to use us (again)
	if (!noise_blanker.first_time)
		noise_blanker_controls_setup();
	noise_blanker.first_time = false;
}

function noise_blanker_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('noise_blanker_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('noise_blanker_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('noise_blanker_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				noise_blanker_controls_setup();
				break;

			default:
				console.log('noise_blanker_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function noise_blanker_controls_html()
{
   var s = '';
   
   switch (noise_blanker.algo) {
   
   case noise_blanker.NB_STD:
      s =
         w3_slider('', 'Gate', 'noise_blanker.gate', noise_blanker.gate, 100, 5000, 100, 'noise_blanker_gate_cb') +
         w3_slider('', 'Threshold', 'noise_blanker.threshold', noise_blanker.threshold, 0, 100, 1, 'noise_blanker_threshold_cb');
      break;
   
   case noise_blanker.NB_WILD:
      break;
   }
   
	var controls_html =
		w3_div('id-noise-blanker-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_inline('w3-margin-between-8',
				   w3_div('w3-medium w3-text-aqua', '<b>Noise blanker: </b>'),
				   w3_div('w3-text-white', noise_blanker.algo_s[noise_blanker.algo])
				),
				w3_inline('w3-margin-between-32',
               w3_checkbox('w3-label-inline w3-text-css-orange/', 'Test', 'noise_blanker.test', noise_blanker.test, 'noise_blanker_test_cb'),
               w3_checkbox('w3-label-inline w3-text-css-orange/', 'Also blank WF', 'noise_blanker.wf', noise_blanker.wf, 'noise_blanker_wf_cb')
            ),
            w3_div('w3-section', s)
			)
		);
	
	return controls_html;
}

function noise_blanker_controls_setup()
{
	ext_panel_show(noise_blanker_controls_html(), null, null);
	ext_set_controls_width_height(400, 400);
}

function noise_blanker_init()
{
	noise_blanker.algo = readCookie('last_nb_algo', 0);
	nb_algo_cb('nb_algo', noise_blanker.algo, false, true);
	
	// NB_STD
	noise_blanker.gate = readCookie('last_noiseGate', 100);
	noise_blanker.threshold = readCookie('last_noiseThresh', 50);
	
	// NB_WILD
	
   noise_blanker_send();
}

function noise_blanker_send()
{
   snd_send('SET nb algo='+ noise_blanker.algo);
   snd_send('SET nb type=0 param=0 pval='+ noise_blanker.gate);
   snd_send('SET nb type=0 param=1 pval='+ noise_blanker.threshold);
   snd_send('SET nb type=0 en='+ (noise_blanker.algo? 1:0));
   snd_send('SET nb type=1 en='+ noise_blanker.wf);
   snd_send('SET nb type=2 en='+ noise_blanker.test);
}

function nb_algo_cb(path, idx, first, init)
{
   console.log('nb_algo_cb idx='+ idx +' first='+ first +' init='+ init);
   if (first) return;
   idx = +idx;
   w3_select_value(path, idx);
   noise_blanker.algo = idx;
   writeCookie('last_nb_algo', idx.toString());
   if (init != true) {
      noise_blanker_send();
   }

	if (ext_panel_displayed()) {
	   ext_panel_redisplay(noise_blanker_controls_html());
	}
}

function noise_blanker_test_cb(path, checked, first)
{
   if (first) return;
   noise_blanker.test = checked? 1:0;
   noise_blanker_send();
}

function noise_blanker_wf_cb(path, checked, first)
{
   if (first) return;
   noise_blanker.wf = checked? 1:0;
   noise_blanker_send();
}

function noise_blanker_gate_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Gate: '+ val.toFixed(0) +' usec', path);
	noise_blanker.gate = val;

	if (complete) {
	   writeCookie('last_noiseGate', val.toString());
      if (!first) noise_blanker_send();
	}
}

function noise_blanker_threshold_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Threshold: '+ val.toFixed(0) +'%', path);
	noise_blanker.threshold = val;
	
	if (complete) {
	   writeCookie('last_noiseThresh', val.toString());
      if (!first) noise_blanker_send();
	}
}
