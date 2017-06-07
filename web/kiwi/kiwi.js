// KiwiSDR
//
// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

var WATERFALL_CALIBRATION_DEFAULT = -13;
var SMETER_CALIBRATION_DEFAULT = -13;

var rx_chans, rx_chan;
var try_again="";
var conn_type;
var seriousError = false;

var modes_u = { 0:'AM', 1:'AMN', 2:'USB', 3:'LSB', 4:'CW', 5:'CWN', 6:'NBFM' };
var modes_l = { 0:'am', 1:'amn', 2:'usb', 3:'lsb', 4:'cw', 5:'cwn', 6:'nbfm' };
var modes_s = { 'am':0, 'amn':1, 'usb':2, 'lsb':3, cw:4, 'cwn':5, 'nbfm':6 };

var timestamp;

// see document.onreadystatechange for how this is called
function kiwi_bodyonload(error)
{
	if (error != '') {
		kiwi_serious_error(error);
	} else {
		conn_type = html('id-kiwi-container').getAttribute('data-type');
		if (conn_type == 'undefined') conn_type = 'kiwi';
		console.log('conn_type='+ conn_type);
		
		var d = new Date();
		timestamp = d.getTime();
		
		if (conn_type == 'kiwi') {
			// A slight hack. For a user connection extint_ws is set here to ws_snd so that
			// calls to e.g. ext_send() for password validation will work. But extint_ws will get
			// overwritten when the first extension is loaded. But this should be okay since
			// subsequent uses of ext_send (mostly via ext_hasCredential/ext_valpwd) can specify
			// an explicit web socket to use (e.g. ws_wf).
			extint_ws = owrx_ws_open_snd(kiwi_open_ws_cb, { conn_type:conn_type });
		} else {
			// e.g. admin or mfg connections
			extint_ws = kiwi_ws_open(conn_type, kiwi_open_ws_cb, { conn_type:conn_type });
		}
	}
}

function kiwi_open_ws_cb(p)
{
	if (p.conn_type != 'kiwi')
		setTimeout(function() { setInterval(function() { ext_send("SET keepalive") }, 5000) }, 5000);

	// always check the first time in case not having a pwd is accepted by local subnet match
	ext_hasCredential(p.conn_type, kiwi_valpwd1_cb, p);
}

function kiwi_ask_pwd()
{
	console.log('kiwi_ask_pwd chan_no_pwd='+ chan_no_pwd);
	html('id-kiwi-msg').innerHTML = "KiwiSDR: software-defined receiver <br>"+
		(chan_no_pwd? 'All channels busy that don\'t require a password ('+ chan_no_pwd +'/'+ rx_chans +')<br>':'') +
		"<form name='pform' action='#' onsubmit='ext_valpwd(\""+ conn_type +"\", this.pwd.value); return false;'>"+
			try_again +"Password: <input type='text' size=10 name='pwd' onclick='this.focus(); this.select()'>"+
		"</form>";
	visible_block('id-kiwi-msg-container', 1);
	document.pform.pwd.focus();
	document.pform.pwd.select();
}

var body_loaded = false;

var dbgUs = false;
var dbgUsFirst = true;

