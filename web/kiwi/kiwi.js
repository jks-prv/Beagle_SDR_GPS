// KiwiSDR
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

var WATERFALL_CALIBRATION_DEFAULT = -13;
var SMETER_CALIBRATION_DEFAULT = -13;

var rx_chans, rx_chan;
var try_again="";
var conn_type;
var seriousError = false;

var toggle_e = {
	SET : 1,
	FROM_COOKIES : 2
};

var modes_u = { 0:'AM', 1:'AMN', 2:'USB', 3:'LSB', 4:'CW', 5:'CWN', 6:'NBFM' };
var modes_l = { 0:'am', 1:'amn', 2:'usb', 3:'lsb', 4:'cw', 5:'cwn', 6:'nbfm' };
var modes_s = { 'am':0, 'amn':1, 'usb':2, 'lsb':3, cw:4, 'cwn':5, 'nbfm':6 };

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
			// A slight hack. For the client connection extint_ws is set here to ws_aud so that
			// calls to e.g. ext_send() for password validation will work. But extint_ws will get
			// overwritten when the first extension is loaded. But this should be okay since by
			// then any non-ext use of ext_send() by the client code should be finished.
			// The admin and mfg code never uses any extensions, and can call ext_send() anytime.
			extint_ws = owrx_ws_open_snd(kiwi_open_ws_cb, { conn_type:conn_type, stream:'AUD' });
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
	visible_block('id-kiwi-msg', 1);
	document.pform.pwd.focus();
	document.pform.pwd.select();
}

var body_loaded = false;

var dbgUs = false;
var dbgUsFirst = true;

function kiwi_valpwd1_cb(badp, p)
{
	//console.log("kiwi_valpwd1_cb conn_type="+ p.conn_type +' badp='+ badp);

	if (badp) {
		kiwi_ask_pwd();
		try_again = 'Try again. ';
	} else {
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
	visible_block('id-kiwi-msg', 0);
	
	if (initCookie('ident', "").startsWith('ZL/KF6VO')) dbgUs = true;

	if (!body_loaded) {
		body_loaded = true;

		if (p.conn_type != 'kiwi')	// kiwi interface delays visibility until some other initialization finishes
			visible_block('id-kiwi-container', 1);
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

var cfg = { };
var adm = { };

function cfg_save_json(path, ws)
{
	//console.log('cfg_save_json: path='+ path);

	var s;
	if (path.startsWith('adm.')) {
		s = encodeURIComponent(JSON.stringify(adm));
		ws.send('SET save_adm='+ s);
	} else {
		s = encodeURIComponent(JSON.stringify(cfg));
		ws.send('SET save_cfg='+ s);
	}
}

var comp_ctr;
var version_maj = -1, version_min = -1;
var tflags = { INACTIVITY:1, WF_SM_CAL:2, WF_SM_CAL2:4 };
var chan_no_pwd;

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
			config_cb(o.r, o.g, o.s, o.pu, o.po, o.pv, o.n, o.m, o.v1, o.v2);
			break;					

		case "update_cb":
			//console.log('update_cb='+ param[1]);
			var o = JSON.parse(param[1]);
			update_cb(o.p, o.i, o.r, o.g, o.v1, o.v2, o.p1, o.p2,
				decodeURIComponent(o.d), decodeURIComponent(o.t));
			break;					

		case "stats_cb":
			//console.log('stats_cb='+ param[1]);
			var o = JSON.parse(param[1]);
			cpu_stats_cb(o.ct, o.cu, o.cs, o.ci, o.ce);
			audio_stats_cb(o.aa, o.aw, o.af, o.at, o.ah, o.as);
			gps_stats_cb(o.ga, o.gt, o.gg, o.gf, o.gc, o.go);
			admin_stats_cb(o.ad, o.au, o.ae);
			break;					

		case "status_msg_text":
			kiwi_status_msg(decodeURIComponent(param[1]).replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\n/g, '<br>'));
			break;

		case "status_msg_html":
			var el = w3_el_id('id-status-msg');
			if (!el) break;
			el.innerHTML = decodeURIComponent(param[1]);		// overwrites last status msg
			break;

		case "down":
			kiwi_down(param[1], comp_ctr);
			break;

		case "comp_ctr":
			comp_ctr = param[1];
			break;
		
		default:
			rtn = false;
			break;
	}
	
	//console.log('>>> '+ ws.stream + ' kiwi_msg: '+ param[0] +' RTN='+ rtn);
	return rtn;
}

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

function kiwi_geolocate()
{
	// FIXME if one times-out try the other
	//kiwi_ajax('http://freegeoip.net/json/', 'callback_freegeoip', function() { setTimeout('kiwi_geolocate();', 1000); } );
	kiwi_ajax('http://ipinfo.io/json/', 'callback_ipinfo', function() {setTimeout(kiwi_geolocate, 1000); } );
}

var geo = "";
var geojson = "";

function callback_freegeoip(json)
{
	console.log('callback_freegeoip():');
	console.log(json);
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
	//console.log(geo);
	//traceA('***geo=<'+geo+'>');
}
    
function callback_ipinfo(json)
{
	console.log('callback_ipinfo():');
	console.log(json);
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
	//console.log(geo);
	//traceA('***geo=<'+geo+'>');
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
	//traceA('kiwi_geo()=<'+geo+'>');
	return encodeURIComponent(geo);
}

function kiwi_geojson()
{
	//traceA('kiwi_geo()=<'+geo+'>');
	return encodeURIComponent(geojson);
}

function kiwi_plot_max(b)
{
   var t = bi[b];
   var plot_max = 1024 / (t.samplerate/t.plot_samplerate);
   return plot_max;
}

