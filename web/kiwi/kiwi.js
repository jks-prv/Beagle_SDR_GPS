// KiwiSDR
//
// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO

var kiwi = {
   d: {},      // debug
   init_cfg: false,
   
   KiwiSDR_1: 1,
   KiwiSDR_2: 2,
   model: 1,   
   
   PLATFORM_BBG_BBB: 0,
   PLATFORM_BB_AI:   1,
   PLATFORM_BB_AI64: 2,
   PLATFORM_RPI:     3,
   platform: -1,
   platform_s: [ 'BBG/B', 'BBAI', 'BBAI-64', 'RPi' ],
   
   cfg:   { seq:0, name:'cfg',   cmd:'save_cfg',   lock:0, timeout:null },
   dxcfg: { seq:0, name:'dxcfg', cmd:'save_dxcfg', lock:0, timeout:null },
   adm:   { seq:0, name:'adm',   cmd:'save_adm',   lock:0, timeout:null },
   //test_cfg_save_seq: true,
   test_cfg_save_seq: false,
   log_cfg_save_seq: false,
   
   WIN_WIDTH_MIN: 1400,
   
   mdev: false,
   mdev_s: '',
   mnew: false,

   skip_small_screen_check: false,
   noLocalStorage: false,
   
   // NB: must match rx_cmd.h
   BADP_OK:                            0,
   BADP_TRY_AGAIN:                     1,
   BADP_STILL_DETERMINING_LOCAL_IP:    2,
   BADP_NOT_ALLOWED_FROM_IP:           3,
   BADP_NO_ADMIN_PWD_SET:              4,
   BADP_NO_MULTIPLE_CONNS:             5,
   BADP_DATABASE_UPDATE_IN_PROGRESS:   6,
   BADP_ADMIN_CONN_ALREADY_OPEN:       7,
   
   AUTH_LOCAL: 0,
   AUTH_PASSWORD: 1,
   AUTH_USER: 2,
   is_local: [],
   tlimit_exempt_by_pwd: [],
   admin_save_pwd: false,

   stats_interval: 10000,
   conn_tstamp: 0,
   isOffset: false,
   loaded_files: {},
   GPS_auto_grid: '',
   GPS_fixes: 0,
   wf_fps: 0,
   is_multi_core: 0,
   log2_seq: 0,
   
   w3_text: 'w3-text-bottom w3-text-css-orange',
   inactivity_panel: false,
   no_admin_conns_pend: 0,
   foff_error_pend: 0,
   notify_seq: 0,
   ident_min: 16,    // e.g. "wsprdaemon_v3.0a" is 16 chars
   
   // fixed height instead of content dependent so height is constant between different optbar types
   OPTBAR_CONTENT_HEIGHT: 150,
   cur_optbar: null,
   virt_optbar_click: false,

   volume: 50,
   volume_f: 1e-6,
   muted: 1,         // mute until muted_initially state determined
   unmuted_color: 'lime',
   pan: 0,

   queued: 0,
   
   // CAUTION: must match order in mode.h
   // CAUTION: add new entries at the end
   modes_lc: [ 'am', 'amn', 'usb', 'lsb', 'cw', 'cwn', 'nbfm', 'iq', 'drm',
              'usn', 'lsn', 'sam', 'sau', 'sal', 'sas', 'qam', 'nnfm', 'amw' ],
   modes_uc: [],
   modes_idx: {},
   
   // order that modes appear in menus as opposed to new-entry-appending behavior of kiwi.modes_lc
   mode_menu: [ 'AM', 'AMN', 'AMW', 'USB', 'USN', 'LSB', 'LSN', 'CW', 'CWN', 'NBFM', 'NNFM',
                'IQ', 'DRM', 'SAM', 'SAU', 'SAL', 'SAS', 'QAM' ],

   tab_s: [ 'last', 'Off', 'Stat', 'User', 'AGC', 'Audio', 'WF', 'RF' ],
   
   ITU_s: [
      'any',
      'R1: Europe, Africa',
      'R2: North & South America',
      'R3: Asia / Pacific',
      'show on band scale only',
      'show on band menu only'
   ],
   
   ITU_ANY: 0,
   BAND_SCALE_ONLY: 4,
   BAND_MENU_ONLY: 5,

   RX4_WF4:0, RX8_WF2:1, RX3_WF3:2, RX14_WF0:3, RX_WB:4,
   
   NAM:0, DUC:1, PUB:2, SIP:3, REV:4,
   
   // colormap definitions needed by admin config
   cmap_s: [
      'Kiwi', 'CSDR', 'grey', 'linear', 'turbo', 'SdrDx',
      'cust1', 'cust2', 'cust3', 'cust4'
   ],
   cmap_e: {
      kiwi:0, CuteSDR:1, greyscale:2, linear:3, turbo:4, SdrDx:5,
      custom_1:6, custom_2:7, custom_3:8, custom_4:9
   },
   aper_s: [ 'man', 'auto' ],
   APER_MAN: 0,
   APER_AUTO: 1,
   
   esc_lt: '\x11',   // ascii dc1
   esc_gt: '\x12',   // ascii dc2
   
   xdLocalStorage_ready: false,
   prefs_import_ch: -1,
   
   ADC_CLK_CORR_DISABLED: 0,
   ADC_CLK_CORR_CONTINUOUS: 1,
   ext_clk: false,
   
   // pre-record / pre-squelch buffer
   pre_samps: 0,
   pre_size: 0,
   pre_last: 0,
   pre_buf: null,
   pre_data: null,
   pre_off: 0,
   pre_captured: false,
   pre_wrapped: false,
   pre_ping_pong: 0,
   
   bands: null,
   bands_community: null,

   RF_ATTN_ALLOW_EVERYONE: 0,
   RF_ATTN_ALLOW_LOCAL_ONLY: 1,
   RF_ATTN_ALLOW_LOCAL_OR_PASSWORD_ONLY: 2,
   rf_attn: 0,
   rf_attn_disabled: false,
   
   freq_memory: [],
   freq_memory_menu_shown: 0,
   fmem_auto_save: 1,
   fmem_mode_save: 0,
   fmem_arr: null,
   
   no_reopen_retry: false,
   wf_preview_mode: false,
   _ver_: 1.578,
   _last_: null
};

kiwi.modes_lc.forEach(function(e,i) { kiwi.modes_uc.push(e.toUpperCase()); kiwi.modes_idx[e] = i; });
//console.log(kiwi.modes_uc);
//console.log(kiwi.modes_idx);

var WATERFALL_CALIBRATION_DEFAULT = -13;
var SMETER_CALIBRATION_DEFAULT = -13;

var rx_chans, wf_chans, wf_chans_real, max_camp;
var rx_chan = null;     // null important: used by a w3_do_when_cond(isArg(rx_chan))
var try_again = "";
var conn_type;
var seriousError = false;

var timestamp;

var dbgUs = false;
var dbgUsFirst = true;

var gmap;

// see document.onreadystatechange for how this is called
function kiwi_bodyonload(error)
{
   var s;
   
	if (error != '') {
		kiwi_serious_error(error);
	}
	else
	
	if (kiwi_isSmartTV() == 'LG' && kiwi_isChrome() < 87) {
	   s = 'Browser: '+ navigator.userAgent +
	      '<br>Sorry, KiwiSDR requires SmartTV Chrome version >= 87';
		kiwi_serious_error(s);
	} else
	
	{
	   if (kiwi_storeInit('ident', "").endsWith('KF6VO')) dbgUs = true;
	   
	   // for testing a clean webpage, e.g. kiwi:8073/test
	   /*
	   var url = kiwi_url();
      console.log('url='+ url);
	   if (url.endsWith('test')) {
	      console.log('test page..');
	      // test something
	      return;
	   }
	   */
	   
		conn_type = w3_el('id-kiwi-container').getAttribute('data-type');
		if (conn_type == 'undefined') conn_type = 'kiwi';
		console.log('conn_type='+ conn_type);
		
      w3int_init();

      if (!kiwi.skip_small_screen_check && conn_type == 'admin' && ext_mobile_info().iPad) {
         s = 'Admin interface not intended for small screens. <br>' +
             'Please use a desktop browser instead. <br><br>' +
             w3_button('w3-css-yellow', 'Continue anyway', 'kiwi_small_screen_continue_cb');
         kiwi_serious_error(s);
         return;
      }

		if (conn_type == 'kiwi') {
		
			// A slight hack. For a user connection extint.ws is set here to ws_snd so that
			// calls to e.g. ext_send() for password validation will work. But extint.ws will get
			// overwritten when the first extension is loaded. But this should be okay since
			// subsequent uses of ext_send (mostly via ext_hasCredential/ext_valpwd) can specify
			// an explicit web socket to use (e.g. ws_wf).
         //
         // BUT NB: if you put an alert before the assignment to extint.ws there will be a race with
         // extint.ws needing to be used by ext_send() called by descendents of kiwi_open_ws_cb().

			extint.ws = owrx_ws_open_snd(kiwi_open_ws_cb, { conn_type:conn_type });
		} else {
			// e.g. admin or mfg connections
			extint.ws = kiwi_ws_open(conn_type, kiwi_open_ws_cb, { conn_type:conn_type });
		}
	}
}

function kiwi_small_screen_continue_cb()
{
   kiwi.skip_small_screen_check = true;
   seriousError = false;
   kiwi_bodyonload('');
}

function kiwi_open_ws_cb(p)
{
	if (p.conn_type != 'kiwi')
		setTimeout(function() { setInterval(function() { ext_send("SET keepalive"); }, 5000); }, 5000);
	
	if (seriousError)
	   return;        // don't go any further

	// always check the first time in case not having a pwd is accepted by local subnet match
	ext_hasCredential(p.conn_type, kiwi_valpwd1_cb, p);
}


////////////////////////////////
// dynamic loading
////////////////////////////////

function kiwi_load_js_polled(obj, js_files)
{
   if (!obj.started) {
      kiwi_load_js(js_files, function() {
         obj.finished = true;
         //console.log('### kiwi_load_js_polled SET fin=TRUE '+ js_files);
      });
      obj.started = true;
      obj.finished = false;
   }
   //if (!obj.finished) console.log('### kiwi_load_js_polled fin='+ obj.finished +' '+ js_files);
   return obj.finished;
}

function kiwi_load_js_dir(dir, js_files, cb_post, cb_pre)
{
	if (isString(js_files)) js_files = [js_files];
   for (var i = 0; i < js_files.length; i++) {
      js_files[i] = dir + js_files[i];
   }
   kiwi_load_js(js_files, cb_post, cb_pre);
}

// cb_pre/cb_post can be string function names or function pointers (w3_call() is used)
function kiwi_load_js(js_files, cb_post, cb_pre)
{
	console.log('DYNLOAD START');
	// kiwi_js_load.js will begin running only after all others have loaded and run.
	// Can then safely call the callback.
	if (isString(js_files)) js_files = [js_files];
	js_files.push('kiwi/kiwi_js_load.js');
	console.log(js_files);

   var loaded_any = false;
   js_files.forEach(function(src, i) {
      // only load once in case used in multiple places (e.g. Google maps)
      if (!kiwi.loaded_files[src]) {
         if (!src.includes('kiwi_js_load.js')) {
            kiwi.loaded_files[src] = 1;
            loaded_any = true;
         } else {
            if (!loaded_any) return;
         }
         
         var unknown_ext = false;
         var script;
         if (src.endsWith('.js') || src.includes('/js?key=')) {
            script = document.createElement('script');
            script.src = src;
            script.type = 'text/javascript';
            
            // callback is associated with kiwi_js_load.js, in case there are
            // multiple js_files to be loaded prior
            if (src == 'kiwi/kiwi_js_load.js') {
               script.kiwi_js = js_files[i-1];
               script.kiwi_cb = cb_post;
            }
         } else
         if (src.endsWith('.css')) {
            script = document.createElement('link');
            script.rel = 'stylesheet';
            script.href = src;
            script.type = 'text/css';
         } else
            unknown_ext = true;
         
         if (unknown_ext) {
            console.log('DYNLOAD UNKNOWN FILETYPE '+ src);
         } else {
            script.async = false;
            document.head.appendChild(script);
            console.log('DYNLOAD loading '+ src);
         }
      } else {
         console.log('DYNLOAD already loaded: '+ src);
      }
   });
	console.log('DYNLOAD FINISH');
	
	// if the kiwi_js_load.js process never loaded anything just call the callback(s) here
	if (!loaded_any) {
	   if (cb_pre) {
         //console.log('DYNLOAD call pre '+ cb_pre);
         console.log('DYNLOAD call pre');
	      w3_call(cb_pre, false);
	   }
	   if (cb_post) {
         //console.log('DYNLOAD call post '+ cb_post);
         console.log('DYNLOAD call post');
         w3_call(cb_post);
      }
	} else {
	   if (cb_pre) {
         //console.log('DYNLOAD call pre subsequent '+ cb_pre);
         console.log('DYNLOAD call pre subsequent');
         w3_call(cb_pre, true);
      }
      // cb_post is called from kiwi_js_load.js after module has actually loaded
	}
}


function kiwi_ask_pwd_cb(path, val, first)
{
	//console.log('kiwi_ask_pwd_cb: path='+ path +' '+ typeof(val) +' "'+ val +'" first='+ first);
   ext_valpwd(conn_type, val);
}

function kiwi_queue_or_camp_cb(path, val, first)
{
   var url = kiwi_url();
   console.log(url);
   url = url + kiwi_add_search_param(window.location, 'camp');
   console.log('--> '+ url);
   kiwi_open_or_reload_page({ url:url });
}

function kiwi_ask_pwd(conn_kiwi)
{
	console.log('kiwi_ask_pwd chan_no_pwd_true='+ chan_no_pwd_true +' client_public_ip='+ client_public_ip);
	var s1 = '', s2 = '';
	if (conn_kiwi && chan_no_pwd_true) {
	   s1 = 'All channels busy that don\'t require a password ('+ chan_no_pwd_true +'/'+ rx_chans +')<br>';
	   s2  = '<br> <b>OR</b> <br><br> click to queue for an available channel, <br>' +
	      'or camp on an existing channel: <br>' +
         w3_button('w3-medium w3-padding-smaller w3-aqua w3-margin-T-8', 'Queue or Camp', 'kiwi_queue_or_camp_cb');
	}

	// "&& conn_kiwi" to ignore pathological "/admin?prot" etc.
   var prot = (kiwi_url_param(['p', 'prot', 'protected'], true, false) && conn_kiwi);
	if (prot) s1 = 'You have requested a password protected channel<br>';

	var user_login = conn_kiwi? w3_innerHTML('id-kiwi-user-login').trim() : '';
	var s = isNonEmptyString(user_login)? (user_login +'<br><br>') : 'KiwiSDR: software-defined receiver<br>';
	s += s1 + try_again +
      w3_input('w3-margin-TB-8/w3-label-inline w3-label-not-bold/kiwi-pw|padding:1px|size=40', 'Password:', 'id-pwd', '', 'kiwi_ask_pwd_cb') +
      s2;

	kiwi_show_msg(s);
	w3_field_select('id-pwd', {mobile:1});
}

var body_loaded = false;

