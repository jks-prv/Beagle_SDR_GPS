// Copyright (c) 2016 John Seamons, ZL/KF6VO

var extint = {
   ws: null,
   extname: null,
   param: null,
   override_pb: false,
   displayed: false,
   help_displayed: false,
   spectrum_used: false,
   current_ext_name: null,
   using_data_container: false,
   default_w: 525,
   default_h: 300,
   prev_mode: null,
   seq: 0,
   
   // extensions not subject to DRM lockout
   // FIXME: allow C-side API to specify
   no_lockout: [ 'noise_blank', 'noise_filter', 'ant_switch', 'iframe', 'colormap', 'devl' ],
   excl_devl: [ 'devl', 's4285' ],
   
   OPT_NOLOCAL: 1,
};

var devl = {
   p0: 0, p1: 0, p2: 0, p3: 0, p4: 0, p5: 0, p6: 0, p7: 0
};

function ext_switch_to_client(ext_name, first_time, recv_func)
{
	//console.log('SET ext_switch_to_client='+ ext_name +' first_time='+ first_time +' rx_chan='+ rx_chan);
	recv_websocket(extint.ws, recv_func);		// change recv callback with each ext activation
	ext_send('SET ext_switch_to_client='+ ext_name +' first_time='+ (first_time? 1:0) +' rx_chan='+ rx_chan);
	w3_call(ext_name +'_focus');
}

function ext_send(msg, ws)
{
	if (ws == undefined)
		ws = extint.ws;

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

// ext_set_controls_width_height defaults: width=525 height=300
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
	
   //console.log('ext_get_cfg_param: path='+ path +' cur_val='+ cur_val +' init_val='+ init_val);
	if ((cur_val == null || cur_val == undefined) && init_val != undefined) {		// scope or parameter doesn't exist, create it
		cur_val = init_val;
		// parameter hasn't existed before or hasn't been set (empty field)
		//console.log('ext_get_cfg_param: creating path='+ path +' cur_val='+ cur_val);
		setVarFromString(path, cur_val);
		
		// save newly initialized value in configuration unless EXT_NO_SAVE specified
		//console.log('ext_get_cfg_param: SAVE path='+ path +' init_val='+ init_val);
		if (save == undefined || save == EXT_SAVE)
			cfg_save_json(path, extint.ws);
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
		cfg_save_json(path, extint.ws);
	}
}

function ext_get_freq_range()
{
   var offset = cfg.freq_offset;
   return { lo_kHz: cfg.sdr_hu_lo_kHz + offset, hi_kHz: cfg.sdr_hu_hi_kHz + offset, offset_kHz: offset };
}

var ext_zoom = {
	TO_BAND: 0,
	IN: 1,
	OUT: -1,
	ABS: 2,
	WHEEL: 3,
	NOM_IN: 8,
	MAX_IN: 9,
	MAX_OUT: -9
};

var extint_ext_is_tuning = false;

// mode, zoom and passband are optional
function ext_tune(freq_dial_kHz, mode, zoom, zoom_level, low_cut, high_cut) {
   var pb_specified = (low_cut != undefined && high_cut != undefined);
	//console.log('ext_tune: '+ freq_dial_kHz +', '+ mode +', '+ zoom +', '+ zoom_level);
	
	extint_ext_is_tuning = true;
      freqmode_set_dsp_kHz(freq_dial_kHz, mode);
      if (pb_specified) ext_set_passband(low_cut, high_cut);
      
      if (zoom != undefined) {
         zoom_step(zoom, zoom_level);
      } else {
         zoom_step(ext_zoom.TO_BAND);
      }
	extint_ext_is_tuning = false;
}

function ext_get_freq_Hz()
{
	return { displayed: freq_displayed_Hz, carrier: freq_car_Hz, passband_center: freq_passband_center() };
}

function ext_get_freq()
{
	return freq_displayed_Hz;
}

function ext_get_freq_kHz()
{
	return (freq_displayed_Hz/1e3).toFixed(2);
}

function ext_get_carrier_freq()
{
	return freq_car_Hz;
}

