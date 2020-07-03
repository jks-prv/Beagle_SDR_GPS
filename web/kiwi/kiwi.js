// KiwiSDR
//
// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

var kiwi = {
   is_local: [],
   loaded_files: {},
   WSPR_rgrid: '',
   GPS_fixes: 0,
   wf_fps: 0,
   inactivity_panel: false,
   is_multi_core: 0,
   
   // must match rx_cmd.cpp
   modes_l: [ 'am', 'amn', 'usb', 'lsb', 'cw', 'cwn', 'nbfm', 'iq', 'drm', 'usn', 'lsn', 'sam', 'sal', 'sau', 'sas' ],
   modes_u: [],
   modes_s: {},
   
   RX4_WF4:0, RX8_WF2:1, RX3_WF3:2, RX14_WF0:3
};

kiwi.modes_l.forEach(function(e,i) { kiwi.modes_u.push(e.toUpperCase()); kiwi.modes_s[e] = i});
//console.log(kiwi.modes_u);
//console.log(kiwi.modes_s);

var WATERFALL_CALIBRATION_DEFAULT = -13;
var SMETER_CALIBRATION_DEFAULT = -13;

var rx_chans, wf_chans, wf_chans_real, rx_chan;
var try_again="";
var conn_type;
var seriousError = false;


var types = [ 'active', 'watch-list', 'sub-band', 'DGPS', 'special event', 'interference', 'masked' ];
var types_s = { active:0, watch_list:1, sub_band:2, DGPS:3, special_event:4, interference:5, masked:6 };
var type_colors = { 0:'cyan', 0x10:'lightPink', 0x20:'aquamarine', 0x30:'lavender', 0x40:'violet', 0x50:'violet', 0x60:'lightGrey' };

var timestamp;

//var optbar_prefix_color = 'w3-text-css-lime';
//var optbar_prefix_color = 'w3-text-aqua';
var optbar_prefix_color = 'w3-text-css-orange';

var dbgUs = false;
var dbgUsFirst = true;

var gmap;

// see document.onreadystatechange for how this is called
function kiwi_bodyonload(error)
{
	if (error != '') {
		kiwi_serious_error(error);
	} else {
	   if (initCookie('ident', "").startsWith('ZL/KF6VO')) dbgUs = true;
	   
	   // for testing a clean webpage, e.g. kiwi:8073/test
	   /*
	   var url = window.location.href;
      console.log('url='+ url);
	   if (url.endsWith('test')) {
	      console.log('test page..');
	      // test something
	      return;
	   }
	   */

		conn_type = html('id-kiwi-container').getAttribute('data-type');
		if (conn_type == 'undefined') conn_type = 'kiwi';
		console.log('conn_type='+ conn_type);
		
      w3int_init();

      var d = new Date();
		timestamp = d.getTime();
		
		if (conn_type == 'kiwi') {
		
			// A slight hack. For a user connection extint.ws is set here to ws_snd so that
			// calls to e.g. ext_send() for password validation will work. But extint.ws will get
			// overwritten when the first extension is loaded. But this should be okay since
			// subsequent uses of ext_send (mostly via ext_hasCredential/ext_valpwd) can specify
			// an explicit web socket to use (e.g. ws_wf).
         //
         // BUT NB: if you put an alert before the assignment to extint.ws there will be a race with
         // extint.ws needing to be used by ext_send() called by descendents of kiwi_open_ws_cb().

	      //deleteCookie('kiwi');    // for testing only
         
			extint.ws = owrx_ws_open_snd(kiwi_open_ws_cb, { conn_type:conn_type });
		} else {
			// e.g. admin or mfg connections
			extint.ws = kiwi_ws_open(conn_type, kiwi_open_ws_cb, { conn_type:conn_type });
		}
	}
}

function kiwi_open_ws_cb(p)
{
	if (p.conn_type != 'kiwi')
		setTimeout(function() { setInterval(function() { ext_send("SET keepalive") }, 5000) }, 5000);
	
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
      });
      obj.started = true;
      obj.finished = false;
   }
   return obj.finished;
}

function kiwi_load_js_dir(dir, js_files, cb_post, cb_pre)
{
   for (var i = 0; i < js_files.length; i++) {
      js_files[i] = dir + js_files[i];
   }
   console.log(js_files);
   kiwi_load_js(js_files, cb_post, cb_pre);
}

function kiwi_load_js(js_files, cb_post, cb_pre)
{
	console.log('DYNLOAD START');
	// kiwi_js_load.js will begin running only after all others have loaded and run.
	// Can then safely call the callback.
	js_files.push('kiwi/kiwi_js_load.js');

   var loaded_any = false;
   js_files.forEach(function(src) {
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
            if (src == 'kiwi/kiwi_js_load.js') script.kiwi_cb = cb_post;
         } else
         if (src.endsWith('.css')) {
            script = document.createElement('link');
            script.rel = 'stylesheet';
            script.href = src;
            script.type = 'text/css';
         } else
            unknown_ext = true;
         
         if (unknown_ext) {
            console.log('UNKNOWN FILETYPE '+ src);
         } else {
            script.async = false;
            document.head.appendChild(script);
            console.log('loading '+ src);
         }
      } else {
         console.log('already loaded: '+ src);
      }
   });
	console.log('DYNLOAD FINISH');
	
	// if the kiwi_js_load.js process never loaded anything just call the callback here
	if (!loaded_any) {
	   w3_call(cb_pre, false);
	   w3_call(cb_post);
	} else {
	   w3_call(cb_pre, true);
	}
}


