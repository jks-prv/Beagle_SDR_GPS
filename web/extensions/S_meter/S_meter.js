// Copyright (c) 2016 John Seamons, ZL/KF6VO

var S_meter = {
   first_time:    true,
   ext_name:      'S_meter',     // NB: must match S_meter.c:S_meter_ext.name
   stop_start_state: 0,
   update_interval:  null,
   
   maxdb_init:    -30,
   mindb_init:    -130,
	maxdb:         0,
	mindb:         0,

	speed_i:       13,
   speed_max:     13,
	
	range_s:       [ 'manual', 'auto' ],
	range_i:       0,
	range_MANUAL:  0,
	range_AUTO:    1,
	
	marker_sec:    [ 1, 5, 10, 15, 30, 60, 5*60, 10*60, 15*60, 30*60, 60*60 ],
	marker_s:      [ '1 sec', '5 sec', '10 sec', '15 sec', '30 sec', '1 min', '5 min', '10 min', '15 min', '30 min', '1 hr' ],
	marker_i:      0,
	marker_v:      1,
	
   sm_last_freq:  null,
   sm_last_mode:  null,

	div:           0,
	
	xendx:         0
};

S_meter.maxdb = S_meter.maxdb_init;
S_meter.mindb = S_meter.mindb_init;

function S_meter_main()
{
	ext_switch_to_client(S_meter.ext_name, S_meter.first_time, S_meter_recv);		// tell server to use us (again)
	if (!S_meter.first_time)
		S_meter_controls_setup();
	S_meter.first_time = false;
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
		var cmd = ba[0];
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
				kiwi_load_js(['pkgs/js/graph.js'], 'S_meter_controls_setup');
				break;

			case "smeter":
			   if (S_meter.stop_start_state == 0) {
			      S_meter.last_plot = parseFloat(param[1]);
				   graph_plot(S_meter.last_plot);
				}
				break;

			default:
				console.log('S_meter_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function S_meter_controls_setup()
{
   var data_html =
      time_display_html('S_meter') +

      w3_div('id-S_meter-data|left:150px; width:1044px; height:200px; background-color:mediumBlue; position:relative;',
   		'<canvas id="id-S_meter-data-canvas" width="1024" height="180" style="position:absolute; padding: 10px;"></canvas>'
      );

	var controls_html =
		w3_div('id-S_meter-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_div('id-S_meter-info w3-medium w3-text-aqua', '<b>S-meter graph</b>'),
            w3_inline('w3-halign-space-between/',
				   w3_select('', 'Range', '', 'S_meter.range_i', S_meter.range_i, S_meter.range_s, 'S_meter_range_select_cb'),
					w3_select('', 'Marker rate', '', 'S_meter.marker_i', S_meter.marker_i, S_meter.marker_s, 'S_meter_marker_select_cb')
				),
				w3_div('id-S_meter-scale-sliders',
					w3_slider('', 'Scale max', 'S_meter.maxdb', S_meter.maxdb, -160, 0, 10, 'S_meter_maxdb_cb'),
					w3_slider('', 'Scale min', 'S_meter.mindb', S_meter.mindb, -160, 0, 10, 'S_meter_mindb_cb')
				),
				w3_slider('', 'Speed', 'S_meter.speed_i', S_meter.speed_i, 1, S_meter.speed_max, 1, 'S_meter_speed_cb'),
            w3_inline('w3-halign-space-between/',
					w3_button('w3-padding-smaller', 'Stop', 'S_meter_stop_start_cb'),
					w3_button('w3-padding-smaller', 'Mark', 'S_meter_mark_cb'),
					w3_button('w3-padding-smaller', 'Clear', 'S_meter_clear_cb')
				),
            w3_checkbox('w3-tspace-8/w3-label-inline', 'Averaging', 'S_meter.averaging', true, 'S_meter_averaging_cb')
			)
		);

	ext_panel_show(controls_html, data_html, null);
	time_display_setup('S_meter');

	S_meter.data = w3_el('id-S_meter-data');
	S_meter.data_canvas = w3_el('id-S_meter-data-canvas');
	S_meter.data_canvas.ctx = S_meter.data_canvas.getContext("2d");

   graph_init(S_meter.data_canvas, { dBm:1, averaging:true });
	graph_mode((S_meter.range_i == S_meter.range_AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);

	S_meter_environment_changed( {resize:1} );
	ext_set_controls_width_height(250);

	ext_send('SET run=1');

	S_meter.update_interval = setInterval(function() {S_meter_update(0);}, 1000);
	S_meter_update(1);
}

function S_meter_environment_changed(changed)
{
   if (!changed.resize) return;
   
   var w;
	var el = w3_el('id-S_meter-data');
	var left = (window.innerWidth - sm_tw - time_display_width()) / 2;
	w3_show_hide('S_meter-time-display', left > 0);

	if (left > 0) {
	   w = sm_w;
	   //w3_innerHTML('id-S_meter-info', '000 '+ w);
      S_meter.data.style.width = px(w + sm_padding*2);
      S_meter.data_canvas.width = w;
	   el.style.left = px(left);
	   graph_clear();
	   return;
	}

   // recalculate with time display off
	left = (window.innerWidth - sm_tw) / 2;
	if (left > 0) {
	   w = sm_w;
	   //w3_innerHTML('id-S_meter-info', '000 (no time) '+ w);
      S_meter.data.style.width = px(w + sm_padding*2);
      S_meter.data_canvas.width = w;
	   el.style.left = px(left);
	   graph_clear();
	   return;
	}
	
	// need to shrink canvas below sm_w
	var border = 15;
	w = window.innerWidth - border*2;
	//w3_innerHTML('id-S_meter-info', '000 '+ w);
	console.log('shrink to '+ w);
	S_meter.data.style.width = px(w);
	S_meter.data_canvas.width = w - sm_padding*2;
	el.style.left = px(border);
   graph_clear();
   S_meter_update(1);
   return;
}

function S_meter_range_select_cb(path, idx, first)
{
	// ignore the auto instantiation callback because we don't want to rescale at this point
	if (first) return;

	S_meter.range_i = +idx;
	if (S_meter.range_i == S_meter.range_MANUAL) {
		w3_show_block('id-S_meter-scale-sliders');
	} else {
		w3_hide('id-S_meter-scale-sliders');
	}
	graph_mode((S_meter.range_i == S_meter.range_AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);
}

function S_meter_maxdb_cb(path, val, complete)
{
   var maxdb = +val;
   maxdb = Math.max(S_meter.mindb, maxdb);		// don't let min & max cross
	w3_num_cb(path, maxdb.toString());
	w3_set_label('Scale max '+ maxdb.toString() +' dBm', path);
	graph_mode((S_meter.range_i == S_meter.range_AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);
}

function S_meter_mindb_cb(path, val, complete)
{
   var mindb = +val;
   mindb = Math.min(mindb, S_meter.maxdb);		// don't let min & max cross
	w3_num_cb(path, mindb.toString());
	w3_set_label('Scale min '+ mindb.toString() +' dBm', path);
	graph_mode((S_meter.range_i == S_meter.range_AUTO)? 1:0, S_meter.maxdb, S_meter.mindb);
}

function S_meter_speed_cb(path, val, complete)
{
	var val_i = +val;
   var speed_pow2 = Math.round(Math.pow(2, S_meter.speed_max - val_i));
	w3_num_cb(path, val_i.toString());
	w3_set_label('Speed 1'+ ((speed_pow2 != 1)? ('/'+speed_pow2.toString()) : ''), path);
	graph_speed(speed_pow2);
}

function S_meter_marker_select_cb(path, idx)
{
	S_meter.marker_v = S_meter.marker_sec[+idx];
	graph_marker(S_meter.marker_v);
}

function S_meter_stop_start_cb(path, idx, first)
{
   S_meter.stop_start_state ^= 1;
   w3_button_text(path, S_meter.stop_start_state? 'Start' : 'Stop');
}

function S_meter_mark_cb(path, idx, first)
{
   graph_divider('magenta');
   
   // if stopped plot last value so mark appears
   if (S_meter.stop_start_state)
      graph_plot(S_meter.last_plot);
}

function S_meter_clear_cb(path, val)
{
	graph_clear();
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function S_meter_averaging_cb(path, checked, first)
{
	graph_averaging(checked);
}

// detect when frequency or mode has changed and mark graph
function S_meter_update(init)
{
	var freq = ext_get_freq();
	var mode = ext_get_mode();
	var freq_chg = (freq != S_meter.sm_last_freq);
	var mode_chg = (mode != S_meter.sm_last_mode);

	if (init || freq_chg || mode_chg) {
	   if (!init) {
         if (freq_chg) graph_divider('red');
         if (mode_chg) graph_divider('lime');
      }
		S_meter.sm_last_freq = freq;
		S_meter.sm_last_mode = mode;
	}
}

// hook that is called when controls panel is closed
function S_meter_blur()
{
	//console.log('### S_meter_blur');
	ext_send('SET run=0');
	kiwi_clearInterval(S_meter.update_interval);
}

// called to display HTML for configuration parameters in admin interface
function S_meter_config_html()
{
	ext_admin_config(S_meter.ext_name, 'S Meter',
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
