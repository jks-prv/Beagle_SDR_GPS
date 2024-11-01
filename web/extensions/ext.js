// Copyright (c) 2016-2023 John Seamons, ZL4VO/KF6VO

var extint = {
   ws: null,
   extname: null,
   
   first_ext_load: true,
   ext_is_tuning: false,
   authkey_cb: null,
   adc_clock_Hz: 0,
   adc_gps_clock_corr: 0,
   adc_clock_nom_Hz: 0,
   audio_data_cb: null,
   extint_pwd_cb: null,
   extint_pwd_cb_param: null,
   extint_conn_type: null,
   ext_names: null,
   hasIframeMenu: false,
   
   isAdmin_cb: null,
   srate: 0,
   wb_srate: 0,
   nom_srate: 0,
   param: null,
   override_pb: false,
   displayed: false,
   help_displayed: false,
   current_ext_name: null,
   using_data_container: false,
   hide_func: null,
   default_w: 525,
   default_h: 300,
   prev_mode: null,
   mode_prior_to_dx_click: null,
   seq: 0,
   scanning: 0,
   
   web_socket_fragmentation: 8192,
   send_hiwat: 0,
   send_hiwat_msg: null,
   
   optbars: {
      'optbar-wf':0, 'optbar-audio':1, 'optbar-agc':2, 'optbar-users':3, 'optbar-status':4, 'optbar-off':5
   },
   
   // extensions not subject to DRM lockout
   // FIXME: allow C-side API to specify
   no_lockout: [ 'noise_blank', 'noise_filter', 'ant_switch', 'iframe', 'colormap', 'devl', 'prefs' ],
   former_exts: ['ant_switch', 'waterfall', 'noise_blank', 'noise_filter'],
   excl_devl: [ 'devl', 'digi_modes', 's4285', 'prefs' ],
   
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
	if (isNoArg(ws)) ws = extint.ws;
	
	var len = msg.length;
	if (len > extint.send_hiwat) {
	   extint.send_hiwat_msg = msg;
	   extint.send_hiwat = len;
	}

	try {
	   //console.log('ext_send: ws='+ ws.stream +' '+ msg);
	   ws.send(msg);
		return 0;
	} catch(ex) {
		console.log("CATCH ext_send('"+ msg +"') ex="+ex);
		kiwi_trace();
	}
}

function ext_send_new(msg, ws)
{
   ext_send(kiwi_stringify(msg), ws);
}

// Handle web socket fragmentation by sending in parts which can be reassembled on server side.
// Due to data getting large after double encoding or web socket buffering limitations on browser.
function ext_send_cfg(cfg, s)
{
	var ws = extint.ws;
	var len = s.length;
	var tr = '='+ len;
	var _send = function(seq, suffix, s) {
	   return ('SET '+ cfg.cmd + suffix + seq +' '+ s);
	};

   var transaction_seq = cfg.seq;
   cfg.seq++;
   
   w3_do_when_cond(
      function(seq) {
         var ba = ws.bufferedAmount? ('['+ ws.bufferedAmount +']') : '0';
         tr = w3_sb(tr, ba);
         if (ws.bufferedAmount != 0) return false;    // wait for buffer to drain

         if (len > extint.web_socket_fragmentation) {
            cfg.lock = 1;     // lock if fragmented transfers are occurring
            ws.send(_send(seq, '_frag=', s.slice(0, extint.web_socket_fragmentation)));
            s = s.slice(extint.web_socket_fragmentation);
            len = s.length;
            tr = w3_sb(tr, 'W'+ extint.web_socket_fragmentation);
            return false;
         } else {
            ws.send(_send(seq, '=', s.slice(0, extint.web_socket_fragmentation)));
            cfg.lock = 0;
            if (kiwi.log_cfg_save_seq)
               kiwi_debug(w3_sb('save_config:', seq +'-'+ cfg.name + tr, 'w'+ len, 'DONE'), /* syslog */ true);
            len = 0;
            return true;
         }
      },
      null,
      transaction_seq, kiwi.test_cfg_save_seq? 1000 : 50
   );
   // REMINDER: w3_do_when_cond() returns immediately and not when the ws.send()s are done
}

// It's hard to do this any other way than a longish fixed delay because of recent
// save cfg web socket write buffering changes (i.e. when we're called right after a cfg save
// returns server may not have yet received and processed all the fragmented data).
function ext_send_after_cfg_save(msg, cfg_s)
{
   //setTimeout(function() { ext_send(msg); }, 1000);
   //ext_send(msg);
   
   cfg_s = cfg_s || 'cfg';
   var kiwi_cfg = kiwi[cfg_s];
   w3_do_when_cond(
      function() {
         if (kiwi.log_cfg_save_seq)
         kiwi_debug('ext_send_after_cfg_save lock='+ kiwi_cfg.lock +' msg=<'+ msg +'>');
         return (kiwi_cfg.lock == 0);
      },
      function(msg) {
         ext_send(msg);
      }, msg, 100
   );
}

