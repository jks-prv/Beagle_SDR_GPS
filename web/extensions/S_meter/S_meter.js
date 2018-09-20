// Copyright (c) 2016 John Seamons, ZL/KF6VO

var S_meter_ext_name = 'S_meter';		// NB: must match S_meter.c:S_meter_ext.name

var S_meter_first_time = true;

function S_meter_main()
{
	ext_switch_to_client(S_meter_ext_name, S_meter_first_time, S_meter_recv);		// tell server to use us (again)
	if (!S_meter_first_time)
		S_meter_controls_setup();
	S_meter_first_time = false;
}

var sm_w = 1024;
var sm_h = 180;
var sm_padding = 10;
var sm_tw = sm_w + sm_padding*2;

function S_meter_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var o = 1;
		var len = ba.length-1;

		console.log('S_meter_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('S_meter_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('S_meter_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				S_meter_controls_setup();
				break;

			case "smeter":
				graph_plot(parseFloat(param[1]));
				break;

			default:
				console.log('S_meter_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var S_meter_maxdb_init = -30;
var S_meter_mindb_init = -130;
var S_meter_speed_max = 10;

var S_meter = {
	'range':0, 'maxdb':S_meter_maxdb_init, 'mindb':S_meter_mindb_init, 'speed':S_meter_speed_max, 'marker':0
};

var S_meter_data_canvas;

function S_meter_controls_setup()
{
   var data_html =
      time_display_html('S_meter') +

      w3_div('id-S_meter-data|left:150px; width:1044px; height:200px; background-color:mediumBlue; position:relative;',
   		'<canvas id="id-S_meter-data-canvas" width="1024" height="180" style="position:absolute; padding: 10px;"></canvas>'
      );

	var range_s = {
		0:'manual',
		1:'auto'
	};

	var marker_s = {
		0:'1 sec',
		1:'5 sec',
		2:'10 sec',
		3:'15 sec',
		4:'30 sec',
		5:'1 min'
	};

	var controls_html =
		w3_div('id-S_meter-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_div('w3-medium w3-text-aqua', '<b>S-meter graph</b>'),
				w3_select('', 'Range', '', 'S_meter.range', S_meter.range, range_s, 'S_meter_range_select_cb'),
				w3_div('id-S_meter-scale-sliders',
					w3_slider('', 'Scale max', 'S_meter.maxdb', S_meter.maxdb, -160, 0, 10, 'S_meter_maxdb_cb'),
					w3_slider('', 'Scale min', 'S_meter.mindb', S_meter.mindb, -160, 0, 10, 'S_meter_mindb_cb')
				),
				w3_slider('', 'Speed', 'S_meter.speed', S_meter.speed, 1, S_meter_speed_max, 1, 'S_meter_speed_cb'),
            w3_inline('/w3-margin-between-16',
					w3_select('', 'Marker rate', '', 'S_meter.marker', S_meter.marker, marker_s, 'S_meter_marker_select_cb'),
					'w3-salign-end', w3_button('', 'Clear', 'S_meter_clear_cb')
				)
			)
		);

	ext_panel_show(controls_html, data_html, null);
	time_display_setup('S_meter');

	S_meter_data_canvas = w3_el('id-S_meter-data-canvas');
	S_meter_data_canvas.ctx = S_meter_data_canvas.getContext("2d");
	S_meter_data_canvas.im = S_meter_data_canvas.ctx.createImageData(sm_w, 1);

   graph_init(S_meter_data_canvas, { dBm:1 });
	graph_mode((S_meter_range == S_meter_range_e.AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);

	S_meter_environment_changed( {resize:1} );
	ext_set_controls_width_height(250);

	ext_send('SET run=1');

	S_meter_update_interval = setInterval(function() {S_meter_update(0);}, 1000);
	S_meter_update(1);
}

function S_meter_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-S_meter-data');
	var left = (window.innerWidth - sm_tw - time_display_width()) / 2;
	el.style.left = px(left);
}

var S_meter_range = 0;
var S_meter_range_e = { MANUAL:0, AUTO:1 };

function S_meter_range_select_cb(path, idx, first)
{
	// ignore the auto instantiation callback because we don't want to rescale at this point
	if (first) return;

	S_meter_range = +idx;
	if (S_meter_range == S_meter_range_e.MANUAL) {
		w3_show_block('id-S_meter-scale-sliders');
	} else {
		w3_hide('id-S_meter-scale-sliders');
	}
	graph_mode((S_meter_range == S_meter_range_e.AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);
}

function S_meter_maxdb_cb(path, val, complete)
{
   var maxdb = +val;
   maxdb = Math.max(S_meter.mindb, maxdb);		// don't let min & max cross
	w3_num_cb(path, maxdb.toString());
	w3_set_label('Scale max '+ maxdb.toString() +' dBm', path);
	graph_mode((S_meter_range == S_meter_range_e.AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);
}

function S_meter_mindb_cb(path, val, complete)
{
   var mindb = +val;
   mindb = Math.min(mindb, S_meter.maxdb);		// don't let min & max cross
	w3_num_cb(path, mindb.toString());
	w3_set_label('Scale min '+ mindb.toString() +' dBm', path);
	graph_mode((S_meter_range == S_meter_range_e.AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);
}

var S_meter_speed;	// not the same as S_meter.speed

function S_meter_speed_cb(path, val, complete)
{
	var val_i = +val;
   S_meter_speed = S_meter_speed_max - val_i + 1;
	w3_num_cb(path, val_i.toString());
	w3_set_label('Speed 1'+ ((S_meter_speed != 1)? ('/'+S_meter_speed.toString()) : ''), path);
	graph_speed(S_meter_speed);
}

var S_meter_marker_e = { SEC_1:0, SEC_5:1, SEC_10:2, SEC_15:3, SEC_30:4, MIN_1:5 };
var S_meter_marker_sec = [ 1, 5, 10, 15, 30, 60 ];
var S_meter_marker = S_meter_marker_sec[S_meter.marker];

function S_meter_marker_select_cb(path, idx)
{
	S_meter_marker = S_meter_marker_sec[+idx];
	graph_marker(S_meter_marker);
}

function S_meter_clear_cb(path, val)
{
	graph_clear();
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

var S_meter_update_interval;
var sm_last_freq;
var sm_last_offset;

// detect when frequency or offset has changed and restart graph
function S_meter_update(init)
{
	var freq = ext_get_freq();
	var offset = freq - ext_get_carrier_freq();
	
	// don't restart on mode(offset) change so user can switch and see difference
	if (init || freq != sm_last_freq /*|| offset != sm_last_offset*/) {
		graph_clear();
		//console.log('freq/offset change');
		sm_last_freq = freq;
		sm_last_offset = offset;
	}
}

// hook that is called when controls panel is closed
function S_meter_blur()
{
	//console.log('### S_meter_blur');
	ext_send('SET run=0');
	kiwi_clearInterval(S_meter_update_interval);
}

// called to display HTML for configuration parameters in admin interface
function S_meter_config_html()
{
	ext_admin_config(S_meter_ext_name, 'S Meter',
		w3_div('id-S_meter w3-text-teal w3-hide',
			'<b>S-meter graph configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('w3-margin-bottom',
					w3_input_get('', 'int1', 'S_meter.int1', 'w3_num_cb'),
					w3_input_get('', 'int2', 'S_meter.int2', 'w3_num_cb')
				), '', ''
			)
			*/
		)
	);
}