function ext_get_passband_center_freq()
{
   return freq_passband_center();
}

function ext_get_mode()
{
	return cur_mode;
}

function ext_is_IQ_or_stereo_mode(mode)
{
   return (mode == 'drm' || mode == 'iq' || mode == 'sas' || mode == 'qam');
}

function ext_is_IQ_or_stereo_curmode()
{
   return ext_is_IQ_or_stereo_mode(cur_mode);
}

function ext_get_prev_mode()
{
	return extint.prev_mode;
}

function ext_set_mode(mode, freq, opt)
{
   var new_drm = (mode == 'drm');
   if (new_drm)
      extint.prev_mode = cur_mode;

   //console.log('### ext_set_mode '+ mode +' prev='+ extint.prev_mode);
	demodulator_analog_replace(mode, freq);
	
	var open_ext = w3_opt(opt, 'open_ext', false);
	var no_drm_proc = w3_opt(opt, 'no_drm_proc', false);
	var drm_active = (typeof(drm) != 'undefined' && drm.active);
	//console.log('$ new_drm='+ new_drm +' open_ext='+ open_ext);

   if (!no_drm_proc) {
      if (new_drm && open_ext) {
         if (drm_active) {
            // DRM already loaded and running, just open the control panel again (mobile mode)
            toggle_panel("ext-controls", 1);
         } else {
            extint_open('drm');
         }
      } else
      if (!new_drm && drm_active) {
         // shutdown DRM (if active) when mode changed
         extint_panel_hide();
         demodulator_analog_replace(mode, freq);   // don't use mode restored by DRM_blur()
      }
   }
}

function ext_get_passband()
{
	var demod = demodulators[0];
	return { low: demod.low_cut, high: demod.high_cut };
}

function ext_set_passband(low_cut, high_cut, set_mode_pb, freq_dial_Hz)		// specifying freq_dial_Hz is optional
{
	var demod = demodulators[0];
	var filter = demod.filter;
	
	if (low_cut  == undefined) low_cut  = demod.low_cut;
	if (high_cut == undefined) high_cut = demod.high_cut;
	
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
	if (isArg(set_mode_pb) && set_mode_pb && okay) {
		passbands[cur_mode].last_lo = low_cut;
		passbands[cur_mode].last_hi = high_cut;
	}
	
	if (freq_dial_Hz != undefined && freq_dial_Hz != null) {
		freq_dial_Hz *= 1000;
		freq_car_Hz = freq_dsp_to_car(freq_dial_Hz);
	}

	extint_ext_is_tuning = true;
	   demodulator_set_offset_frequency(0, freq_car_Hz - center_freq);
	extint_ext_is_tuning = false;
}

function ext_get_tuning()
{
   return { low: demod.low_cut, high: demod.high_cut, mode: cur_mode, freq_dial_kHz: freq_displayed_Hz/1000 };
}

function ext_get_zoom()
{
	return zoom_level;
}

function ext_agc_delay(set_val)
{
   var prev_val = w3_el('input-decay').value;
   if (isDefined(set_val)) setDecay(true, set_val);
   return prev_val;
}

extint.optbars = {
   'optbar-wf':0, 'optbar-audio':1, 'optbar-agc':2, 'optbar-users':3, 'optbar-status':4, 'optbar-off':5
};

function ext_get_optbar()
{
   var optbar = readCookie('last_optbar');      // optbar-xxx
   return optbar;
}