function kiwi_valpwd1_cb(badp, p)
{
	//console.log("kiwi_valpwd1_cb conn_type="+ p.conn_type +' badp='+ badp);

	if (seriousError)
	   return;        // don't go any further

	if (badp == kiwi.BADP_TRY_AGAIN) {
		kiwi_ask_pwd(p.conn_type == 'kiwi');
		try_again = 'Try again. ';
	} else
	if (badp == kiwi.BADP_STILL_DETERMINING_LOCAL_IP) {
	   kiwi_show_msg('Still determining local interface address.<br>Please try reloading page in a few moments.');
	} else
	if (badp == kiwi.BADP_NOT_ALLOWED_FROM_IP) {
	   kiwi_show_msg('Admin connection not allowed from this ip address.');
	} else
	if (badp == kiwi.BADP_NO_ADMIN_PWD_SET) {
	   kiwi_show_msg('No admin password set. Can only connect from same local network as Kiwi.<br>Client ip = '+ client_public_ip);
	} else
	if (badp == kiwi.BADP_NO_MULTIPLE_CONNS) {
	   kiwi_show_msg('Multiple connections from the same ip address not allowed.<br>Client ip = '+ client_public_ip);
	} else
	if (badp == kiwi.BADP_DATABASE_UPDATE_IN_PROGRESS) {
	   kiwi_show_msg('Database update in progress.<br>Please try reloading page after one minute.');
	} else
	if (badp == kiwi.BADP_ADMIN_CONN_ALREADY_OPEN) {
	   kiwi_show_msg('Another admin connection already open. Only one at a time allowed. <br>' +
	      'Kick the other connection and retry? <br>' +
         w3_button('w3-medium w3-padding-smaller w3-red w3-margin-T-8', 'Kick other admin', 'kick_other_admin_cb')
      );
	} else
	if (badp == kiwi.BADP_OK) {
		if (p.conn_type == 'kiwi') {
		
			// For the client connection, repeat the auth process for the second websocket.
			// It should always work since we only get here if the first auth has worked.
			extint.ws = owrx_ws_open_wf(kiwi_open_ws_cb2, p);
		} else {
			kiwi_valpwd2_cb(0, p);
		}
	}
}

function kick_other_admin_cb()
{
	console.log('kick_other_admin_cb');
	msg_send('SET kick_admins');
   setTimeout(function() { window.location.reload(); }, 1000);
}

function kiwi_open_ws_cb2(p)
{
	ext_hasCredential(p.conn_type, kiwi_valpwd2_cb, p);
}

function kiwi_valpwd2_cb(badp, p)
{
	if (seriousError)
	   return;        // don't go any further

	kiwi_show_msg('');
	
	if (!body_loaded) {
		body_loaded = true;

		if (p.conn_type != 'kiwi')	{	// kiwi interface delays visibility until some other initialization finishes
			w3_hide('id-kiwi-msg-container');
			w3_show_block('id-kiwi-container');
         w3_el('id-kiwi-body').style.overflow = 'hidden';
		}
		
		//console.log("calling "+ p.conn_type+ "_main()..");
		try {
         w3_call(p.conn_type +'_main');
		} catch(ex) {
			console.log('EX: '+ ex);
			console.log('kiwi_valpwd2_cb: no interface routine for '+ p.conn_type +'?');
		}
	} else {
		console.log("kiwi_valpwd2_cb: body_loaded previously!");
		return;
	}
}

function kiwi_xdLocalStorage_init()
{
	var iframeUrls = [];
	var N_PUB = 2;
	for (var i = 0; i < N_PUB; i++) {
		iframeUrls[i] = 'http://pub'+ i +'.kiwisdr.com/pkgs/xdLocalStorage/xdLocalStorage.php/?key=4e92a0c3194c62b2a067c494e2473e8dfe261138';
	}
	
	xdLocalStorageHA.init({
		iframeUrls: iframeUrls,
		initCallback: function() {
		   kiwi.xdLocalStorage_ready = true;
			console.log('xdLocalStorageHA READY');
		}
	});
}

var override_freq, override_mode, override_zoom, override_max_dB, override_min_dB, override_9_10;

function kiwi_get_init_settings()
{
	// if not configured, take value from config.js, if present, for backward compatibility

	var init_f = (init_frequency == undefined)? 7020 : init_frequency;
	init_f = ext_get_cfg_param('init.freq', init_f, EXT_NO_SAVE);
	init_frequency = override_freq? override_freq : init_f;

	var init_m = ext_get_cfg_param('init.mode', init_mode || 'lsb', EXT_NO_SAVE);
	console.log('INIT init_mode='+ init_mode +' init.mode='+ init_m +' override_mode='+ override_mode);
	init_mode = override_mode? override_mode : init_m;
	if (init_mode === 'drm') init_mode = 'am';      // don't allow inherited drm mode from another channel

	var init_z = (init_zoom == undefined)? 0 : init_zoom;
	init_z = ext_get_cfg_param('init.zoom', init_z, EXT_NO_SAVE);
	init_zoom = isNumber(override_zoom)? override_zoom : init_z;

	var init_max = (init_max_dB == undefined)? -10 : init_max_dB;
	init_max = ext_get_cfg_param('init.max_dB', init_max, EXT_NO_SAVE);
	init_max_dB = override_max_dB? override_max_dB : init_max;

	var init_min = (init_min_dB == undefined)? -110 : init_min_dB;
	init_min = ext_get_cfg_param('init.min_dB', init_min, EXT_NO_SAVE);
	init_min_dB = override_min_dB? override_min_dB : init_min;
	
	console.log('INIT f='+ init_frequency +' m='+ init_mode +' z='+ init_zoom +
		' min='+ init_min_dB +' max='+ init_max_dB);

	w3_call('init_scale_dB');
	
	//console.log('$ override_1Hz='+ override_1Hz +' cfg.show_1Hz='+ cfg.show_1Hz);
	if (override_1Hz)
	   kiwi_storeWrite('freq_dsp_1Hz', 1);
	else
	   kiwi_storeInit('freq_dsp_1Hz', cfg.show_1Hz? 1:0);  // set init value from cfg if unset
	//console.log('$ freq_dsp_1Hz='+ kiwi_storeInit('freq_dsp_1Hz'));

	w3_innerHTML('rx-antenna', 'Antenna: '+ ext_get_cfg_param_string('rx_antenna'));
}


////////////////////////////////
// configuration
////////////////////////////////

var cfg = {};
var dxcfg = null;
var dxcomm_cfg = null;     // read-only, doesn't appear in cfg_save_json()
var adm = {};

function config_save(cfg_s, cfg)
{
   var kiwi_cfg = kiwi[cfg_s];
   if (!isArg(kiwi_cfg) || !isArg(kiwi_cfg.cmd)) {
      kiwi_debug('DANGER: config_save() cfg_s=<'+ cfg_s +'> NOT FOUND in kiwi[] ???', true);
      return;
   }
   var cfg_len = JSON.stringify(cfg).length;
   if (cfg_len < 32) {
      kiwi_debug('DANGER: config_save() cfg_s=<'+ cfg_s +'> cfg_len='+ cfg_len +' TOO SMALL???', true);
      return;
   }
   
   // Can't do this because it defeats the kiwi.cfg.lock mechanism!
   // Better to just fix the places where back-to-back requests are occurring
   // e.g. use w3-defer, EXT_SAVE_DEFER etc.
   /*
   // coalesce back-to-back save requests
   if (kiwi.test_cfg_save_seq == false) {
      kiwi_clearTimeout(kiwi_cfg.timeout);
      kiwi_cfg.timeout = setTimeout(function() {
         config_save2(kiwi_cfg, cfg);
      }, 250);
   } else
   */
   {
      config_save2(kiwi_cfg, cfg);
   }
}

function config_save2(kiwi_cfg, cfg)
{
   //kiwi_trace();
   var s = encodeURIComponent(JSON.stringify(cfg, null, 3));   // pretty-print the JSON
   ext_send_cfg(kiwi_cfg, s);
}

function cfg_save_json(id, path, val)
{
	//console.log('cfg_save_json: BEGIN from='+ id +' path='+ path + (isArg(val)? (' val='+ val) : ''));
	//if (path.includes('kiwisdr_com_register')) kiwi_trace();
	//if (path.includes('rev_')) kiwi_trace();
	//if (path.includes('rx_gps')) kiwi_trace();

	var s;
	if (path.startsWith('adm.')) {
	   config_save('adm', adm);
	} else
	if (path.startsWith('dxcfg.')) {
	   if (dx.dxcfg_parse_error) {
	      console.log('$cfg_save_json dxcfg '+ id +': NOT SAVED DUE TO dxcfg_parse_error');
	      return;
	   }
	   config_save('dxcfg', dxcfg);
	} else {    // cfg.*
      config_save('cfg', cfg);
	}
	console.log('cfg_save_json: from='+ id +' path='+ path +' val=<'+ val +'> DONE');
}

////////////////////////////////
// geolocation
////////////////////////////////

var geo = {
   geo: '',
   json: '',
   retry: 0,
};

function kiwi_geolocate(which)
{
   var ff = kiwi_isFirefox();
   if (ff && ff <= 28) return;   // something goes wrong with kiwi_ajax() w/ FF 28 during a CORS error
   
   if (which == undefined) which = (new Date()).getSeconds();
   which = which % 3;
   var server;

   switch (which) {
      case 0: server = 'https://ipapi.co/json'; break;
      case 1: server = 'https://get.geojs.io/v1/ip/geo.json'; break;
      case 2: server = 'http://ip-api.com/json?fields=49177'; break;
      default: break;
   }
   
   kiwi_ajax(server, 
      function(json) {
         if (isUndefined(json.AJAX_error)) {
            console.log('GEOLOC '+ server);
            console.log(json);
            geoloc_json(json);
         } else {
            if (geo.retry++ <= 3)
               kiwi_geolocate(which+1);
         }
      }, null, 5000
   );
}

function geoloc_json(json)
{
	if (isDefined(json.AJAX_error))
		return;
	
	if (window.JSON && window.JSON.stringify)
      geo.json = JSON.stringify(json);
   else
      geo.json = json.toString();
   
   var country = json.country_name || json.country;
   
   var region = json.regionName || json.region;
	
	if (country == "United States" && region) {
		country = region +', USA';
	}
	
	geo.geo = '';
	if (cfg.show_geo_city) {
      if (json.city) geo.geo += json.city;
      if (country) geo.geo += (json.city? ', ':'') + country;
   } else {
      if (country) geo.geo += country;
   }
   console.log('GEOLOC '+ geo.geo);
}
    
function kiwi_geo()
{
	return encodeURIComponent(geo.geo);
}

function kiwi_geojson()
{
	return encodeURIComponent(geo.json);
}


////////////////////////////////
// time display
////////////////////////////////

var server_time_utc, server_time_local, server_time_tzid, server_time_tzname, server_tz;
var time_display_current = 0;

function time_display_cb(o)
{
	if (isUndefined(o.tu)) return;
	server_time_utc = o.tu;
	server_time_local = o.tl;
	server_time_tzid = decodeURIComponent(o.ti);
	server_time_tzname = decodeURIComponent(o.tn).replace(/\\/g, '').replace(/_/g, ' ');
	server_tz = server_time_tzname;
	if (server_time_tzid) server_tz += ' ('+ server_time_tzid +')';

	if (!time_display_started) {
		time_display_periodic();
		time_display_started = true;
	} else
		time_display(time_display_current);
}

function time_display(display_time)
{
	var el = w3_el('id-time-display-text-inner');
	if (!el) return;

	var noLatLon = (server_time_local == '' || server_time_tzname == 'null');
	w3_innerHTML('id-time-display-UTC', server_time_utc? server_time_utc : '?');
	w3_innerHTML('id-time-display-local', noLatLon? '?' : server_time_local);
	w3_innerHTML('id-time-display-tzname', noLatLon? 'Lat/lon needed for local time' : server_tz);

	w3_opacity('id-time-display-logo-inner', display_time? 0:1);
	w3_opacity('id-time-display-inner', display_time? 1:0);
}

function time_display_periodic()
{
	time_display(time_display_current);
	time_display_current ^= 1;
	setTimeout(function() { time_display_periodic(); }, time_display_current? 50000:10000);
}

var time_display_started = false;
var time_display_prev;

function time_display_setup(ext_name_or_id)
{
   if (ext_name_or_id.startsWith('id-') == false)
      ext_name_or_id += '-time-display';    // called from extension that has used time_display_html()

	var el;
	
	if (time_display_prev) {
		el = w3_el(time_display_prev);
		if (el) el.innerHTML = '';
	}
	time_display_prev = ext_name_or_id;
	
	var el = w3_el(ext_name_or_id);
	el.innerHTML =
	   w3_div('',
         w3_div('id-time-display-inner',
            w3_div('id-time-display-text-inner',
               w3_inline('',
                  w3_div('id-time-display-UTC'),
                  w3_div('cl-time-display-text-suffix', 'UTC')
               ),
               w3_inline('',
                  w3_div('id-time-display-local'),
                  w3_div('cl-time-display-text-suffix', 'Local')
               ),
               w3_div('id-time-display-tzname')
            )
         ),
         w3_div('id-time-display-logo-inner',
            w3_div('id-time-display-logo-text', 'Powered by'),
            '<a href="https://github.com/ha7ilm/openwebrx" target="_blank"><img id="id-time-display-logo" src="gfx/openwebrx-top-logo.png" /></a>'
         )
      );

	time_display(time_display_current);
}

function time_display_height()
{
   return 80;
}

function time_display_width()
{
   return 200;
}

function time_display_html(ext_name, top)
{
   top = top || '50px';
   return w3_div(ext_name +'-time-display|top:'+ top +'; background-color:black; position:relative;');
}


////////////////////////////////
// #ANSI #console output
////////////////////////////////

var ansi = {
   colors:  [
   
      // colors optimized for contrast against id-console-msg #a8a8a8 background
      // regular and bright pallet are the same because MacOS regular colors were too dim

      // regular
      [0,0,0],          // black
      [252,57,31],      // red
      [49,231,34],      // green
      [234,236,35],     // yellow
      [88,51,255],      // blue
      [249,53,248],     // magenta
      [68,249,249],     // cyan
      [233,235,235],    // grey
      
      // bright
      [0,0,0],          // black
      [252,57,31],      // red
      [49,231,34],      // green
      [234,236,35],     // yellow
      [88,51,255],      // blue
      [249,53,248],     // magenta
      [68,249,249],     // cyan
      [233,235,235],    // grey

   /*
      // MacOS Terminal.app colors
   
      // regular
      [0,0,0],
      [194,54,33],
      [37,188,36],
      [173,173,39],
      [73,46,225],
      [211,56,211],
      [51,187,200],
      [203,204,205],
      
      // bright
      [129,131,131],
      [252,57,31],
      [49,231,34],
      [234,236,35],
      [88,51,255],
      [249,53,248],
      [20,240,240],
      [233,235,235]
   */
   
      null
   ],
   
   BRIGHT: 8,
   
   // black on color unless otherwise noted
   RED:     "\u001b[97m\u001b[101m",   // white on red
   YELLOW:  "\u001b[103m",
   GREEN:   "\u001b[102m",
   CYAN:    "\u001b[106m",
   BLUE:    "\u001b[97m\u001b[104m",   // white on blue
   MAGENTA: "\u001b[97m\u001b[105m",   // white on magenta
   GREY:    "\u001b[47m",
   NORM:    "\u001b[m",
   
   rolling: [ 'RED', 'YELLOW', 'GREEN', 'CYAN', 'BLUE', 'MAGENTA', 'GREY' ],
   rolling_n: 7
};

