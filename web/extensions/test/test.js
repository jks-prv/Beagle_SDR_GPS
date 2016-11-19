// Copyright (c) 2016 John Seamons, ZL/KF6VO

var test_ext_name = 'test';		// NB: must match test.c:test_ext.name

var test_first_time = true;

function test_main()
{
	ext_switch_to_client(test_ext_name, test_first_time, test_recv);		// tell server to use us (again)
	if (!test_first_time)
		test_controls_setup();
	test_first_time = false;
}

function test_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var o = 1;
		var len = ba.length-1;

		console.log('test_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_encoded_msg()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('test_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('test_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				test_controls_setup();
				break;

			default:
				console.log('test_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var test_init_freq = 10000;

var test = {
	'gen_freq':test_init_freq, 'gen_attn':0, 'gen_fix':1, 'rx_fix':1, 'wf_fix':1, 'filter':0
};

function test_controls_setup()
{
	var off_on_s = { 0:'off', 1:'on' };
	var filter_s = { 0:'off', 1:'IIR' };
	
	var controls_html =
		w3_divs('id-test-controls w3-text-white', '',
			w3_divs('w3-container', 'w3-tspace-8',
				w3_divs('', 'w3-medium w3-text-aqua', '<b>Test controls</b>'),
				w3_half('', '',
					w3_input('Gen freq (kHz)', 'test.gen_freq', test.gen_freq, 'test_gen_freq_cb', '', 'w3-width-64'),
					//w3_input('Gen attn (dB)', 'test.gen_attn', test.gen_attn, 'test_gen_attn_cb', '', 'w3-width-64')
					w3_slider('Gen attn', 'test.gen_attn', test.gen_attn, 0, 110, 5, 'test_gen_attn_cb')
				),
				w3_select('Spectrum filtering', '', 'test.filter', test.filter, filter_s, 'test_filter_cb')

				/*
				w3_select('GEN fix', '', 'test.gen_fix', test.gen_fix, off_on_s, 'test_off_on_cb'),
				w3_select('RX mix fix (ch 0)', '', 'test.rx_fix', test.rx_fix, off_on_s, 'test_off_on_cb'),
				w3_select('WF mix fix (ch 0)', '', 'test.wf_fix', test.wf_fix, off_on_s, 'test_off_on_cb'),
				*/
			)
		);

	ext_panel_show(controls_html, null, null);
	//ext_set_controls_width(300);
	test_gen_freq_cb('test.gen_freq', test.gen_freq);
	test_gen_attn_cb('test.gen_attn', 0, true);
	ext_tune(test.gen_freq, 'am', ext_zoom.MAX_IN);
	toggle_or_set_spec(1);
	ext_send('SET run=1');
}

var test_gen_freq = 0;

function test_gen_freq_cb(path, val)
{
	test_gen_freq = parseFloat(val);
	w3_num_cb(path, test_gen_freq);
	w3_set_value(path, test_gen_freq);
	set_gen(test_gen_freq, test_gen_attn);
}

var test_gen_attn = 0;

function test_gen_attn_cb(path, val, complete)
{
	var dB = +val;
	var ampl_gain = Math.pow(10, -dB/20);		// use the amplitude form since we are multipling a signal
	test_gen_attn = 0x01ffff * ampl_gain;
	//console.log('gen_attn dB='+ dB +' ampl_gain='+ ampl_gain +' attn='+ test_gen_attn +' / '+ test_gen_attn.toHex());
	w3_num_cb(path, dB);
	w3_set_label('Gen attn '+ (-dB).toString() +' dB', path);
	
	if (complete)
		set_gen(test_gen_freq, test_gen_attn);
}

function test_off_on_cb(path, idx, first)
{
	// ignore the auto instantiation callback
	//if (first) return;

	idx = +idx;
	w3_num_cb(path, idx);
	w3_set_value(path, idx);
	console.log('SET '+ w3_basename(path) +'='+ idx);
	ext_send('SET '+ w3_basename(path) +'='+ idx);
}

function test_filter_cb(path, idx, first)
{
	// ignore the auto instantiation callback
	//if (first) return;

	idx = +idx;
	spectrum_filter(idx);
}

// hook that is called when controls panel is closed
function test_blur()
{
	//console.log('### test_blur');
	set_gen(0, 0);
	spectrum_filter(1);
	ext_send('SET run=0');
}

// called to display HTML for configuration parameters in admin interface
function test_config_html()
{
	ext_admin_config(test_ext_name, 'Test',
		w3_divs('id-test w3-text-teal w3-hide', '',
			'<b>Test controls configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('int1', 'test.int1', 'admin_num_cb'),
					admin_input('int2', 'test.int2', 'admin_num_cb')
				), '', ''
			)
			*/
		)
	);
}