function ext_set_optbar(optbar)
{
   if (extint.optbars[optbar]) {
      writeCookie('last_optbar', optbar);
      w3_el('id-nav-'+ optbar).click();
   }
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
	
	// must always store the pwd because it is typically requested (and stored) during the
	// SND negotiation, but then needs to be available for the subsequent W/F connection
	if (conn_type != 'admin')
	   writeCookie(conn_type, pwd);
	
	//console.log('ext_valpwd: writeCookie '+ conn_type +'="'+ pwd +'"');
	extint_conn_type = conn_type;
	
	// pwd can't be empty if there is an ipl= (use # since it would have been encoded if in real pwd)
	pwd = (pwd != '')? pwd : '#';
	
	var ipl = null;
	var iplimit_cookie = readCookie('iplimit');
   var iplimit_pwd = kiwi_url_param(['pwd', 'password'], '', '');

	if (iplimit_pwd != '') {   // URL param overrides cookie
	   ipl = iplimit_pwd;
	} else
	if (iplimit_cookie && iplimit_cookie != '') {
	   ipl = iplimit_cookie;
	}
	ipl = ipl? (' ipl='+ ipl) : '';

   // don't change to conn type 'prot' if e.g. admin panel privilege escalation during user connection (e.g. DX label editing)
   if (kiwi_url_param(['p', 'prot', 'protected'], 'true', null) != null && conn_type != 'admin')
      conn_type = 'prot';
	//console.log('SET auth t='+ conn_type +' p='+ pwd + ipl);
	
	var options = 0;
	if (kiwi_url_param('nolocal')) options |= extint.OPT_NOLOCAL;
	if (options != 0) ext_send('SET options='+ options);

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
	var p = extint.param;
	extint.param = null;
	return p;
}

function ext_panel_set_name(name)
{
	extint.current_ext_name = name;
}


/*
screen.{width,height}	P=portrait L=landscape
			   w     h		screen.[wh] in portrait
			   h     w		rotated to landscape
iPhone 5S	320   568	P
iPhone X	   414   896	P
levono		600   976	P 7"
huawei		600   976	P 7"

iPad 2		768   1024	P
MBP 15"		1440  900	L
*/

function ext_mobile_info(last)
{
   var w = window.innerWidth;
   var h = window.innerHeight;
   var rv = { width:w, height:h };
   rv.wh_unchanged = (last && last.width == w && last.height == h)? 1:0;

	var isPortrait = (w < h || mobile_laptop_test)? 1:0;
   rv.orient_unchanged = (last && last.isPortrait == isPortrait)? 1:0;

	rv.isPortrait = isPortrait? 1:0;
	rv.iPad     = (isPortrait && w <= 768)? 1:0;    // iPad or smaller
	rv.small    = (isPortrait && w <  768)? 1:0;    // anything smaller than iPad
	rv.narrow   = (isPortrait && h <= 600)? 1:0;    // narrow screens, i.e. phones and 7" tablets
   return rv;
}


////////////////////////////////
// internal routines
////////////////////////////////

function extint_news(s)
{
   var el = w3_el('id-news');
   el.style.width = '400px';
   el.style.height = '300px';
   el.style.visibility = 'visible';
   el.style.zIndex = 9999;
   w3_innerHTML('id-news-inner', s);
}

function ext_panel_init()
{
   w3_el('id-panels-container').innerHTML +=
      '<div id="id-ext-controls" class="class-panel" data-panel-name="ext-controls" data-panel-pos="bottom-left" data-panel-order="0" ' +
      'data-panel-size="'+ extint.default_w +','+ extint.default_h +'"></div>';

	var el = w3_el('id-ext-data-container');
	// removed because cascading canvases in FFT extension overlay RF waterfall
	// why was this needed anyway?
	//el.style.zIndex = 100;

	el = w3_el('id-ext-controls');
	el.innerHTML =
		w3_div('id-ext-controls-container w3-relative|width:100%;height:100%;') +
		w3_div('id-ext-controls-vis class-vis') +
		w3_div('id-ext-controls-help cl-ext-help',
		   w3_button('id-ext-controls-help-btn w3-green w3-small w3-padding-small w3-disabled||onclick="extint_help_click()"', 'help')
		);
	
	// close ext panel if escape key while input field has focus
	/*
	el.addEventListener('keyup', function(evt) {
		//event_dump(evt, 'Escape-ext');
		if (evt.key == 'Escape' && evt.target.nodeName == 'INPUT')
			extint_panel_hide();
	}, false);
	*/

	// close ext panel if escape key
	w3_el('id-kiwi-body').addEventListener('keyup',
	   function(evt) {
	      if (evt.key == 'Escape' && extint.displayed && !confirmation.displayed) {
	         // simulate click in case something other than extint_panel_hide() has been hooked
	         //extint_panel_hide();
	         if (w3_call(extint.current_ext_name +'_escape_key_cb') != true)
	            w3_el('id-ext-controls-close').click();
	      }
	   }, true);
}

