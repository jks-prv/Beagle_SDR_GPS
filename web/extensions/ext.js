// Copyright (c) 2016 John Seamons, ZL/KF6VO

function ext_switch_to_client(ext_name, first_time, recv_func)
{
	//console.log('SET ext_switch_to_client='+ ext_name +' first_time='+ first_time +' rx_chan='+ rx_chan);
	recv_websocket(extint_ws, recv_func);		// change recv callback with each ext activation
	ext_send('SET ext_switch_to_client='+ ext_name +' first_time='+ (first_time? 1:0) +' rx_chan='+ rx_chan);
}

function ext_send(msg)
{
	//console.log('ext_send: '+ msg);
	extint_ws.send(msg);
}

function ext_panel_show(controls_html, data_html, show_func)
{
	extint_panel_show(controls_html, data_html, show_func);
}

function ext_set_controls_width(width)
{
	panel_set_width('ext-controls', width);
}

function ext_get_cfg_param(path, init_val)
{
	var cur_val;
	
	path = w3_add_toplevel(path);
	
	try {
		cur_val = getVarFromString(path);
	} catch(ex) {
		// when scope is missing create all the necessary scopes and variable as well
		cur_val = null;
	}
	
	if ((cur_val == null || cur_val == undefined) && init_val != undefined) {		// scope or parameter doesn't exist, create it
		cur_val = init_val;
		// parameter hasn't existed before or hasn't been set (empty field)
		//console.log('ext_get_cfg_param: creating path='+ path +' cur_val='+ cur_val);
		setVarFromString(path, cur_val);
		//console.log('ext_get_cfg_param: SAVE path='+ path +' init_val='+ init_val);
		cfg_save_json(path, extint_ws);
	}
	
	return cur_val;
}

function ext_get_cfg_param_string(path, init_val)
{
	return decodeURIComponent(ext_get_cfg_param(path, init_val));
}

function ext_set_cfg_param(path, val, save)
{
	path = w3_add_toplevel(path);	
	setVarFromString(path, val);
	if (save != undefined && save == true) {
		//console.log('ext_set_cfg_param: SAVE path='+ path +' val='+ val);
		cfg_save_json(path, extint_ws);
	}
}

function ext_set_save_cfg_param(path, val)
{
	ext_set_cfg_param(path, val, /* save */ true);
}

var ext_zoom = {
	TO_BAND: 0,
	IN: 1,
	OUT: -1,
	ABS: 2,
	MAX_IN: 9,
	MAX_OUT: -9
};

function ext_tune(fdsp, mode, zoom, zoom_level) {		// specifying mode is optional
	//console.log('ext_tune: '+ fdsp +', '+ mode +', '+ zoom +', '+ zoom_level);
	freqmode_set_dsp_kHz(fdsp, mode);
	
	if (zoom != undefined) {
		zoom_step(zoom, zoom_level);
	} else {
		zoom_step(ext_zoom.TO_BAND);
	}
}

function ext_get_freq()
{
	return freq_displayed_Hz;
}

function ext_get_carrier_freq()
{
	return freq_car_Hz;
}

function ext_get_mode()
{
	return cur_mode;
}

function ext_set_mode(mode)
{
	demodulator_analog_replace(mode);
}

function ext_set_passband(low_cut, high_cut, fdsp)		// specifying fdsp is optional
{
	var demod = demodulators[0];
	demod.low_cut = low_cut;
	demod.high_cut = high_cut;
	
	if (fdsp != undefined && fdsp != null) {
		fdsp *= 1000;
		freq_car_Hz = freq_dsp_to_car(fdsp);
	}

	demodulator_set_offset_frequency(0, freq_car_Hz - center_freq);
}

// This just decides if a password exchange is needed to establish the credential.
// The actual change of server state by any client code must be validated by
// a per-change check ON THE SERVER (e.g. validate that the connection is a STREAM_ADMIN)

