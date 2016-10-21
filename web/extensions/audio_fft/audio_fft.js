// Copyright (c) 2016 John Seamons, ZL/KF6VO

var audio_fft_ext_name = 'audio_fft';		// NB: must match audio_fft.c:audio_fft_ext.name

var audio_fft_first_time = 1;

function audio_fft_main()
{
	ext_switch_to_client(audio_fft_ext_name, audio_fft_first_time, audio_fft_recv);		// tell server to use us (again)
	if (!audio_fft_first_time)
		audio_fft_controls_setup();
	audio_fft_first_time = 0;
}

var afft_w = 1024;
var afft_h = 200;
var afft_yo = 0;
var afft_bins;
var afft_draw = false;

function audio_fft_clear()
{
	ext_send('SET clear');
	afft_draw = false;
	var c = audio_fft_canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, afft_w, afft_h);
}

var audio_fft_imageData;

var audio_fft_cmd_e = { FFT:0, CLEAR:1 };
var afft_dbl;
var afft_y = 10, last_bin = 0, max_bin = 0;
var afft_maxdb = -10, afft_mindb = -100;

function audio_fft_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var o = 1;
		var len = ba.length-1;

		if (cmd == audio_fft_cmd_e.FFT) {
			if (!afft_draw) return;
			
			var c = audio_fft_canvas.ctx;
			var bin = ba[o];
			o++; len--;

			//console.log('bin='+ bin +' len='+ len);
			/*
			if (last_bin > 0 && bin == 0)
				afft_y += max_bin;
			last_bin = bin;
			if (bin > max_bin) max_bin = bin;
			*/

			for (var x=0; x < afft_w; x++) {
				var color = waterfall_color_index_max_min(ba[x+o], afft_maxdb, afft_mindb);
				audio_fft_imageData.data[x*4+0] = color_map_r[color];
				audio_fft_imageData.data[x*4+1] = color_map_g[color];
				audio_fft_imageData.data[x*4+2] = color_map_b[color];
				audio_fft_imageData.data[x*4+3] = 0xff;
			}
			
			for (var y=0; y < afft_dbl; y++) {
				c.putImageData(audio_fft_imageData, 0, afft_yo + (bin*afft_dbl + y));
			}
			
			//afft_y++;
			//if (afft_y >= afft_h)
			//	afft_y = 0;
		} else
		
		if (cmd == audio_fft_cmd_e.CLEAR) {	// not currently used
			audio_fft_clear();
		} else {
			console.log('audio_fft_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		}
		
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_encoded_msg()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('audio_fft_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('audio_fft_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				audio_fft_controls_setup();
				break;

			case "bins":
				afft_bins = param[1];
				afft_dbl = Math.floor(afft_h / afft_bins);
				if (afft_dbl < 1) afft_dbl = 1;
				afft_yo = (afft_h - afft_dbl * afft_bins) / 2;
				if (afft_yo < 0) afft_yo = 0;
				console.log('audio_fft_recv bins='+ afft_bins +' dbl='+ afft_dbl +' yo='+ afft_yo);
				afft_draw = true;
				
				break;

			default:
				console.log('audio_fft_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var audio_fft_itime_init = 3.6;
var audio_fft_maxdb_init = -10;
var audio_fft_mindb_init = -100;

var audio_fft = {
	'itime':audio_fft_itime_init, 'pre':0, 'maxdb':audio_fft_maxdb_init, 'mindb':audio_fft_mindb_init
};

var audio_fft_canvas;

function audio_fft_controls_setup()
{
   var data_html =
      '<div id="id-audio_fft-data" style="left:200px; width:1024px; height:200px; background-color:mediumBlue; position:relative; display:none" title="audio_fft">' +
   		'<canvas id="id-audio_fft-canvas" width="1024" height="200" style="position:absolute"></canvas>'+
      '</div>';

	var pre_s = { 0:'Alpha', 1:'60 kHz', 2:'40 kHz' };

	var controls_html =
		w3_divs('id-audio_fft-controls w3-text-white', '',
			w3_divs('w3-container', 'w3-tspace-8',
				w3_divs('', 'w3-medium w3-text-aqua', '<b>Audio FFT display</b>'),
				w3_input('Integrate time (secs)', 'audio_fft.itime', audio_fft.itime, 'audio_fft_itime_cb', '', 'w3-width-128'),
				w3_select('Presets', 'select', 'audio_fft.pre', audio_fft.pre, pre_s, 'audio_fft_pre_select_cb'),
				w3_slider('WF max', 'audio_fft.maxdb', audio_fft.maxdb, -100, 20, 'audio_fft_maxdb_cb'),
				w3_slider('WF min', 'audio_fft.mindb', audio_fft.mindb, -190, -30, 'audio_fft_mindb_cb'),
				w3_btn('Clear', 'audio_fft_clear_cb')
			)
		);

	ext_panel_show(controls_html, data_html, null);

	audio_fft_canvas = html_id('id-audio_fft-canvas');
	audio_fft_canvas.ctx = audio_fft_canvas.getContext("2d");
	audio_fft_imageData = audio_fft_canvas.ctx.createImageData(afft_w, 1);

	audio_fft_visible(1);

	audio_fft_itime_cb('audio_fft.itime', audio_fft_itime_init);
	audio_fft_maxdb_cb('audio_fft.maxdb', audio_fft.maxdb);
	audio_fft_mindb_cb('audio_fft.mindb', audio_fft.mindb);
	
	ext_send('SET run=1');
	audio_fft_clear();
}

// FIXME: allow zero to indicate continue acquire mode
function audio_fft_itime_cb(path, val)
{
	if (val < 1) val = 1;
	w3_num_cb(path, val);
	var itime = val.toFixed(3);
	ext_send('SET itime='+ itime);
	console.log('itime='+ itime);
	audio_fft_clear();
}

function audio_fft_set_itime(itime)
{
	w3_set_value('audio_fft.itime', itime);
	audio_fft_itime_cb('audio_fft.itime', itime);
}

function audio_fft_pre_select_cb(path, idx)
{
	var pre = idx-1;
	
	switch (pre) {
	
	case 0:
		audio_fft_set_itime(3.6);
		ext_tune(11.5, 'usb', zoom.max_in);
		ext_passband(300, 3470);
		break;
	
	case 1:
		audio_fft_set_itime(1.0);
		ext_tune(60, 'cw', zoom.max_in);
		break;
	
	case 2:
		audio_fft_set_itime(1.0);
		ext_tune(40, 'cw', zoom.max_in);
		break;
	
	default:
		break;
	}
	
	audio_fft_clear();
}

function audio_fft_maxdb_cb(path, val)
{
   afft_maxdb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF max '+ val +' dBFS', path);
}

function audio_fft_mindb_cb(path, val)
{
   afft_mindb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF min '+ val +' dBFS', path);
}

function audio_fft_clear_cb(path, val)
{
	audio_fft_clear();
	setTimeout('w3_radio_unhighlight('+ q(path) +')', w3_highlight_time);
}

function audio_fft_blur()
{
	//console.log('### audio_fft_blur');
	audio_fft_visible(0);		// hook to be called when controls panel is closed
}

// called to display HTML for configuration parameters in admin interface
function audio_fft_config_html()
{
	ext_admin_config(audio_fft_ext_name, 'FFT',
		w3_divs('id-audio_fft w3-text-teal w3-hide', '',
			'<b>Audio FFT display configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('int1', 'audio_fft.int1', 'admin_num_cb'),
					admin_input('int2', 'audio_fft.int2', 'admin_num_cb')
				), '', ''
			)
		)
	);
}

function audio_fft_visible(v)
{
	visible_block('id-audio_fft-data', v);
}
