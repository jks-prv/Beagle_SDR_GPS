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

var audio_fft_map = new Uint32Array(256*256);
var audio_fft_max = 1;
var afft_w = 1024;

function audio_fft_clear()
{
	var c = audio_fft_canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, afft_w, afft_w);
	ext_send('SET clear');
}

var audio_fft_imageData;

function audio_fft_density_draw()
{
	//console.log('audio_fft_density_draw '+ audio_fft_max);
	var c = audio_fft_canvas.ctx;
	var y=0;
	for (var q=0; q < (256*256); q += 256) {
		for (var i=0; i < 256; i++) {
			var color = Math.round(audio_fft_map[q + i] / audio_fft_max * 0xff);
			audio_fft_imageData.data[i*4+0] = color_map_r[color];
			audio_fft_imageData.data[i*4+1] = color_map_g[color];
			audio_fft_imageData.data[i*4+2] = color_map_b[color];
			audio_fft_imageData.data[i*4+3] = 0xff;
		}
		c.putImageData(audio_fft_imageData, 0, y);
		y++;
	}
}

var audio_fft_cmd_e = { IQ_POINTS:0, IQ_DENSITY:1, IQ_CLEAR:4 };
var afft_y = 10, last_bin = 0, max_bin = 0;

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

		if (cmd == audio_fft_cmd_e.IQ_POINTS) {
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
				var color = waterfall_color_index(ba[x+o]);
				audio_fft_imageData.data[x*4+0] = color_map_r[color];
				audio_fft_imageData.data[x*4+1] = color_map_g[color];
				audio_fft_imageData.data[x*4+2] = color_map_b[color];
				audio_fft_imageData.data[x*4+3] = 0xff;
			}
			c.putImageData(audio_fft_imageData, 0, (afft_y + bin)*2);
			c.putImageData(audio_fft_imageData, 0, (afft_y + bin)*2 +1);
			//afft_y++;
			//if (afft_y >= 200)
			//	afft_y = 0;
			return;

			var i, q;

			for (var j=1; j < len; j += 4) {
				i = ba[j+0];
				q = ba[j+1];
				c.fillStyle = 'black';
				c.fillRect(i, q, 2, 2);
	
				i = ba[j+2];
				q = ba[j+3];
				c.fillStyle = ch? 'lime':'cyan';
				c.fillRect(i, q, 2, 2);
			}
		} else
		
		if (cmd == audio_fft_cmd_e.IQ_DENSITY) {
			//console.log('IQ_DENSITY '+ len);
			var c = audio_fft_canvas.ctx;
			var i, q;

			for (var j=1; j < len; j += 2) {
				i = ba[j+0];
				q = ba[j+1];
				var m = audio_fft_map[q*256 + i];
				m++;
				if (m > audio_fft_max) audio_fft_max = m;
				audio_fft_map[q*256 + i] = m;
			}
		} else
		
		if (cmd == audio_fft_cmd_e.IQ_CLEAR) {	// not currently used
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

		if (1 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('audio_fft_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('audio_fft_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				audio_fft_controls_setup();
				break;

			default:
				console.log('audio_fft_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var audio_fft_gain_init = 15;
var audio_fft_points_init = 10;

var audio_fft = {
	'gain':audio_fft_gain_init, 'draw':0+1, 'points':audio_fft_points_init, 'offset':0
};

var audio_fft_canvas;

function audio_fft_controls_setup()
{
   var data_html =
      '<div id="id-audio_fft-data" style="left:200px; width:1024px; height:200px; background-color:mediumBlue; position:relative; display:none" title="audio_fft">' +
   		'<canvas id="id-audio_fft-canvas" width="1024" height="200" style="position:absolute"></canvas>'+
      '</div>';

	// FIXME
	var draw_s = dbgUs? { 0:'points', 1:'density' } : { 0:'points', 1:'density' };

	var controls_html =
		w3_divs('id-audio_fft-controls w3-text-white', '',
			w3_divs('w3-container', 'w3-tspace-8',
				w3_divs('', 'w3-medium w3-text-aqua', '<b>Audio FFT display</b>'),
				w3_slider('Gain', 'audio_fft.gain', audio_fft.gain, 0, 100, 'audio_fft_gain_cb'),
				w3_select('Draw', 'select', 'audio_fft.draw', audio_fft.draw, draw_s, 'audio_fft_draw_select_cb'),
				w3_input('Clock offset', 'audio_fft.offset', audio_fft.offset, 'audio_fft_offset_cb', '', 'w3-width-128'),
				w3_slider('Points', 'audio_fft.points', audio_fft.points, 4, 14, 'audio_fft_points_cb'),
				w3_btn('Clear', 'audio_fft_clear_cb')
			)
		);

	ext_panel_show(controls_html, data_html, null);

	audio_fft_canvas = html_id('id-audio_fft-canvas');
	audio_fft_canvas.ctx = audio_fft_canvas.getContext("2d");
	audio_fft_imageData = audio_fft_canvas.ctx.createImageData(afft_w, 1);

	audio_fft_visible(1);

	audio_fft_gain_cb('audio_fft.gain', audio_fft_gain_init);
	audio_fft_points_cb('audio_fft.points', audio_fft_points_init);
	ext_send('SET run=1');
	audio_fft_clear();
}

function audio_fft_gain_cb(path, val)
{
	w3_num_cb(path, val);
	w3_set_label('Gain '+ ((val == 0)? '(auto-scale)' : val +' dB'), path);
	ext_send('SET gain='+ val);
	audio_fft_clear();
}

function audio_fft_points_cb(path, val)
{
	var points = 1 << val;
	w3_num_cb(path, val);
	w3_set_label('Points '+ points, path);
	ext_send('SET points='+ points);
	audio_fft_clear();
}

var audio_fft_density_interval;

function audio_fft_draw_select_cb(path, idx)
{
	var draw = idx-1;
	ext_send('SET draw='+ draw);
	kiwi_clearInterval(audio_fft_density_interval);
	if (draw == audio_fft_cmd_e.IQ_DENSITY) {
		audio_fft_density_interval = setInterval('audio_fft_density_draw()', 250);
	}
	audio_fft_clear();
}

function audio_fft_offset_cb(path, val)
{
	w3_num_cb(path, val);
	ext_send('SET offset='+ val);
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
