// Copyright (c) 2016 John Seamons, ZL/KF6VO

function ext_switch_to_client(ext_name, first_time, recv_func)
{
	//console.log('SET ext_switch_to_client='+ ext_name +' first_time='+ first_time +' rx_chan='+ rx_chan);
	recv_websocket(extint_ws, recv_func);		// change recv callback with each ext activation
	ext_send('SET ext_switch_to_client='+ ext_name +' first_time='+ (first_time? 1:0) +' rx_chan='+ rx_chan);
}

function ext_send(msg, ws)
{
	if (ws == undefined)
		ws = extint_ws;

	try {
	   //console.log('ext_send: ws='+ ws.stream +' '+ msg);
	   ws.send(msg);
		return 0;
	} catch(ex) {
		console.log("CATCH ext_send('"+s+"') ex="+ex);
		kiwi_trace();
	}
}

function ext_panel_show(controls_html, data_html, show_func)
{
	extint_panel_show(controls_html, data_html, show_func);
}

function ext_set_data_height(height)
{
   var el = w3_el('id-ext-data-container');
   if (!height) {
      if (el) el.style.height = '';      // revert to default
   } else {
      if (el) el.style.height = px(height);
   }
}

// default size: w=525, h=300
function ext_set_controls_width_height(width, height)
{
	panel_set_width_height('ext-controls', width, height);
}

var EXT_SAVE = true;
var EXT_NO_SAVE = false;   // set the local JSON cfg, but don't set on server which would require admin privileges.

function ext_get_cfg_param(path, init_val, save)
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
		
		// save newly initialized value in configuration unless EXT_NO_SAVE specified
		//console.log('ext_get_cfg_param: SAVE path='+ path +' init_val='+ init_val);
		if (save == undefined || save == EXT_SAVE)
			cfg_save_json(path, extint_ws);
	}
	
	return cur_val;
}

function ext_get_cfg_param_string(path, init_val, save)
{
	return decodeURIComponent(ext_get_cfg_param(path, init_val, save));
}

function ext_set_cfg_param(path, val, save)
{
	path = w3_add_toplevel(path);	
	setVarFromString(path, val);
	
	// save unless EXT_NO_SAVE specified
	if (save != undefined && save == EXT_SAVE) {
		//console.log('ext_set_cfg_param: SAVE path='+ path +' val='+ val);
		cfg_save_json(path, extint_ws);
	}
}

function ext_get_freq_range()
{
   return { lo_kHz: cfg.sdr_hu_lo_kHz, hi_kHz: cfg.sdr_hu_hi_kHz, offset_kHz: cfg.freq_offset };
}

var ext_zoom = {
	TO_BAND: 0,
	IN: 1,
	OUT: -1,
	ABS: 2,
	NOM_IN: 8,
	MAX_IN: 9,
	MAX_OUT: -9
};

var extint_ext_is_tuning = false;