// esc[ ... m
// vt100.net/docs/vt510-rm/SGR.html
function kiwi_output_sgr(p)
{
   var dbg_sgr = 0;
   var msg = 'SGR', str = '';
   var sgr, saw_reset = 0, saw_color = 0;
   var sa = p.esc.s.substr(1).split(';');
   var sl = sa.length;
   if (dbg_sgr) console.log('SGR '+ kiwi_JSON(p.esc.s) +' sl='+ sl);
   if (dbg_sgr) console.log(sa);
   if (p.isAltBuf) msg += '('+ p.r +','+ p.c +')';

   for (var ai = 0; ai < sl && !isNumber(msg); ai++) {
      sgr = (sa[ai] == '' || sa[ai] == 'm')? 0 : parseInt(sa[ai]);
      if (dbg_sgr) console.log('sgr['+ ai +']='+ sgr);
      if (sgr == 0) {      // \e[m or \e[0m  all attributes off
         p.sgr.fg = p.sgr.bg = null;
         msg += ', reset'; 
         saw_reset = 1;
      } else

      if (isNaN(sgr)) {
         msg = 2;
      } else

      if (sgr == 1)  { p.sgr.bright = ansi.BRIGHT; msg += ', bright'; } else
      if (sgr == 2)  { p.sgr.bright = 0; msg += ', faint'; } else
      if (sgr == 22) { p.sgr.bright = 0; msg += ', normal'; } else
      
      if (sgr == 7) {      // reverse video (swap fg/bg)
         var tf = p.sgr.fg;
         p.sgr.fg = p.sgr.bg;
         p.sgr.bg = tf;
         if (p.sgr.fg == null) p.sgr.fg = [255,255,255];
         if (p.sgr.bg == null) p.sgr.bg = [0,0,0];
         msg += ', reverse video'; 
         saw_color = 1;
      } else

      if (sgr == 27) {     // inverse off
         p.sgr.fg = p.sgr.bg = null;
         msg += ', reverse video off'; 
         saw_reset = 1;
      } else
      
      // foreground color
      if (sgr >= 30 && sgr <= 37) {
         if (dbg_sgr) console.log('SGR='+ sgr +' bright='+ p.sgr.bright);
         p.sgr.fg = ansi.colors[sgr-30 + p.sgr.bright];
         msg += ', fg color'; 
         saw_color = 1;
      } else

      if (sgr >= 90 && sgr <= 97) {    // force bright
         p.sgr.fg = ansi.colors[sgr-90 + ansi.BRIGHT];
         msg += ', fg color bright'; 
         saw_color = 1;
      } else

      if (sgr == 39) {     // normal
         p.sgr.fg = null;
         msg += ', fg normal'; 
         saw_color = 1;
      } else

      // background color
      if (sgr >= 40 && sgr <= 47) {
         p.sgr.bg = ansi.colors[sgr-40 + p.sgr.bright];
         msg += ', bg color'; 
         saw_color = 1;
      } else

      if (sgr >= 100 && sgr <= 107) {     // force bright
         p.sgr.bg = ansi.colors[sgr-100 + ansi.BRIGHT];
         msg += ', bg color bright'; 
         saw_color = 1;
      } else

      if (sgr == 49) {     // normal
         p.sgr.bg = null;
         msg += ', bg normal'; 
         saw_color = 1;
      } else
      
      // 8 or 24-bit fg/bg
      if (sgr == 38 || sgr == 48) {
         if (dbg_sgr) console.log('SGR-8/24 sl='+ sl);
         var n8, r, g, b, color, ci;

         if (sl == 3 && (parseInt(sa[1]) == 5) && (!isNaN(n8 = parseInt(sa[2])))) {
            if (dbg_sgr) console.log('SGR n8='+ n8);
            ai += 2;
            if (n8 <= 15) {      // standard colors
               color = ansi.colors[n8];
               if (sgr == 38) p.sgr.fg = color; else p.sgr.bg = color;
               msg += ', 38/48 mode color'; 
               saw_color = 1;
            } else
            if (n8 <= 231) {     // 6x6x6 color cube
               n8 -= 16;
               r = Math.floor(n8/36); n8 -= r*36;
               g = Math.floor(n8/6); n8 -= g*6;
               b = n8;
               r = Math.floor(255 * r/5);
               g = Math.floor(255 * g/5);
               b = Math.floor(255 * b/5);
               color = [r,g,b];
               if (sgr == 38) p.sgr.fg = color; else p.sgr.bg = color;
               msg += ', color cube'; 
               saw_color = 1;
            } else
            if (n8 <= 255) {     // grayscale ramp
               ci = 8 + (n8-232)*10;
               if (dbg_sgr) console.log('n8='+ n8 +' ci='+ ci);
               color = [ci,ci,ci];
               if (sgr == 38) p.sgr.fg = color; else p.sgr.bg = color;
               msg += ', grayscale ramp'; 
               saw_color = 1;
            } else
               msg = 2;
         } else

         if (sl == 5 && (parseInt(sa[1]) == 2) &&
            (!isNaN(r = parseInt(sa[2]))) && (!isNaN(g = parseInt(sa[3]))) && (!isNaN(b = parseInt(sa[4])))) {
               r = w3_clamp(r, 0,255);
               g = w3_clamp(g, 0,255);
               b = w3_clamp(b, 0,255);
               color = [r,g,b];
               if (sgr == 38) p.sgr.fg = color; else p.sgr.bg = color;
               msg += ', 24-bit color'; 
               saw_color = 1;
         } else
            msg = 2;
      } else
         msg = 2;
   }
   
   if (saw_reset) {  // \e[m or \e[0m
      if (dbg_sgr) console.log('SGR DONE');
      if (p.sgr.span) {
         str += '</span>';
         p.sgr.span = 0;
      }
   } else
   if (saw_color) {
      if (dbg_sgr) {
         console.log('SGR saw_color fg='+ kiwi_rgb(p.sgr.fg) +' bg='+ kiwi_rgb(p.sgr.bg));
         console.log(p.sgr.fg);
         console.log(p.sgr.bg);
      }
      if (p.sgr.span) str += '</span>';
      str += '<span style="'+ (p.sgr.fg? ('color:'+ kiwi_rgb(p.sgr.fg) +';') :'') + (p.sgr.bg? ('background-color:'+ kiwi_rgb(p.sgr.bg) +';') :'') +'">';
      p.sgr.span = 1;
   } else {
      if (dbg_sgr) console.log('SGR ERROR');
   }
   
   return { msg: msg, str: str };
}