function ext_hasCredential(conn_type, cb, cb_param)
{
	if (conn_type == 'mfg') conn_type = 'admin';
	
	var pwd = readCookie(conn_type);
	pwd = pwd? pwd:'';	// make non-null
	pwd = decodeURIComponent(pwd);
	//console.log('ext_hasCredential: readCookie '+ conn_type +'="'+ pwd +'"');
	
	// always check in case not having a pwd is accepted by local subnet match
	extint_pwd_cb = cb;
	extint_pwd_cb_param = cb_param;
	ext_valpwd(conn_type, pwd);
}

function ext_valpwd(conn_type, pwd)
{
	// send and store the password encoded to prevent problems:
	//		with scanf() on the server end, e.g. embedded spaces
	//		with cookie storage that deletes leading and trailing whitespace
	pwd = encodeURIComponent(pwd);
	writeCookie(conn_type, pwd);
	//console.log('ext_valpwd: writeCookie '+ conn_type +'="'+ pwd +'"');
	extint_conn_type = conn_type;

	ext_send('SET auth t='+ conn_type +' p='+ pwd);
	// the server reply then calls extint_valpwd_cb() below
}


// private

function extint_init()
{
	window.addEventListener("resize", extint_resize);
}

function extint_resize()
{
	if (extint_current_ext_name)
		w3_call(extint_current_ext_name +'_resize');
}

var extint_pwd_cb = null;
var extint_pwd_cb_param = null;
var extint_conn_type = null;

function extint_valpwd_cb(badp)
{
	if (extint_pwd_cb) extint_pwd_cb(badp, extint_pwd_cb_param);
}

var extint_ws;

function extint_open_ws_cb()
{
	// should always work since extensions are only loaded from an already validated client
	ext_hasCredential('kiwi', null, null);
	setTimeout(function() { setInterval(function() { ext_send("SET keepalive") }, 5000) }, 5000);
}

function extint_connect_server()
{
	extint_ws = open_websocket('EXT', extint_open_ws_cb, null, extint_msg_cb);

	// when the stream thread is established on the server it will automatically send a "SET init" to us
	return extint_ws;
}

function extint_msg_cb(param, ws)
{
	switch (param[0]) {
		case "keepalive":
			break;

		case "ext_client_init":
			extint_focus();
			break;
	}
}

var extint_current_ext_name = null;

function extint_blur_prev()
{
	if (extint_current_ext_name != null) {
		w3_call(extint_current_ext_name +'_blur');
		recv_websocket(extint_ws, null);		// ignore further server ext messages
		ext_set_controls_width();		// restore width
		extint_current_ext_name = null;
	}
	
	if (extint_ws)
		ext_send('SET ext_blur='+ rx_chan);
}

function extint_focus()
{
	//console.log('extint_focus: calling '+ extint_current_ext_name +'_main()');
	w3_call(extint_current_ext_name +'_main');
}

var extint_first_ext_load = true;

// called on extension menu item selection
function extint_select(idx)
{
	extint_blur_prev();
	
	idx = +idx;
	html('select-ext').value = idx;
	extint_current_ext_name = extint_names[idx];
	if (extint_first_ext_load) {
		extint_ws = extint_connect_server();
		extint_first_ext_load = false;
	} else {
		extint_focus();
	}
}

var extint_names;

function extint_list_json(param)
{
	extint_names = JSON.parse(decodeURIComponent(param));
	//console.log('extint_names=');
	//console.log(extint_names);
}

function extint_select_menu()
{
	var s = '';
	if (extint_names) for (var i=0; i < extint_names.length; i++) {
		if (!dbgUs && extint_names[i] == 's4285') continue;	// FIXME: hide while we develop
		if (!dbgUs && extint_names[i] == 'test') continue;	// FIXME: hide while we develop
		s += '<option value="'+ i +'">'+ extint_names[i] +'</option>';
	}
	//console.log('extint_select_menu = '+ s);
	return s;
}

function extint_override(name)
{
	for (var i=0; i < extint_names.length; i++) {
		if (extint_names[i].includes(name)) {
			//console.log('extint_override match='+ extint_names[i]);
			setTimeout('extint_select('+ i +')', 3000);
			break;
		}
	}
}