function ext_panel_show(controls_html, data_html, show_func, hide_func)
{
	extint_panel_show(controls_html, data_html, show_func, hide_func);
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
var EXT_NO_SAVE = false;      // set the local JSON cfg, but don't set on server which would require admin privileges.
var EXT_SAVE_DEFER = false;   // defer one of multiple sequential saves

// init_val => (cfg|adm).path if was null|undef AND init_val not undef
//    if isAdmin: save cfg if save = undef|EXT_SAVE
//    update_path_var: cur_val/init_val => path [NB: w/o (cfg|adm)]
// return (cfg|adm).path

function ext_get_cfg_param(path, init_val, save, update_path_var)
{
	var cur_val;
	var cfg_path = w3_add_toplevel(path);
	
	try {
		cur_val = getVarFromString(cfg_path);
	} catch(ex) {
		// when scope is missing create all the necessary scopes and variable as well
		cur_val = null;
	}
	
   //console.log('ext_get_cfg_param: path='+ path +' admin='+ isAdmin() +' cur_val='+ cur_val +' init_val='+ init_val +' save='+ save);
   
	if (isNoArg(cur_val) && init_val != undefined) {    // scope or parameter doesn't exist, create it
		cur_val = init_val;
		// parameter hasn't existed before or hasn't been set (empty field)
		//console.log('ext_get_cfg_param: creating cfg_path='+ cfg_path +' cur_val='+ cur_val);
		setVarFromString(cfg_path, cur_val);
		
		// save newly initialized value in configuration (if admin) unless EXT_NO_SAVE specified
		//console.log('ext_get_cfg_param: SAVE cfg_path='+ cfg_path +' init_val='+ init_val +' save='+ save);
		if ((save == undefined && isAdmin()) || save == EXT_SAVE)
			cfg_save_json('ext_get_cfg_param', cfg_path, cur_val);
	}
	
	if (update_path_var) {
	   if (path.startsWith('cfg.') || path.startsWith('adm.')) {
		   console.log('ext_get_cfg_param: update_path_var CAUTION: NOT SETTING path='+ path +' cur_val='+ cur_val);
	   } else {
		   //console.log('ext_get_cfg_param: update_path_var SETTING path='+ path +' cur_val='+ cur_val);
	      setVarFromString(path, cur_val);
	   }
	}
	
	return cur_val;
}

function ext_get_cfg_param_string(path, init_val, save)
{
   var s = ext_get_cfg_param(path, init_val, save);
   if (isNoArg(s)) s = '';
	return kiwi_decodeURIComponent('ext_get_cfg_param_string' +':'+ path, s);
}

function ext_set_cfg_param(path, val, save)
{
   if (path.startsWith('id-')) {
      console.log('$WARNING: ext_set_cfg_param path='+ path);
      kiwi_trace();
      return;
   }
   
	path = w3_add_toplevel(path);	
	setVarFromString(path, val);
	save = (isArg(save) && save == EXT_SAVE)? true : false;
	//if (path.includes('model'))
	//console.log('ext_set_cfg_param: path='+ path +' val='+ val +' save='+ save);
	
	// save only if EXT_SAVE specified
	if (save) {
		//console.log('ext_set_cfg_param: SAVE path='+ path +' val='+ val);
		cfg_save_json('ext_set_cfg_param', path, val);
	}
}

function ext_get_freq_range()
{
   var offset = kiwi.freq_offset_kHz;
   //return { lo_kHz: cfg.sdr_hu_lo_kHz + offset, hi_kHz: cfg.sdr_hu_hi_kHz + offset, offset_kHz: offset };
   return { lo_kHz: offset, hi_kHz: offset + 32000, offset_kHz: offset };
}

function ext_set_freq_offset(foff_kHz)
{
   //console.log('ext_set_freq_offset: SET foff_kHz='+ foff_kHz);
   kiwi_set_freq_offset(foff_kHz);
   ext_send('SET antsw_freq_offset='+ foff_kHz);
}

var ext_zoom = {
	TO_BAND: 0,
	IN: 1,
	OUT: -1,
	ABS: 2,
	WHEEL: 3,
	CUR: 4,
	NOM_IN: 8,
	MAX_IN: 9,
	MAX_OUT: -9,
	TO_PASSBAND: null
};

// mode, zoom and passband are optional
function ext_tune(freq_dial_kHz, mode, zoom, zlevel, low_cut, high_cut, opt) {
   var pb_specified = (isArg(low_cut) && isArg(high_cut));
	//console.log('ext_tune: '+ freq_dial_kHz +', '+ mode +', '+ zoom +', '+ zlevel);
	
	extint.ext_is_tuning = true;
	   freq_dial_kHz = freq_dial_kHz || (freq_displayed_Hz / 1000);
      freqmode_set_dsp_kHz(freq_dial_kHz, mode, opt);
      if (pb_specified) ext_set_passband(low_cut, high_cut);
      
      if (isArg(zoom)) {
         zoom_step(zoom, zlevel);
      } else {
         zoom_step(ext_zoom.TO_BAND);
      }
	extint.ext_is_tuning = false;
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

function ext_save_setup()
{
   var mode = isArg(extint.mode_prior_to_dx_click)? extint.mode_prior_to_dx_click : cur_mode;
   console.log('ext_save_setup mode='+ mode +' mode_prior_to_dx_click='+ extint.mode_prior_to_dx_click +' cur_mode='+ cur_mode);
   extint.mode_prior_to_dx_click = null;
	return { mode: mode, zoom: zoom_level };
}

function ext_restore_setup(s)
{
   console.log('ext_restore_setup mode='+ s.mode +' zoom='+ s.zoom);
   ext_set_mode(s.mode);
   ext_set_zoom(ext_zoom.ABS, s.zoom);
}

function ext_get_mode()
{
	return cur_mode;
}

function ext_mode(mode)
{
   var fail = false;
   var str;
   
   if (isArg(mode)) {
      mode = mode.toLowerCase();
      str = mode.substr(0,2);
      if (str == 'qa') str = 'sa';  // QAM -> SAM case
      if (str == 'nn') str = 'nb';  // NNFM -> NBFM case
   } else {
      str = '';
      fail = true;
   }
   
   var o    = { fail:fail, str:str };
   o.AM     = fail? false : (str == 'am');
   o.LSB    = fail? false : (str == 'ls');
   o.USB    = fail? false : (str == 'us');
   o.CW     = fail? false : (str == 'cw');
   o.IQ     = fail? false : (str == 'iq');
   o.NBFM   = fail? false : (str == 'nb');
   o.SAx    = fail? false : (str == 'sa');
   o.DRM    = fail? false : (str == 'dr');
   o.AM     = fail? false : (str == 'am');
   
   o.LSB_SAL   = fail? false : (o.LSB || mode == 'sal');
   o.SSB       = fail? false : (o.USB || o.LSB);
   o.SSB_CW    = fail? false : (o.SSB || o.CW);
   o.STEREO    = fail? false : (o.IQ || o.DRM || mode == 'sas' || mode == 'qam');
   o.AM_SAx_IQ_DRM = fail? false : (o.AM || o.SAx || o.SSB || o.DRM);

   //console.log(o);
   //kiwi_trace('ext_mode');
   return o;
}

function ext_is_IQ_or_stereo_mode(mode)
{
   return ext_mode(mode).STEREO;
}

function ext_is_IQ_or_stereo_curmode()
{
   return ext_is_IQ_or_stereo_mode(cur_mode);
}

function ext_get_prev_mode()
{
	return extint.prev_mode;
}

function ext_set_mode(mode, freq_Hz, opt)
{
   if (isUndefined(mode)) return;

   var new_drm = (mode == 'drm');
   if (new_drm)
      extint.prev_mode = cur_mode;

   //console.log('### ext_set_mode '+ mode +' prev='+ extint.prev_mode);
	demodulator_analog_replace(mode, freq_Hz);
	
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
         demodulator_analog_replace(mode, freq_Hz);   // don't use mode restored by DRM_blur()
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
	bw = Math.abs(high_cut - low_cut);
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
		owrx.last_lo[cur_mode] = low_cut;
		owrx.last_hi[cur_mode] = high_cut;
	}
	
	if (isArg(freq_dial_Hz)) {
		freq_dial_Hz *= 1000;
		freq_car_Hz = freq_dsp_to_car(freq_dial_Hz);

      extint.ext_is_tuning = true;
         demodulator_set_frequency(owrx.FSET_EXT_SET_PB, freq_car_Hz);
      extint.ext_is_tuning = false;
	} else {
	   // only set the passband
	   demodulators[0].set();
		if (!wf.audioFFT_active)
		   mkenvelopes(get_visible_freq_range());
	}
}

function ext_get_tuning()
{
   return { low: demod.low_cut, high: demod.high_cut, mode: cur_mode, freq_dial_kHz: freq_displayed_Hz/1000 };
}

function ext_get_zoom()
{
	return zoom_level;
}

function ext_set_zoom(zoom_mode, zoom_level)
{
   zoom_step(zoom_mode, zoom_level);
}

function ext_get_audio_comp()
{
   return btn_compression? true:false;
}

function ext_set_audio_comp(comp, no_write_cookie)
{
   no_write_cookie = (no_write_cookie === true)? toggle_e.NO_WRITE_COOKIE : 0;
   toggle_or_set_compression(toggle_e.SET | no_write_cookie, comp? 1:0);
}

function ext_set_scanning(scanning)
{
	extint.scanning = scanning? 1:0;
}

function ext_agc_delay(set_val)
{
   var prev_val = w3_el('input-decay').value;
   if (isDefined(set_val)) setDecay_cb('', set_val, 1);
   return prev_val;
}

function ext_get_optbar()
{
   var optbar = kiwi_storeRead('last_optbar');      // optbar-xxx
   return optbar;
}

function ext_set_optbar(optbar, cb_param)
{
   if (extint.optbars[optbar]) {
      kiwi_storeWrite('last_optbar', optbar);
      w3_click_nav('id-navbar-optbar', optbar, 'optbar', cb_param);
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
	   if (kiwi.admin_save_pwd) {
         pwd = kiwi_storeInit('admin', '');
	   } else {
	      pwd = '';
         kiwi_storeDelete('admin');    // erase it if previously set
	   }
	   kiwi_storeDelete('admin');
	   kiwi_storeDelete('admin-pwd');
	} else {
      pwd = kiwi_storeRead(conn_type);
   }
   pwd = pwd? decodeURIComponent(pwd) : '';     // make non-null
	//console.log('ext_hasCredential: LOAD PWD '+ conn_type +'="'+ pwd +'" admin_save_pwd='+ kiwi.admin_save_pwd);
	
	// always check in case not having a pwd is accepted by local subnet match
	extint.pwd_cb = cb;
	extint.pwd_cb_param = cb_param;
	ext_valpwd(conn_type, pwd, ws);
}

function ext_valpwd(conn_type, pwd, ws)
{
	if (conn_type == 'mfg') conn_type = 'admin';

	// send and store the password encoded to prevent problems:
	//		with scanf() on the server end, e.g. embedded spaces
	//		with cookie storage that deletes leading and trailing whitespace
	pwd = encodeURIComponent(pwd);
	
	// must always store the pwd because it is typically requested (and stored) during the
	// SND negotiation, but then needs to be available for the subsequent W/F connection
	if (conn_type == 'admin') {
	   if (kiwi.admin_save_pwd) {
	      kiwi_storeWrite('admin', pwd);
	   }
	} else {
	   kiwi_storeWrite(conn_type, pwd);
	}
	
	//console.log('ext_valpwd: SAVE PWD '+ conn_type +'="'+ pwd +'" admin_save_pwd='+ kiwi.admin_save_pwd);
	extint.conn_type = conn_type;
	
	// pwd can't be empty if there is an ipl= (use # since it would have been encoded if in real pwd)
	pwd = (pwd != '')? pwd : '#';
	
	var ipl = null;
	var iplimit_cookie = kiwi_storeRead('iplimit');
   var iplimit_pwd = kiwi_url_param(['pwd', 'password'], '', '');

	if (iplimit_pwd != '') {   // URL param overrides cookie
	   ipl = iplimit_pwd;
      // also saved when successfully entered in response to timeout password panel, see kiwi_ip_limit_pwd_cb()
	   kiwi_storeWrite('iplimit', encodeURIComponent(ipl));
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

function ext_isAdmin(cb)
{
   extint.isAdmin_cb = cb;
	ext_send('SET is_admin');
}

function ext_get_authkey(func)
{
	ext_send('SET get_authkey');
	extint.authkey_cb = func;
}

// updated by gps_stats_cb() from kiwi_msg() "stats_cb="
function ext_adc_clock_Hz()
{
	return extint.adc_clock_Hz;
}

// updated by gps_stats_cb() from kiwi_msg() "stats_cb="
function ext_adc_gps_clock_corr()
{
	return extint.adc_gps_clock_corr;
}

// updated from kiwi_msg() "adc_clk_nom="
function ext_adc_clock_nom_Hz()
{
	return extint.adc_clock_nom_Hz;
}

// updated from kiwi_msg() "sample_rate=" and periodically from "stats_cb="
function ext_sample_rate()
{
	return extint.srate;
}

function ext_nom_sample_rate()
{
	return extint.nom_srate;
}

function ext_register_audio_data_cb(func)
{
	extint.audio_data_cb = func;
}

function ext_unregister_audio_data_cb()
{
	extint.audio_data_cb = null;
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

function ext_get_name()
{
	return extint.current_ext_name;
}


/*
www.ios-resolution.com
screen.{width,height}   (in logical pixels)
                        P=portrait L=landscape
			   w     h		screen.[wh] in portrait
			   h     w		rotated to landscape
iPhone 5S	320   568	P
iPhone 6S   375   667   P
iPhone X    375   812   P
iPhone XR   414   896	P
iPhone 15   430   932   P

levono		600   1024	P 7"
huawei		600   982	P 7"

iPad 2		768   1024	P

MBP 15"		1440  900	L
M1  16"		1496        L
*/

// Must delay the determination of orientation change while the popup keyboard is active.
// Otherwise an incorrect window.innerHeight value is possible leading to an invalid
// isPortrait determination. This is why the check for owrx.popup_keyboard_active
// Tried checking w against window.screen.width, but latter changed with rotation on Android
// but not with iOS. So isn't constant and can't be used.
function ext_mobile_info(last)
{
   var w = window.innerWidth;
   var h = window.innerHeight;      // reduced if popup keyboard active
   if (mobile_laptop_test) { w = 375; h = 812; }   // simulate iPhone X
   var rv = { width:w, height:h };
   var isPortrait;

   if (owrx.popup_keyboard_active) {
      isPortrait = last? last.isPortrait : 1;
   } else {
      // if popup keyboard active h could be <= w making test invalid
      isPortrait = (w < h || mobile_laptop_test)? 1:0;
   }
   rv.orient_unchanged = (last && last.isPortrait == isPortrait)? 1:0;

   //if (!rv.orient_unchanged && last)
   //   canvas_log(w +' '+ h +' '+ last +' '+ last.isPortrait + isPortrait);

	rv.isPortrait = isPortrait? 1:0;
	rv.small    = (isPortrait && w <  768)? 1:0;    // anything smaller than iPad
	rv.narrow   = (isPortrait && w <= 600)? 1:0;    // narrow screens, i.e. phones and 7" tablets

	rv.iPad     = (isPortrait && w <= 768)? 1:0;    // iPad or smaller
	rv.tablet   = (isPortrait && w <= 600)? 1:0;    // narrow screens, i.e. phones and 7" tablets
	rv.phone    = (isPortrait && w <= 430)? 1:0;    // largest iPhone portrait width
   return rv;
}


function ext_get_menus_cb(ctx, freqs, retry_cb, done_cb, done_cb_param)
{
   var fault = false;
   
   if (!freqs) {
      console.log('ext_get_menus_cb: freqs='+ freqs);
      fault = true;
   } else
   
   if (freqs.AJAX_error && freqs.AJAX_error == 'timeout') {
      console.log('ext_get_menus_cb: TIMEOUT');
      ctx.using_default = true;
      fault = true;
   } else

   if (freqs.AJAX_error && freqs.AJAX_error == 'status') {
      console.log('ext_get_menus_cb: status='+ freqs.status);
      ctx.using_default = true;
      fault = true;
   } else

   if (!isObject(freqs)) {
      console.log('ext_get_menus_cb: not array');
      fault = true;
   } else
   
   if (isDefined(freqs.AJAX_error)) {
      console.log('ext_get_menus_cb: AJAX_error');
      fault = true;
   }

   var url;
   if (fault) {
      if (ctx.double_fault) {
         console.log('ext_get_menus_cb: default freq list fetch FAILED');
         console.log(freqs);
         return;
      }
      console.log(freqs);
      
      // load the default freq list from a file embedded with the extension (should always succeed)
      url = ctx.int_url;
      console.log('ext_get_menus_cb: using default freq list '+ url);
      ctx.using_default = true;
      ctx.double_fault = true;
      kiwi_ajax(url, retry_cb, 0, /* timeout */ 10000);
      return;
   } else {
      url = ctx.ext_url;
   }
   
   console.log('ext_get_menus_cb: from '+ url);
   if (dbgUs) console.log(freqs);
   
   w3_call(done_cb, done_cb_param);
}

function ext_render_menus(ctx, ext_name, ctx_name)
{
   // {
   //    o[i0]: {
   //       o2[j0]: [ f, ... ],
   //          ...
   //       o2[jn]: [ f, ... ]
   //    },
   //       ...
   //    o[in]: {
   //       ...
   //    }
   // }
   var new_menu = function(i, o, menu_s) {
      ctx.n_menu++;
      ctx.menus[i] = o;
      ctx.menu_s[i] = menu_s;
      var s =
         w3_div('id-'+ menu_s +' w3-margin-between-24',
            w3_select_hier(ctx.sfmt, menu_s, 'select', ctx_name +'.menu'+ i, -1, o, ext_name +'_pre_select_cb')
         );
      return s;
   };
   
   if (isUndefined(ctx_name)) ctx_name = ext_name;
   var s = '';
   ctx.n_menu = 0;

   w3_obj_enum(ctx.freqs, function(menu_s, i, o2) {   // each object
      //console.log(menu_s +'.'+ i +' NEW:');
      //console.log(o2);
      s += new_menu(i, o2, menu_s);
   });

   w3_innerHTML('id-'+ ext_name +'-menus', s);   
   console.log('ext_render_menus n_menu='+ ctx.n_menu);
}

function ext_help_click(delay)
{
   // will send click event even if w3-disabled!
   if (w3_contains('id-ext-controls-help-btn', 'w3-disabled')) return;
   console.log(extint.current_ext_name +'_help_click CLICKED');
   
   if (delay)
      setTimeout(function() { extint_help_click_now(); }, 2000);
   else
      extint_help_click_now();
}


////////////////////////////////
// internal routines
////////////////////////////////

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
		   w3_button('id-ext-controls-help-btn w3-green w3-small w3-padding-small w3-disabled||onclick="ext_help_click()"', 'help')
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
	         if (w3_call(extint.current_ext_name +'_escape_key_cb') != true) {
	            console.log('EXT ESC: no _escape_key_cb() routine, so doing id-ext-controls-close.click()');
	            w3_el('id-ext-controls-close').click();
	         }
	      }
	   }, true);
}