function kiwi_valpwd1_cb(badp, p)
{
	//console.log("kiwi_valpwd1_cb conn_type="+ p.conn_type +' badp='+ badp);

	if (badp == 1) {
		kiwi_ask_pwd();
		try_again = 'Try again. ';
	} else
	if (badp == 2) {
	   return;     // wasn't able to validate so just ignore connection attempt
	} else
	if (badp == 0) {
		if (p.conn_type == 'kiwi') {
		
			// For the client connection, repeat the auth process for the second websocket.
			// It should always work since we only get here if the first auth has worked.
			extint_ws = owrx_ws_open_wf(kiwi_open_ws_cb2, p);
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
	html('id-kiwi-msg').innerHTML = "";
	visible_block('id-kiwi-msg-container', 0);
	
	if (initCookie('ident', "").startsWith('ZL/KF6VO')) dbgUs = true;

	if (!body_loaded) {
		body_loaded = true;

		if (p.conn_type != 'kiwi')	{	// kiwi interface delays visibility until some other initialization finishes
			visible_block('id-kiwi-msg-container', 0);
			visible_block('id-kiwi-container', 1);
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
	init_f = ext_get_cfg_param('init.freq', init_f);
	init_frequency = override_freq? override_freq : init_f;

	var init_m = (init_mode == undefined)? modes_s['lsb'] : modes_s[init_mode];
	init_m = ext_get_cfg_param('init.mode', init_m);
	init_mode = override_mode? override_mode : modes_l[init_m];

	var init_z = (init_zoom == undefined)? 0 : init_zoom;
	init_z = ext_get_cfg_param('init.zoom', init_z);
	init_zoom = override_zoom? override_zoom : init_z;

	var init_max = (init_max_dB == undefined)? -10 : init_max_dB;
	init_max = ext_get_cfg_param('init.max_dB', init_max);
	init_max_dB = override_max_dB? override_max_dB : init_max;

	var init_min = (init_min_dB == undefined)? -110 : init_min_dB;
	init_min = ext_get_cfg_param('init.min_dB', init_min);
	init_min_dB = override_min_dB? override_min_dB : init_min;
	
	console.log('INIT f='+ init_frequency +' m='+ init_mode +' z='+ init_zoom
		+' min='+ init_min_dB +' max='+ init_max_dB);

	w3_call('init_scale_dB');

	var ant = ext_get_cfg_param('rx_antenna');
	var el = w3_el_id('rx-antenna');
	if (el != undefined && ant) {
		el.innerHTML = 'Antenna: '+ decodeURIComponent(ant);
	}
}


////////////////////////////////
// configuration
////////////////////////////////

var cfg = { };
var adm = { };

function cfg_save_json(path, ws)
{
	//console.log('cfg_save_json: path='+ path);

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
}

////////////////////////////////
// geolocation
////////////////////////////////

function kiwi_geolocate()
{
	// manually: curl ipinfo.io/<IP_address>
   kiwi_ajax('http://ipinfo.io/json/',
   //kiwi_ajax('http://grn:80/foo',
      function(json) {
         if (json.AJAX_error === undefined)
            ipinfo_cb(json);
         else
            // manually: curl freegeoip.net/json/<IP_address>
            // NB: trailing '/' in '/json/' in the following:
            kiwi_ajax('http://freegeoip.net/json/', 'freegeoip_cb');
      }, 10000
   );
}

var geo = "";
var geojson = "";

function freegeoip_cb(json)
{
	console.log('freegeoip_cb():');
	console.log(json);
	
	if (json.AJAX_error != undefined)
		return;
	
	if (window.JSON && window.JSON.stringify)
      geojson = JSON.stringify(json);
   else
      geojson = json.toString();
	
	if (json.country_code && json.country_code == "US" && json.region_name) {
		json.country_name = json.region_name + ', USA';
	}
	
	geo = "";
	if (json.city)
		geo += json.city;
	if (json.country_name)
		geo += (json.city? ', ':'')+ json.country_name;
   //kiwi_debug('freegeoip_cb='+ geo);
}
    
function ipinfo_cb(json)
{
	console.log('ipinfo_cb():');
	console.log(json);
	
	if (json.AJAX_error != undefined)
		return;
	
	if (window.JSON && window.JSON.stringify)
      geojson = JSON.stringify(json);
   else
		geojson = json.toString();

	if (json.country && json.country == "US" && json.region) {
		json.country = json.region + ', USA';
	} else
	if (json.country && json.country == "HK" && json.city && json.city == 'Hong Kong') {
		json.country = null;
	} else
	if (json.country && country_ISO2_name[json.country]) {
		json.country = country_ISO2_name[json.country];
	}
	
	geo = "";
	if (json.city)
		geo += json.city;
	if (json.country)
		geo += (json.city? ', ':'')+ json.country;
   //kiwi_debug('ipinfo_cb='+ geo);
}

// copied from country.io/names.json on 4/9/2016
// modified "US": "United States" => "US": "USA" to shorten since we include state
var country_ISO2_name =
{"BD": "Bangladesh", "BE": "Belgium", "BF": "Burkina Faso", "BG": "Bulgaria",
"BA": "Bosnia and Herzegovina", "BB": "Barbados", "WF": "Wallis and Futuna",
"BL": "Saint Barthelemy", "BM": "Bermuda", "BN": "Brunei", "BO": "Bolivia",
"BH": "Bahrain", "BI": "Burundi", "BJ": "Benin", "BT": "Bhutan", "JM":
"Jamaica", "BV": "Bouvet Island", "BW": "Botswana", "WS": "Samoa", "BQ":
"Bonaire, Saint Eustatius and Saba ", "BR": "Brazil", "BS": "Bahamas", "JE":
"Jersey", "BY": "Belarus", "BZ": "Belize", "RU": "Russia", "RW": "Rwanda", "RS":
"Serbia", "TL": "East Timor", "RE": "Reunion", "TM": "Turkmenistan", "TJ":
"Tajikistan", "RO": "Romania", "TK": "Tokelau", "GW": "Guinea-Bissau", "GU":
"Guam", "GT": "Guatemala", "GS": "South Georgia and the South Sandwich Islands",
"GR": "Greece", "GQ": "Equatorial Guinea", "GP": "Guadeloupe", "JP": "Japan",
"GY": "Guyana", "GG": "Guernsey", "GF": "French Guiana", "GE": "Georgia", "GD":
"Grenada", "GB": "United Kingdom", "GA": "Gabon", "SV": "El Salvador", "GN":
"Guinea", "GM": "Gambia", "GL": "Greenland", "GI": "Gibraltar", "GH": "Ghana",
"OM": "Oman", "TN": "Tunisia", "JO": "Jordan", "HR": "Croatia", "HT": "Haiti",
"HU": "Hungary", "HK": "Hong Kong", "HN": "Honduras", "HM": "Heard Island and McDonald Islands",
"VE": "Venezuela", "PR": "Puerto Rico", "PS": "Palestinian Territory",
"PW": "Palau", "PT": "Portugal", "SJ": "Svalbard and Jan Mayen",
"PY": "Paraguay", "IQ": "Iraq", "PA": "Panama", "PF": "French Polynesia", "PG":
"Papua New Guinea", "PE": "Peru", "PK": "Pakistan", "PH": "Philippines", "PN":
"Pitcairn", "PL": "Poland", "PM": "Saint Pierre and Miquelon", "ZM": "Zambia",
"EH": "Western Sahara", "EE": "Estonia", "EG": "Egypt", "ZA": "South Africa",
"EC": "Ecuador", "IT": "Italy", "VN": "Vietnam", "SB": "Solomon Islands", "ET":
"Ethiopia", "SO": "Somalia", "ZW": "Zimbabwe", "SA": "Saudi Arabia", "ES":
"Spain", "ER": "Eritrea", "ME": "Montenegro", "MD": "Moldova", "MG":
"Madagascar", "MF": "Saint Martin", "MA": "Morocco", "MC": "Monaco", "UZ":
"Uzbekistan", "MM": "Myanmar", "ML": "Mali", "MO": "Macao", "MN": "Mongolia",
"MH": "Marshall Islands", "MK": "Macedonia", "MU": "Mauritius", "MT": "Malta",
"MW": "Malawi", "MV": "Maldives", "MQ": "Martinique", "MP": "Northern Mariana Islands",
"MS": "Montserrat", "MR": "Mauritania", "IM": "Isle of Man", "UG":
"Uganda", "TZ": "Tanzania", "MY": "Malaysia", "MX": "Mexico", "IL": "Israel",
"FR": "France", "IO": "British Indian Ocean Territory", "SH": "Saint Helena",
"FI": "Finland", "FJ": "Fiji", "FK": "Falkland Islands", "FM": "Micronesia",
"FO": "Faroe Islands", "NI": "Nicaragua", "NL": "Netherlands", "NO": "Norway",
"NA": "Namibia", "VU": "Vanuatu", "NC": "New Caledonia", "NE": "Niger", "NF":
"Norfolk Island", "NG": "Nigeria", "NZ": "New Zealand", "NP": "Nepal", "NR":
"Nauru", "NU": "Niue", "CK": "Cook Islands", "XK": "Kosovo", "CI": "Ivory Coast",
"CH": "Switzerland", "CO": "Colombia", "CN": "China", "CM": "Cameroon",
"CL": "Chile", "CC": "Cocos Islands", "CA": "Canada", "CG": "Republic of the Congo",
"CF": "Central African Republic", "CD": "Democratic Republic of the Congo",
"CZ": "Czech Republic", "CY": "Cyprus", "CX": "Christmas Island", "CR":
"Costa Rica", "CW": "Curacao", "CV": "Cape Verde", "CU": "Cuba", "SZ":
"Swaziland", "SY": "Syria", "SX": "Sint Maarten", "KG": "Kyrgyzstan", "KE":
"Kenya", "SS": "South Sudan", "SR": "Suriname", "KI": "Kiribati", "KH":
"Cambodia", "KN": "Saint Kitts and Nevis", "KM": "Comoros", "ST": "Sao Tome and Principe",
"SK": "Slovakia", "KR": "South Korea", "SI": "Slovenia", "KP": "North Korea",
"KW": "Kuwait", "SN": "Senegal", "SM": "San Marino", "SL": "Sierra Leone",
"SC": "Seychelles", "KZ": "Kazakhstan", "KY": "Cayman Islands", "SG":
"Singapore", "SE": "Sweden", "SD": "Sudan", "DO": "Dominican Republic", "DM":
"Dominica", "DJ": "Djibouti", "DK": "Denmark", "VG": "British Virgin Islands",
"DE": "Germany", "YE": "Yemen", "DZ": "Algeria", "US": "USA", "UY":
"Uruguay", "YT": "Mayotte", "UM": "United States Minor Outlying Islands", "LB":
"Lebanon", "LC": "Saint Lucia", "LA": "Laos", "TV": "Tuvalu", "TW": "Taiwan",
"TT": "Trinidad and Tobago", "TR": "Turkey", "LK": "Sri Lanka", "LI":
"Liechtenstein", "LV": "Latvia", "TO": "Tonga", "LT": "Lithuania", "LU":
"Luxembourg", "LR": "Liberia", "LS": "Lesotho", "TH": "Thailand", "TF": "French Southern Territories",
"TG": "Togo", "TD": "Chad", "TC": "Turks and Caicos Islands", "LY": "Libya",
"VA": "Vatican", "VC": "Saint Vincent and the Grenadines", "AE": "United Arab Emirates",
"AD": "Andorra", "AG": "Antigua and Barbuda", "AF": "Afghanistan", "AI": "Anguilla", "VI": "U.S. Virgin Islands",
"IS": "Iceland", "IR": "Iran", "AM": "Armenia", "AL": "Albania", "AO": "Angola",
"AQ": "Antarctica", "AS": "American Samoa", "AR": "Argentina", "AU":
"Australia", "AT": "Austria", "AW": "Aruba", "IN": "India", "AX": "Aland Islands",
"AZ": "Azerbaijan", "IE": "Ireland", "ID": "Indonesia", "UA":
"Ukraine", "QA": "Qatar", "MZ": "Mozambique"};

function kiwi_geo()
{
	//console.log('kiwi_geo()=<'+geo+'>');
	return encodeURIComponent(geo);
}

function kiwi_geojson()
{
	//console.log('kiwi_geo()=<'+geo+'>');
	return encodeURIComponent(geojson);
}


////////////////////////////////
// time display
////////////////////////////////

var server_time_utc, server_time_local, server_time_tzid, server_time_tzname;
var time_display_current = true;

function time_display_cb(o)
{
	if (typeof o.tu == 'undefined') return;
	server_time_utc = o.tu;
	server_time_local = o.tl;
	server_time_tzid = decodeURIComponent(o.ti);
	server_time_tzname = decodeURIComponent(o.tn);

	if (!time_display_started) {
		time_display_periodic();
		time_display_started = true;
	} else
		time_display(time_display_current);
}

function time_display(display_time)
{
	var el = w3_el_id('id-time-display-text-inner');
	if (!el) return;

	var noLatLon = (server_time_local == '' || server_time_tzname == 'null');
	w3_el_id('id-time-display-UTC').innerHTML = server_time_utc? server_time_utc : '?';
	w3_el_id('id-time-display-local').innerHTML = noLatLon? '?' : server_time_local;
	w3_el_id('id-time-display-tzname').innerHTML = noLatLon? 'Lat/lon needed for local time' : server_time_tzname;

	w3_el_id('id-time-display-logo-inner').style.opacity = display_time? 0:1;
	w3_el_id('id-time-display-inner').style.opacity = display_time? 1:0;
}

function time_display_periodic()
{
	time_display(time_display_current);
	time_display_current ^= 1;
	setTimeout(function() { time_display_periodic(); }, time_display_current? 50000:10000);
}

var time_display_started = false;
var time_display_prev;

function time_display_setup(id)
{
	var el;
	
	if (time_display_prev) {
		el = w3_el_id(time_display_prev);
		if (el) el.innerHTML = '';
	}
	time_display_prev = id;
	
	var el = w3_el_id(id);
	el.innerHTML =
		'<div id="id-time-display-inner">' +
			'<div id="id-time-display-text-inner">' +
				w3_divs('', 'w3-vcenter',
					w3_divs('', 'w3-show-inline-block',
						w3_div('id-time-display-UTC'),
						w3_div('cl-time-display-text-suffix', 'UTC')
					),
					w3_divs('', 'w3-show-inline-block',
						w3_div('id-time-display-local'),
						w3_div('cl-time-display-text-suffix', 'Local')
					),
					w3_div('id-time-display-tzname w3-hcenter')
				) +
			'</div>' +
		'</div>' +
		'<div id="id-time-display-logo-inner">' +
			'<div id="id-time-display-logo-text">Powered by</div>' +
			'<a href="http://openwebrx.org/" target="_blank"><img id="id-time-display-logo" src="gfx/openwebrx-top-logo.png" /></a>' +
		'</div>';

	time_display(time_display_current);
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
			w3_divs('w3-text-bright-yellow', 'w3-tspace-16',
				w3_divs('', 'w3-medium w3-text-aqua', '<b>User preferences</b>'),
				w3_col_percent('', '',
					w3_input('Pref', 'pref.p', pref.p, 'pref_p_cb', 'something'), 30
				),
				
				w3_divs('', 'w3-show-inline-block w3-hspace-16',
					w3_btn('Export', 'pref_export_btn_cb', ''),
					w3_btn('Import', 'pref_import_btn_cb', ''),
					'<b>Status:</b> ' + w3_inline('id-pref-status w3-snap-back')
				)
			);
	
		extint_panel_hide();
		ext_panel_func('show_pref');
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
	var el = w3_el_id('id-pref-status');
	
	if (perf_status_anim) {
		kiwi_clearTimeout(perf_status_timeout);
		//el.style.color = 'red';
		//el.innerHTML = 'CANCEL';
		w3_unclass_class(el, 'w3-fade-out', 'w3-snap-back');
		setTimeout(function() {
			perf_status_anim = false;
			pref_status(color, msg);
		}, 1000);
		return;
	}
	
	el.style.color = color;
	el.innerHTML = msg;
	w3_unclass_class(el, 'w3-snap-back', 'w3-fade-out');
	perf_status_anim = true;
	perf_status_timeout = setTimeout(function() {
		el.innerHTML = '';
		w3_unclass_class(el, 'w3-fade-out', 'w3-snap-back');
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
	var id = ident_name? ident_name : '_blank_';
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
	var id = ident_name? ident_name : '_blank_';
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
	var id = ident_name? ident_name : '_blank_';
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

var process_return_nexttime = false;

function kiwi_status_msg(s)
{
	var el = w3_el_id('id-status-msg');
	if (!el) return;
	var o = el.innerHTML;
	
	for (var i=0; i < s.length; i++) {
		if (process_return_nexttime) {
			var ci = o.lastIndexOf('<br>');
			if (ci == -1) {
				o = '';
			} else {
				o = o.substring(0, ci+4);
			}
			process_return_nexttime = false;
		}
		var c = s.charAt(i);
		if (c == '\r') {
			process_return_nexttime = true;
		} else
		if (c == '\f') {		// form-feed is how we clear element from appending
			o = '';
		} else {
			o += c;
		}
	}
	el.innerHTML = o;

	if (typeof el.getAttribute != "undefined" && el.getAttribute('data-scroll-down') == 'true')
		el.scrollTop = el.scrollHeight;
}

function gps_stats_cb(acquiring, tracking, good, fixes, adc_clock, adc_clk_corrections)
{
	html("id-msg-gps").innerHTML = 'GPS: acquire '+(acquiring? 'yes':'pause')+', track '+tracking+', good '+good+', fixes '+ fixes.toUnits();
	if (adc_clk_corrections)
		html("id-msg-gps").innerHTML += ', ADC clock '+adc_clock.toFixed(6)+' ('+ adc_clk_corrections.toUnits()  +' avgs)';
}

function admin_stats_cb(audio_dropped, underruns, seq_errors, dpump_resets, dpump_nbufs, dpump_hist)
{
	var el = w3_el_id('id-msg-status');
	if (el) el.innerHTML = 'Errors: '+ audio_dropped +' dropped, '+ underruns +' underruns, '+ seq_errors +' sequence';

	el = w3_el_id('id-status-dpump-resets');
   if (el) el.innerHTML = 'Data pump resets: '+ dpump_resets;
   
	el = w3_el_id('id-status-dpump-hist');
	if (el) {
	   var s = 'Data pump histogram: ';
		for (var i = 0; i < dpump_nbufs; i++) {
		   s += (i? ', ':'') + dpump_hist[i];
		}
      el.innerHTML = s;
	}
}

function kiwi_too_busy(rx_chans)
{
	html('id-kiwi-msg').innerHTML=
	'Sorry, the KiwiSDR server is too busy right now ('+ rx_chans+((rx_chans>1)? ' users':' user') +' max). <br>' +
	'Please check <a href="http://sdr.hu/?top=kiwi" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.';
	visible_block('id-kiwi-msg-container', 1);
	visible_block('id-kiwi-container', 0);
}

function kiwi_up(up)
{
	if (!seriousError) {
		visible_block('id-kiwi-msg-container', 0);
		visible_block('id-kiwi-container', 1);
	}
}

function kiwi_down(type, comp_ctr, reason)
{
	//console.log("kiwi_down comp_ctr="+ comp_ctr);
	var s;
	type = +type;

	if (type == 1) {
		s = 'Sorry, software update in progress. Please check back in a few minutes.<br>' +
			'Or check <a href="http://sdr.hu/?top=kiwi" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.';
		
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
				'Please check <a href="http://sdr.hu/?top=kiwi" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.';
		}
		s = reason;
	}
	
	html('id-kiwi-msg').innerHTML = s;
	visible_block('id-kiwi-msg-container', 1);
	visible_block('id-kiwi-container', 0);
}

var stats_interval = 10000;
var stat_init = false;
var need_config = true;

function stats_init()
{
	stats_update();
	stat_init = true;
}

function stats_update()
{
	if (need_config) {
		msg_send('SET GET_CONFIG');
		need_config = false;
	}
	msg_send('SET STATS_UPD ch='+ rx_chan);
	setTimeout(stats_update, stats_interval);
}

function status_periodic()
{
	//console.log('status_periodic');
	var i1 = w3_el_id('id-info-1');
	var i2 = w3_el_id('id-info-2');
	if (!i1 || !i2) return;
	i1.style.fontSize = i2.style.fontSize = (conn_type == 'admin')? '100%':'60%';
	i1.innerHTML = kiwi_cpu_stats_str;
	i2.innerHTML = kiwi_audio_stats_str;
}

var kiwi_audio_stats_str = "";

function audio_stats_cb(audio_kbps, waterfall_kbps, waterfall_fps, waterfall_total_fps, http_kbps, sum_kbps)
{
	kiwi_audio_stats_str = 'audio '+audio_kbps.toFixed(0)+' kB/s, waterfall '+waterfall_kbps.toFixed(0)+
		' kB/s ('+waterfall_fps.toFixed(0)+'/'+waterfall_total_fps.toFixed(0)+' fps)' +
		', http '+http_kbps.toFixed(0)+' kB/s, total '+sum_kbps.toFixed(0)+' kB/s ('+(sum_kbps*8).toFixed(0)+' kb/s)';
}

var kiwi_cpu_stats_str = "";
var kiwi_config_str = "";

function cpu_stats_cb(uptime_secs, user, sys, idle, ecpu)
{
	kiwi_cpu_stats_str = 'Beagle CPU '+user+'% usr / '+sys+'% sys / '+idle+'% idle, FPGA eCPU '+ecpu.toFixed(0)+'%';
	var t = uptime_secs;
	var sec = Math.trunc(t % 60); t = Math.trunc(t/60);
	var min = Math.trunc(t % 60); t = Math.trunc(t/60);
	var hr  = Math.trunc(t % 24); t = Math.trunc(t/24);
	var days = t;
	var kiwi_uptime_str = ' | Uptime: ';
	if (days) kiwi_uptime_str += days +' '+ ((days > 1)? 'days':'day') +' ';
	kiwi_uptime_str += hr +':'+ min.leadingZeros(2) +':'+ sec.leadingZeros(2);
	html("id-msg-config").innerHTML = kiwi_config_str + kiwi_uptime_str;
}

function config_cb(rx_chans, gps_chans, serno, pub, port_ext, pvt, port_int, nm, mac, vmaj, vmin)
{
	var s;
	kiwi_config_str = 'Config: v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR channels, '+ gps_chans +' GPS channels';
	html("id-msg-config").innerHTML = kiwi_config_str;

	var net_config = w3_el_id("id-net-config");
	if (net_config)
		net_config.innerHTML =
			w3_divs('', '',
				w3_col_percent('', '',
					w3_div('', 'Public IP address (outside your firewall/router): '+ pub +' [port '+ port_ext +']'), 50,
					w3_div('', 'Ethernet MAC address: '+ mac.toUpperCase()), 30,
					w3_div('', 'KiwiSDR serial number: '+ serno), 20
				),
				w3_col_percent('', '',
					w3_div('', 'Private IP address (inside your firewall/router): '+ pvt +' [port '+ port_int +']'), 50,
					w3_div('', 'Private netmask: /'+ nm), 50
				)
			);
}

function update_cb(pending, in_progress, rx_chans, gps_chans, vmaj, vmin, pmaj, pmin, build_date, build_time)
{
	kiwi_config_str = 'Config: v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR channels, '+ gps_chans +' GPS channels';
	html("id-msg-config").innerHTML = kiwi_config_str;

	var msg_update = w3_el_id("id-msg-update");
	
	if (msg_update) {
		var s;
		s = 'Installed version: v'+ vmaj +'.'+ vmin +', built '+ build_date +' '+ build_time;
		if (in_progress) {
			s += '<br>Update to version v'+ + pmaj +'.'+ pmin +' in progress';
		} else
		if (pending) {
			s += '<br>Update check pending';
		} else
		if (pmaj == -1) {
			s += '<br>Available version: unknown until checked';
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

function users_init()
{
	console.log("users_init #rx="+ rx_chans);
	for (var i=0; i < rx_chans; i++) divlog('RX'+i+': <span id="id-user-'+i+'"></span>');
	users_update();
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
		var s = '';
		var i = obj.i;
		var name = obj.n;
		var freq = obj.f;
		var geoloc = obj.g;
		var ip = (typeof obj.a != 'undefined' && obj.a != '')? (obj.a +', ') : '';
		var mode = obj.m;
		var zoom = obj.z;
		var connected = obj.t;
		var ext = obj.e;
		
		if (typeof name != 'undefined') {
			var id = kiwi_strip_tags(decodeURIComponent(name), '');
			if (id != '') id = '"'+ id + '" ';
			var g = (geoloc == '(null)' || geoloc == '')? 'unknown location' : decodeURIComponent(geoloc);
			ip = ip.replace(/::ffff:/, '');		// remove IPv4-mapped IPv6 if any
			g = '('+ ip + g +') ';
			var f = (freq/1000).toFixed(2);
			var f_s = f + ' kHz ';
			var anchor = '<a href="javascript:tune('+ f +','+ q(mode) +','+ zoom +');">';
			if (ext != '') ext = decodeURIComponent(ext) +' ';
			s = id + g + anchor + f_s + mode +' z'+ zoom +'</a> '+ ext + connected;
		}
		
		//if (s != '') console.log('user'+ i +'='+ s);
		if (user_init) html('id-user-'+ i).innerHTML = s;
	});
	
}


////////////////////////////////
// misc
////////////////////////////////

var toggle_e = {
	SET : 1,
	FROM_COOKIES : 2
};

// set depending on flags: cookie value, set value, previous value, no change
function kiwi_toggle(set, val, prev, cookie)
{
	var rv = null;
	if (set & toggle_e.FROM_COOKIES) {
		var v = parseFloat(readCookie(cookie));
		if (!isNaN(v)) {
			rv = v;
			//console.log('kiwi_toggle COOKIE '+ cookie +'='+ rv);
		}
	}
	if (rv == null) {
		if (set & toggle_e.SET) {
			rv = val;
			//console.log('kiwi_toggle SET '+ cookie +'='+ rv);
		}
	}
	
	return ((rv == null)? prev : rv);
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
		toggle_or_set_spec(1);
		setmaxdb(10);
	} else {
		setmaxdb(-30);
	}
}


////////////////////////////////
// control messages
////////////////////////////////

var comp_ctr, reason_disabled = '';
var version_maj = -1, version_min = -1;
var tflags = { INACTIVITY:1, WF_SM_CAL:2, WF_SM_CAL2:4 };
var chan_no_pwd;
var gps = { };
var pref_import_ch;

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

		case "badp":
			extint_valpwd_cb(parseInt(param[1]));
			break;					

		case "chan_no_pwd":
			chan_no_pwd = parseInt(param[1]);
			break;					

		case "rx_chans":
			rx_chans = parseInt(param[1]);
			break;

		case "rx_chan":
			rx_chan = parseInt(param[1]);
			break;

		case "load_cfg":
			var cfg_json = decodeURIComponent(param[1]);
			//console.log('### load_cfg '+ ws.stream +' '+ cfg_json.length);
			cfg = JSON.parse(cfg_json);
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
			dx(JSON.parse(param[1]));
			break;					

		case "user_cb":
			//console.log('user_cb='+ param[1]);
			user_cb(JSON.parse(param[1]));
			break;					

		case "config_cb":
			//console.log('config_cb='+ param[1]);
			var o = JSON.parse(param[1]);
			config_cb(o.r, o.g, o.s, o.pu, o.pe, o.pv, o.pi, o.n, o.m, o.v1, o.v2);
			break;					

		case "update_cb":
			//console.log('update_cb='+ param[1]);
			var o = JSON.parse(param[1]);
			update_cb(o.p, o.i, o.r, o.g, o.v1, o.v2, o.p1, o.p2,
				decodeURIComponent(o.d), decodeURIComponent(o.t));
			break;					

		case "gps_update_cb":
			//console.log('gps_update_cb='+ param[1]);
			try {
				var gps_json = decodeURIComponent(param[1]);
				gps = JSON.parse(gps_json);
				w3_call('gps_update_admin_cb');
			} catch(ex) {
				console.log('<'+ param[1] +'>');
				console.log('kiwi_msg() gps_update_cb: JSON parse fail');
			}
			break;					

		case "stats_cb":
			//console.log('stats_cb='+ param[1]);
			try {
				var o = JSON.parse(param[1]);
				cpu_stats_cb(o.ct, o.cu, o.cs, o.ci, o.ce);
				audio_stats_cb(o.aa, o.aw, o.af, o.at, o.ah, o.as);
				gps_stats_cb(o.ga, o.gt, o.gg, o.gf, o.gc, o.go);
				admin_stats_cb(o.ad, o.au, o.ae, o.ar, o.an, o.ap);
				time_display_cb(o);
			} catch(ex) {
				console.log('<'+ param[1] +'>');
				console.log('kiwi_msg() stats_cb: JSON parse fail');
				console.log(ex);
			}
			break;					

		case "status_msg_text":
			kiwi_status_msg(decodeURIComponent(param[1]).replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\n/g, '<br>'));
			break;

		case "status_msg_html":
			var el = w3_el_id('id-status-msg');
			if (!el) break;
			el.innerHTML = decodeURIComponent(param[1]);		// overwrites last status msg
			break;
		
		case "authkey_cb":
			extint_authkey_cb(param[1]);
			break;

		case "down":
			kiwi_down(param[1], comp_ctr, reason_disabled);
			break;

		case "comp_ctr":
			comp_ctr = param[1];
			break;
		
		// can't simply come from 'cfg.*' because config isn't available without a web socket
		case "reason_disabled":
			reason_disabled = decodeURIComponent(param[1]);
			break;
		
		case "sample_rate":
			extint_sample_rate(param[1]);
			break;
		
		case 'pref_import_ch':
			pref_import_ch = +param[1];
			break;

		case 'pref_import':
			pref_import_cb(param[1], pref_import_ch);
			break;

		default:
			rtn = false;
			break;
	}
	
	//console.log('>>> '+ ws.stream + ' kiwi_msg: '+ param[0] +' RTN='+ rtn);
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
	if (typeof is_error !== "undefined" && is_error) what = '<span class="class-error">'+ what +"</span>";
	html('id-debugdiv').innerHTML += what +"<br />";
}

function kiwi_server_error(s)
{
	html('id-kiwi-msg').innerHTML =
	'Hmm, there seems to be a problem. <br> \
	The server reported the error: <span style="color:red">'+s+'</span> <br> \
	Please <a href="javascript:sendmail(\'ihpCihp-`ln\',\'server error: '+s+'\');">email me</a> the above message. Thanks!';
	visible_block('id-kiwi-msg-container', 1);
	visible_block('id-kiwi-container', 0);
	seriousError = true;
}

function kiwi_serious_error(s)
{
	html('id-kiwi-msg').innerHTML = s;
	visible_block('id-kiwi-msg-container', 1);
	visible_block('id-kiwi-container', 0);
	seriousError = true;
	console.log(s);
}

function kiwi_trace()
{
	try { console.trace(); } catch(ex) {}		// no console.trace() on IE
}