function kiwi_output_msg(id, id_scroll, p)
{
   var i, j;
   var dbg = (0 && dbgUs);
   var dbg2 = ((0 && dbgUs) || dbg);
   if (dbgUs) kiwi.d.p = p;
   
   if (0 && dbg) {
      console.log('$kiwi_output_msg id='+ id +' init='+ p.init +' isAltBuf='+ p.isAltBuf +' s='+ p.s);
      console.log('$ '+ kiwi_JSON(p));
      //if (!p.init) kiwi_trace();
   }
   
	var parent_el = w3_el(id);
	if (!parent_el) {
	   console.log('kiwi_output_msg NOT_FOUND id='+ id);
	   return;
	}
	
	var appendEmptyLine = function(parent_el) {
	   var el = w3_create_appendElement(parent_el, 'pre', '');
	   p.nlines++;
	   while (p.nlines > p.max_lines) {
	      parent_el.removeChild(parent_el.firstChild);
	      p.nlines--;
	   }
	   return el;
	};

	var removeAllLines = function(parent_el) {
	   while (parent_el.firstChild) {
	      parent_el.removeChild(parent_el.firstChild);
	   }
	   p.nlines = 0;
	};

   var console_cursor = function(ch) {
      ch = ch || '&nbsp;';
      return '<span class="cl-admin-console-cursor">'+ ch +'</span>';
   };
   
	var render2 = function() {
	   var fg = null, bg = null, span = false;
	   //var r_s = '';
	   for (var r = 1; r <= p.nrows; r++) {
         if (!p.dirty[r]) continue;
         //r_s += r +' ';
         p.dirty[r] = false;
         var s = '';
         for (var c = 1; c <= p.cols; c++) {
            var color = p.color[r][c];
            if (isUndefined(color)) {
               console_nv('color undef', {r}, {c});
            }
            if (color.fg != fg || color.bg != bg) {
               if (span) s += '</span>';
               if (color.fg || color.bg) {
                  s += '<span style="'+ (color.fg? ('color:'+ kiwi_rgb(color.fg) +';') :'') +
                     (color.bg? ('background:'+ kiwi_rgb(color.bg) +';') :'') +'">';
               } else {
                  span = false;
               }
               fg = color.fg;
               bg = color.bg;
               span = true;
            }
            
            // HTML escapes
            var ch = p.screen[r][c];
            if (ch == '<') ch = '&lt;'; else
            if (ch == '>') ch = '&gt;'; else
            if (ch == '&') ch = '&amp;'; else
            if (ch == '\'') ch = '&apos;'; else
            if (ch == '/') ch = '&#47;'; else
               ;

            // cursor
            if (p.show_cursor && r == p.r && c == p.c) {
               s += console_cursor(ch);
               p.r_cursor = r; p.c_cursor = c;
            } else {
               s += ch;
            }
         }
         if (s == '') s = '&nbsp;';    // make empty lines render
         else
         if (span) s += '</span>';
         try {
            p.els[r].innerHTML = s;
         } catch(ex) {
            if (dbg) {
               console.log('r='+ r);
               console.log(p.els[r]);
               console.log(ex);
            }
         }
      }
      //if (dbgUs) console.log('RENDER '+ (p.rend_seq++));
      //if (dbg) console.log('RENDER '+ r_s);
	};
	
	var render = function() {
	   //if (dbg) console.log(p);
	   if (p.r_cursor != p.r && p.r_cursor != 0) {
	      p.dirty[p.r_cursor] = true;      // re-render row with old cursor (if not the current row)
	   }
      render2();
	};
	
   // schedule rendering
	var sched = function() {
	   p.sched_time = Date.now();
	   if (!p.metronome_running) {
	      p.metronome_running = true;
	      //if (dbg) console.log('RUN');
	      p.metronome_interval = setInterval(
	         function() {
	            if (p.sched_time < (Date.now() - 200)) {
                  kiwi_clearInterval(p.metronome_interval);
                  p.metronome_running = false;
	               //if (dbg) console.log('STOP');
                  render();
	            }
	         }, 200
	      );
	   }
	   /*
      kiwi_clearTimeout(p.rend_timeout);
	   p.rend_timeout = setTimeout(function() { render(); }, 250);
	   */
	};
	
	var dirty = function() {
	   p.dirty[p.r] = true;
	};
	
	var screen_char = function(ch, color, shift_mode) {
	   var r = p.r, c = p.c;
      //if (dbg && p.traceEvery && ord(ch) > 0x7f)
         //console.log('$every '+ r +','+ c +' '+ ch +'('+ ord(ch) +')');
      
      if (shift_mode != p.NOSHIFT_MODE && p.insertMode) {
         for (i = r.cols-1; i >= c; i--) {
            p.screen[r][i+1] = p.screen[r][i];
            p.color[r][i+1] = p.color[r][i];
         }
      }
      
      try {
	      p.screen[r][c] = ch;
	   } catch(ex) {
         if (dbg) {
            console.log('r='+ r +' c='+ c);
            console.log(kiwi_JSON(ch));
            //console.log(kiwi_JSON(p));
            console.log(p);
            console.log(ex);
         }
	   }
	   
      if (isUndefined(p.screen[r])) {
         if (dbg) {
            console_nv('screen_char', {r}, {c}, 'kiwi.d.p.nrows');
            console.log(p);
            //kiwi_trace();
         }
         return;
      }
	   p.color[r][c] = color? color : { fg: p.sgr.fg, bg: p.sgr.bg };
	   dirty();
      p.c++;
      if (p.c > p.cols) {
         if (p.eol_wrap && shift_mode != p.NOSHIFT_MODE) {
            p.c = 1; p.r++;
            if (p.r > p.nrows) { p.r = 1; dirty(); }
         } else {
            p.c = p.cols;
         }
      }
	   sched();
	};

	var move_in_display = function(dr_start, sr_start, sr_end, step) {
      //if (dbg) console_nv('move_in_display', {dr_start}, {sr_start}, {sr_end}, {step}, 'kiwi.d.p.nrows');
      dr_start = w3_clamp(dr_start, 0, p.nrows);
      sr_start = w3_clamp(sr_start, 0, p.nrows);
      sr_end = w3_clamp(sr_end, 0, p.nrows);
	   var row = dr_start;
	   var down = (step > 0);
      for (var ri = sr_start; (down && ri <= sr_end) || (!down && ri >= sr_end); ri += step) {
         p.r = row; row += step;
         p.c = 1;
         //if (dbg) console_nv('move_in_display', 'kiwi.d.p.r', {ri});
         for (var ci = 1; ci <= p.cols; ci++) {
            screen_char(p.screen[ri][ci], p.color[ri][ci]);
         }
      }
	};
	
	var erase_in_line = function(c_start, c_end) {
      for (var c = c_start; c_start && c <= c_end; c++) {
         p.screen[p.r][c] = ' ';
         p.color[p.r][c] = { fg: null, bg: null };
      }
      dirty();
      sched();
	};

	var erase_in_display = function(r_start, r_end, c_start, c_end) {
      for (var r = r_start; r_start && r <= r_end; r++) {
         for (var c = c_start; c <= ((r == p.r)? c_end : p.cols); c++) {
            if (isUndefined(p.screen[r])) {
               console_nv('erase_in_display', {r}, {c}, 'kiwi.d.p.nrows', 'kiwi.d.p.ncols');
               console.log(p);
            }
            p.screen[r][c] = ' ';
            p.color[r][c] = { fg: null, bg: null };
         }
         p.dirty[r] = true;
      }
      sched();
	};
	
	var snew_add = function(c) {
	   if (snew == '') {
	      p.snew_r = p.r;
	      p.snew_c = p.c;
	   }
	   snew += c;
   };
   
	var snew_dump = function(where) {
      if (snew != '') {
         if (dbg) console.log('> '+ (p.isAltBuf? 'ALT' : 'REG') +' '+ where +
            ' ('+ p.snew_r +','+ p.snew_c +') '+ ' CHARS #'+ snew.length +' '+ kiwi_JSON(snew));
         snew = '';
      }
   };
   
	var html_add = function(c) {
      if (isString(p.line_html[p.ccol]))
         p.line_html[p.ccol] += c;
      else
         p.line_html[p.ccol] = c;
   };
   
   var line_s = function() {
      return kiwi_JSON(p.line.join(''));
   };
   
   var line_join = function(col_start, col_stop) {
      col_stop = col_stop || p.cols;
      var s = '';
      var a = [];
      var i;
      for (var c = col_start, i = 0; c < col_stop; c++, i++) {
         var sgr = (isString(p.line_sgr[c]))? p.line_sgr[c] : '';
         var html = (isString(p.line_html[c]))? p.line_html[c] : '';
         var ch = (isString(p.line[c]))? p.line[c] : '';
         s += sgr + html + ch;
         a[i] = { sgr:sgr, html:html, ch:ch, s:s };
      }
      return { s:s, a:a };
   };
   
   var init_common = function(init) {
      var r, c;
      if (dbg2) console_nv('init_common', {init}, 'kiwi.d.p.nrows');

      if (init == p.INIT_ONCE || init == p.INIT_RESIZE) {
         p.screen = [];
         p.color = [];
         p.dirty = [];
         p.els = [];

         for (var r = 0; r <= p.nrows; r++) {
            p.screen[r] = [];
            p.color[r] = [];
         }
      }

      if (init == p.INIT_ALTBUF || init == p.INIT_RESIZE) {
         removeAllLines(parent_el);
         
         for (var r = 1; r <= p.nrows; r++) {
            try {
               for (var c = 1; c <= p.cols; c++) {
                  p.screen[r][c] = ' ';
                  p.color[r][c] = { fg: null, bg: null };
               }
            } catch(ex) {
               if (dbg) {
                  console.log('--------');
                  console.log('r='+ r +' c='+ c);
                  console.log(p.screen);
                  console.log(ex);
               }
            }
            p.dirty[r] = false;
            p.els[r] = appendEmptyLine(parent_el);
            p.els[r].innerHTML = '&nbsp;';      // force initial rendering
         }
      }
   };
   
	var s;
	if (isNoArg(p.no_decode) || p.no_decode != true) {
      try {
         //if (dbg) console.log(kiwi_JSON(p.s));
         s = kiwi_decodeURIComponent('kiwi_output_msg', p.s);
      } catch(ex) {
         console.log('decodeURIComponent FAIL:');
         console.log(p.s);
         s = p.s;
      }
      if (s == null) s = '(kiwi_decodeURIComponent():null)';
   } else {
      s = p.s;
   }
	
   if (p.intercept) {
      p.intercept(s);
   }

   if (p.init != true) {
      //if (dbg) console.log('$console INIT '+ p.init);
      //kiwi_trace();
      p.INIT_ONCE = 0;
      p.INIT_RESIZE = 1;
      p.INIT_ALTBUF = 2;
      p.resized = false;

      removeAllLines(parent_el);
      p.el = appendEmptyLine(parent_el);
      p.cols = p.cols || 140;
      p.NONE = 0;
      p.ESC = 1;
      p.CSI = 2;
      p.NON_CSI = 3;
      p.NOSHIFT_MODE = true;
      p.esc = { s: '', state: p.NONE };
      p.sgr = { span:0, bright:0, fg:null, bg:null };
      p.return_pending = false;
      p.must_scroll_down = false;
      p.traceEvery = false;
      
      // line-oriented
      // p.rows p.cols     set by caller
      p.ccol = 1;
      p.line = [];
      p.line_sgr = [];
      p.line_html = [];
      p.inc = 1;
      
      // char-oriented
      p.nrows = p.rows;
      p.r = p.c = 1;
      p.margin_set = false;
      p.margin_top = 1;
      p.margin_bottom = p.nrows;    // initial value until changed by setting margins
      p.show_cursor = p.show_cursor || false;
      p.r_cursor = p.c_cursor = 0;
      p.insertMode = true;
      p.eol_wrap = false;     // NB: "top -c" doesn't like EOL wrapping
      init_common(p.INIT_ONCE);
      p.isAltBuf = false;
      p.altbuf_via_cursor_visible = false;
      p.altbuf_via_cup = false;
      p.alt_save = '';
      //p.rend_timeout = null;
      p.rend_seq = 0;
      p.metronome_running = false;
      p.sched_time = 0;

      p.init = true;
   }
   
   if (p.resized && p.isAltBuf) {
      if (dbg2) console.log('console resized nrows: '+ p.nrows +' => '+ p.rows +' #############################################################################################');
      p.nrows = p.rows;
      init_common(p.INIT_RESIZE);
      p.margin_top = 1;
      p.margin_bottom = p.nrows;    // initial value until changed by setting margins
      p.r = p.c = 1; dirty();       // reset cursor
      p.r_cursor = p.c_cursor = 0;
      render();
      p.resized = false;
   }

   var snew = '', stmp;
	var el_scroll = w3_el(id_scroll);
   var wasScrolledDown = null;
   
   // handle output beginning with '\r' alone (or starting with '\r\n') to overwrite current line
   // see also p.remove_returns and p.inline_returns
   // NB: there are no users currently that set "process_return_alone = true"
   //if (dbg) console.log(kiwi_JSON(s));
   if (p.process_return_alone && s.charAt(0) == '\r' && (s.length == 1 || s.charAt(1) != '\n')) {
      //console.log('\\r @ beginning:');
      //console.log(kiwi_JSON(s));
      if (p.isAltBuf) {
      } else {
         s = s.substring(1);
         p.line = [];
         p.line_sgr = [];
         p.line_html = [];
         p.ccol = 1;
      }
   }

   if (0) {
      if (dbg) console.log('kiwi_output_msg:');
      if (dbg) console.log(kiwi_JSON(p));
      if (dbg) console.log(kiwi_JSON(s));
   }
   if (p.remove_returns) s = s.replace(/\r/g, '');
   if (p.inline_returns && !p.isAltBuf) {
      // done twice to handle case observed with "pkup":
      // \r\r\n => \r\n => \n
      // that would otherwise result in spurious blank lines
      s = s.replace(/\r\n/g, '\n');
      s = s.replace(/\r\n/g, '\n');
   }
   if (dbg) console.log('OUTPUT '+ kiwi_JSON(s).replace(/\\u001b/g, '\\e'));
   
   var cnt = 0;
	for (var si = 0; si < s.length; si++) {
	   var result = null;

		var c = s.charAt(si);
      //if (dbg) console.log(i +' c='+ kiwi_JSON(c));

		if (p.inline_returns && p.return_pending && c != '\r') {
		   if (p.isAltBuf) {
		   } else {
            p.ccol = 1;
            p.line_sgr[p.ccol] = null;    // e.g. "more <file>" then \n to advance single line
            p.line_html[p.ccol] = null;
            p.return_pending = false;
         }
		}

		if (c == '\f') {
		   if (p.isAltBuf) {
		   } else {
            // form-feed is how we clear accumulated pre elements (screen clear, pre-ANSI)
            if (dbg) console.log('console \f');
            removeAllLines(parent_el);
            p.el = appendEmptyLine(parent_el);
            p.line = [];
            p.line_sgr = [];
            p.line_html = [];
            p.ccol = 1;
         }
		} else
		
		if (c == '\r') {
         //if (dbg) console.log('\\r inline, isAltBuf='+ p.isAltBuf);
         if (p.isAltBuf) {
            p.c = 1; dirty();
         }
         if (p.inline_returns) {
            p.return_pending = true;
         }
         snew_dump('rtn');
         result = '\\r col = 1 ('+ p.r +',1)';
      } else
      
      // scroll text up
		if (c == '\n' && p.isAltBuf) {
		   if (p.margin_set) {
		      if (p.r < p.nrows) {
               p.r++; p.c = 1;
               dirty();
               result = '\\n row++ ('+ p.r +'('+ p.nrows +'),'+ p.c +')';
            } else {
               //var save_r = p.r, save_c = p.c;
               move_in_display(p.margin_top, p.margin_top + 1, p.margin_bottom, +1);
               //p.r = save_r; p.c = save_c;
               result = '\\n (scroll text up) nrows='+ p.nrows;
            }
         } else {
            p.c = 1;
            if (p.r < p.rows) p.r++;
            dirty();
            result = '\\n row++ ('+ p.r +','+ p.c +')';
         }
         if (dbg) console.log('scroll text up FIN: '+ p.r +' '+ p.c +' '+ p.r_cursor +' '+ p.c_cursor +' '+ result);
      } else
      
		if (c == '\b') {
         result = 'backspace (arrow left)';
		   if (p.isAltBuf) {
            if (p.c > 1) { dirty(); p.c--; }
		   } else {
            if (p.ccol > 1) {
               p.ccol--;
               if (dbg) console.log('BACKSPACE col='+ p.ccol +' '+ line_s());
            }
         }
		} else
		
		// tab-8
		if (c == '\t') {
		   result = 'tab';
		   if (p.isAltBuf) {
		   } else {
            snew_add('&nbsp;');
            p.line[p.ccol] = '&nbsp;';
            p.ccol++;
            while ((p.ccol & 7) != 0) {
               snew_add('&nbsp;');
               p.line[p.ccol] = '&nbsp;';
               p.ccol++;
            }
         }
		} else
	
	   // CSI:  ESC [ ...
	   //    0-n 0x30-3f    0-9:;<=>?               parameter bytes
	   //    0-n 0x20-2f    space !"#$%&'()*+,-./   intermediate bytes
	   //      1 0x40-7e    @ A-Z [\]^_` a-z {|}~   final byte (0x70-7e are private)
	   
	   // non-CSI:
	   //    Fe seq (C1 control codes)     ESC 0x40-5e    @ A-Z [\]^
	   // H     set ht (nop)
	   // M     reverse index
	   // O_    func keys
	   // ]     OS command (uses ST)
	   // \     string terminator (ST)

	   //    Fs seq                        ESC 0x60-7e    ` a-z {|}~
	   // c     reset to initial state
	   // l     UNK
	   // m     UNK

	   //    Fp seq                        ESC 0x30-3f    0-9:;<=>?
	   // 7     save cursor
	   // 8     restore cursor
	   // >     num keypad
	   // =     keypad applic mode

	   //    Nf seq                        ESC 0x20-2f    space !"#$%&'()*+,-./
	   // (     select char set G0
		
		if (c == '\x1b') {      // esc = ^[ = 0x1b = 033 = 27.
		   p.esc.s = '';
		   p.esc.state = p.ESC;
		   p.traceEvery = false;
		} else

      // en.wikipedia.org/wiki/ANSI_escape_code
      // vt100.net/docs/vt510-rm/chapter4.html
      // output from "infocmp xterm-256color" command
      // pubs.opengroup.org/onlinepubs/7908799/xcurses/terminfo.html

		if (p.esc.state >= p.ESC) {
         var interp = false;
         //if (dbg) console.log(i +' acc '+ kiwi_JSON(c));
         
         if (p.esc.state == p.ESC) {
            if (c == '[') {
               p.esc.state = p.CSI;
            } else {
               p.esc.state = p.NON_CSI;
               switch (c) {
                  case '(':
                  case 'O':
                     cnt = 2; break;

                  case '>':
                  case '=':
                     cnt = 1; break;

                  default:
                     if (dbg) console.log('$LEN? c='+ c);
                     cnt = 1; break;
               }
               //if (dbg) console.log('$NON c='+ c +' cnt='+ cnt);
            }
         }
         
         // CSI
         // accumulate CSI '[' (0x5b) and CSI 0x20-2f, 0x30-3f chars (i.e. outside 0x40-7e)
		   if (p.esc.state == p.CSI) {
            p.esc.s += c;
            //if (dbg) console.log('$acc CSI '+ c +'('+ ord(c) +') '+ p.esc.s);
            if (p.esc.s.length > 1 && c >= '@' && c <= '~') interp = true;
		   } else

         // non CSI
         // accumulate a fixed number of chars defined by first char seen
		   if (p.esc.state == p.NON_CSI && cnt) {
            p.esc.s += c;
            //if (dbg) console.log('$acc NON '+ c +'('+ ord(c) +') '+ p.esc.s);
            cnt--;
            if (cnt == 0) interp = true;
		   }

         // sequence complete, interpret
		   if (interp) {
		      //if (dbg && p.esc.state == p.NON_CSI) console.log('$INTERPRET ESC '+ kiwi_JSON(p.esc.s));
		      var first = p.esc.s.charAt(0);
		      var second = p.esc.s.charAt(1);
		      var last_hl = (c == 'h' || c == 'l');
		      var enable = (c == 'h');
            result = 0;
            var error = 0;
            
            var n1 = 1, n2 = 1;
            var t = p.esc.s.slice(1);
            if (t.length > 1 && isNumber(+t[0])) {
               var a = t.slice(0, t.length-1);
               a = a.split(';');
               n1 = +a[0];
               n1 = n1 || 1;
               if (a.length > 1) n2 = +a[1];
               n2 = n2 || 1;
            }
		      
            // CSI handled: (* = non-isAltBuf)
            // ESC [    A B C* D d G H J* K* M m* P* r S T X 4 ? @ t *p
		      if (first == '[') {

		         // ANSI color escapes
               if (c == 'm') {      // esc [ ... m  fg/bg color
                  var rv = kiwi_output_sgr(p);
                  result = rv.msg;
                  if (p.isAltBuf) {
                  } else {
                     snew_add(rv.str);
                     p.line_sgr[p.ccol] = rv.str;
                     if (dbg) console.log('AFTER SGR snew='+ kiwi_JSON(snew));
                  }
                  p.traceEvery = true;
               } else
               
               if (c == 'K') {      // erase in line
                  if (p.isAltBuf) {
                     var c_start, c_end;
                     result = 'erase in line: ';

                     switch (second) {
                        case '0':
                        case 'K': c_start = p.c; c_end = p.cols; result += 'cur to EOL'; break;
                        case '1': c_start = 1;   c_end = p.c;    result += 'BOL to cur'; break;
                        case '2': c_start = 1;   c_end = p.cols; result += 'full line'; break;
                        default:  c_start = 0; break;
                     }
                     erase_in_line(c_start, c_end);
                  } else {
                     if (second == 'K') {
                        if (dbg) console.log('EEOL BEFORE col='+ p.ccol +' '+ line_s());
                        var col_stop = p.line.length;
                        for (i = p.ccol; i <= col_stop; i++) { p.line[i] = null; p.line_sgr[i] = null; p.line_html[i] = null; }
                        if (dbg) console.log('EEOL AFTER col='+ p.ccol +' '+ line_s());
                        result = 'erase in line: cur to EOL';
                     }
                  }
               } else
               
               if (c == 'J') {      // erase in display
                  if (p.isAltBuf) {
                     var r_start, r_end, c_start, c_end;

                     if (second == '0' || second == 'J') {     // [J  [0J
                        r_start = p.r; r_end = p.nrows;
                        c_start = p.c; c_end = p.cols;
                        result = 'erase cur to EOS';
                     } else
                     if (second == '2' || second == '3') {     // [2J  [3J
                        r_start = 1; r_end = p.nrows;
                        c_start = 1; c_end = p.cols;
                        result = 'erase full screen';
                     } else
                     if (second == '1') {                      // [1J
                        r_start = 1; r_end = p.r;
                        c_start = 1; c_end = p.c;
                        result = 'erase BOS to cur';
                     } else {
                        r_start = 0;
                        result = 2;
                     }
                     erase_in_display(r_start, r_end, c_start, c_end);
                  } else {
                     if (second == '2' || second == '3') {     // [2J  [3J
                        if (dbg) console.log('console ERASE FULL SCREEN');
                        removeAllLines(parent_el);
                        p.el = appendEmptyLine(parent_el);
                        p.line = [];
                        p.line_sgr = [];
                        p.line_html = [];
                        p.ccol = 1;
                        result = 'erase full screen';
                     }
                  }
               } else
               
               if (c == 'H') {      // cursor position
                  result = 'move '+ n1 +','+ n2;
                  if (p.isAltBuf) {
                     if (w3_clamp3(n1, 0, p.nrows)) {
                        dirty(); p.r = n1; p.c = n2; dirty();
                     }
                  } else {
                     error = 1;
                  }
               } else
		      
               if (c == 'd') {      // vertical line position absolute
                  //result = 'move row '+ n1 +' (col = 1)';
                  //p.r = n1; p.c = 1;
                  result = 'move row '+ n1;
                  if (p.isAltBuf) {
                     if (w3_clamp3(n1, 0, p.nrows)) {
                        dirty(); p.r = n1; dirty();
                     }
                  } else {
                     error = 1;
                  }
               } else
		      
               if (c == 'G') {      // cursor horizontal absolute
                  result = 'move col '+ n1;
                  if (p.isAltBuf) {
                     p.c = n1; dirty();
                  } else {
                     error = 1;
                  }
               } else
		      
		         // see: pubs.opengroup.org/onlinepubs/7908799/xcurses/terminfo.html
               // esc [ ? # h  esc[ ? # l
               // esc [ ! p
               if ((second == '?' && last_hl) || (second == '!' && c == 'p')) {
                  var as = p.esc.s.substr(2);
                  aa = as.split(';');
                  n1 = parseInt(aa[0]);
                  n2 = parseInt(aa[1]);
                  if (dbg) console_nv(as, {n1}, {n2});

                  if (second == '?') {
                     result = (enable? 'SET':'RESET') +' ';
                  } else {
                     result = 'soft term reset: ';
                     n1 = 1049;
                     enable = false;
                  }
                  var enter_altbuf = false, exit_altbuf = false, exit_altbuf_no_restore = false;

                  switch (n1) {
                  
                     // 3 [hl]   col mode, 132/80
                     // 4 [hl]   scrolling mode, smooth/jump
                     // 5 [hl]   screen mode light/dark, used in flashing screen:
                     //          flash=\E[?5h$<100/>\E[?5l ($<100/> is time delay)
                     // 6 [hl]   origin mode
                     // 69 [hl]  
                     // other hl: 42 95 96 98 34 64 61 35 36 104
                     //
                     // non-hl:
                     // 5 W      tab every 8 cols
                     // 6 n      extended cursor report
                  
                     // [sr]mkx=\E[?1 [hl]
                     // see: stackoverflow.com/questions/13585131/keyboard-transmit-mode-in-vt100-terminal-emulator
                     case 1: result += 'cursor keys mode'; break;

                     case 3: result += 'col mode 132/80 (ignored)'; break;
                     case 4: result += 'scrolling smooth/jump (ignored)'; break;
                     
                     // init_2string   is2=\E[!p\E[?3;4l\E[4l\E>  col-mode-80/scroll-jump, replace-mode
                     // reset_2string  rs2=\E[!p\E[?3;4l\E[4l\E>

                     // [sr]mam=\E[?7 [hl]
                     case 7: result += 'vertical autowrap'; break;

                     // cvvis=\E[?12;25h
                     // civis=\E[?25l
                     // cnorm=\E[?12l \E[?25h
                     // ?25 = cursor visible(h)/invisible(l)
                     // ?12 = make cursor very visible(h) / normal(l)
                     case 12: result += 'cursor bold ';
                              if (n2 != 25) break;
                              // "12;25" fall through ...
                     case 25: result += 'cursor visible';
                              p.show_cursor = enable? true:false;
                              if (!enable && !p.isAltBuf) {
                                 enter_altbuf = true;
                                 p.altbuf_via_cursor_visible = true;
                              } else
                              if (enable && p.isAltBuf && p.altbuf_via_cursor_visible) {
                                 exit_altbuf = true;
                                 //exit_altbuf_no_restore = true;
                              }
                              break;

                     // clear_margins mgc=\E[?69l
                     // set_lr_margin smglr=\E[?69h\E[%i%p1%d;%p2%ds
                     case 69: result += 'LR margins (ignored)';
                              break;

                     // enter/exit "cup" (cursor position) mode: [sr]mcup=\E[?1049 [hl]
                     case 1049:
                        result += 'alt screen buf';
                        if (enable && !p.isAltBuf) {
                           if (dbg) console.log('1049: p.nrows='+ p.nrows);
                           enter_altbuf = true;
                           p.altbuf_via_cup = true;
                        } else
                        if (!enable && p.isAltBuf && p.altbuf_via_cup) {
                           exit_altbuf = true;
                        }
                        break;

                     default: result += '$UNKNOWN ='+ n1; break;
                  }
                  
                  if (enter_altbuf) {
                     p.nrows = p.rows;    // in case partial margin still in effect
                     if (dbg2) console.log('$ENTER alt buf via '+ (p.altbuf_via_cup? 'cup' : 'cursor_visible') +
                        ' nrows='+ p.nrows +' rows='+ p.rows +' ====================================');

                     // remove any cursor at end that we don't want to display on restore
                     p.alt_save = parent_el.innerHTML.replace(/<pre><span class="cl-admin-console-cursor">.*<\/span><\/pre>$/, '');
                     if (dbg) console.log(kiwi_JSON(p.alt_save));
                     init_common(p.INIT_ALTBUF);
                     if (isAdmin()) console_is_char_oriented(true);
                     p.margin_set = false;
                     p.isAltBuf = true;
                     p.r = p.c = 1; dirty();    // reset cursor
                     if (dbg2) console.log('1049: '+ p.r +' '+ p.c +' '+ p.r_cursor +' '+ p.c_cursor);
                  } else
                  
                  if (exit_altbuf) {
                     if (dbg2) console.log('$EXIT alt buf via '+ (p.altbuf_via_cup? 'cup' : 'cursor_visible') +
                        ' nrows='+ p.nrows +' rows='+ p.rows +' ====================================');
                     p.altbuf_via_cursor_visible = false;
                     p.altbuf_via_cup = false;

                     if (dbg) console.log(kiwi_JSON(p.alt_save));
                     parent_el.innerHTML = p.alt_save;
                     p.el = appendEmptyLine(parent_el);
                     p.must_scroll_down = true;
                     if (isAdmin()) console_is_char_oriented();
                     p.isAltBuf = false;
                  } else

                  if (exit_altbuf_no_restore) {
                     if (dbg2) console.log('$EXIT alt buf NO RESTORE =====================================');
                     p.must_scroll_down = true;
                     if (isAdmin()) console_is_char_oriented();
                     p.isAltBuf = false;
                  }
               } else
               
               // insert/replace mode
               if (second == '4' /* && p.isAltBuf */ && last_hl) {      // esc[4h  esc[4l
                  p.insertMode = enable;
                  result = enable? 'INSERT mode' : 'REPLACE mode';
               } else
               
               // erase characters
               if (c == 'X') {      // ech=\E[%p1%dX
                  if (n1 == 0) n1 = 1;
                  if (p.isAltBuf) {
                     var col;
                     for (var ci = 0, col = p.c; ci < n1 && col <= p.cols; ci++, col++) {
                        //if (dbg) console.log('erase '+ p.r +','+ col +'|'+ ci +'/'+ n1);
                        p.screen[p.r][col] = ' ';
                        p.color[p.r][col] = { fg: null, bg: null };
                     }
                     dirty();
                     sched();
                     result = 'erase '+ n1 +' chars';
                  } else {
                     var col;
                     for (var ci = 0, col = p.ccol; ci < n1 && col <= p.line.length; ci++, col++) {
                        //if (dbg) console.log('erase '+ p.r +','+ col +'|'+ ci +'/'+ n1);
                        p.line[ci] = null;
                     }
                  }
               } else
               
               // delete characters (shift left)
               if (c == 'P') {
                  if (n1 == 0) n1 = 1;
                  if (p.isAltBuf) {
                     var save_col = p.c, src_col, dst_col;
                     for (src_col = p.c + n1, dst_col = p.c; src_col <= p.cols; src_col++, dst_col++) {
                        p.screen[p.r][dst_col] = p.screen[p.r][src_col];
                        p.color [p.r][dst_col] = p.color [p.r][src_col];
                     }
                     for (; dst_col < p.cols; dst_col++) {
                        p.screen[p.r][dst_col] = ' ';
                        p.color [p.r][dst_col] = { fg: null, bg: null };
                     }
                     p.c = save_col;
                     dirty();
                     sched();
                  } else {
                     if (dbg) console.log('DEL-CHARS BEFORE col='+ p.ccol +'/'+ p.line.length +' '+ line_s());
                     for (i = 0; i < n1; i++) {
                        var col_stop = p.line.length;
                        for (j = p.ccol; j < col_stop; j++) p.line[j] = p.line[j+1];
                        p.line[j] = null;
                     }
                     if (dbg) console.log('DEL-CHARS AFTER col='+ p.ccol +'/'+ p.line.length +' '+ line_s());
                  }
                  result = 'del #'+ n1 +' chars';
               } else
               
               // insert characters (shift right)
               if (c == '@') {
                  if (n1 == 0) n1 = 1;
                  if (p.isAltBuf) {
                     var save_col = p.c, src_col, dst_col;
                     for (src_col = p.cols - n1, dst_col = p.cols; src_col >= save_col; src_col--, dst_col--) {
                        p.screen[p.r][dst_col] = p.screen[p.r][src_col];
                        p.color [p.r][dst_col] = p.color [p.r][src_col];
                     }
                     for (; dst_col >= save_col; dst_col--) {
                        p.screen[p.r][dst_col] = ' ';
                        p.color [p.r][dst_col] = { fg: null, bg: null };
                     }
                     p.c = save_col;
                     dirty();
                     sched();
                  } else {
                     if (dbg) console.log('INS-CHARS BEFORE col='+ p.ccol +'/'+ p.line.length +' '+ line_s());
                     for (i = 0; i < n1; i++) {
                        var col_stop = p.line.length;
                        for (j = col_stop; j > p.ccol; j--) p.line[j] = p.line[j-1];
                     }
                     if (dbg) console.log('INS-CHARS AFTER col='+ p.ccol +'/'+ p.line.length +' '+ line_s());
                  }
                  result = 'ins #'+ n1 +' chars';
               } else
               
               // set top and bottom margin, defaults: top = 1, bottom = lines-per-screen
               // AKA: change scroll region
               if (c == 'r' && p.isAltBuf) {
                  if (dbg) console.log('margin set: PREV p.nrows='+ p.nrows +' NEW p.nrows='+ n2);
                  p.margin_top = n1;
                  p.margin_bottom = n2;
                  p.margin_set = true;
                  p.nrows = n2;
                  result = 'set margins, top='+ n1 +', bottom/nrows='+ n2;
               } else
               
               // pan down (text moves up)
               if (c == 'S' && p.isAltBuf) {
                  move_in_display(p.margin_top, p.margin_top + n1, p.margin_bottom, +1);
                  if (p.r < p.nrows) {
                     p.r++;
                     erase_in_display(p.r, p.margin_bottom, 1, p.cols);
                  }
                  p.insertMode = save_insertMode;
                  p.r = p.c = 1; dirty();    // reset cursor
                  result = 'pan down '+ n1;
               } else
               
               // pan up (text moves down)
               if (c == 'T' && p.isAltBuf) {
                  var save_insertMode = p.insertMode;
                  p.insertMode = false;
                  move_in_display(p.margin_bottom, p.margin_bottom - n1, p.margin_top, -1);
                  erase_in_display(p.margin_top, p.margin_top + n1 - 1, 1, p.cols);
                  p.insertMode = save_insertMode;
                  p.r = p.c = 1; dirty();    // reset cursor
                  result = 'pan up '+ n1;
               } else
               
               // delete line(s)
               if (c == 'M' && p.isAltBuf) {
                  var save_insertMode = p.insertMode;
                  p.insertMode = false;
                  move_in_display(p.r, p.r + 1, p.r + n1, +1);
                  erase_in_display(p.r + n1, p.r + n1, 1, p.cols);
                  p.insertMode = save_insertMode;
                  result = 'delete line(s) '+ n1;
               } else
               
               // set lines per page (ignored currently)
               if (c == 't') {
                  result = 'set lines per page '+ n1 +' (ignored)';
               } else

               if (c == 'A') {   // actual
                  if (p.isAltBuf) {
                     if (p.r > 1) { dirty(); p.r--; dirty(); }
                     result = 'arrow up';
                  } else {
                  }
               } else
               if (c == 'B') {   // done via esc[#d
                  if (p.isAltBuf) {
                     if (p.r < p.nrows) { dirty(); p.r++; dirty(); }
                     result = 'arrow down';
                  } else {
                  }
               } else
               if (c == 'C') {
                  if (p.isAltBuf) {
                     if (p.c < p.cols) { dirty(); p.c++; }
                  } else {
                     if (dbg) console.log('ARROW RIGHT col='+ p.ccol +'/'+ p.line.length +
                        ' '+ sq(p.line[p.ccol]) +' '+ line_s());
                     if (p.ccol < p.line.length)
                        p.ccol++;
                  }
                  result = 'arrow right';
               } else
               if (c == 'D') {
                  if (p.isAltBuf) {
                     if (p.c > 1) { dirty(); p.c--; }
                     result = 'arrow left';
                  } else {
                  }
               } else
               
               {
                  result = 2;
               }
            } /* esc [ ... */ else


            // non-CSI handled:
            // ESC   (* ]*
            // ESC   H M c 7 8 > =

            // esc (
		      if (first == '(') {
               result = 'define char set';
               //p.traceEvery = true;
		      } else
		      
		      // ESC ]
            // orig_colors oc=\E]104\007  rs1=\Ec\E]104\007  
            // initc=\E]4;%p1%d;rgb\:%p2%{255}%*%{1000}%/%2.2X/%p3%{255}%*%{1000}%/%2.2X/%p4%{255}%*%{1000}%/%2.2X\E\\
		      if (first == ']') {
               result = 'define colors (ignored)';
		      } else
		      
		      switch (c) {
		      
               case 'H': result = 'set horiz tab (nop)'; break;
            
               case 'M':
                  if (p.isAltBuf) {
                     var save_insertMode = p.insertMode;
                     p.insertMode = false;
                     move_in_display(p.margin_bottom, p.margin_bottom - 1, p.margin_top, -1);
                     erase_in_display(p.margin_top, p.margin_top, 1, p.cols);
                     p.insertMode = save_insertMode;
                     result = 'scroll text down';
                  }
                  break;
            
               // i.e. arrow keys interpreted as numbers (like a numlock)
               // FIXME? nop on mac laptops, but e.g. pc keyboards can have numbers on the arrow keys
               case '>': result = 'set numeric keypad'; break;
            
               // i.e. arrow keys interpreted as cursor movement
               case '=': result = 'keypad application mode'; break;
            
               case '7': result = 'save cursor'; break;
               case '8': result = 'restore cursor'; break;
            
               case 'c': result = 'reset initial state (nop)'; break;
            
               default: result = 2; break;
            }
            
            snew_dump('cmd');
            
            if (error) result += ' UNEXPECTED ====================================================================================';
            error = 0;
            
            if (result === 1) {
                  if (dbg) console.log('> ESC '+ kiwi_JSON(p.esc.s) +' $IGNORED ==========================================================================================');
            } else
            if (result === 2) {
                  if (1 || dbg) console.log('> ESC '+ kiwi_JSON(p.esc.s) +' $UNKNOWN ======================================================================================');
            } else
            if (isString(result)) {
               if (dbg) console.log('> ESC '+ (p.isAltBuf? 'ALT' : 'REG') +' '+ kiwi_JSON(p.esc.s) +' '+ result);
            }
   
            result = null;
            p.esc.state = p.NONE;
         }
		} else   // p.esc.state >= p.ESC
		
		// let UTF-16 surrogates go through (e.g. 0xd83d for benefit of htop)
      //if (dbg) console.log('$every '+ c +'('+ ord(c).toHex(-2) +')');
		if (c >= ' ' || c == '\n' || c == '\t') {
		   var prt = false;
		   
		   if (!p.isAltBuf && c == '<') {
            if (p.inc) {
		         snew_add('<');
               p.line[p.ccol] = '&lt;';
               p.ccol += p.inc;
            }
            prt = true;
		   } else
		   if (!p.isAltBuf && c == '>') {
            if (p.inc) {
		         snew_add('>');
               p.line[p.ccol] = '&gt;';
               p.ccol += p.inc;
            }
            prt = true;
		   } else {
            if (c != '\n') {

               // ordinary chars
               snew_add(c);
               if (p.isAltBuf) {
                  //if (dbg) console.log('ord char: '+ p.r +' '+ p.c +' '+ p.r_cursor +' '+ p.c_cursor);
                  screen_char(c);
               } else {
                  if (p.inc) {
                     p.line[p.ccol] = c;
                     p.ccol++;
                  } else {
                     html_add(c);
                  }
                  prt = true;
               }
            }
            
            var at_EOL = (p.ccol == p.cols);
            if (c == '\n' || (at_EOL && (!p.isAltBuf || (p.isAltBuf && p.eol_wrap)))) {   // newline or wrap
               wasScrolledDown = w3_isScrolledDown(el_scroll);
               if (p.isAltBuf) {
                  p.c = 1; dirty(); p.r++; dirty();
                  if (p.r > p.nrows) p.r = 1;    // \n
               } else {
                  var stmp = line_join(1).s;
                  p.el.innerHTML = (stmp == '')? '&nbsp;' : stmp;
                  if (dbg) console.log('TEXT-EMIT '+ kiwi_JSON(stmp));
                  p.line = [];
                  p.line_sgr = [];
                  p.line_html = [];
                  p.el = appendEmptyLine(parent_el);
                  p.ccol = 1;
               }
            
               if (w3_contains(el_scroll, 'w3-scroll-down') && (!p.scroll_only_at_bottom || (p.scroll_only_at_bottom && wasScrolledDown)))
                  w3_scrollDown(el_scroll);
            }
         }

         //if (dbg && prt) console.log('ORD '+ kiwi_JSON(c) +' col='+ p.ccol +' inc='+ p.inc);
		} else
		
		// don't count HTML escape sequences
		if (!p.isAltBuf && c == kiwi.esc_lt) {
		   snew_add('<');
         html_add('<');
		   p.inc = 0;
		} else
		
		if (!p.isAltBuf && c == kiwi.esc_gt) {
		   snew_add('>');
         html_add('>');
		   p.inc = 1;
		} else
		
		if (p.isAltBuf) {
		   if (dbg) console.log('> 0x7f CHAR '+ ord(c) +' '+ ord(c).toHex(-2));
		}
		
      // ignore any other chars

      if (result) {
         if (dbg) console.log('> '+ result);
      }
	} // for (var si = 0; si < s.length; si++) {

   wasScrolledDown = w3_isScrolledDown(el_scroll);
   if (p.isAltBuf) {
   } else {
      if (dbg) console.log('TEXT-ACCUM col='+ p.ccol +'/'+ p.line.length +' '+ line_s());
      if (p.show_cursor) {
         var first_s = line_join(1, p.ccol).s;
         var second_a = line_join(p.ccol, p.ccol+1).a;
         var third_s = line_join(p.ccol+1).s;
         if (second_a.length) {
            // cursor is on top of a character in the line
            stmp  = first_s;
            stmp += second_a[0].sgr + second_a[0].html + console_cursor(second_a[0].ch);
            stmp += third_s;
         } else {
            // cursor is after the end of the line
            stmp = first_s + console_cursor();
         }
      } else {
         stmp = line_join(1).s;
      }
      p.el.innerHTML = stmp;
      if (dbg) console.log('TEXT-ACCUM col='+ p.ccol +' '+ kiwi_JSON(stmp));
   }

   //if (dbg) console.log('wasScrolledDown='+ wasScrolledDown);
	if (w3_contains(el_scroll, 'w3-scroll-down') &&
	   (!p.scroll_only_at_bottom || (p.scroll_only_at_bottom && wasScrolledDown) || p.must_scroll_down)) {
      w3_scrollDown(el_scroll);
      p.must_scroll_down = false;
      //if (dbg) console.log('w3_scrollDown()');
   }

   if (p.isAltBuf) {
      //if (dbg) { console.log(p); kiwi_trace(); }
      sched();
   }
   snew_dump('end');
}