function kiwi_too_busy(rx_chans)
{
	html('id-kiwi-msg').innerHTML=
	'Sorry, the KiwiSDR server is too busy right now ('+ rx_chans+((rx_chans>1)? ' users':' user') +' max). <br>' +
	'Please check <a href="http://sdr.hu/?top=kiwi" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.';
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container', 0);
}

function kiwi_up(up)
{
	if (!seriousError) {
		visible_block('id-kiwi-msg', 0);
		visible_block('id-kiwi-container', 1);
	}
}

function kiwi_down(update_in_progress, comp_ctr)
{
	//console.log("kiwi_down enter "+ comp_ctr);
	var s;
	if (parseInt(update_in_progress)) {
		s = 
		'Sorry, software update in progress. Please check back in a few minutes.<br>' +
		'Or check <a href="http://sdr.hu/?top=kiwi" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.<br>' +
		'';
		
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
	} else {
		s =
		//'<span style="position:relative; float:left"><a href="http://bluebison.net" target="_blank"><img id="id-left-logo" src="gfx/kiwi-with-headphones.51x67.png" /></a> ' +
		//<div id="id-left-logo-text"><a href="http://bluebison.net" target="_blank">&copy; bluebison.net</a></div>' +
		//</span><span style="position:relative; float:right">' +
		'Sorry, this KiwiSDR server is being used for development right now. <br>' +
		//"Sorry, a big storm has blown down this KiwiSDR's antenna. <br>" +
		'Please check <a href="http://sdr.hu/?top=kiwi" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.' +
		//"<b>We're moving!</b> <br> This KiwiSDR receiver will be down until the antenna is relocated. <br> Thanks for your patience.<br><br>" +
		//'Until then, please try the <a href="http://websdr.ece.uvic.ca" target="_blank">KiwiSDR at the University of Victoria</a>.' +
		//'</span>';
		' ';
	}
	html('id-kiwi-msg').innerHTML = s;
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container', 0);
}

function kiwi_fft()
{
	if (0) {
		toggle_or_set_spec(1);
		setmaxdb(10);
	} else {
		setmaxdb(-30);
	}
}

function gps_stats_cb(acquiring, tracking, good, fixes, adc_clock, adc_clk_corr)
{
	html("id-msg-gps").innerHTML = 'GPS: acquire '+(acquiring? 'yes':'pause')+', track '+tracking+', good '+good+', fixes '+ fixes.toUnits();
	if (adc_clk_corr)
		html("id-msg-gps").innerHTML += ', ADC clock '+adc_clock.toFixed(6)+' ('+ adc_clk_corr.toUnits()  +' avgs)';
}

function admin_stats_cb(audio_dropped, underruns, seq_errors)
{
	var el = w3_el_id('id-msg-status');
	if (el) {
		el.innerHTML = 'Errors: '+ audio_dropped +' dropped, '+ underruns +' underruns, '+ seq_errors +' sequence';
	}
}


////////////////////////////////
// status information
////////////////////////////////

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
		//msg_send('SET CHECK_UPDATE');
		need_config = false;
	}
	msg_send('SET STATS_UPD ch='+ rx_chan);
	setTimeout(stats_update, stats_interval);
}

function update_TOD()
{
	//console.log('update_TOD');
	var i1 = html('id-info-1');
	var i2 = html('id-info-2');
	var d = new Date();
	var s = d.getSeconds() % 30;
	//if (s >= 16 && s <= 25) {
	if (true) {
		i1.style.fontSize = i2.style.fontSize = (conn_type == 'admin')? '100%':'60%';
		i1.innerHTML = kiwi_cpu_stats_str;
		i2.innerHTML = kiwi_audio_stats_str;
	} else {
		i1.style.fontSize = i2.style.fontSize = '85%';
		i1.innerHTML = d.toString();
		i2.innerHTML = d.toUTCString();
	}
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

function config_cb(rx_chans, gps_chans, serno, pub, port, pvt, nm, mac, vmaj, vmin)
{
	var s;
	kiwi_config_str = 'Config: v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR channels, '+ gps_chans +' GPS channels';
	html("id-msg-config").innerHTML = kiwi_config_str;

	var msg_config2 = w3_el_id("id-msg-config2");
	if (msg_config2)
		msg_config2.innerHTML = 'KiwiSDR serial number: '+ serno;

	var net_config = w3_el_id("id-net-config");
	if (net_config)
		net_config.innerHTML =
			"Public IP address (outside your firewall/router): "+ pub +"<br>\n" +
			"Private IP address (inside your firewall/router): "+ pvt +"<br>\n" +
			"Netmask: /"+ nm +"<br>\n" +
			"KiwiSDR listening on TCP port number: "+ port +"<br>\n" +
			"Ethernet MAC address: "+ mac.toUpperCase();
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
			s += '<br>Update pending';
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
// debug
////////////////////////////////

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
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container', 0);
	seriousError = true;
}

function kiwi_serious_error(s)
{
	html('id-kiwi-msg').innerHTML = s;
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container', 0);
	seriousError = true;
}

function kiwi_trace()
{
	try { console.trace(); } catch(ex) {}		// no console.trace() on IE
}

var tr1=0;
var tr2=0;

function trace_on()
{
	if (!tr1) html('trace').innerHTML = 'trace:';
	if (!tr2) html('trace2').innerHTML = 'trace:';
	visible_inline('tr1', 1);
	visible_inline('tr2', 1);
}

function trace(s)
{
   trace_on();
   tr1++;
   html('trace').innerHTML = 'trace: '+s;
   //traceA(s);
}

function traceA(s)
{
   trace_on();
   tr1++;
   //if (tr1==30) tr1=0;
   html('trace').innerHTML += " "+s;
}

function traceB(s)
{
   trace_on();
	tr2 += s.length;
   if (tr2>=150) tr2=0;
   html('trace2').innerHTML += " "+s;
}
