// Copyright (c) 2016 John Seamons, ZL/KF6VO

var s4285_ext_name = 's4285';		// NB: must match s4285.c:s4285_ext.name

var s4285_first_time = true;

function s4285_main()
{
	ext_switch_to_client(s4285_ext_name, s4285_first_time, s4285_recv);		// tell server to use us (again)
	if (!s4285_first_time)
		s4285_controls_setup();
	s4285_first_time = false;
}

var s4285_map = new Uint32Array(200*200);
var s4285_max = 1;

function s4285_clear()
{
	var c = s4285_canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, 200, 200);
	c.fillStyle = 'white';
	c.fillRect(0, 100, 200, 1);
	c.fillRect(100, 0, 1, 200);
	ext_send('SET clear');
	
	for (var q=0; q < 200; q++)
		for (var i=0; i < 200; i++)
			s4285_map[q*200 + i] = 0;
	s4285_max = 1;
}

var s4285_imageData;

function s4285_density_draw()
{
	//console.log('s4285_density_draw '+ s4285_max);
	var c = s4285_canvas.ctx;
	var y=0;
	for (var q=0; q < (200*200); q += 200) {
		for (var i=0; i < 200; i++) {
			var color = Math.round(s4285_map[q + i] / s4285_max * 0xff);
			s4285_imageData.data[i*4+0] = color_map_r[color];
			s4285_imageData.data[i*4+1] = color_map_g[color];
			s4285_imageData.data[i*4+2] = color_map_b[color];
			s4285_imageData.data[i*4+3] = 0xff;
		}
		c.putImageData(s4285_imageData, 0, y);
		y++;
	}
}

var s4285_cmd_e = { IQ_POINTS:0, IQ_DENSITY:1 };

function s4285_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var len = ba.length-1;

		if (cmd == s4285_cmd_e.IQ_POINTS) {
			var c = s4285_canvas.ctx;
			var i, q;

			for (var j=1; j < len; j += 4) {
				i = ba[j+0];
				q = ba[j+1];
				c.fillStyle = 'black';
				c.fillRect(i, q, 2, 2);
	
				i = ba[j+2];
				q = ba[j+3];
				c.fillStyle = 'cyan';
				c.fillRect(i, q, 2, 2);
			}
		} else
		
		if (cmd == s4285_cmd_e.IQ_DENSITY) {
			//console.log('IQ_DENSITY '+ len);
			var c = s4285_canvas.ctx;
			var i, q;

			for (var j=1; j < len; j += 2) {
				i = ba[j+0];
				q = ba[j+1];
				var m = s4285_map[q*200 + i];
				m++;
				if (m > s4285_max) s4285_max = m;
				s4285_map[q*200 + i] = m;
			}
		} else {
			console.log('s4285_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		}
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('s4285_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('s4285_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				s4285_controls_setup();
				break;

			case "status":
				var status = decodeURIComponent(param[1]);
				w3_el('id-s4285-status').innerHTML = status;
				//console.log('s4285_recv: status='+ status);
				break;

			default:
				console.log('s4285_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var s4285_mode = { MODE_RX:0, MODE_TX_LOOPBACK:1 };

//var s4285_mode_init = s4285_mode.MODE_RX;
var s4285_mode_init = s4285_mode.MODE_TX_LOOPBACK;		//jks
var s4285_gain_init = 5;
var s4285_points_init = 10;

var s4285 = {
	'mode':s4285_mode_init, 'gain':s4285_gain_init, 'draw':0, 'points':s4285_points_init
};

var s4285_canvas;

function s4285_controls_setup()
{
   var data_html =
      time_display_html('s4285') +

      w3_div('id-s4285-data|left:100px; width:200px; height:200px; background-color:mediumBlue; overflow:hidden; position:relative;',
   		'<canvas id="id-s4285-canvas" width="200" height="200" style="position:absolute"></canvas>'
      );

	var mode_s = { 0:'receive', 1:'tx loopback' };
	var draw_s = { 0:'points', 1:'density' };
	
	var controls_html =
		w3_div('id-s4285-controls w3-text-aqua',
			w3_divs('w3-container/w3-tspace-8',
				w3_div('w3-medium w3-text-aqua', '<b>STANAG 4285 decoder</b>'),
				w3_select('', 'Mode', '', 's4285.mode', s4285.mode, mode_s, 's4285_mode_select_cb'),
				w3_slider('', 'Gain', 's4285.gain', s4285.gain, 0, 100, 1, 's4285_gain_cb'),
				w3_select('', 'Draw', '', 's4285.draw', s4285.draw, draw_s, 's4285_draw_select_cb'),
				w3_slider('', 'Points', 's4285.points', s4285.points, 4, 14, 1, 's4285_points_cb'),
				w3_button('', 'Clear', 's4285_clear_cb'),
				w3_div('w3-text-aqua',
					'<b>Status:</b>',
					w3_div('id-s4285-status w3-small w3-text-white')
				)
			)
		);

	ext_panel_show(controls_html, data_html, null);
	time_display_setup('s4285');
	s4285_environment_changed( {resize:1} );

	s4285_canvas = w3_el('id-s4285-canvas');
	s4285_canvas.ctx = s4285_canvas.getContext("2d");
	s4285_imageData = s4285_canvas.ctx.createImageData(200, 1);

	ext_set_mode('usb');
	ext_set_passband(600, 3000);
	//msg_send("SET wf_speed=0");    // WF_SPEED_OFF
	ext_send('SET mode='+ s4285_mode_init);
	ext_send('SET run=1');
	s4285_clear();
}

function s4285_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-s4285-data');
	var left = (window.innerWidth - 200 - time_display_width()) / 2;
	el.style.left = px(left);
}

function s4285_mode_select_cb(path, idx)
{
	idx = +idx;
	ext_send('SET mode='+ idx);
}

function s4285_gain_cb(path, val)
{
	w3_num_cb(path, val);
	w3_set_label('Gain '+ ((val == 0)? '(auto-scale)' : val +' dB'), path);
	ext_send('SET gain='+ val);
	s4285_clear();
}

function s4285_points_cb(path, val)
{
	var points = 1 << val;
	w3_num_cb(path, val);
	w3_set_label('Points '+ points, path);
	ext_send('SET points='+ points);
	s4285_clear();
}

var s4285_density_interval;

function s4285_draw_select_cb(path, idx)
{
	idx = +idx;
	ext_send('SET draw='+ idx);
	kiwi_clearInterval(s4285_density_interval);
	if (idx == s4285_cmd_e.IQ_DENSITY) {
		s4285_density_interval = setInterval(s4285_density_draw, 250);
	}
	s4285_clear();
}

function s4285_clear_cb(path, val)
{
	s4285_clear();
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function s4285_blur()
{
	console.log('### s4285_blur');
	ext_send('SET run=0');
	msg_send("SET wf_speed=-1");   // WF_SPEED_FAST
}

// called to display HTML for configuration parameters in admin interface
function s4285_config_html()
{
	ext_admin_config(s4285_ext_name, 's4285',
		w3_div('id-s4285 w3-text-teal w3-hide',
			'<b>s4285 configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('w3-margin-bottom',
					w3_input_get('', 'int1', 's4285.int1', 'w3_num_cb'),
					w3_input_get('', 'int2', 's4285.int2', 'w3_num_cb')
				), '', ''
			)
			*/
		)
	);
}