function ext_show_spectrum()
{
   w3_show_block('id-spectrum-container');
   extint.spectrum_used = true;
}

function ext_hide_spectrum()
{
   w3_hide('id-spectrum-container');
   extint.spectrum_used = false;
}

function extint_panel_show(controls_html, data_html, show_func, show_help_button)
{
	extint.using_data_container = (data_html? true:false);
	//console.log('extint_panel_show using_data_container='+ extint.using_data_container);

	if (extint.using_data_container) {
		toggle_or_set_spec(toggle_e.SET, 0);
		w3_hide('id-top-container');
		w3_show_block(w3_innerHTML('id-ext-data-container', data_html));
	} else {
		w3_hide('id-ext-data-container');
		if (!spec.source_wf)
			w3_show_block('id-top-container');
	}

   // remove previous use of spectrum (if any)
   ext_hide_spectrum();

   // remove previous help panel if displayed
   if (extint.help_displayed == true) {
      confirmation_panel_close();
      extint.help_displayed = false;
   }

	// hook the close icon to call extint_panel_hide()
	var el = w3_el('id-ext-controls-close');
	el.onclick = function() { toggle_panel("ext-controls"); extint_panel_hide(); };
	//console.log('extint_panel_show onclick='+ el.onclick);
	
	// some exts change these -- change back to default
	ext_set_data_height();     // restore default height
	w3_el('id-ext-controls').style.zIndex = 150;
   w3_create_attribute('id-ext-controls-close-img', 'src', 'icons/close.24.png');
	
	var el = w3_el('id-ext-controls-container');
	el.innerHTML = controls_html;
	//console.log(controls_html);
	
	if (show_func) show_func();
	
	el = w3_el('id-ext-controls');
	el.style.zIndex = 150;
	//el.style.top = px((extint.using_data_container? height_spectrum_canvas : height_top_bar_parts) +157+10);
	w3_visible(el, true);
	el.panelShown = 1;
   toggle_or_set_hide_panels(0);    // cancel panel hide mode

	
	// help button
	w3_el('id-confirmation-container').style.height = '';    // some exts modify this
	show_help_button = isDefined(show_help_button)?
	      show_help_button
	   :
	      w3_call(extint.current_ext_name +'_help', false);
	//console.log('show_help_button '+ extint.current_ext_name +' '+ show_help_button);
   w3_set_props('id-ext-controls-help-btn', 'w3-disabled', isUndefined(show_help_button) || show_help_button == false);
   w3_show_hide('id-ext-controls-help-btn', show_help_button != 'off');
	
	extint.displayed = true;
}

function ext_panel_displayed(ext_name) {
   return (extint.displayed && (ext_name? (ext_name == extint.current_ext_name) : true));
}

function ext_panel_redisplay(s) { w3_innerHTML('id-ext-controls-container', s); }

function extint_panel_hide()
{
	//console.log('extint_panel_hide using_data_container='+ extint.using_data_container);

	if (extint.using_data_container) {
		w3_hide('id-ext-data-container');
      ext_hide_spectrum();
		w3_show_block('id-top-container');
		extint.using_data_container = false;
		
		if (extint.help_displayed == true) {
		   confirmation_panel_close();
		   extint.help_displayed = false;
		}
	}
	
	w3_visible('id-ext-controls', false);
	//w3_visible('id-msgs', true);
	
	extint_blur_prev(1);
	
	// on close, reset extension menu
	w3_select_value('id-select-ext', -1);
	
	resize_waterfall_container(true);	// necessary if an ext was present so wf canvas size stays correct
   freqset_select();

   extint.displayed = false;
}

function extint_help_click_now()
{
	extint.help_displayed = w3_call(extint.current_ext_name +'_help', true);
}