function ext_show_spectrum(which)
{
   toggle_or_set_spec(toggle_e.SET | toggle_e.NO_CLOSE_EXT, which);
}

function ext_hide_spectrum()
{
   toggle_or_set_spec(toggle_e.SET | toggle_e.NO_CLOSE_EXT, spec.NONE);
}

function extint_panel_show(controls_html, data_html, show_func, hide_func, show_help_button)
{
   //console.log('extint_panel_show: extint.displayed='+ extint.displayed);
   var el;
   
	extint.using_data_container = (data_html? true:false);
	//console.log('extint_panel_show using_data_container='+ extint.using_data_container);

	if (extint.using_data_container) {
      // remove previous use of spectrum
		toggle_or_set_spec(toggle_e.SET, spec.NONE);
		w3_hide('id-top-container');
		w3_show_block(w3_innerHTML('id-ext-data-container', data_html));
	} else {
		w3_hide('id-ext-data-container');
		if (spec.source == spec.NONE)
			w3_show_block('id-top-container');
	}

   // remove previous help panel if displayed
   if (extint.help_displayed == true) {
      confirmation_panel_close();
      extint.help_displayed = false;
   }
   
   var ext_name = extint.current_ext_name;

	// hook the close icon to call extint_panel_hide()
	el = w3_el('id-ext-controls-close');
	el.onclick = function() { toggle_panel("ext-controls"); extint_panel_hide(); };
	//console.log('extint_panel_show onclick='+ el.onclick);
	
	// some exts change these -- change back to default
	ext_set_data_height();     // restore default height
	w3_el('id-ext-controls').style.zIndex = 150;
   w3_create_attribute('id-ext-controls-close-img', 'src', 'icons/close.24.png');
	
	el = w3_el('id-ext-controls-container');
	el.innerHTML = controls_html;
	//console.log(controls_html);
	
	if (show_func) show_func();
	extint.hide_func = hide_func;
	
	el = w3_el('id-ext-controls');
	el.style.zIndex = 150;
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
   //console.log('ext_panel_displayed displayed='+ extint.displayed +' current_ext_name='+ extint.current_ext_name);
   return (extint.displayed && (ext_name? (ext_name == extint.current_ext_name) : true));
}

