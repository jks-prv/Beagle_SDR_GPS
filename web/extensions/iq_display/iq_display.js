// Copyright (c) 2016 John Seamons, ZL/KF6VO

var iq_display_ext_name = 'iq_display';		// NB: must match iq_display.c:iq_display_ext.name

var iq_display_first_time = true;

function iq_display_main()
{
	ext_switch_to_client(iq_display_ext_name, iq_display_first_time, iq_display_recv);		// tell server to use us (again)
	if (!iq_display_first_time)
		iq_display_controls_setup();
	iq_display_first_time = false;
}

var iq_display_map = new Uint32Array(256*256);
var iq_display_max = 1;

function iq_display_clear()
{
	var c = iq_display_canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, 256, 256);
	c.fillStyle = 'white';
	c.fillRect(0, 128, 256, 1);
	c.fillRect(128, 0, 1, 256);
	ext_send('SET clear');
	
	for (var q=0; q < 256; q++)
		for (var i=0; i < 256; i++)
			iq_display_map[q*256 + i] = 0;
	iq_display_max = 1;
}

var iq_display_imageData;
var iq_display_draw;
var iq_display_cmaI, iq_display_cmaQ;

function iq_display_update()
{
	//console.log('iq_display_update '+ iq_display_max);
	var c = iq_display_canvas.ctx;

	if (iq_display_draw == iq_display_cmd_e.IQ_DENSITY || iq_display_draw == iq_display_cmd_e.IQ_S4285_D) {
		var y=0;
		for (var q=0; q < (256*256); q += 256) {
			for (var i=0; i < 256; i++) {
				var color = Math.round(iq_display_map[q + i] / iq_display_max * 0xff);
				iq_display_imageData.data[i*4+0] = color_map_r[color];
				iq_display_imageData.data[i*4+1] = color_map_g[color];
				iq_display_imageData.data[i*4+2] = color_map_b[color];
				iq_display_imageData.data[i*4+3] = 0xff;
			}
			c.putImageData(iq_display_imageData, 0, y);
			y++;
		}
	}
	
	w3_el_id('iq_display-cma').innerHTML =
		'CMA I: '+ iq_display_cmaI.toExponential(3) +'&nbsp; &nbsp; CMA Q: '+ iq_display_cmaI.toExponential(3);
}

var iq_display_cmd_e = { IQ_POINTS:0, IQ_DENSITY:1, IQ_S4285_P:2, IQ_S4285_D:3, IQ_CLEAR:4 };