function kiwi_ask_pwd(conn_kiwi)
{
	console.log('kiwi_ask_pwd chan_no_pwd='+ chan_no_pwd +' client_public_ip='+ client_public_ip);
	var s1 = '';
	if (conn_kiwi && chan_no_pwd) s1 = 'All channels busy that don\'t require a password ('+ chan_no_pwd +'/'+ rx_chans +')<br>';
	
	// "&& conn_kiwi" to ignore pathological "/admin?prot" etc.
   var prot = (kiwi_url_param(['p', 'prot', 'protected'], true, false) && conn_kiwi);
	if (prot) s1 = 'You have requested a password protected channel<br>';
	var s = "KiwiSDR: software-defined receiver <br>"+ s1 +
		"<form name='pform' style='display:inline-block' action='#' onsubmit='ext_valpwd(\""+ conn_type +"\", this.pwd.value); return false;'>"+
			try_again +
			w3_input('w3-label-inline w3-label-not-bold/kiwi-pw|padding:1px|name="pwd" size=40 onclick="this.focus(); this.select()"', 'Password:') +
		"</form>";
	kiwi_show_msg(s);
	document.pform.pwd.focus();
	document.pform.pwd.select();
}

var body_loaded = false;

function kiwi_valpwd1_cb(badp, p)
{
	//console.log("kiwi_valpwd1_cb conn_type="+ p.conn_type +' badp='+ badp);

	if (seriousError)
	   return;        // don't go any further

	if (badp == 1) {
		kiwi_ask_pwd(p.conn_type == 'kiwi');
		try_again = 'Try again. ';
	} else
	if (badp == 2) {
	   kiwi_show_msg('Still determining local interface address.<br>Please try reloading page in a few moments.');
	   return;
	} else
	if (badp == 3) {
	   kiwi_show_msg('Admin connections not allowed from this ip address.');
	   return;
	} else
	if (badp == 4) {
	   kiwi_show_msg('No admin password set. Can only connect from same local network as Kiwi.<br>Client ip = '+ client_public_ip);
	   return;
	} else
	if (badp == 5) {
	   kiwi_show_msg('Multiple connections from the same ip address not allowed.<br>Client ip = '+ client_public_ip);
	   return;
	} else
	if (badp == 0) {
		if (p.conn_type == 'kiwi') {
		
			// For the client connection, repeat the auth process for the second websocket.
			// It should always work since we only get here if the first auth has worked.
			extint.ws = owrx_ws_open_wf(kiwi_open_ws_cb2, p);
		} else {
			kiwi_valpwd2_cb(0, p);
		}
	}
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
			kiwi_init();
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

function kiwi_init()
{
}

function kiwi_xdLocalStorage_init()
{
	//jksx XDLS
	if (!dbgUs) return;
	
	var iframeUrls = [];
	for (var i = 0; i < 5; i++) {
		if (dbgUs && i == 0)		// jksx XDLS
			iframeUrls[i] = 'http://kiwi:8073/pkgs/xdLocalStorage/xdLocalStorage-min.html';
		else
			iframeUrls[i] = 'http://pub'+ i +'.kiwisdr.com:8073/pkgs/xdLocalStorage/xdLocalStorage-min.html';
	}
	
	xdLocalStorageHA.init({
		iframeUrls: iframeUrls,
		initCallback: function() {
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

	var init_m = (init_mode == undefined)? kiwi.modes_s['lsb'] : kiwi.modes_s[init_mode];
	init_m = ext_get_cfg_param('init.mode', init_m, EXT_NO_SAVE);
	//console.log('INIT init_mode='+ init_mode +' init.mode='+ init_m +' override_mode='+ override_mode);
	init_mode = override_mode? override_mode : kiwi.modes_l[init_m];
	if (init_mode === 'drm') init_mode = 'am';      // don't allow inherited drm mode from another channel

	var init_z = (init_zoom == undefined)? 0 : init_zoom;
	init_z = ext_get_cfg_param('init.zoom', init_z, EXT_NO_SAVE);
	init_zoom = override_zoom? override_zoom : init_z;

	var init_max = (init_max_dB == undefined)? -10 : init_max_dB;
	init_max = ext_get_cfg_param('init.max_dB', init_max, EXT_NO_SAVE);
	init_max_dB = override_max_dB? override_max_dB : init_max;

	var init_min = (init_min_dB == undefined)? -110 : init_min_dB;
	init_min = ext_get_cfg_param('init.min_dB', init_min, EXT_NO_SAVE);
	init_min_dB = override_min_dB? override_min_dB : init_min;
	
	console.log('INIT f='+ init_frequency +' m='+ init_mode +' z='+ init_zoom
		+' min='+ init_min_dB +' max='+ init_max_dB);

	w3_call('init_scale_dB');

	var ant = ext_get_cfg_param('rx_antenna');
	var el = w3_el('rx-antenna');
	if (el != undefined && ant) {
		el.innerHTML = 'Antenna: '+ decodeURIComponent(ant);
	}

   kiwi.WSPR_rgrid = ext_get_cfg_param_string('WSPR.grid', '', EXT_NO_SAVE);
}


////////////////////////////////
// configuration
////////////////////////////////

var cfg = { };
var adm = { };

function cfg_save_json(path, ws)
{
	//console.log('cfg_save_json: path='+ path);
	//kiwi_trace();

	if (ws == undefined || ws == null)
		return;
	var s;
	if (path.startsWith('adm.')) {
		s = encodeURIComponent(JSON.stringify(adm));
		ws.send('SET save_adm='+ s);
	} else {
		s = encodeURIComponent(JSON.stringify(cfg));
		ws.send('SET save_cfg='+ s);
	}
	console.log('cfg_save_json: DONE');
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
      case 0: server = 'ipapi.co/json'; break;
      case 1: server = 'extreme-ip-lookup.com/json'; break;
      case 2: server = 'get.geojs.io/v1/ip/geo.json'; break;
      default: break;
   }
   
   kiwi_ajax('https://'+ server, 
      function(json) {
         if (json.AJAX_error === undefined) {
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
	if (json.AJAX_error != undefined)
		return;
	
	if (window.JSON && window.JSON.stringify)
      geo.json = JSON.stringify(json);
   else
      geo.json = json.toString();
   
   var country = json.country_name || json.country;
	
	if (country == "United States" && json.region) {
		country = json.region +', USA';
	}
	
	geo.geo = '';
	if (json.city) geo.geo += json.city;
	if (country) geo.geo += (json.city? ', ':'') + country;
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
var time_display_current = true;

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
	w3_el('id-time-display-UTC').innerHTML = server_time_utc? server_time_utc : '?';
	w3_el('id-time-display-local').innerHTML = noLatLon? '?' : server_time_local;
	w3_el('id-time-display-tzname').innerHTML = noLatLon? 'Lat/lon needed for local time' : server_tz;

	w3_el('id-time-display-logo-inner').style.opacity = display_time? 0:1;
	w3_el('id-time-display-inner').style.opacity = display_time? 1:0;
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
		) +
		w3_div('id-time-display-logo-inner',
			w3_div('id-time-display-logo-text', 'Powered by'),
			'<a href="https://github.com/ha7ilm/openwebrx" target="_blank"><img id="id-time-display-logo" src="gfx/openwebrx-top-logo.png" /></a>'
		);

	time_display(time_display_current);
}

function time_display_width()
{
   return 200;
}

function time_display_html(ext_name)
{
   return w3_div(ext_name +'-time-display|top:50px; background-color:black; position:relative;');
}


////////////////////////////////
// user preferences
////////////////////////////////

var pref = {};
var pref_default = { p:'default-p' };

function show_pref()
{
	pref_load(function() {
		var s =
			w3_divs('w3-text-css-yellow/w3-tspace-16',
				w3_div('w3-medium w3-text-aqua', '<b>User preferences</b>'),
				w3_col_percent('',
					w3_input('', 'Pref', 'pref.p', pref.p, 'pref_p_cb', 'something'), 30
				),
				
				w3_div('w3-show-inline-block w3-hspace-16',
					w3_button('', 'Export', 'pref_export_btn_cb'),
					w3_button('', 'Import', 'pref_import_btn_cb'),
					'<b>Status:</b> ' + w3_div('id-pref-status w3-show-inline-block w3-snap-back')
				)
			);
	
		extint_panel_hide();
		ext_panel_set_name('show_pref');
		ext_panel_show(s, null, show_pref_post);
		//ext_set_controls_width_height(1024, 500);
		ext_set_controls_width_height(1024, 250);
		freqset_select();
	});
}

function pref_p_cb(path, val)
{
	w3_string_cb(path, val);
	pref_save();
}

function pref_refresh_ui()
{
	w3_set_value('pref.p', pref.p);
}

var perf_status_anim, perf_status_timeout;

function pref_status(color, msg)
{
	var el = w3_el('id-pref-status');
	
	if (perf_status_anim) {
		kiwi_clearTimeout(perf_status_timeout);
		//el.style.color = 'red';
		//el.innerHTML = 'CANCEL';
		w3_remove_then_add(el, 'w3-fade-out', 'w3-snap-back');
		setTimeout(function() {
			perf_status_anim = false;
			pref_status(color, msg);
		}, 1000);
		return;
	}
	
	el.style.color = color;
	el.innerHTML = msg;
	w3_remove_then_add(el, 'w3-snap-back', 'w3-fade-out');
	perf_status_anim = true;
	perf_status_timeout = setTimeout(function() {
		el.innerHTML = '';
		w3_remove_then_add(el, 'w3-fade-out', 'w3-snap-back');
		perf_status_anim = false;
	}, 1500);
}

function pref_export_btn_cb(path, val)
{
	console.log('pref_export_btn_cb');
	pref_save(function() {
		console.log('msg_send pref_export');
		msg_send('SET pref_export id='+ encodeURIComponent(pref.id) +' pref='+ encodeURIComponent(JSON.stringify(pref)));
	});
	pref_status('lime', 'preferences exported');
}

function pref_import_btn_cb(path, val)
{
	console.log('pref_import_btn_cb');
	var id = ident_user? ident_user : '_blank_';
	msg_send('SET pref_import id='+ encodeURIComponent(id));
}

function pref_import_cb(p, ch)
{
	var s = decodeURIComponent(p);
	console.log('pref_import_cb '+ s);
	if (s == 'null') {
		pref_status('yellow', 'no preferences previously exported');
		return;
	} else {
		pref = JSON.parse(s);
		console.log(pref);
		pref_save();
		pref_refresh_ui();
		pref_status('lime', 'preferences successfully imported from RX'+ ch);
	}
}

function pref_load(cb)
{
	var id = ident_user? ident_user : '_blank_';
	xdLocalStorageHA.getItem('pref.'+ id, function(d) {
		console.log('xdLocalStorage.getItem pref.'+ id);
		console.log(d);
		if (d.value == null) {
			pref = pref_default;
		} else {
			pref = JSON.parse(d.value);
		}
		console.log(pref);
		if (cb) cb();
	});
}

function pref_save(cb)
{
	var id = ident_user? ident_user : '_blank_';
	pref.id = id;
	var val = JSON.stringify(pref);
	xdLocalStorageHA.setItem('pref.'+ id, val, function(d) {
		console.log('xdLocalStorage.setItem pref.'+ id);
		console.log(d);
		if (cb) cb();
	});
}

function show_pref_post()
{
	console.log('show_pref_post');
}

function show_pref_blur()
{
	console.log('show_pref_blur');
}


////////////////////////////////
// status
////////////////////////////////

var ansi = {
   colors:  [  // MacOS Terminal.app colors
   
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
            ]
};

function kiwi_output_msg(id, id_scroll, p)
{
	var parent_el = w3_el(id);
	if (!parent_el) {
	   console.log('kiwi_output_msg NOT_FOUND id='+ id);
	   return;
	}

	var s;
	try {
	   s = decodeURIComponent(p.s);
	} catch(ex) {
	   console.log('decodeURIComponent FAIL:');
	   console.log(p.s);
	   s = p.s;
	}
	
   if (!p.init) {
      p.el = w3_appendElement(parent_el, 'pre', '');
      p.esc = { s:'', state:0 };
      p.sgr = { span:0, bright:0, fg:null, bg:null };
      p.init = true;
   }

   var snew = '';
	var el_scroll = w3_el(id_scroll);
   var wasScrolledDown = null;
   
   if (isUndefined(p.tstr)) p.tstr = '';
   if (isUndefined(p.col)) p.col = 0;

   // handle beginning output with '\r' only to overwrite current line
   //console.log(JSON.stringify(s));
   if (p.process_return_alone && s.charAt(0) == '\r' && (s.length == 1 || s.charAt(1) != '\n')) {
      //console.log('\\r @ beginning:');
      //console.log(JSON.stringify(s));
      //console.log(JSON.stringify(p.tstr));
      s = s.substring(1);
      p.tstr = snew = '';
      p.col = 0;
   }

   if (p.remove_returns) s = s.replace(/\r/g, '');
      
	for (var i=0; i < s.length; i++) {

		var c = s.charAt(i);
      //console.log('c='+ JSON.stringify(c));

		if (c == '\f') {		// form-feed is how we clear accumulated pre elements (screen clear)
		   while (parent_el.firstChild) {
		      parent_el.removeChild(parent_el.firstChild);
		   }
         p.el = w3_appendElement(parent_el, 'pre', '');
         p.tstr = snew = '';
			p.col = 0;
		} else
		
		// tab-8
		if (c == '\t') {
         snew += '&nbsp;';
         p.col++;
		   while ((p.col & 7) != 0) {
		      snew += '&nbsp;';
		      p.col++;
		   }
		} else
		
		// ANSI color escapes
		if (c == '\x1b') {
		   p.esc.s = '';
		   p.esc.state = 1;
		} else

		if (p.esc.state == 1) {
         //console.log('acc '+ JSON.stringify(c));
		   if (c < '@' || c == '[') {
            p.esc.s += c;
		   } else {
            p.esc.s += c;
            //console.log('process ESC '+ JSON.stringify(p.esc.s));
		      var first = p.esc.s.charAt(0);
		      var second = p.esc.s.charAt(1);
            var result = 0;
		      
		      if (first == '[') {

               // fg/bg color
               if (c == 'm') {
                  var sgr, saw_reset = 0, saw_color = 0;
                  var sa = p.esc.s.substr(1).split(';');
                  var sl = sa.length
                  //console.log('SGR '+ JSON.stringify(p.esc.s) +' sl='+ sl);
                  //console.log(sa);
         
                  for (var ai=0; ai < sl && result == 0; ai++) {
                     sgr = (sa[ai] == '' || sa[ai] == 'm')? 0 : parseInt(sa[ai]);
                     //console.log('sgr['+ ai +']='+ sgr);
                     if (sgr == 0) {   // \e[m or \e[0m
                        p.sgr.fg = p.sgr.bg = null;
                        saw_reset = 1;
                     } else
                     if (isNaN(sgr)) {
                        result = 2;
                     } else
                     if (sgr == 1)  { p.sgr.bright = 8; } else
                     if (sgr == 2)  { p.sgr.bright = 0; } else
                     if (sgr == 22) { p.sgr.bright = 0; } else
                     
                     if (sgr == 7) {      // reverse video (swap fg/bg)
                        var tf = p.sgr.fg;
                        p.sgr.fg = p.sgr.bg;
                        p.sgr.bg = tf;
                        if (p.sgr.fg == null) p.sgr.fg = [255,255,255];
                        if (p.sgr.bg == null) p.sgr.bg = [0,0,0];
                        saw_color = 1;
                     } else
                     if (sgr == 27) {     // inverse off
                        p.sgr.fg = p.sgr.bg = null;
                        saw_color = 1;
                     } else
                     
                     // foreground
                     if (sgr >= 30 && sgr <= 37) {
                        //console.log('SGR='+ sgr +' bright='+ p.sgr.bright);
                        p.sgr.fg = ansi.colors[sgr-30 + p.sgr.bright];
                        saw_color = 1;
                     } else
                     if (sgr >= 90 && sgr <= 97) {
                        p.sgr.fg = ansi.colors[sgr-90 + 8];
                        saw_color = 1;
                     } else
                     if (sgr == 39) {     // normal
                        p.sgr.fg = null;
                        saw_color = 1;
                     } else
      
                     // background
                     if (sgr >= 40 && sgr <= 47) {
                        p.sgr.bg = ansi.colors[sgr-40 + p.sgr.bright];
                        saw_color = 1;
                     } else
                     if (sgr >= 100 && sgr <= 107) {
                        p.sgr.bg = ansi.colors[sgr-100 + 8];
                        saw_color = 1;
                     } else
                     if (sgr == 49) {     // normal
                        p.sgr.bg = null;
                        saw_color = 1;
                     } else
                     
                     // 8 or 24-bit fg/bg
                     if (sgr == 38 || sgr == 48) {
                        //console.log('SGR-8/24 sl='+ sl);
                        var n8, r, g, b, color, ci;
   
                        if (sl == 3 && (parseInt(sa[1]) == 5) && (!isNaN(n8 = parseInt(sa[2])))) {
                           //console.log('SGR n8='+ n8);
                           ai += 2;
                           if (n8 <= 15) {      // standard colors
                              color = ansi.colors[n8];
                              if (sgr == 38) p.sgr.fg = color; else p.sgr.bg = color;
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
                              saw_color = 1;
                           } else
                           if (n8 <= 255) {     // grayscale ramp
                              ci = 8 + (n8-232)*10;
                              //console.log('n8='+ n8 +' ci='+ ci);
                              color = [ci,ci,ci];
                              if (sgr == 38) p.sgr.fg = color; else p.sgr.bg = color;
                              saw_color = 1;
                           } else
                              result = 2;
                        } else
   
                        if (sl == 5 && (parseInt(sa[1]) == 2) &&
                           (!isNaN(r = parseInt(sa[2]))) && (!isNaN(g = parseInt(sa[3]))) && (!isNaN(b = parseInt(sa[4])))) {
                              r = w3_clamp(r, 0,255);
                              g = w3_clamp(g, 0,255);
                              b = w3_clamp(b, 0,255);
                              color = [r,g,b];
                              if (sgr == 38) p.sgr.fg = color; else p.sgr.bg = color;
                              saw_color = 1;
                        } else
                           result = 2;
                     } else
                        result = 2;
                  }
                  
                  if (saw_reset) {  // \e[m or \e[0m
                     //console.log('SGR DONE');
                     if (p.sgr.span) {
                        snew += '</span>';
                        p.sgr.span = 0;
                     }
                  } else
                  if (saw_color) {
                     //console.log('SGR saw_color fg='+ rgb(p.sgr.fg) +' bg='+ rgb(p.sgr.bg));
                     //console.log(p.sgr.fg);
                     //console.log(p.sgr.bg);
                     if (p.sgr.span) snew += '</span>';
                     snew += '<span style="'+ (p.sgr.fg? ('color:'+ rgb(p.sgr.fg) +';') :'') + (p.sgr.bg? ('background-color:'+ rgb(p.sgr.bg) +';') :'') +'">';
                     p.sgr.span = 1;
                  } else {
                     //console.log('SGR ERROR');
                     result = 2;
                  }
               } else
               
               if (c == 'K') {
                  result = 'erase in line';
               } else
               
               if (c == 'P') {
                  result = 'del # chars';
               } else
               
               if (second == '?') {    // set/reset mode
                  result = 'set/reset mode';
                  //result = 1;
               } else
               
               if (c == 'r') {      // set scrolling region (t_row;b_row)
                  //result = 1;
               } else
               
               if (c == 'l') {      // reset mode (i.e. "4l" = reset insert mode)
                  result = 'reset mode';
                  //result = 1;
               } else
		      
               if (c == 'J') {
                  if (second == '2' || second == '3') result = 'erase whole display'; else
                  if (second == '1') result = 'erase start to cursor'; else
                     result = 2;
               } else
               
               if (c == 'H') {
                  result = 'move row,col';
               } else
		      
               if (c == 'd') {
                  result = 'move row';
               } else
		      
               if (c == 'G') {
                  result = 'move col';
               } else
		      
               {
                  result = 2;
               }
            } else

		      if (first == '(') {     // define char set
               //result = 1;
		      } else
		      
		      {
               result = 2;
            }
            
            if (result === 1) {
                  console.log('ESC '+ JSON.stringify(p.esc.s) +' IGNORED');
            } else
            if (result === 2) {
                  console.log('ESC '+ JSON.stringify(p.esc.s) +' UNKNOWN');
            } else
            if (isString(result)) {
               console.log('ESC '+ JSON.stringify(p.esc.s) +' '+ result);
            }
   
            p.esc.state = 0;
         }
		} else
		
		if ((c >= ' ' && c <= '~') || c == '\n') {
		   if (c == '<') {
		      snew += '&lt;';
            p.col++;
		   } else
		   if (c == '>') {
		      snew += '&gt;';
            p.col++;
		   } else {
            if (c != '\n') {
               snew += c;
               p.col++;
            }
            if (c == '\n' || p.col == p.ncol) {
               wasScrolledDown = kiwi_isScrolledDown(el_scroll);
               p.tstr += snew;
               if (p.tstr == '') p.tstr = '&nbsp;';
               p.el.innerHTML = p.tstr;
               //console.log('TEXT '+ JSON.stringify(p.tstr));
               p.tstr = snew = '';
               p.el = w3_appendElement(parent_el, 'pre', '');
               p.col = 0;
            
               if (w3_contains(el_scroll, 'w3-scroll-down') && (!p.scroll_only_at_bottom || (p.scroll_only_at_bottom && wasScrolledDown)))
                  el_scroll.scrollTop = el_scroll.scrollHeight;
            }
         }
		}  // ignore any other chars
	}

   wasScrolledDown = kiwi_isScrolledDown(el_scroll);
   p.tstr += snew;
   //console.log('TEXT '+ JSON.stringify(p.tstr));
   p.el.innerHTML = p.tstr;

	if (w3_contains(el_scroll, 'w3-scroll-down') && (!p.scroll_only_at_bottom || (p.scroll_only_at_bottom && wasScrolledDown)))
		el_scroll.scrollTop = el_scroll.scrollHeight;
}

function gps_stats_cb(acquiring, tracking, good, fixes, adc_clock, adc_gps_clk_corrections)
{
   var s = (acquiring? 'yes':'pause') +', track '+ tracking +', good '+ good +', fixes '+ fixes.toUnits();
	w3_el_softfail('id-msg-gps').innerHTML = 'GPS: acquire '+ s;
	w3_innerHTML('id-status-gps',
	   w3_text(optbar_prefix_color, 'GPS'),
	   w3_text('', 'acq '+ s)
	);
	extint_adc_clock_Hz = adc_clock * 1e6;
	extint_adc_gps_clock_corr = adc_gps_clk_corrections;
	if (adc_gps_clk_corrections) {
	   s = adc_clock.toFixed(6) +' ('+ adc_gps_clk_corrections.toUnits() +' avgs)';
		w3_el_softfail("id-msg-gps").innerHTML += ', ADC clock '+ s;
		w3_innerHTML('id-status-adc',
	      w3_text(optbar_prefix_color, 'ADC clock '),
	      w3_text('', s)
		);
	}
}

function admin_stats_cb(audio_dropped, underruns, seq_errors, dp_resets, dp_hist_cnt, dp_hist, in_hist_cnt, in_hist)
{
   if (audio_dropped == undefined) return;
   
	var el = w3_el('id-msg-errors');
	if (el) el.innerHTML = 'Stats: '+
	   audio_dropped.toUnits() +' dropped, '+
	   underruns.toUnits() +' underruns, '+
	   seq_errors.toUnits() +' sequence, '+
	   dp_resets.toUnits() +' realtime';

	el = w3_el('id-status-dp-hist');
	if (el) {
	   var s = 'Datapump: ';
		for (var i = 0; i < dp_hist_cnt; i++) {
		   s += (i? ', ':'') + dp_hist[i].toUnits();
		}
      el.innerHTML = s;
	}

	el = w3_el('id-status-in-hist');
	if (el) {
	   var s = 'SoundInQ: ';
		for (var i = 0; i < in_hist_cnt; i++) {
		   s += (i? ', ':'') + in_hist[i].toUnits();
		}
      el.innerHTML = s;
	}
}

function kiwi_too_busy(rx_chans)
{
	var s = 'Sorry, the KiwiSDR server is too busy right now ('+ rx_chans+((rx_chans>1)? ' users':' user') +' max). <br>' +
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
	writeCookie('iplimit', encodeURIComponent(pwd));
   window.location.reload(true);
}

function kiwi_show_error_ask_exemption(s)
{
   s += '<br><br>If you have an exemption password from the KiwiSDR owner/admin <br> please enter it here: ' +
      '<form name="pform" style="display:inline-block" action="#" onsubmit="kiwi_ip_limit_pwd_cb(this.pinput.value); return false">' +
			w3_input('w3-label-inline w3-label-not-bold/kiwi-pw|padding:1px|name="pinput" size=40 onclick="this.focus(); this.select()"', 'Password:') +
      '</form>';

	kiwi_show_msg(s);
	document.pform.pinput.focus();
	document.pform.pinput.select();
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

function kiwi_up(up)
{
	if (!seriousError) {
      w3_hide('id-kiwi-msg-container');
      w3_show_block('id-kiwi-container');
      w3_el('id-kiwi-body').style.overflow = 'hidden';
	}
}

function kiwi_down(type, comp_ctr, reason)
{
	//console.log("kiwi_down comp_ctr="+ comp_ctr);
	var s;
	type = +type;

	if (type == 1) {
		s = 'Sorry, software update in progress. Please check back in a few minutes.<br>' +
			'Or check <a href="http://rx.kiwisdr.com" target="_self">rx.kiwisdr.com</a> for more KiwiSDR receivers available world-wide.';
		
		if (comp_ctr > 0 && comp_ctr < 9000)
			s += '<br>Build: compiling file #'+ comp_ctr;
		else
		if (comp_ctr == 9997)
			s += '<br>Build: linking';
		else
		if (comp_ctr == 9998)
			s += '<br>Build: installing';
		else
		if (comp_ctr == 9999)
			s += '<br>Build: done';
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

var stats_interval = 10000;
var need_config = true;

function stats_init()
{
	if (need_config) {
		msg_send('SET GET_CONFIG');
		need_config = false;
	}
	stats_update();
}

function stats_update()
{
   //console.log('SET STATS_UPD ch='+ rx_chan);
	msg_send('SET STATS_UPD ch='+ rx_chan);
	var now = new Date();
	var aligned_interval = Math.ceil(now/stats_interval)*stats_interval - now;
	if (aligned_interval < stats_interval/2) aligned_interval += stats_interval;
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
	   w3_text(optbar_prefix_color, 'Net') +
	   w3_text('', 'aud '+ audio_kbps.toFixed(0) +', wf '+ waterfall_kbps.toFixed(0) +', http '+
		http_kbps.toFixed(0) +', total '+ sum_kbps.toFixed(0) +' kB/s');

	kiwi_xfer_stats_str_long = 'Network (all channels): audio '+audio_kbps.toFixed(0)+' kB/s, waterfall '+waterfall_kbps.toFixed(0)+
		' kB/s ('+ waterfall_fps.toFixed(0)+' fps)' +
		', http '+http_kbps.toFixed(0)+' kB/s, total '+sum_kbps.toFixed(0)+' kB/s ('+(sum_kbps*8).toFixed(0)+' kb/s)';
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
	kiwi_cpu_stats_str =
	   w3_text(optbar_prefix_color, 'BB ') +
	   w3_text('', o.cu[0] +','+ o.cs[0] +','+ o.ci[0] +' usi% ') +
	   (cputempC? w3_text(temp_color, cputemp) :'') +
	   w3_text('', cpufreq +' ') +
	   w3_text(optbar_prefix_color, 'FPGA') +
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
         w3_text('w3-text-black', 'Beagle: '+ cpus +' '+ user +' usr | '+ sys +' sys | '+ idle +' idle,' + (cputempC? '':' ')) +
         (cputempC? ('&nbsp;'+ w3_text(temp_color +' w3-text-outline w3-large', cputemp) +'&nbsp;') :'') +
         w3_text('w3-text-black', cpufreq + ', ') +
         w3_text('w3-text-black', 'FPGA eCPU: '+ ecpu.toFixed(0) +'%')
      );

	var t = uptime_secs;
	var sec = Math.trunc(t % 60); t = Math.trunc(t/60);
	var min = Math.trunc(t % 60); t = Math.trunc(t/60);
	var hr  = Math.trunc(t % 24); t = Math.trunc(t/24);
	var days = t;

	var s = ' ';
	if (days) s += days +'d:';
	s += hr +':'+ min.leadingZeros(2) +':'+ sec.leadingZeros(2);
	w3_innerHTML('id-status-config',
      w3_text(optbar_prefix_color, 'Up'),
      w3_text('', s +', '+ kiwi_config_str)
	);

	s = ' | Uptime: ';
	if (days) s += days +' '+ ((days > 1)? 'days':'day') +' ';
	s += hr +':'+ min.leadingZeros(2) +':'+ sec.leadingZeros(2);
	w3_innerHTML('id-msg-config', kiwi_config_str_long + s);
}

function config_str_update(rx_chans, gps_chans, vmaj, vmin)
{
	kiwi_config_str = 'v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR ch, '+ gps_chans +' GPS ch';
	w3_innerHTML('id-status-config', kiwi_config_str);
	kiwi_config_str_long = 'Config: v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR channels, '+ gps_chans +' GPS channels';
	w3_innerHTML('id-msg-config', kiwi_config_str);
}

var config_net = {};

function config_cb(rx_chans, gps_chans, serno, pub, port_ext, pvt, port_int, nm, mac, vmaj, vmin)
{
	var s;
	config_str_update(rx_chans, gps_chans, vmaj, vmin);

	var net_config = w3_el("id-net-config");
	if (net_config) {
		net_config.innerHTML =
			w3_div('',
				w3_col_percent('',
					w3_div('', 'Public IP address (outside your firewall/router): '+ pub +' [port '+ port_ext +']'), 50,
					w3_div('', 'Ethernet MAC address: '+ mac.toUpperCase()), 30,
					w3_div('', 'KiwiSDR serial number: '+ serno), 20
				),
				w3_col_percent('',
					w3_div('', 'Private IP address (inside your firewall/router): '+ pvt +' [port '+ port_int +']'), 50,
					w3_div('', 'Private netmask: /'+ nm), 50
				)
			);
		config_net.pub_ip = pub;
		config_net.pub_port = port_ext;
		config_net.pvt_ip = pub;
		config_net.pvt_port = port_int;
		config_net.mac = mac;
		config_net.serno = serno;
	}
}

function update_cb(fs_full, pending, in_progress, rx_chans, gps_chans, vmaj, vmin, pmaj, pmin, build_date, build_time)
{
	config_str_update(rx_chans, gps_chans, vmaj, vmin);

	var msg_update = w3_el("id-msg-update");
	
	if (msg_update) {
		var s;
		s = 'Installed version: v'+ vmaj +'.'+ vmin +', built '+ build_date +' '+ build_time;
		if (fs_full) {
			s += '<br>Cannot build, filesystem is FULL!';
		} else
		if (in_progress) {
			s += '<br>Update to version v'+ + pmaj +'.'+ pmin +' in progress';
		} else
		if (pending) {
			s += '<br>Update check pending';
		} else
		if (pmaj == -1) {
			s += '<br>Error determining the latest version -- check log';
		} else {
			if (vmaj == pmaj && vmin == pmin)
				s += '<br>Running most current version';
			else
				s += '<br>Available version: v'+ pmaj +'.'+ pmin;
		}
		msg_update.innerHTML = s;
	}
}


////////////////////////////////
// user list
////////////////////////////////

var users_interval = 2500;
var user_init = false;

function users_init(called_from_admin)
{
	console.log("users_init #rx="+ rx_chans);
	for (var i=0; i < rx_chans; i++) {
	   divlog(
	      'RX'+ i +': <span id="id-user-'+ i +'"></span> ' +
	      (called_from_admin?
	         w3_button('id-user-kick-'+ i +' w3-small w3-white w3-border w3-border-red w3-round-large w3-padding-0 w3-padding-LR-8',
	            'Kick', 'status_user_kick_cb', i)
	         : ''
	      )
	   );
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
	obj.forEach(function(obj) {
		//console.log(obj);
		var s1 = '', s2 = '';
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
			var id = kiwi_strip_tags(decodeURIComponent(name), '');
			if (id != '') id = '"'+ id + '" ';
			var g = (geoloc == '(null)' || geoloc == '')? 'unknown location' : decodeURIComponent(geoloc);
			ip = ip.replace(/::ffff:/, '');		// remove IPv4-mapped IPv6 if any
			g = '('+ ip + g +') ';
			var f = freq + cfg.freq_offset*1e3;
			var f = (f/1000).toFixed((f > 100e6)? 1:2);
			var f_s = f + ' kHz ';
			var fo = (freq/1000).toFixed(2);
			var anchor = '<a href="javascript:tune('+ fo +','+ sq(mode) +','+ zoom +');">';
			if (ext != '') ext = decodeURIComponent(ext) +' ';
			s1 = id + g;
			s2 = anchor + f_s + mode +' z'+ zoom +'</a> '+ ext + connected + remaining;
		}
		
		//if (s1 != '') console.log('user'+ i +'='+ s1 + s2);
		if (user_init) {
		   // status display used by admin page
		   w3_innerHTML('id-user-'+ i, s1 + s2);
		   var kick = 'id-user-kick-'+ i;
	      if (w3_el(kick)) {
            if (s1 != '')
               w3_show_inline_block(kick);
            else
               w3_hide(kick);
         }
         
         // new users display
         //for (var i=0; i < rx_chans; i++) if (s1 != '')
         w3_innerHTML('id-optbar-user-'+ i, (s1 != '')? (s1 +'<br>'+ s2) : '');
		}
		
		if (obj.c) {
		   //console.log('SAM carrier '+ obj.c);
		   var el = w3_el('id-sam-carrier');
		   if (el) w3_innerHTML(el, 'carrier '+ obj.c.toFixed(1) +' Hz');
		}
		
		// inactivity timeout warning panel
		if (i == rx_chan && obj.rn) {
		   if (obj.rn <= 55 && !kiwi.inactivity_panel) {
            var s = 'Inactivity timeout in one minute.<br>Close this panel to avoid disconnection.';
            confirmation_show_content(s, 350, 55,
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
		
		// another action like a frequency change reset timer
      if (obj.rn > 55 && kiwi.inactivity_panel) {
         confirmation_panel_close();
         kiwi.inactivity_panel = false;
      }
	});
	
}


////////////////////////////////
// misc
////////////////////////////////

var toggle_e = {
   // zero implies toggle
	SET : 1,
	SET_URL : 2,
	FROM_COOKIE : 4,
	WRITE_COOKIE : 8
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
		rv = readCookie(cookie_id);
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

function kiwi_plot_max(b)
{
   var t = bi[b];
   var plot_max = 1024 / (t.samplerate/t.plot_samplerate);
   return plot_max;
}

function kiwi_fft_mode()
{
	if (0) {
		toggle_or_set_spec(toggle_e.SET, 1);
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


////////////////////////////////
// control messages
////////////////////////////////

var comp_ctr, reason_disabled = '';
var version_maj = -1, version_min = -1;
var tflags = { INACTIVITY:1, WF_SM_CAL:2, WF_SM_CAL2:4 };
var chan_no_pwd;
var pref_import_ch;
var kiwi_output_msg_p = { scroll_only_at_bottom: true, process_return_alone: false };
var client_public_ip;

function kiwi_msg(param, ws)
{
	var rtn = true;
	
	switch (param[0]) {
		case "version_maj":
			version_maj = parseInt(param[1]);
			break;
			
		case "version_min":
			version_min = parseInt(param[1]);
			break;

		case "client_public_ip":
			client_public_ip = param[1].replace(/::ffff:/, '');    // remove IPv4-mapped IPv6 if any
			console.log('client public IP: '+ client_public_ip);
			break;

		case "badp":
			console.log('badp='+ param[1]);
			extint_valpwd_cb(parseInt(param[1]));
			break;					

		case "chan_no_pwd":
			chan_no_pwd = parseInt(param[1]);
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
			break;

		case "load_cfg":
			var cfg_json = decodeURIComponent(param[1]);
			//console.log('### load_cfg '+ ws.stream +' '+ cfg_json.length);
			cfg = JSON.parse(cfg_json);
			owrx_cfg();
			break;

		case "load_adm":
			var adm_json = decodeURIComponent(param[1]);
			//console.log('### load_adm '+ ws.stream +' '+ adm_json.length);
			adm = JSON.parse(adm_json);
			break;

		case "request_dx_update":
			dx_update();
			break;					

		case "mkr":
			//console.log('MKR '+ param[1]);
			dx_label_cb(JSON.parse(param[1]));
			break;					

		case "user_cb":
			//console.log('user_cb='+ param[1]);
			user_cb(JSON.parse(param[1]));
			break;					

		case "config_cb":
			//console.log('config_cb='+ param[1]);
			var o = JSON.parse(param[1]);
			config_cb(o.r, o.g, o.s, o.pu, o.pe, o.pv, o.pi, o.n, o.m, o.v1, o.v2, o.ai);
			break;					

		case "update_cb":
			//console.log('update_cb='+ param[1]);
			var o = JSON.parse(param[1]);
			update_cb(o.f, o.p, o.i, o.r, o.g, o.v1, o.v2, o.p1, o.p2,
				decodeURIComponent(o.d), decodeURIComponent(o.t));
			break;					

		case "stats_cb":
			//console.log('stats_cb='+ param[1]);
			try {
				var o = JSON.parse(param[1]);
				//console.log(o);
				if (o.ce != undefined)
				   cpu_stats_cb(o, o.ct, o.ce, o.fc);
				xfer_stats_cb(o.ac, o.wc, o.fc, o.ah, o.as);
				extint_srate = o.sr;
				gps_stats_cb(o.ga, o.gt, o.gg, o.gf, o.gc, o.go);
				if (o.gr) {
				   kiwi.WSPR_rgrid = decodeURIComponent(o.gr);
				   kiwi.GPS_fixes = o.gf;
				   //console.log('stat kiwi.WSPR_rgrid='+ kiwi.WSPR_rgrid);
				}
				admin_stats_cb(o.ad, o.au, o.ae, o.ar, o.an, o.ap, o.an2, o.ai);
				time_display_cb(o);
			} catch(ex) {
				console.log('<'+ param[1] +'>');
				console.log('kiwi_msg() stats_cb: JSON parse fail');
				console.log(ex);
			}
			break;					

		case "status_msg_text":
		   // kiwi_output_msg() does decodeURIComponent()
		   //console.log('status_msg_text: '+ param[1]);
		   kiwi_output_msg_p.s = param[1];
			kiwi_output_msg('id-output-msg', 'id-output-msg', kiwi_output_msg_p);
			break;

		case "status_msg_html":
		   var s = decodeURIComponent(param[1]);
		   //console.log('status_msg_html: '+ s);
			w3_innerHTML('id-status-msg', s);		// overwrites last status msg
			w3_innerHTML('id-msg-status', s);		// overwrites last status msg
			break;
		
		case "is_admin":
			extint_isAdmin_cb(param[1]);
			break;

		case "is_local":
		   var p = param[1].split(',');
		   console.log('kiwi_msg rx_chan='+ p[0] +' is_local='+ p[1]);
			kiwi.is_local[+p[0]] = +p[1];
			break;
		
      /*
      // enable DRM mode button
      var el = w3_el('id-button-drm');
      if (el && kiwi.is_multi_core) {
         w3_remove(el, 'class-button-disbled');
         w3_attribute(el, 'onclick', 'mode_button(event, this)');
      }
      */
		case "is_multi_core":
		   kiwi.is_multi_core = 1;
		   break;
		
		case "authkey_cb":
			extint_authkey_cb(param[1]);
			break;

		case "down":
			kiwi_down(param[1], comp_ctr, reason_disabled);
			break;

		case "too_busy":
			kiwi_too_busy(parseInt(param[1]));
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

		case "comp_ctr":
			comp_ctr = param[1];
			break;
		
		// can't simply come from 'cfg.*' because config isn't available without a web socket
		case "reason_disabled":
			reason_disabled = decodeURIComponent(param[1]);
			break;
		
		case "sample_rate":
	      extint_srate = parseFloat(param[1]);
			break;
		
		case 'pref_import_ch':
			pref_import_ch = +param[1];
			break;

		case 'pref_import':
			pref_import_cb(param[1], pref_import_ch);
			break;

		case 'adc_clk_nom':
			extint_adc_clock_nom_Hz = +param[1];
			break;

		default:
			rtn = false;
			break;
	}
	
	//console.log('>>> '+ ws.stream + ' kiwi_msg: '+ param[0] +'='+ param[1] +' RTN='+ rtn);
	return rtn;
}


////////////////////////////////
// debug
////////////////////////////////

function kiwi_debug(msg)
{
	console.log(msg);
	msg_send('SET debug_msg='+ encodeURIComponent(msg));
}
	
function divlog(what, is_error)
{
	//console.log('divlog: '+ what);
	if (isDefined(is_error) && is_error) what = '<span class="class-error">'+ what +"</span>";
	w3_el_softfail('id-debugdiv').innerHTML += what +"<br />";
}

function kiwi_show_msg(s)
{
   html('id-kiwi-msg').innerHTML = s;
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
   if (msg) console.log('console.trace: '+ msg);
	try { console.trace(); } catch(ex) {}		// no console.trace() on IE
}

function kiwi_trace_mobile(msg)
{
   alert(msg +' '+ Error().stack);
}