////////////////////////////////
// status
////////////////////////////////

function gps_stats_cb(acquiring, tracking, good, fixes, adc_clock, adc_gps_clk_corrections)
{
   var s = (acquiring? 'yes':'pause') +', track '+ tracking +', good '+ good +', fixes '+ fixes.toUnits();
	w3_innerHTML('id-msg-gps', 'GPS: acquire '+ s);
	w3_innerHTML('id-status-gps',
	   w3_text('w3-text-css-orange', 'GPS'),
	   w3_text('', 'acq '+ s)
	);
	extint.adc_clock_Hz = adc_clock * 1e6;
	extint.adc_gps_clock_corr = adc_gps_clk_corrections;
	if (adc_gps_clk_corrections) {
	   s = adc_clock.toFixed(6) +' ('+ adc_gps_clk_corrections.toUnits() +' avgs)';
	   var el = w3_el('id-msg-gps');
	   if (el) el.innerHTML += ', ADC clock '+ s;
		w3_innerHTML('id-status-adc',
	      w3_text('w3-text-css-orange', 'ADC clock '),
	      w3_text('', s)
		);
	}
}

function kiwi_too_busy(rx_chans)
{
	var s = 'Sorry, the KiwiSDR server is too busy right now ('+ rx_chans +' users max). <br>' +
	'There is also a limit on the total number of channel queuers and campers. <br>' +
	'Please check <a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> for more KiwiSDR receivers available world-wide.';
	kiwi_show_msg(s);
}