function extint_help_click(delay)
{
   // will send click event even if w3-disabled!
   if (w3_contains('id-ext-controls-help-btn', 'w3-disabled')) return;
   console.log(extint.current_ext_name +'_help_click CLICKED');
   
   if (delay)
      setTimeout(function() { extint_help_click_now(); }, 2000);
   else
      extint_help_click_now();
}

function extint_environment_changed(changed)
{
   // have to wait a bit since extint_environment_changed({freq:1}) is called before
   // e.g. ext_get_freq_kHz() has been updated with latest value
   
   setTimeout(
      function() {
         //console_log_fqn('extint_environment_changed', 'extint.current_ext_name');
         if (extint.current_ext_name) {
            w3_call(extint.current_ext_name +'_environment_changed', changed);
         }

         // for benefit of programs like CATSync that use injected javascript to catch events
         w3_call('injection_environment_changed', changed);
      }, 100
   );
}

/*
var iec_seq = 0;
function injection_environment_changed(changed)
{
   console.log('injection_environment_changed '+ iec_seq +': '+
      (changed.freq? 'FREQ ':'') +
      (changed.passband? 'PASSBAND ':'') +
      (changed.mode? 'MODE ':'') +
      (changed.zoom? 'ZOOM ':'') +
      (changed.resize? 'RESIZE ':'')
   );
   console.log('ext_get_freq_kHz='+ ext_get_freq_kHz());
   iec_seq++;
   //kiwi_trace();
}
*/

var extint_pwd_cb = null;
var extint_pwd_cb_param = null;
var extint_conn_type = null;

function extint_valpwd_cb(badp)
{
	if (extint_pwd_cb) extint_pwd_cb(badp, extint_pwd_cb_param);
}

function extint_open_ws_cb()
{
	// should always work since extensions are only loaded from an already validated client
	ext_hasCredential('kiwi', null, null);
	setTimeout(function() { setInterval(function() { ext_send("SET keepalive") }, 5000) }, 5000);
}

function extint_connect_server()
{
	extint.ws = open_websocket('EXT', extint_open_ws_cb, null, extint_msg_cb);

	// when the stream thread is established on the server it will automatically send a "MSG ext_client_init" to us
	return extint.ws;
}

function extint_msg_cb(param, ws)
{
	switch (param[0]) {
		case "keepalive":
			break;

		case "ext_client_init":
		   console.log('ext_client_init is_locked='+ +param[1]);
			extint_focus(+param[1]);
			break;
		
		default:
		   return false;
	}
	
	return true;
}

function extint_blur_prev(restore)
{
	if (extint.current_ext_name != null) {
		w3_call(extint.current_ext_name +'_blur');
		recv_websocket(extint.ws, null);		// ignore further server ext messages
		if (restore) ext_set_controls_width_height();		// restore width/height
		extint.current_ext_name = null;
		time_display_setup('id-topbar-right-container');
	}
	
	if (extint.ws)
		ext_send('SET ext_blur='+ rx_chan);
}

function extint_focus(is_locked)
{
   // dynamically load extension (if necessary) before calling <ext>_main()
   var ext = extint.current_ext_name;
	console.log('extint_focus: loading '+ ext +'.js');
	
	if (is_locked && !extint.no_lockout.includes(ext)) {
	   var s =
         w3_text('w3-medium w3-text-css-yellow',
            'Cannot use extensions while <br> another channel is in DRM mode.'
         );
      extint_panel_show(s, null, null, 'off');
      ext_set_controls_width_height(300, 75);
      return;
	}

   kiwi_load_js_dir('extensions/'+ ext +'/'+ ext, ['.js', '.css'],

      // post-load
      function() {
         console.log('extint_focus: calling '+ ext +'_main()');
         //setTimeout('ext_set_controls_width_height(); w3_call('+ ext +'_main);', 3000);
         ext_set_controls_width_height();
         w3_call(ext +'_main');
      },

      // pre-load
      function(loaded) {
         console.log('extint_focus: '+ ext +' loaded='+ loaded);
         if (loaded) {
            var s = 'loading extension...';
            extint_panel_show(s, null, null, 'off');
            ext_set_controls_width_height(325, 45);
            if (kiwi.is_locked)
               console.log('==== IS_LOCKED =================================================');
         }
      }
   );
}