function iq_display_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var len = ba.length-1;

		if (cmd == iq_display_cmd_e.IQ_POINTS || cmd == iq_display_cmd_e.IQ_S4285_P) {
			var c = iq_display_canvas.ctx;
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
		
		if (cmd == iq_display_cmd_e.IQ_DENSITY || cmd == iq_display_cmd_e.IQ_S4285_D) {
			//console.log('IQ_DENSITY '+ len);
			var c = iq_display_canvas.ctx;
			var i, q;

			for (var j=1; j < len; j += 2) {
				i = ba[j+0];
				q = ba[j+1];
				var m = iq_display_map[q*256 + i];
				m++;
				if (m > iq_display_max) iq_display_max = m;
				iq_display_map[q*256 + i] = m;
			}
		} else
		
		if (cmd == iq_display_cmd_e.IQ_CLEAR) {	// not currently used
			iq_display_clear();
		} else {
			console.log('iq_display_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
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
				console.log('iq_display_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('iq_display_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				iq_display_controls_setup();
				break;

			case "cmaI":
				iq_display_cmaI = parseFloat(param[1]);
				break;

			case "cmaQ":
				iq_display_cmaQ = parseFloat(param[1]);
				break;

			default:
				console.log('iq_display_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var iq_display_gain_init = 15;
var iq_display_points_init = 10;

var iq_display = {
	'gain':iq_display_gain_init, 'draw':0, 'points':iq_display_points_init, 'offset':0
};

var iq_display_canvas;

function iq_display_controls_setup()
{
   var data_html =
      '<div id="id-iq_display-data" style="left:0px; width:256px; height:256px; background-color:mediumBlue; overflow:hidden; position:relative; display:none" title="iq_display">' +
   		'<canvas id="id-iq_display-canvas" width="256" height="256" style="position:absolute">test</canvas>'+
      '</div>';

	// FIXME
	var draw_s = dbgUs? { 0:'points', 1:'density', 2:'s4285 pts', 3:'s4285 den' } : { 0:'points', 1:'density' };

	var controls_html =
		w3_divs('id-iq_display-controls w3-text-white', '',
			w3_half('', '',
				data_html,
				w3_divs('w3-container', 'w3-tspace-8',
					w3_divs('', 'w3-medium w3-text-aqua', '<b>IQ display</b>'),
					w3_slider('Gain', 'iq_display.gain', iq_display.gain, 0, 100, 1, 'iq_display_gain_cb'),
					w3_select('Draw', '', 'iq_display.draw', iq_display.draw, draw_s, 'iq_display_draw_select_cb'),
					w3_input('Clock offset', 'iq_display.offset', iq_display.offset, 'iq_display_offset_cb', '', 'w3-width-128'),
					w3_slider('Points', 'iq_display.points', iq_display.points, 4, 14, 1, 'iq_display_points_cb'),
					w3_inline('', '',
						w3_btn('Clear', 'iq_display_clear_cb'),
						w3_btn('IQ bal', 'iq_display_IQ_balance_cb', 'w3-override-yellow')
					)
				)
			),
			w3_divs('id-iq_display-cma', '')
		);

	ext_panel_show(controls_html, null, null);

	iq_display_canvas = w3_el_id('id-iq_display-canvas');
	iq_display_canvas.ctx = iq_display_canvas.getContext("2d");
	iq_display_imageData = iq_display_canvas.ctx.createImageData(256, 1);

	iq_display_visible(1);

	iq_display_gain_cb('iq_display.gain', iq_display_gain_init);
	iq_display_points_cb('iq_display.points', iq_display_points_init);
	ext_send('SET run=1');
	iq_display_clear();
}

function iq_display_gain_cb(path, val)
{
	w3_num_cb(path, val);
	w3_set_label('Gain '+ ((val == 0)? '(auto-scale)' : val +' dB'), path);
	ext_send('SET gain='+ val);
	iq_display_clear();
}

function iq_display_points_cb(path, val)
{
	var points = 1 << val;
	w3_num_cb(path, val);
	w3_set_label('Points '+ points, path);
	ext_send('SET points='+ points);
	iq_display_clear();
}

var iq_display_update_interval;

function iq_display_draw_select_cb(path, idx)
{
	iq_display_draw = +idx;
	ext_send('SET draw='+ iq_display_draw);
	kiwi_clearInterval(iq_display_update_interval);
	iq_display_update_interval = setInterval('iq_display_update()', 250);
	iq_display_clear();
}

function iq_display_offset_cb(path, val)
{
	w3_num_cb(path, val);
	ext_send('SET offset='+ val);
}

function iq_display_clear_cb(path, val)
{
	iq_display_clear();
	setTimeout('w3_radio_unhighlight('+ q(path) +')', w3_highlight_time);
}

function iq_display_IQ_bal_adjust()
{
	console.log('iq_display_IQ_bal_adjust: ADJUSTING I='+ (-iq_display_cmaI) +' Q='+ (-iq_display_cmaQ));
	ext_send('SET DC_offset I='+ (-iq_display_cmaI) +' Q='+ (-iq_display_cmaQ));
}

function iq_display_IQ_balance_cb(path, val)
{
	var func = function(badp) { if (!badp) iq_display_IQ_bal_adjust(); };
	ext_hasCredential('admin', func);
	setTimeout('w3_radio_unhighlight('+ q(path) +')', w3_highlight_time);
}

function iq_display_blur()
{
	//console.log('### iq_display_blur');
	ext_send('SET run=0');
	kiwi_clearInterval(iq_display_update_interval);
	iq_display_visible(0);		// hook to be called when controls panel is closed
}

// called to display HTML for configuration parameters in admin interface
function iq_display_config_html()
{
	ext_admin_config(iq_display_ext_name, 'IQ',
		w3_divs('id-iq_display w3-text-teal w3-hide', '',
			'<b>IQ display configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					w3_input_get_param('int1', 'iq_display.int1', 'w3_num_cb'),
					w3_input_get_param('int2', 'iq_display.int2', 'w3_num_cb')
				), '', ''
			)
			*/
		)
	);
}

function iq_display_visible(v)
{
	visible_block('id-iq_display-data', v);
}