function kiwi_wb_only()
{
	var s = 'Sorry, this KiwiSDR is configured for non-web, wideband connections only. <br>' +
	'See the Kiwi forum for details about wideband mode. <br>' +
	'Please check <a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> for more KiwiSDR receivers available world-wide.';
	kiwi_show_msg(s);
}

function kiwi_exclusive_use()
{
	var s = 'Sorry, this Kiwi has been locked for special use. <br>' +
	'This happens when using an extension (e.g. DRM decoder) that requires all available resources. <br>' +
	'Please check <a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> for more KiwiSDR receivers available world-wide. <br><br>' +
	'申し訳ありませんが、このキーウィは特別な使用のためにロックされています。 <br>' +
	'これは、利用可能なすべてのリソースを必要とする拡張機能（DRM デコーダーなど）を使用している場合に発生します。 <br>' +
	'世界中で利用できる KiwiSDR レシーバーについては、<a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> を確認してください。';
	kiwi_show_msg(s);
}

function kiwi_ip_limit_pwd_cb(pwd)
{
   console.log('kiwi_ip_limit_pwd_cb pwd='+ pwd);
	kiwi_storeWrite('iplimit', encodeURIComponent(pwd));
   window.location.reload();
}

function kiwi_show_error_ask_exemption_cb(path, val, first)
{
	//console.log('kiwi_show_error_ask_exemption_cb: path='+ path +' '+ typeof(val) +' "'+ val +'" first='+ first);
   kiwi_ip_limit_pwd_cb(val);
}

function kiwi_show_error_ask_exemption(s)
{
   s += '<br><br>If you have an exemption password from the KiwiSDR owner/admin <br> please enter it here: ' +
      w3_input('w3-margin-TB-8/w3-label-inline w3-label-not-bold/kiwi-pw|padding:1px|size=40',
         'Password:', 'id-epwd', '', 'kiwi_show_error_ask_exemption_cb');
	kiwi_show_msg(s);
	w3_field_select('id-epwd', {mobile:1});
}

function kiwi_inactivity_timeout(mins)
{
   var s = 'Sorry, this KiwiSDR has an inactivity timeout after '+ mins +' minutes.<br>Reload the page to continue.';
	kiwi_show_msg(s);
}

function kiwi_24hr_ip_limit(mins, ip)
{
	var s = 'Sorry, this KiwiSDR can only be used for '+ mins +' minutes every 24 hours by each IP address.<br>' +
      //'Your IP address is: '+ ip +'<br>' +
      'Please check <a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> for more KiwiSDR receivers available world-wide.';
	
	kiwi_show_error_ask_exemption(s);
}

function kiwi_password_entry_timeout()
{
   var s = 'Timeout. Please reload page to continue.';
	kiwi_show_msg(s);
}

function kiwi_up(up)
{
	if (!seriousError) {
      w3_hide('id-kiwi-msg-container');
      w3_show_block('id-kiwi-container');
      w3_el('id-kiwi-body').style.overflow = 'hidden';
	}
}

function kiwi_down(type, reason)
{
	var s;
	type = +type;

	if (type == 1) {
		s = 'Sorry, software update in progress. Please check back in a few minutes.<br>' +
			'Or check <a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> for more KiwiSDR receivers available world-wide.';
	} else
	if (type == 2) {
		s = "Backup in progress.";
	} else {
		if (reason == null || reason == '') {
			reason = 'Sorry, this KiwiSDR server is being used for development right now. <br>' +
				'Please check <a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> for more KiwiSDR receivers available world-wide.';
		}
		s = reason;
	}
	
	kiwi_show_msg(s);
}

// called from both admin and user connections
function stats_init()
{
   msg_send('SET GET_CONFIG');
	stats_update();
}

function stats_update()
{
   //console.log('SET STATS_UPD ch='+ rx_chan);
	msg_send('SET STATS_UPD ch='+ rx_chan);
	
   // align requesting stats until stats interval
	var now = new Date();
	var aligned_interval = Math.ceil(now/kiwi.stats_interval) * kiwi.stats_interval - now;
	if (aligned_interval < kiwi.stats_interval/2) aligned_interval += kiwi.stats_interval;
	//console.log('STATS_UPD aligned_interval='+ aligned_interval);
	setTimeout(stats_update, aligned_interval);
}

function status_periodic()
{
	//console.log('status_periodic');
	w3_innerHTML('id-status-stats-cpu', kiwi_cpu_stats_str);
	w3_innerHTML('id-status-stats-xfer', kiwi_xfer_stats_str);
	w3_innerHTML('id-msg-stats-cpu', kiwi_cpu_stats_str_long);
	w3_innerHTML('id-msg-stats-xfer', kiwi_xfer_stats_str_long);
}

var kiwi_xfer_stats_str = "";
var kiwi_xfer_stats_str_long = "";

function xfer_stats_cb(audio_kbps, waterfall_kbps, waterfall_fps, http_kbps, sum_kbps)
{
	kiwi_xfer_stats_str =
	   w3_text('w3-text-css-orange', 'Net') +
	   w3_text('', 'aud '+ audio_kbps.toFixed(0) +', wf '+ waterfall_kbps.toFixed(0) +', http '+
		http_kbps.toFixed(0) +', total '+ sum_kbps.toFixed(0) +' kB/s');

	kiwi_xfer_stats_str_long = 'Network (all channels): audio '+ audio_kbps.toFixed(0) +' kB/s, waterfall '+ waterfall_kbps.toFixed(0) +
		' kB/s ('+ waterfall_fps.toFixed(0) +' fps)' +
		', http '+ http_kbps.toFixed(0) +' kB/s, total '+ sum_kbps.toFixed(0) +' kB/s ('+ (sum_kbps*8).toFixed(0) +' kb/s)';
}

var kiwi_cpu_stats_str = '';
var kiwi_cpu_stats_str_long = '';
var kiwi_config_str = '';
var kiwi_config_str_long = '';

function cpu_stats_cb(o, uptime_secs, ecpu, waterfall_fps)
{
   idle %= 100;   // handle multi-core cpus
   var cputempC = o.cc? o.cc : 0;
   var cputempF = cputempC * 9/5 + 32;
   var temp_color = o.cc? ((o.cc >= 60)? 'w3-text-css-red w3-bold' : ((o.cc >= 50)? 'w3-text-css-yellow' : 'w3-text-css-lime')) : '';
   var cputemp = cputempC? (cputempC.toFixed(0) +'&deg;C '+ cputempF.toFixed(0) +'&deg;F ') : '';
   var cpufreq = (o.cf >= 1000)? ((o.cf/1000).toFixed(1) +' GHz') : (o.cf.toFixed(0) +' MHz');
   var platform = kiwi.platform_s[kiwi.platform];
   
	kiwi_cpu_stats_str =
	   w3_text('w3-text-css-orange', platform +' ') +
	   w3_text('', o.cu[0] +','+ o.cs[0] +','+ o.ci[0] +' usi% ') +
	   (cputempC? w3_text(temp_color, cputemp) :'') +
	   w3_text('', cpufreq +' ') +
	   w3_text('w3-text-css-orange', 'eCPU') +
	   w3_text('', ecpu.toFixed(0) +'%');
	kiwi.wf_fps = waterfall_fps;

   var user = '', sys = '', idle = '';
   var first = true;
   for (var i = 0; i < o.cu.length; i++) {
      user += (first? '':' ') + o.cu[i] +'%';
      sys  += (first? '':' ') + o.cs[i] +'%';
      idle += (first? '':' ') + o.ci[i] +'%';
      first = false;
   }
   var cpus = 'cpu';
   if (o.cu.length > 1) {
      cpus += '0';
		for (var i = 1; i < o.cu.length; i++)
		   cpus += ' cpu' + i;
   }
	kiwi_cpu_stats_str_long =
	   w3_inline('',
         w3_text('w3-text-black', platform +': '+ cpus +' '+ user +' usr | '+ sys +' sys | '+ idle +' idle,' + (cputempC? '':' ')) +
         (cputempC? ('&nbsp;'+ w3_text(temp_color +' w3-text-outline w3-large', cputemp) +'&nbsp;') :'') +
         w3_text('w3-text-black', cpufreq + ', ') +
         w3_text('w3-text-black', 'FPGA eCPU: '+ ecpu.toFixed(0) +'%')
      );

	var o = kiwi_dhms(uptime_secs);
	w3_innerHTML('id-status-config',
	   w3_inline('',
         w3_text('w3-text-css-orange', 'KiwiSDR '+ kiwi.model),
         w3_text('', ' up '+ o.dhms +', '+ kiwi_config_str)
      )
	);

	s = ' | Uptime: ';
	if (o.days) s += o.days +' '+ ((o.days > 1)? 'days':'day') +' ';
	s += o.hms;

	var noLatLon = (server_time_local == '' || server_time_tzname == 'null');
	if (server_time_utc) s += ' | UTC: '+ server_time_utc;
	if (isDefined(server_time_tzname)) {
	   s += ' | Local: ';
      if (!noLatLon) s += server_time_local +' ';
      s += noLatLon? 'Lat/lon needed for local time' : server_tz;
   }

	w3_innerHTML('id-msg-config', kiwi_config_str_long + s);
}

function config_str_update(rx_chans, gps_chans, vmaj, vmin)
{
	kiwi_config_str = 'v'+ vmaj +'.'+ vmin +', ch: '+ rx_chans +' SDR '+ gps_chans +' GPS';
	w3_innerHTML('id-status-config', kiwi_config_str);
	kiwi_config_str_long = 'KiwiSDR '+ kiwi.model +', v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR channels, '+ gps_chans +' GPS channels';
	w3_innerHTML('id-msg-config', kiwi_config_str);
}

var config_net = {};

function config_cb(rx_chans, gps_chans, serno, pub, port_ext, pvt, port_int, nm, mac, vmaj, vmin, dmaj, dmin)
{
	var s;
	config_str_update(rx_chans, gps_chans, vmaj, vmin);
	w3_innerHTML('id-msg-debian', 'Debian '+ dmaj +'.'+ dmin);
	kiwi.debian_maj = dmaj;
	kiwi.debian_min = dmin;

	var net_config = w3_el("id-net-config");
	if (net_config) {
		net_config.innerHTML =
			w3_div('',
				w3_col_percent('',
					w3_div('', 'Public IP address (outside your firewall/router): '+ pub +' [port '+ port_ext +']'), 50,
					w3_div('', 'Ethernet MAC address: '+ mac.toUpperCase()), 30,
					w3_div('', 'Model: KiwiSDR '+ kiwi.model), 20
				),
				w3_col_percent('',
					w3_div('', 'Private IP address (inside your firewall/router): '+ pvt +' [port '+ port_int +']'), 50,
					w3_div('', 'Private netmask: /'+ nm), 30,
					w3_div('', 'Serial number: '+ serno), 20
				)
			);
		config_net.pub_ip = pub;
		config_net.pub_port = port_ext;
		config_net.pvt_ip = pub;
		config_net.pvt_port = port_int;
		config_net.mac = mac;
		config_net.serno = serno;
		
		w3_call('connect_update_url');
	}
}

function update_cb(fail_reason, pending, in_progress, rx_chans, gps_chans, vmaj, vmin, pmaj, pmin, build_date, build_time)
{
	config_str_update(rx_chans, gps_chans, vmaj, vmin);

	var msg_update = w3_el("id-msg-update");
	
	if (msg_update) {
		var s;
		s = 'Installed version: v'+ vmaj +'.'+ vmin +', built '+ build_date +' '+ build_time;
		if (fail_reason) {
		   var r;
		   switch (fail_reason) {
			   case 1: r = 'Filesystem is FULL!'; break;
			   case 2: r = 'No Internet connection? (can\'t ping 1.1.1.1 or 8.8.8.8)'; break;
			   case 3: r = 'No connection to github.com?'; break;
			   case 4: r = 'Git clone damaged!'; break;
			   case 5: r = 'Makefile update failed -- check /root/build.log file'; break;
			   case 6: r = 'Build failed, check /root/build.log file'; break;
			   case 7: r = 'Parse of Makefile.1 failed!'; break;
			   default: r = 'Unknown reason, code='+ fail_reason; break;
			}
			s += '<br>'+ r;

         // remove restart/reboot banners from "build now" button
	      w3_hide('id-build-restart');
	      w3_hide('id-build-reboot');
		} else
		if (in_progress) {
			s += '<br>Update to version v'+ pmaj +'.'+ pmin +' in progress';
		} else
		if (pending) {
			s += '<br>Update check pending';
		} else
		if (pmaj == -1) {
			s += '<br>Error determining the latest version -- check log';
		} else {
			if (vmaj == pmaj && vmin == pmin) {
				s += '<br>Running the most current version';
			} else {
				s += '<br>Available version: v'+ pmaj +'.'+ pmin;
				if (adm.update_major_only && pmaj <= vmaj)
				   s += ' (but major version number unchanged)';
			}
		}
		msg_update.innerHTML = s;
	}
}