var extint_first_ext_load = true;

// called on extension menu item selection
function extint_select(value)
{
	extint_blur_prev(0);
	
	value = +value;
	w3_select_value('id-select-ext', value);
	var menu = w3_el('id-select-ext').childNodes;
	var name = menu[value+1].innerHTML.toLowerCase();
	console.log('extint_select val='+ value +' name='+ name);
	var idx;
   extint_names_enum(function(i, value, id, id_en) {
      if (id.toLowerCase().includes(name)) {
	      console.log('extint_select HIT id='+ id +' id_en='+ id_en +' i='+ i +' value='+ value);
	      idx = i;
      }
   });

	extint.current_ext_name = extint_names[idx];
	if (extint_first_ext_load) {
		extint.ws = extint_connect_server();
		extint_first_ext_load = false;
	} else {
		//extint_focus();
		ext_send('SET ext_is_locked_status');     // request is_locked status
	}
}

var extint_names;

function extint_list_json(param)
{
   extint_names = kiwi_JSON_parse('extint_list_json', decodeURIComponent(param));
	//console.log('extint_names=');
	//console.log(extint_names);
}

function extint_names_enum(func)
{
   var i, value;
   for (i = value = 0; i < extint_names.length; i++) {
      var id = extint_names[i];
      if (!dbgUs && extint.excl_devl.includes(id)) continue;
      if (id == 'sig_gen' && (rx_chan != 0 || rx_chans >= 14)) continue;   // sig gen only visible to chan 0
      if (id == 'wspr') id = 'WSPR';      // FIXME: workaround

      // workaround mistake that config enable ids don't match ext names
      var id_en = id;
      if (id_en == 'cw_decoder') id_en = 'cw';
      if (id_en == 'SSTV') id_en = 'sstv';   // don't .toLowerCase() because e.g. 'DRM' is valid
      if (id_en == 'TDoA') id_en = 'tdoa';
      
      func(i, value, id, id_en);
      value++;
   }
}

function extint_select_build_menu()
{
   //console.log('extint_select_menu rx_chan='+ rx_chan +' is_local='+ kiwi.is_local[rx_chan]);
	var s = '';
	if (extint_names && isArray(extint_names)) {
	   extint_names_enum(function(i, value, id, id_en) {
         var enable = ext_get_cfg_param(id_en +'.enable');
         console.log('extint_select_menu id_en='+ id_en +' en='+ enable);
         if (enable == null || kiwi.is_local[rx_chan]) enable = true;   // enable if no cfg param or local connection
         if (id == 'DRM') kiwi.DRM_enable = enable;
		   s += '<option value='+ dq(value) +' kiwi_idx='+ dq(i) +' '+ (enable? '':'disabled') +'>'+ id +'</option>';
		});
	}
	//console.log('extint_select_menu = '+ s);
	return s;
}

function extint_open(name, delay)
{
   //console.log('extint_open rx_chan='+ rx_chan +' is_local='+ kiwi.is_local[rx_chan]);
   name = name.toLowerCase();
   var found = 0;
   extint_names_enum(function(i, value, id, id_en) {
      var enable = ext_get_cfg_param(id_en +'.enable');
      if (enable == null || kiwi.is_local[rx_chan]) enable = true;   // enable if no cfg param or local connection

      if (!found && enable && id.toLowerCase().includes(name)) {
         //console.log('extint_open match='+ id);
         if (delay) {
            //console.log('extint_open '+ name +' delay='+ delay);
            setTimeout(function() { extint_select(value); }, delay);
         } else {
            //console.log('extint_open '+ name +' NO DELAY');
            extint_select(value);
         }
         found = 1;
      }
   });
}

function extint_audio_data(data, samps)
{
	if (isFunction(extint_audio_data_cb))
		extint_audio_data_cb(data, samps);
}