function ext_panel_redisplay(s) { w3_innerHTML('id-ext-controls-container', s); }

function extint_panel_hide(skip_calling_hide_spec)
{
	console.log('extint_panel_hide using_data_container='+ extint.using_data_container +' skip_calling_hide_spec='+ skip_calling_hide_spec);

	if (extint.using_data_container) {
		w3_hide('id-ext-data-container');
		if (skip_calling_hide_spec != true)
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
	w3_call(extint.hide_func);
	
	// on close, reset extension menu
	w3_select_value('id-select-ext', -1);
	
	resize_waterfall_container(true);	// necessary if an ext was present so wf canvas size stays correct

   extint.displayed = false;     // NB: must occur before freqset_select() below so closed_ext_input_still_holding_focus logic works
   freqset_select();
}

function extint_help_click_now()
{
	extint.help_displayed = w3_call(extint.current_ext_name +'_help', true);
}

function extint_environment_changed(changed)
{
   // have to wait a bit since extint_environment_changed({freq:1}) is called before
   // e.g. ext_get_freq_kHz() has been updated with latest value
   //
   // Possible values of "changed":
   //    freq, mode, passband, zoom, waterfall_pan, resize
   
   setTimeout(
      function() {
         //console_nv('extint_environment_changed', 'extint.current_ext_name');
         if (extint.current_ext_name) {
            w3_call(extint.current_ext_name +'_environment_changed', changed);
         }
         
         if (changed.freq || changed.mode || changed.zoom || changed.waterfall_pan || changed.resize)
            mouse_freq_remove();
         
         // former extensions
         noise_blank_environment_changed(changed);
         noise_filter_environment_changed(changed);

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

function extint_valpwd_cb(badp)
{
	if (extint.pwd_cb) extint.pwd_cb(badp, extint.pwd_cb_param);
}

function extint_open_ws_cb()
{
	// should always work since extensions are only loaded from an already validated client
	ext_hasCredential('kiwi', null, null);
	setTimeout(function() { setInterval(function() { ext_send("SET keepalive"); }, 5000); }, 5000);
}

function extint_connect_server()
{
	extint.ws = open_websocket('EXT', extint_open_ws_cb, null, extint_msg_cb);

	// when the stream thread is established on the server it will automatically send a "MSG ext_client_init" to us
}

function extint_msg_cb(param, ws)
{
	switch (param[0]) {
		case "keepalive":
			break;

		case "ext_client_init":
		   console.log('ext_client_init is_locked='+ (+param[1]));
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
		time_display_setup('id-topbar-R-container');
	}
	
	if (extint.ws)
		ext_send('SET ext_blur='+ rx_chan);
}

function extint_focus(is_locked)
{
   // dynamically load extension (if necessary) before calling <ext>_main()
   var ext_name = extint.current_ext_name;
	console.log('extint_focus: loading '+ ext_name +'.js');
	
	if (is_locked && !extint.no_lockout.includes(ext_name)) {
	   var s =
         w3_text('w3-medium w3-text-css-yellow',
            'Cannot use extensions while <br> another channel is in DRM mode.'
         );
      extint_panel_show(s, null, null, null, 'off');
      ext_set_controls_width_height(300, 75);
      return;
	}

   kiwi_load_js_dir('extensions/'+ ext_name +'/'+ ext_name, ['.js', '.css'],

      // post-load
      function() {
         console.log('extint_focus: calling '+ ext_name +'_main()');
         //setTimeout('ext_set_controls_width_height(); w3_call('+ ext_name +'_main);', 3000);
         ext_set_controls_width_height();
         w3_call(ext_name +'_main');
         
         if (isNonEmptyArray(shortcut.keys))
	         setTimeout(keyboard_shortcut_url_keys, 3000);
      },

      // pre-load
      function(loaded) {
         console.log('extint_focus: '+ ext_name +' loaded='+ loaded);
         if (loaded) {
            extint_panel_show('loading extension...', null, null, null, 'off');
            ext_set_controls_width_height(325, 45);
            if (kiwi.is_locked)
               console.log('==== IS_LOCKED =================================================');
         }
      }
   );
}

// called on extension menu item selection
function extint_select(value)
{
   freqset_select();
	
	value = +value;
	var el = w3_el('id-select-ext');
	if (!el) {
	   //console.log('$ extint_select NOT READY v='+ value);
	   setTimeout(extint_select, 1000, value);
	   return;
	}
	w3_select_value(el, value);
	var menu = el.childNodes;
	var name = menu[value+1].innerHTML.toLowerCase();
	//console.log('extint_select val='+ value +' name='+ name);
	var idx;
   extint_enum_names(function(i, value, id, id_en) {
	   //console.log('extint_select CONSIDER id='+ id +' name='+ name +' id_en='+ id_en +' i='+ i +' value='+ value);
      //if (id.toLowerCase().includes(name)) {
      if (name.startsWith(id.toLowerCase())) {
	      //console.log('extint_select HIT id='+ id +' name='+ name +' id_en='+ id_en +' i='+ i +' value='+ value);
	      idx = i;
      }
   });
   
   // handle former extensions now contained in main control panel
   if (name == 'ant_switch') {
      ant_switch_focus();
      ant_switch_view();
   } else {
      ant_switch_blur();
      if (extint.former_exts.includes(name)) {
         w3_call(name +'_view');
      } else {
         extint_blur_prev(0);
         w3_call(extint.hide_func);
         
         // remap the iframe virtual menu name
         var ext_name = extint.ext_names[idx];
         if (extint.hasIframeMenu && ext_name == cfg.iframe.menu) ext_name = 'iframe';
         extint.current_ext_name = ext_name;

         if (extint.first_ext_load) {
            extint_connect_server();
            extint.first_ext_load = false;
         } else {
            //extint_focus();
            ext_send('SET ext_is_locked_status');     // request is_locked status
         }
      }
   }
}

function extint_list_json(param)
{
   extint.ext_names = kiwi_JSON_parse('extint_list_json', decodeURIComponent(param));
   
   // add virtual entries for ui compatibility
   extint.former_exts.forEach(function(e,i) { extint.ext_names.push(e); });
   console.log(extint.ext_names);
   console.log(extint.ext_names.includes(cfg.iframe.menu));
   
   // careful: check that menu name doesn't match an existing one
   if (isNonEmptyString(cfg.iframe.menu) && !extint.ext_names.includes(cfg.iframe.menu)) {
      extint.ext_names.push(cfg.iframe.menu);
      extint.hasIframeMenu = true;
   }
   
	kiwi_dedup_array(extint.ext_names);
	extint.ext_names.sort(kiwi_sort_ignore_case);
	//console.log('ext_names=');
	//console.log(extint.ext_names);
}

function extint_enum_names(func)
{
   var i, value;
   for (i = value = 0; i < extint.ext_names.length; i++) {
      var id = extint.ext_names[i];
      if (!dbgUs && extint.excl_devl.includes(id)) continue;
      if (id == 'sig_gen' && rx_chan != 0) continue;     // sig gen only visible to chan 0
      if (id == 'wspr') id = 'WSPR';      // FIXME: workaround

      // workaround mistake that stored config enable ids don't match ext names
      var id_en = id.toLowerCase();
      if (id_en == 'cw_decoder') id_en = 'cw';
      if (id_en == 'drm') id_en = 'DRM';
      if (id_en == 's_meter') id_en = 'S_meter';
      if (id_en == 'wspr') id_en = 'WSPR';
      
      func(i, value, id, id_en);
      value++;
   }
}

function extint_exts_call(func_s)
{
   extint_enum_names(
      function(i, value, id, id_en) {
         id_en = id_en.toLowerCase();
         //console.log(id_en + func_s);
         w3_call(id_en + func_s);
      }
   );
}

function ext_auth()
{
   if (kiwi.is_local[rx_chan]) return kiwi.AUTH_LOCAL;
   if (kiwi.tlimit_exempt_by_pwd[rx_chan]) return kiwi.AUTH_PASSWORD;
   return kiwi.AUTH_USER;
}

function extint_select_build_menu()
{
   //console.log('extint_select_menu rx_chan='+ rx_chan +' ext_auth='+ ext_auth());
	var s = '';
	if (extint.ext_names && isArray(extint.ext_names)) {
	   extint_enum_names(function(i, value, id, id_en) {
         var enable = ext_get_cfg_param(id_en +'.enable');
         //console.log('extint_select_menu id_en='+ id_en +' en='+ enable);
         if (enable == null || ext_auth() == kiwi.AUTH_LOCAL) enable = true;   // enable if no cfg param or local connection
         if (id == 'DRM') kiwi.DRM_enable = enable;
         
         // menu entry name exceptions
         if (id == 'NAVTEX') id = 'NAVTEX/DSC'; else
         if (id == 'FT8') id = 'FT8/FT4'; else
         if (id == 'wspr') id = 'WSPR';
         
		   s += '<option value='+ dq(value) +' kiwi_idx='+ dq(i) +' '+ (enable? '':'disabled') +'>'+ id +'</option>';
		});
	}
	//console.log('extint_select_menu = '+ s);
	return s;
}

function extint_open(name, delay)
{
   //console.log('extint_open rx_chan='+ rx_chan +' ext_auth='+ ext_auth());
   name = name.toLowerCase();
   
   // aliases, for the benefit of dx.json file
   if (name == 'dsc') name = 'navtex'; else
   if (name == 'ft4') name = 'ft8'; else
      ;
   
   var found = 0;
   extint_enum_names(function(i, value, id, id_en) {
      var enable = ext_get_cfg_param(id_en +'.enable');
      if (enable == null || ext_auth() == kiwi.AUTH_LOCAL) enable = true;   // enable if no cfg param or local connection

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
	if (isFunction(extint.audio_data_cb))
		extint.audio_data_cb(data, samps);
}