////////////////////////////////
// user list
////////////////////////////////

var users_interval = 2500;
var user_init = false;

function users_init(called_from)
{
	kiwi.called_from_admin = called_from.admin;
	kiwi.called_from_user = called_from.user;
	kiwi.called_from_monitor = called_from.monitor;

   if (kiwi.called_from_admin || kiwi.called_from_monitor) {
      var id_prefix = kiwi.called_from_admin? 'id-admin-user-' : 'id-monitor-user-';
      var pad = kiwi.called_from_monitor? ' w3-padding-LR-2' : '';
      var s1 = '', s2;
   
      for (var i=0; i < rx_chans; i++) {
         if (kiwi.called_from_admin) {
            s1 = w3_button('id-user-kick-'+ i +' w3-hide w3-small w3-white w3-border w3-border-red w3-round-large w3-padding-0 w3-padding-LR-8',
               'Kick', 'status_user_kick_cb', i);
            /*
            s1 += w3_button('id-user-bl32-'+ i +' w3-hide w3-margin-L-8 w3-small w3-white w3-border w3-border-red w3-round-large w3-padding-0 w3-padding-LR-8',
               'IP blacklist /32', 'network_user_blacklist_cb', i);
            s1 += w3_button('id-user-bl24-'+ i +' w3-hide w3-margin-L-8 w3-small w3-white w3-border w3-border-red w3-round-large w3-padding-0 w3-padding-LR-8',
               'IP blacklist /24', 'network_user_blacklist_cb', i+100);
            */
         }
         s2 = w3_div('id-campers-'+ i +' w3-css-orange w3-padding-LR-4');
         w3_el('id-users-list').innerHTML += w3_inline('/w3-hspace-8', w3_div('id-user-'+ i + pad, 'RX'+ i), w3_div(id_prefix + i), s1, s2);
      }
   }
	
	users_update();
	w3_call('users_setup');
	user_init = true;
}

function users_update()
{
	//console.log('users_update');
	msg_send('SET GET_USERS');
	setTimeout(users_update, users_interval);
}

function user_cb(obj)
{
	var id_prefix = kiwi.called_from_admin? 'id-admin-user-' : 'id-monitor-user-';
	var host = kiwi_url_origin();

	obj.forEach(function(obj) {
		//console.log(obj);
		var s1 = '', s2 = '', s3 = '';
		var i = obj.i;
		var name = obj.n;
		var freq = obj.f;
		var geoloc = obj.g;
		var ip = (isDefined(obj.a) && obj.a != '')? (obj.a +', ') : '';
		var mode = obj.m;
		var zoom = obj.z;
		var connected = obj.t;
		var remaining = '';
		if (obj.rt) {
		   var t = (obj.rt == 1)? ' act' : ' 24h';
		   remaining = ' '+ w3_text('w3-text-css-orange|vertical-align:bottom', obj.rs + t);
		}
		var ext = obj.e;
		
		if (isDefined(name)) {
		
		   // Server imposes a hard limit on name length. But this might result in a Unicode
		   // sequence at the end of a max length name that is truncated causing
		   // decodeURIComponent() to fail. It's difficult to fix this on the server where there is no
		   // decodeURIComponent() equivalent. So do it here in an iterative way.
		   var okay = false;
		   var deco;
		   do {
            try {
               deco = decodeURIComponent(name);
               okay = true;
            } catch(ex) {
               name = name.slice(0, -1);
               //console.log('try <'+ name +'>');
            }
         } while (!okay);
		   
			var id = kiwi_strip_tags(deco, '');
			if (!kiwi.called_from_admin && id == '') id = '(no identity)';
			if (id != '') id = '"'+ id + '"';
			var g = (geoloc == '(null)' || geoloc == '')?
			      (kiwi.called_from_admin? 'unknown location' : '')
			   :
			      decodeURIComponent(geoloc);
			ip = ip.replace(/::ffff:/, '');		// remove IPv4-mapped IPv6 if any
			g = (ip != '' || g != '')? ('('+ ip + g +')') : '';
			var f = freq + kiwi.freq_offset_Hz;
			var f = (f/1000).toFixed((f > 100e6)? 1:2);
			var f_s = f + ' kHz ';
			var fo = (freq/1000).toFixed(2);

			var link, target;
		   if (kiwi.called_from_admin) {
			   link = host +'/?f='+ fo + mode +'z'+ zoom;
			   target = ' target="_blank"';
			} else {
			   link = 'javascript:'+ (kiwi.called_from_user? ('tune('+ fo +','+ sq(mode) +','+ zoom +')') : ('camp('+ i +')'));
			   target = '';
			}

			if (ext != '') ext = decodeURIComponent(ext) +' ';
			s1 = w3_sb(id, g) +' ';
			s2 = w3_link('w3-link-darker-color', link, f_s + (obj.wf? 'WF' : mode) +' z'+ zoom) +' '+ ext + connected + remaining;
		}
		
		//if (s1 != '') console.log('user'+ i +'='+ s1 + s2);
		if (user_init) {

		   if (kiwi.called_from_user) {
            w3_innerHTML('id-optbar-user-'+ i, (s1 != '')? (s1 +'<br>'+ s2) : '');
         } else {
         
		      // status display used by admin & monitor page
            w3_innerHTML(id_prefix + i, s1 + s2 + s3);
            w3_hide2('id-user-kick-'+ i, s1 == '');
            w3_hide2('id-user-bl32-'+ i, s1 == '');
            w3_hide2('id-user-bl24-'+ i, s1 == '');
         }
		}
		
		if (i == rx_chan && isDefined(obj.c)) {
		   //console.log('SAM carrier '+ obj.c);
		   var el = w3_el('id-sam-carrier');
		   if (el) w3_innerHTML(el, 'carrier '+ obj.c.toFixed(1) +' Hz');
		}
		
      w3_innerHTML('id-campers-'+ i, obj.ca? (obj.ca + plural(obj.ca, ' camper')) : '');

		// inactivity timeout warning panel
		if (i == rx_chan && obj.rn) {
		   if (obj.rn <= 55 && !kiwi.inactivity_panel) {
            var s =
               (obj.rt == 1)?
                  'Inactivity timeout in one minute.<br>Close this panel to avoid disconnection.'
               :
                  'Per 24-hour connection timeout in one minute.';
            confirmation_show_content(s, 360, 55,
               function() {
                  msg_send('SET inactivity_ack');
                  confirmation_panel_close();
                  kiwi.inactivity_panel = false;
               },
               'red'
            );
            kiwi.inactivity_panel = true;
         }
		}
		
		// another action like a frequency change resets timer
      if (i == rx_chan && obj.rn > 55 && kiwi.inactivity_panel) {
         confirmation_panel_close();
         kiwi.inactivity_panel = false;
      }
      
      // detect change in frequency scale offset
      //if (i == rx_chan) console.log('$obj.fo='+ obj.fo +' freq.offset_kHz='+ kiwi.freq_offset_kHz);
      if (isNumber(obj.fo) && obj.fo != kiwi.freq_offset_kHz) {
         if (kiwi.called_from_admin || kiwi.called_from_monitor) {
            console.log('$$$ ADMIN kiwi_set_freq_offset '+ obj.fo);
            kiwi_set_freq_offset(obj.fo);
         } else
         if (i == rx_chan && !confirmation.displayed) {
            ext_send('SET msg_log='+ encodeURIComponent('frequency scale offset changed '+ kiwi.freq_offset_kHz +' to '+ obj.fo +', page reloading'));
            var s =
               w3_div('',
                  'Frequency scale offset changed. Page will be reloaded.'
                  //w3_inline('w3-halign-space-around/', w3_button('w3-margin-T-16 w3-aqua', 'OK', 'freq_offset_page_reload'))
               );
            confirmation_show_content(s, 425, 50, null, 'red');
            //console.log("kiwi_open_or_reload_page({ url:'reload', delay:2000 })");
            kiwi_open_or_reload_page({ url:'reload', delay:2000 });
         }
      }

      //if (i == rx_chan && isNumber(obj.nc) && obj.nc != rx_chan && isNumber(obj.ns) && obj.ns != kiwi.notify_seq) {
      if (i == rx_chan && isNumber(obj.nc) && obj.nc != rx_chan) {
         //console.log('NOTIFY nc|rx_chan='+ obj.nc +'|'+ rx_chan +' seq='+ obj.ns +'|'+ kiwi.notify_seq);
         if (isNumber(obj.ns) && obj.ns != kiwi.notify_seq) {
         //console.log('NOTIFY sn='+ obj.ns);
		   msg_send('SET notify_msg');
         kiwi.notify_seq = obj.ns;
         }
      }
	});
	
}

//function freq_offset_page_reload() { window.location.reload(); }


////////////////////////////////
// misc
////////////////////////////////

var toggle_e = {
   // zero implies toggle
	SET: 1,
	SET_URL: 2,
	FROM_COOKIE: 4,
	WRITE_COOKIE: 8,
	NO_CLOSE_EXT: 16,
	NO_WRITE_COOKIE: 32
};

// return value depending on flags: cookie value, set value, default value, no change
function kiwi_toggle(flags, val_set, val_default, cookie_id)
{
	var rv = null;

   // a positive set from URL overrides cookie value
	if (flags & toggle_e.SET_URL && (val_set != null || val_set != undefined)) {
      rv = val_set;
      //console.log('kiwi_toggle SET_URL '+ cookie_id +'='+ rv);
	} else
	
	if (flags & toggle_e.FROM_COOKIE) {
		rv = kiwi_storeRead(cookie_id);
		if (rv != null) {
		   // backward compatibility: interpret as number
		   // FIXME: fails if string value looks like a number
	      var rv_num = parseFloat(rv);
	      if (!isNaN(rv_num)) rv = rv_num;
			//console.log('kiwi_toggle FROM_COOKIE '+ cookie_id +'='+ rv);
		}
	}

	if (rv == null) {
		if (flags & toggle_e.SET) {
			rv = val_set;
			//console.log('kiwi_toggle SET '+ cookie_id +'='+ rv);
		}
	}
	
	if (rv == null) {
	   rv = val_default;
			//console.log('kiwi_toggle DEFAULT '+ cookie_id +'='+ rv);
	}
	
	
   //console.log('kiwi_toggle RV '+ cookie_id +'='+ rv);
	return rv;
}

function kiwi_fft_mode()
{
	if (0) {
		toggle_or_set_spec(toggle_e.SET, spec.RF);
		setmaxdb(10);
	} else {
		setmaxdb(-30);
	}
}

function kiwi_mapPinSymbol(fillColor, strokeColor) {
   fillColor = fillColor || 'red';
   strokeColor = strokeColor || 'white';
   return {
      path: 'M 0,0 C -2,-20 -10,-22 -10,-30 A 10,10 0 1,1 10,-30 C 10,-22 2,-20 0,0 z',
      fillColor: fillColor,
      fillOpacity: 1,
      strokeColor: strokeColor,
      strokeWeight: 1,
      scale: 1,
   };
}

// not the same as ext_isAdmin() which asks the server for current admin status confirmation
function isAdmin()
{
   return (Object.keys(adm).length != 0);
}

function kiwi_force_admin_close_cb(path, val, first)
{
   if (first) return;
   val = +val;
   //console.log('$$$$ kiwi_force_admin_close_cb val='+ val);
   ext_send('SET close_admin_force');
   confirmation_panel_close();
   kiwi.no_admin_conns_pend = 0;
   
   // complete the original operation
   if (val) {
      // give console time to close before retrying
      setTimeout(function() { dx_send_update_retry(); }, 1000);
      //console.log('$$$$ kiwi_force_admin_close_cb: COMPLETE ORIGINAL dx_send_update_retry');
   } else {
      // give console time to close before retrying
      setTimeout(function() { cfg_save_json('kiwi_force_admin_close_cb', 'adm'); }, 1000);
      //console.log('$$$$ kiwi_force_admin_close_cb: COMPLETE ORIGINAL cfg_save_json(adm)');
   }
}

function kiwi_set_freq_offset(freq_offset_kHz)
{
   kiwi.isOffset = (freq_offset_kHz != 0);
   kiwi.freq_offset_kHz = freq_offset_kHz;
   kiwi.freq_offset_Hz  = freq_offset_kHz * 1000;
   kiwi.offset_frac = (freq_offset_kHz % 1000) * 1000;
}

function kiwi_init_cfg(stream_name)
{
   // in user and admin html both
   var page_title = ext_get_cfg_param_string('cfg.index_html_params.PAGE_TITLE');
   if (page_title == '') page_title = 'KiwiSDR';
   w3_innerHTML('id-page-title', ((stream_name == 'admin')? 'Admin ':'') + page_title);

   w3_innerHTML('id-rx-photo-title', w3_json_to_html('kiwi_init_cfg', ext_get_cfg_param_string('cfg.index_html_params.RX_PHOTO_TITLE')));
   w3_innerHTML('id-rx-photo-desc', w3_json_to_html('kiwi_init_cfg', ext_get_cfg_param_string('cfg.index_html_params.RX_PHOTO_DESC')));
   w3_innerHTML('id-rx-title', w3_json_to_html('kiwi_init_cfg', ext_get_cfg_param_string('cfg.index_html_params.RX_TITLE')));
   w3_innerHTML('id-owner-info', w3_json_to_html('kiwi_init_cfg', ext_get_cfg_param_string('cfg.owner_info')));
}

// called by both user and admin
function kiwi_snr_stats(all, hf)
{
   //console.log('SNR ', all, ':', hf, ' dB ');
   if (all == -1 && hf == -1) {
      w3_innerHTML('id-msg-snr-now', 'ERROR: antenna is grounded');
   } else {
      if (hf == -1) {
         // only show single SNR when transverter frequency offset
         //w3_innerHTML('id-rx-snr', ', SNR: All ', all, ' dB');
         w3_innerHTML('id-rx-snr', ', SNR ', all, ' dB');
   
         w3_els('id-msg-snr', function(el) { w3_innerHTML(el, 'SNR: All ', all, ' dB'); });
      } else {
         //w3_innerHTML('id-rx-snr', ', SNR: All ', all, ' dB, HF ', hf, ' dB');
         w3_innerHTML('id-rx-snr', ', SNR ', all, '/', hf, ' dB');
         w3_title('id-rx-snr', all +' dB, 0-30 MHz\n'+ hf +' dB, 1.8-30 MHz');
   
         w3_els('id-msg-snr', function(el) { w3_innerHTML(el, 'SNR: ', all, ' dB (0-30 MHz), ', hf, ' dB (1.8-30 MHz)'); });
      }
   }
   
   if (!isAdmin())
      w3_call('position_top_bar');
}


////////////////////////////////
// control messages
////////////////////////////////

var reason_disabled = '';
var version_maj = -1, version_min = -1, debian_ver = -1;
var tflags = { INACTIVITY:1, WF_SM_CAL:2, WF_SM_CAL2:4 };
var chan_no_pwd, chan_no_pwd_true;
var kiwi_output_msg_p = { scroll_only_at_bottom: true, inline_returns: true, process_return_alone: false, remove_returns: false };
var client_public_ip;