function ext_tune(fdsp, mode, zoom, zoom_level) {		// specifying mode is optional
	//console.log('ext_tune: '+ fdsp +', '+ mode +', '+ zoom +', '+ zoom_level);
	
	extint_ext_is_tuning = true;
      freqmode_set_dsp_kHz(fdsp, mode);
      
      if (zoom != undefined) {
         zoom_step(zoom, zoom_level);
      } else {
         zoom_step(ext_zoom.TO_BAND);
      }
	extint_ext_is_tuning = false;
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

function ext_get_passband()
{
	var demod = demodulators[0];
	return { low: demod.low_cut, high: demod.high_cut };
}

function ext_set_passband(low_cut, high_cut, set_mode_pb, fdsp)		// specifying fdsp is optional
{
	var demod = demodulators[0];
	var filter = demod.filter;
	var bw = Math.abs(high_cut - low_cut);
	//console.log('SET_PB bw='+ bw +' lo='+ low_cut +' hi='+ high_cut +' set_mode_pb='+ set_mode_pb);
	//console.log('SET_PB Lbw='+ filter.min_passband +' Llo='+ filter.low_cut_limit +' Lhi='+ filter.high_cut_limit);
	//console.log('SET_PB freq_car_Hz='+ freq_car_Hz +' center_freq='+ center_freq +' off='+ (freq_car_Hz - center_freq));
	
	low_cut = (low_cut < filter.low_cut_limit)? filter.low_cut_limit : low_cut;
	high_cut = (high_cut > filter.high_cut_limit)? filter.high_cut_limit : high_cut;
	var bw = Math.abs(high_cut - low_cut);
	//console.log('SET_PB_CLIP bw='+ bw +' lo='+ low_cut +' hi='+ high_cut);
	
	var okay = false;
	if (bw >= filter.min_passband && low_cut < high_cut) {
		demod.low_cut = low_cut;
		demod.high_cut = high_cut;
		okay = true;
	}
	//console.log('SET_PB okay='+ okay);
	
	// set the passband for the current mode as well (sticky)
	if (set_mode_pb != undefined && set_mode_pb && okay) {
		passbands[cur_mode].last_lo = low_cut;
		passbands[cur_mode].last_hi = high_cut;
	}
	
	if (fdsp != undefined && fdsp != null) {
		fdsp *= 1000;
		freq_car_Hz = freq_dsp_to_car(fdsp);
	}

	extint_ext_is_tuning = true;
	   demodulator_set_offset_frequency(0, freq_car_Hz - center_freq);
	extint_ext_is_tuning = false;
}

function ext_get_zoom()
{
	return zoom_level;
}


// This just decides if a password exchange is needed to establish the credential.
// The actual change of server state by any client code must be validated by
// a per-change check ON THE SERVER (e.g. validate that the connection is a STREAM_ADMIN)

function ext_hasCredential(conn_type, cb, cb_param, ws)
{
	if (conn_type == 'mfg') conn_type = 'admin';
	
	var pwd;
	if (conn_type == 'admin') {
	   deleteCookie('admin');
	   deleteCookie('admin-pwd');
	   pwd = '';
	} else {
      pwd = readCookie(conn_type);
      pwd = pwd? decodeURIComponent(pwd):'';    // make non-null
   }
	//console.log('ext_hasCredential: readCookie '+ conn_type +'="'+ pwd +'"');
	
	// always check in case not having a pwd is accepted by local subnet match
	extint_pwd_cb = cb;
	extint_pwd_cb_param = cb_param;
	ext_valpwd(conn_type, pwd, ws);
}

function ext_valpwd(conn_type, pwd, ws)
{
	// send and store the password encoded to prevent problems:
	//		with scanf() on the server end, e.g. embedded spaces
	//		with cookie storage that deletes leading and trailing whitespace
	pwd = encodeURIComponent(pwd);
	if (conn_type != 'admin')
	   writeCookie(conn_type, pwd);
	//console.log('ext_valpwd: writeCookie '+ conn_type +'="'+ pwd +'"');
	extint_conn_type = conn_type;
	
	// pwd can't be empty if there is an ipl= (use # since it would have been encoded if in real pwd)
	pwd = (pwd != '')? pwd : '#';
	
	var ipl = readCookie('iplimit');
	ipl = ipl? (' ipl='+ ipl) : '';

	//console.log('SET auth t='+ conn_type +' p='+ pwd + ipl);
	ext_send('SET auth t='+ conn_type +' p='+ pwd + ipl, ws);
	// the server reply then calls extint_valpwd_cb() below
}

var extint_isAdmin_cb;

function ext_isAdmin(cb)
{
   extint_isAdmin_cb = cb;
	ext_send('SET is_admin');
}

var extint_authkey_cb;

function ext_get_authkey(func)
{
	ext_send('SET get_authkey');
	extint_authkey_cb = func;
}

// updated by gps_stats_cb() from kiwi_msg() "stats_cb="
var extint_adc_clock_Hz = 0;

function ext_adc_clock_Hz()
{
	return extint_adc_clock_Hz;
}

// updated by gps_stats_cb() from kiwi_msg() "stats_cb="
var extint_adc_gps_clock_corr = 0;

function ext_adc_gps_clock_corr()
{
	return extint_adc_gps_clock_corr;
}

// updated from kiwi_msg() "adc_clk_nom="
var extint_adc_clock_nom_Hz = 0;

function ext_adc_clock_nom_Hz()
{
	return extint_adc_clock_nom_Hz;
}

// updated from kiwi_msg() "sample_rate="
var extint_srate = 0;

function ext_sample_rate()
{
	return extint_srate;
}

var extint_audio_data_cb = null;

function ext_register_audio_data_cb(func)
{
	extint_audio_data_cb = func;
}

function ext_unregister_audio_data_cb()
{
	extint_audio_data_cb = null;
}

// return (once) extension parameter supplied in URL query
function ext_param()
{
	var p = extint_param;
	extint_param = null;
	return p;
}

var extint_current_ext_name = null;

function ext_panel_func(name)
{
	extint_current_ext_name = name;
}


////////////////////////////////
// internal routines
////////////////////////////////

function extint_init()
{

}

function extint_environment_changed(changed)
{
	if (extint_current_ext_name) {
	   w3_call(extint_current_ext_name +'_environment_changed', changed);
	}
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

	// when the stream thread is established on the server it will automatically send a "MSG ext_client_init" to us
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

function extint_blur_prev()
{
	if (extint_current_ext_name != null) {
		w3_call(extint_current_ext_name +'_blur');
		recv_websocket(extint_ws, null);		// ignore further server ext messages
		ext_set_controls_width_height();		// restore width
		extint_current_ext_name = null;
		time_display_setup('id-topbar-right-container');
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
		//if (!dbgUs && extint_names[i] == 'devl') continue;

		if (!dbgUs && extint_names[i] == 's4285') continue;	// FIXME: hide while we develop
		if (!dbgUs && extint_names[i] == 'test') continue;	// FIXME: hide while we develop
		if (!dbgUs && extint_names[i] == 'timecode') continue;	// FIXME: hide while we develop
		if (!dbgUs && extint_names[i] == 'colormap') continue;	// FIXME: hide while we develop

		s += '<option value="'+ i +'">'+ extint_names[i] +'</option>';
	}
	//console.log('extint_select_menu = '+ s);
	return s;
}

function extint_open(name, delay)
{
   name = name.toLowerCase();
	for (var i=0; i < extint_names.length; i++) {
		if (extint_names[i].toLowerCase().includes(name)) {
			//console.log('extint_open match='+ extint_names[i]);
			if (delay) {
			   //console.log('extint_open '+ name +' delay='+ delay);
			   setTimeout(function() {extint_select(i);}, delay);
			} else {
			   //console.log('extint_open '+ name +' NO DELAY');
			   extint_select(i);
			}
			break;
		}
	}
}

function extint_audio_data(data, samps)
{
	if (typeof extint_audio_data_cb == 'function')
		extint_audio_data_cb(data, samps);
}
