// Copyright (c) 2016 John Seamons, ZL/KF6VO

var iq_display_ext_name = 'iq_display';		// NB: must match iq_display.c:iq_display_ext.name

var iq_display_first_time = 1;

function iq_display_main()
{
	ext_switch_to_client(iq_display_ext_name, iq_display_first_time, iq_display_recv);		// tell server to use us (again)
	iq_display_first_time = 0;
	iq_display_controls_setup();
}

function iq_display_clear()
{
	var c = iq_display_canvas.ct;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, 256, 256);
	c.fillStyle = 'white';
	c.fillRect(0, 128, 256, 1);
	c.fillRect(128, 0, 1, 256);
}

var iq_display_cmd_e = { IQ_DATA:0, IQ_CLEAR:1 };

function iq_display_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var len = ba.length-1;

		if (cmd == iq_display_cmd_e.IQ_DATA) {
			var c = iq_display_canvas.ct;
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
		
		if (cmd == iq_display_cmd_e.IQ_CLEAR) {
			iq_display_clear();
		} else {
			console.log('iq_display_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
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
				console.log('iq_display_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('iq_display_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				iq_display_controls_setup();
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

var iq_display_draw_s = { 0:'points', 1:'lines' };

var iq_display_canvas;

function iq_display_controls_setup()
{
   var data_html =
      '<div id="id-iq_display-data" style="left:0px; width:256px; height:256px; background-color:mediumBlue; overflow:hidden; position:relative; display:none" title="iq_display">' +
   		'<canvas id="id-iq_display-canvas" width="256" height="256" style="position:absolute">test</canvas>'+
      '</div>';

	var controls_html =
		w3_divs('id-iq_display-controls w3-text-white', '',
			w3_half('', '',
				data_html,
				w3_divs('w3-container', 'w3-tspace-16',
					w3_divs('', 'w3-medium w3-text-aqua', '<b>IQ display</b>'),
					w3_slider('Gain', 'iq_display.gain', iq_display.gain, 0, 100, 'iq_display_gain_cb'),
					//w3_select('Draw', 'select', 'loran_c.draw', loran_c.draw, iq_display_draw_s, 'iq_display_draw_select_cb'),
					w3_input('Clock offset', 'iq_display.offset', iq_display.offset, 'iq_display_offset_cb', '', 'w3-width-64'),
					w3_slider('Points', 'iq_display.points', iq_display.points, 4, 14, 'iq_display_points_cb'),
					w3_btn('Clear', 'iq_display_clear_cb')
				)
			)
		);

	ext_panel_show(controls_html, null, null);

	iq_display_canvas = html_id('id-iq_display-canvas');
	iq_display_canvas.ct = iq_display_canvas.getContext("2d");

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

function iq_display_draw_select_cb(path, idx)
{
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

function iq_display_blur()
{
	console.log('### iq_display_blur');
	iq_display_visible(0);		// hook to be called when controls panel is closed
}

// called to display HTML for configuration parameters in admin interface
function iq_display_config_html()
{
	ext_admin_config(iq_display_ext_name, 'IQ',
		w3_divs('id-iq_display w3-text-teal w3-hide', '',
			'<b>IQ display configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('int1', 'iq_display.int1', 'admin_num_cb'),
					admin_input('int2', 'iq_display.int2', 'admin_num_cb')
				), '', ''
			)
		)
	);
}

function iq_display_visible(v)
{
	visible_block('id-iq_display-data', v);
}