// includes msgs relevant for both user and admin modes
function kiwi_msg(param, ws)     // #msg-proc #MSG
{
	var rtn = true;
	//if (param[0] != 'stats_cb' && param[0] != 'user_cb')
   //   console.log('$$ kiwi_msg    '+ (ws? ws.stream : '[ws?]') +' MSG '+ param[0] +'='+ param[1]);
	
	switch (param[0]) {
		case "version_maj":
			version_maj = parseInt(param[1]);
			break;
			
		case "version_min":
			version_min = parseInt(param[1]);
			break;

		case "debian_ver":
			debian_ver = parseInt(param[1]);
			break;

		case "model":
			kiwi.model = parseInt(param[1]);
			break;

		case "platform":
			kiwi.platform = parseInt(param[1]);
			break;

		case "ext_clk":
			kiwi.ext_clk = parseInt(param[1]);
			break;

		case "abyy":
         dx.db_s[dx.DB_EiBi] = 'EiBi-'+ param[1] +' (read-only)';    // abyy value sent by server
			break;

		case "client_public_ip":
			client_public_ip = param[1].replace(/::ffff:/, '');    // remove IPv4-mapped IPv6 if any
			console.log('client public IP: '+ client_public_ip);
			break;

		case "badp":
			//console.log('badp='+ param[1]);
			extint_valpwd_cb(parseInt(param[1]));
			break;

		case "chan_no_pwd":
			chan_no_pwd = parseInt(param[1]);
			break;

		case "chan_no_pwd_true":
			chan_no_pwd_true = parseInt(param[1]);
			break;

		case "rx_chans":
			rx_chans = parseInt(param[1]);
			break;

		case "wf_chans":
			wf_chans = parseInt(param[1]);
			break;

		case "wf_chans_real":
			wf_chans_real = parseInt(param[1]);
			break;

		case "rx_chan":
			rx_chan = parseInt(param[1]);
			//console.log('rx_chan='+ rx_chan);
			break;

		case "max_camp":
			max_camp = parseInt(param[1]);
			break;

      // Don't need kiwi_decodeURIComponent() here because the server-side is encoding before sending.
      // It's only when the decoded cleartext cfg params contain invalid UTF-8 that we get into trouble.
      // E.g. ext_get_cfg_param_string(): kiwi_decodeURIComponent(ext_get_cfg_param(...))
		case "load_cfg":
			var cfg_json = decodeURIComponent(param[1]);
			console.log('### load_cfg '+ ws.stream +' '+ cfg_json.length);

         // introduce delayed async cfg load to test initialization locking
         //setTimeout(function() {
            //console.log('### DELAYED load_cfg '+ ws.stream +' '+ cfg_json.length);
            cfg = kiwi_JSON_parse('load_cfg', cfg_json);
            kiwi_init_cfg(ws.stream);
         //}, 2000);
			break;

		case "load_dxcfg":
			var dxcfg_json = decodeURIComponent(param[1]);
			console.log('### load_dxcfg '+ ws.stream +' '+ dxcfg_json.length);
         dxcfg = kiwi_JSON_parse('load_dxcfg', dxcfg_json,
            function(ex) {
               dx.dxcfg_parse_error = ex;
            }
         );
         
         if (dx.dxcfg_parse_error) {
            // null configuration so user page continues to work
            dxcfg = {};
            dxcfg.dx_type = [];
            for (var i = 0; i < 16; i++) {
               dxcfg.dx_type.push({key:i, name:'type-'+i, color:'white'});
            }
            dxcfg.band_svc = [];
            dxcfg.bands = [];
            console.log(dxcfg);
         }
			break;

		case "load_dxcomm_cfg":
			var dxcomm_cfg_json = decodeURIComponent(param[1]);
			console.log('### load_dxcomm_cfg '+ ws.stream +' '+ dxcomm_cfg_json.length);
         dxcomm_cfg = kiwi_JSON_parse('load_dxcomm_cfg', dxcomm_cfg_json,
            function(ex) {
               dx.dxcomm_cfg_parse_error = ex;
            }
         );
         
         if (dx.dxcomm_cfg_parse_error) {
            // null configuration so user page continues to work
            dxcomm_cfg = {};
            dxcomm_cfg.dx_type = [];
            for (var i = 0; i < 16; i++) {
               dxcomm_cfg.dx_type.push({key:i, name:'type-'+i, color:'white'});
            }
            dxcomm_cfg.band_svc = [];
            dxcomm_cfg.bands = [];
            console.log(dxcomm_cfg);
         }
			break;

		case "load_adm":
			var adm_json = decodeURIComponent(param[1]);
			console.log('### load_adm '+ ws.stream +' '+ adm_json.length);
			adm = kiwi_JSON_parse('load_adm', adm_json);
			break;
		
		case "cfg_loaded":
			console.log('### cfg_loaded');
			if (!kiwi.init_cfg) {
            owrx_init_cfg();        // do only once
            kiwi.init_cfg = true;
         }
			break;
		
		case "no_admin_conns":
			var retry_dx_update = parseInt(param[1]);
		   kiwi.no_admin_conns_pend++;
		   //console.log('$$$$ no_admin_conns '+ kiwi.no_admin_conns_pend +' retry_dx_update='+ retry_dx_update);
		   if (kiwi.no_admin_conns_pend == 1) {
            //console.log('$$$$ confirmation_show_content');
            confirmation_panel_close();
            confirmation_show_content(
               'Must close admin connection before attempting this operation.' +
               w3_button('w3-small w3-padding-smaller w3-yellow w3-margin-T-8',
                  'Close admin connection and complete original operation',
                  'kiwi_force_admin_close_cb', retry_dx_update),
               500, 75,
               function() {
                  // NB: this is the panel close icon at upper right, not the w3_button() click cb above
                  //console.log('$$$$ confirmation_panel_close');
                  confirmation_panel_close();
                  kiwi.no_admin_conns_pend = 0;
               },
               'red');
         }
         break;
      
		case "foff_error":
		   kiwi.foff_error_pend++;
		   if (kiwi.foff_error_pend == 1) {
		      setTimeout(function() {
               confirmation_panel_close();
               confirmation_show_content(
                  (+param[1] == 0)?
                     '"foff=" URL parameter available from local connections only.'
                  :
                     'Must close admin connection before using "foff=" URL parameter.',
                  500, 55,
                  function() {
                     confirmation_panel_close();
                     kiwi.foff_error_pend = 0;
                  },
                  'red');
            }, 5000);
         }
         break;
      
		case "request_dx_update":
		   if (isAdmin()) {
		      // NB: tabbing between fields won't work if field select undone by list re-render
		      if (dx.ignore_dx_update) {
		         //console.log('request_dx_update: ignore_dx_update');
		         dx.ignore_dx_update = false;
		      } else {
			      dx_update_admin();
			   }
		   } else {
		      dx_update_request();
			}
			break;

		case "mkr":
			var mkr = param[1];
			//console.log('MKR '+ mkr);
			var obj = kiwi_JSON_parse('mkr', mkr);
			if (obj) dx_label_render_cb(obj);
			break;

		case "user_cb":
			//console.log('user_cb='+ param[1]);
			var obj = kiwi_JSON_parse('user_cb', param[1]);
			if (obj) user_cb(obj);
			break;

		case "config_cb":    // in response to "SET GET_CONFIG"
			//console.log('config_cb='+ param[1]);
			var o = kiwi_JSON_parse('config_cb', param[1]);
			if (o) config_cb(o.r, o.g, o.s, o.pu, o.pe, o.pv, o.pi, o.n, o.m, o.v1, o.v2, o.d1, o.d2);
			break;

		case "update_cb":
			//console.log('update_cb='+ param[1]);
			var o = kiwi_JSON_parse('update_cb', param[1]);
			if (o) update_cb(o.f, o.p, o.i, o.r, o.g, o.v1, o.v2, o.p1, o.p2,
				decodeURIComponent(o.d), decodeURIComponent(o.t));
			break;

		case "stats_cb":     // in response to "SET STATS_UPD" (from both user and admin)
			//console.log('stats_cb='+ param[1]);
			var o = kiwi_JSON_parse('stats_cb', param[1]);
			if (o) {
				//console.log(o);
				if (o.ce != undefined)
				   cpu_stats_cb(o, o.ct, o.ce, o.fc);
				xfer_stats_cb(o.ac, o.wc, o.fc, o.ah, o.as);
				extint.srate = o.sr;
				extint.wb_srate = Math.round(o.wsr / 1e3);
				extint.nom_srate = o.nsr;

				gps_stats_cb(o.ga, o.gt, o.gg, o.gf, o.gc, o.go);
				if (o.gr) {
				   kiwi.GPS_auto_grid = decodeURIComponent(o.gr);
				   kiwi.GPS_auto_latlon = decodeURIComponent(o.gl);
				   kiwi.GPS_fixes = o.gf;
				   //console.log('stats_cb kiwi.GPS_auto_grid='+ kiwi.GPS_auto_grid);
				}
				
				// present in both admin & user
				w3_call('update_web_grid');
				w3_call('update_web_map');

				kiwi_snr_stats(o.sa, o.sh);
				
				w3_call('config_status_cb', o);     // admin only
				time_display_cb(o);
			}
			break;

		case "output_msg":
		   // kiwi_output_msg() does decodeURIComponent()
		   //console.log('output_msg: '+ param[1]);
		   kiwi_output_msg_p.s = param[1];
			kiwi_output_msg('id-output-msg', 'id-output-msg', kiwi_output_msg_p);
			break;

		case "status_msg_html":
		   var s = w3_json_to_html('status_msg_html', param[1]);
		   //console.log('status_msg_html: <'+ s +'>');
			w3_innerHTML('id-status-msg', s);		// overwrites last status msg
			w3_innerHTML('id-msg-status', s);		// overwrites last status msg
			break;
		
		case "is_admin":
			extint.isAdmin_cb(param[1]);
			break;

		case "is_local":
		   var p = param[1].split(',');
		   console.log('kiwi_msg rx_chan='+ p[0] +' is_local='+ p[1]);
			kiwi.is_local[+p[0]] = +p[1];
			kiwi.tlimit_exempt_by_pwd[+p[0]] = +p[2];
			break;
		
		case "no_reopen_retry":
			kiwi.no_reopen_retry = true;
			break;

      /*
      // enable DRM mode button
      var el = w3_el('id-button-drm');
      if (el && kiwi.is_multi_core) {
         w3_remove(el, 'class-button-disabled');
         w3_create_attribute(el, 'onclick', 'mode_button(event, this)');
      }
      */
		case "is_multi_core":
		   kiwi.is_multi_core = 1;
		   break;
		
		case "authkey_cb":
			extint.authkey_cb(param[1]);
			break;

		case "down":
			kiwi_down(param[1], reason_disabled);
			break;

		case "too_busy":
			kiwi_too_busy(parseInt(param[1]));
			break;

		case "wb_only":
			kiwi_wb_only();
			break;

		case "monitor":
			kiwi_monitor();
			break;

		case "exclusive_use":
			kiwi_exclusive_use();
			break;

		case "inactivity_timeout":
			kiwi_inactivity_timeout(param[1]);
			break;

		case "ip_limit":
		   var p = decodeURIComponent(param[1]).split(',');
			kiwi_24hr_ip_limit(parseInt(p[0]), p[1]);
			break;

		case "password_timeout":
			kiwi_password_entry_timeout();
			break;

		// can't simply come from 'cfg.*' because config isn't available without a web socket
		case "reason_disabled":
		   //console.log('reason_disabled param1=<'+ param[1] +'>');
			reason_disabled = 'User connections disabled.'+
			   ((param[1] != '')? ('<br>Reason: '+ w3_json_to_html('reason_disabled', param[1])) : '');
			break;
		
		case "sample_rate":
	      extint.srate = parseFloat(param[1]);
			break;
		
		// NB: use of 'pref' vs 'prefs'
		case 'pref_import_ch':
			kiwi.prefs_import_ch = +param[1];
			break;

		// NB: use of 'pref' vs 'prefs'
		case 'pref_import':
			prefs_import_cb(param[1], kiwi.prefs_import_ch);
			break;

		case 'adc_clk_nom':
			extint.adc_clock_nom_Hz = +param[1];
			break;

		case 'notify_msg':
		   var s = w3_json_to_html('notify_msg', param[1]);
			console.log('notify_msg: '+ s);
			if (confirmation.displayed) break;
         s = w3_div('', s);
         confirmation_show_content(s, 425, 50);
         setTimeout(confirmation_panel_close, 3000);
			break;

		case 'kiwi_kick':
		   var p = decodeURIComponent(param[1]).split(',');
		   var kicked = +p[0];
		   //console.log('kiwi_kick kicked='+ kicked);
		   var s = ext_get_cfg_param_string(kicked? 'cfg.reason_kicked' : 'cfg.reason_disabled');
		   //console.log('kick_kick p1=<'+ p[1] +'> s=<'+ s +'>');
		   var s = w3_json_to_html('kiwi_kick', p[1]) + ((s != '')? ('<br>Reason: '+ s) : '');
         //kiwi_show_msg(s);
			if (confirmation.displayed) break;
         s = w3_div('', s);
         confirmation_show_content(s, 425, 75, null, 'red');
			break;
		
		case 'snr_stats':
		   var p = param[1].split(',');
         kiwi_snr_stats(p[0], p[1]);
         break;

		case "extint_list_json":
			extint_list_json(param[1]);
			
			if (!isAdmin()) {
			   owrx_msg_cb(['have_ext_list']);
			}
			break;

		case "freq_offset":     // also for benefit of kiwirecorder
		   var foff_kHz = +param[1];
		   console.log('$$$ MSG freq_offset='+ foff_kHz +' cfg.freq_offset='+ cfg.freq_offset);
         kiwi_set_freq_offset(foff_kHz);
			break;

		default:
		   if (param[0].startsWith('antsw_')) {
		      if (ant_switch_msg(param))
		         break;
		   }
         //console.log('$$ kiwi_msg    [PASS]');
			rtn = false;
			break;
	}
	
	//console.log('>>> '+ ws.stream + ' kiwi_msg: '+ param[0] +'='+ param[1] +' RTN='+ rtn);
	return rtn;
}


////////////////////////////////
// debug
////////////////////////////////

function kiwi_debug(msg, syslog)
{
	console.log(msg);
	msg_send('SET '+ (syslog? 'dbug_msg=' : 'x-DEBUG=') + encodeURIComponent(msg));
}
	
function kiwi_show_msg(s)
{
   if (!w3_innerHTML('id-kiwi-msg', s)) return;
   if (s == '') {
	   w3_hide('id-kiwi-msg-container');
      w3_el('id-kiwi-body').style.overflow = 'hidden';
	   // don't make id-kiwi-container visible here -- it needs to be delayed
	   // see code in kiwi_valpwd2_cb()
   } else {
      w3_hide('id-kiwi-container');
      w3_show_block('id-kiwi-msg-container');

      // The default body used by id-kiwi-container needs to be overflow:hidden,
      // so change to scrolling here in case error message is long.
      w3_el('id-kiwi-body').style.overflow = 'scroll';
   }
}

function kiwi_server_error(s)
{
	kiwi_show_msg('Hmm, there seems to be a problem. <br>' +
	   'The server reported the error: <span style="color:red">'+ s +'</span>');
	seriousError = true;
}

function kiwi_serious_error(s)
{
	kiwi_show_msg(s);
	seriousError = true;
	console.log(s);
}

function kiwi_trace(msg)
{
   if (msg) console.log('kiwi_trace: '+ msg);
	try { console.trace(); } catch(ex) {}		// no console.trace() on IE
}

function kiwi_trace_mobile(msg)
{
   alert(msg +' '+ Error().stack);
}

function mdev_init()
{
   if (kiwi_url_param('mdev')) kiwi.mdev = true;
   if (kiwi_url_param('mnew')) kiwi.mnew = true;
}

function mdev_log(s)
{
   if (!kiwi.mdev || !dbgUs) return;
   
   if (isAdmin()) {
      w3_innerHTML('id-mdev-msg', 'Admin interface '+ s);
   } else {
      kiwi.mdev_s = s;
      mkenvelopes();
   }
}
