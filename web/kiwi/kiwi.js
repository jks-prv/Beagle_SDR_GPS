// KiwiSDR
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

// Now -18 instead of -12 because of fix to signed multiplies in IQ mixers resulting in
// shift left one bit (+6 dB)
var WATERFALL_CALIBRATION_DEFAULT = -10;
var SMETER_CALIBRATION_DEFAULT = -13;

var try_again="";
var conn_type;
var seriousError = false;

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

// see document.onreadystatechange for how this is called
function kiwi_bodyonload()
{
	conn_type = html('id-kiwi-container').getAttribute('data-type');
	if (conn_type == "undefined") conn_type = "kiwi";
	console.log("conn_type="+ conn_type);
	
	// always check the first time in case not having a pwd is accepted by local subnet match
	ext_hasCredential(conn_type, kiwi_valpwd_cb);
	try_again = "Try again. ";
}

function kiwi_ask_pwd()
{
	html('id-kiwi-msg').innerHTML = "KiwiSDR: software-defined receiver <br>"+
		"<form name='pform' action='#' onsubmit='ext_valpwd(\""+ conn_type +"\", this.pwd.value); return false;'>"+
			try_again +"Password: <input type='text' size=10 name='pwd' onclick='this.focus(); this.select()'>"+
		"</form>";
	visible_block('id-kiwi-msg', 1);
	document.pform.pwd.focus();
	document.pform.pwd.select();
}

var body_loaded = false;
var timestamp;

var dbgUs = false;
var dbgUsFirst = true;

function kiwi_valpwd_cb(badp)
{
	console.log("kiwi_valpwd_cb conn_type="+ conn_type +' badp='+ badp);
	if (!badp) {
		html('id-kiwi-msg').innerHTML = "";
		visible_block('id-kiwi-msg', 0);
		
		if (initCookie('ident', "").search('ZL/KF6VO') != -1) {
			dbgUs = true;
		}

		if (!body_loaded) {
			body_loaded = true;
			var d = new Date();
			timestamp = d.getTime();

			if (conn_type != 'kiwi')	// kiwi interface delays visibility until some other initialization
				visible_block('id-kiwi-container', 1);
			console.log("calling "+ conn_type+ "_main()..");
			var interface_cb = conn_type +'_main()';
			if (conn_type == 'kiwi') {
				kiwi_main();		// FIXME without eval() & try{} so get backtrace info
			} else {
				try {
					eval(interface_cb);
				} catch(ex) {
					console.log('EX: '+ ex);
					console.log('kiwi_valpwd_cb: no interface routine for '+ conn_type +'?');
				}
			}
		} else {
			console.log("kiwi_valpwd_cb: body_loaded previously!");
			return;
		}

	} else {
		console.log("kiwi_valpwd_cb: try again");
		kiwi_ask_pwd();
	}
}

var cfg = { };
var comp_ctr;

function cfg_save_json(ws)
{
	var s = encodeURIComponent(JSON.stringify(cfg));
	ws.send('SET save='+ s);
}

var version_maj = -1, version_min = -1;
var tflags = { INACTIVITY:1, WF_SM_CAL:2, WF_SM_CAL2:4 };

function kiwi_msg(param, ws)
{
	switch (param[0]) {
		case "version_maj":
			version_maj = parseInt(param[1]);
			break;
			
		case "version_min":
			version_min = parseInt(param[1]);
			break;
			
		case "load_cfg":
			var cfg_json = decodeURIComponent(param[1]);
			//console.log('### load_cfg '+ ws.stream +' '+ cfg_json.length);
			cfg = JSON.parse(cfg_json);
			var update_cfg = false;
			var update_flags = false;

			// if not configured, take value from config.js, if present, for backward compatibility

			var init_f = ext_get_cfg_param('init.freq');
			if (init_f == null || init_f == undefined) {
				init_f = (init_frequency == undefined)? 7020 : init_frequency;
				ext_set_cfg_param('init.freq', init_f);
				update_cfg = true;
			}
			init_frequency = override_freq? override_freq : init_f;

			var init_m = ext_get_cfg_param('init.mode');
			if (init_m == null || init_m == undefined) {
				init_m = (init_mode == undefined)? modes_s['lsb'] : modes_s[init_mode];
				ext_set_cfg_param('init.mode', init_m);
				update_cfg = true;
			}
			init_mode = override_mode? override_mode : modes_l[init_m];

			var init_z = ext_get_cfg_param('init.zoom');
			if (init_z == null || init_z == undefined) {
				init_z = (init_zoom == undefined)? 0 : init_zoom;
				ext_set_cfg_param('init.zoom', init_z);
				update_cfg = true;
			}
			init_zoom = override_zoom? override_zoom : init_z;

			var init_max = ext_get_cfg_param('init.max_dB');
			if (init_max == null || init_max == undefined) {
				init_max = (init_max_dB == undefined)? -10 : init_max_dB;
				ext_set_cfg_param('init.max_dB', init_max);
				update_cfg = true;
			}
			init_max_dB = override_max_dB? override_max_dB : init_max;

			var init_min = ext_get_cfg_param('init.min_dB');
			if (init_min == null || init_min == undefined) {
				init_min = (init_min_dB == undefined)? -110 : init_min_dB;
				ext_set_cfg_param('init.min_dB', init_min);
				update_cfg = true;
			}
			init_min_dB = override_min_dB? override_min_dB : init_min;
			
			if (ws.stream == 'AUD' || ws.stream == 'FFT')
				init_scale_dB();
			
			console.log('INIT f='+ init_frequency +' m='+ init_mode +' z='+ init_zoom
				+' min='+ init_min_dB +' max='+ init_max_dB +' update='+ update_cfg);
			
			var ant = cfg.rx_antenna;
			ant = (ant != undefined && ant != null)? decodeURIComponent(ant) : null;
			var el = html_idname('rx-antenna');
			if (el != undefined && ant) {
				el.innerHTML = 'Antenna: '+ ant;
			}
			
			// XXX TRANSITIONAL
			var transition_flags = ext_get_cfg_param('transition_flags');
			console.log('** transition_flags='+ transition_flags);
			if (typeof transition_flags == 'undefined') {
				console.log('** init transition_flags=0');
				transition_flags = 0;
				update_flags = true;
				update_cfg = true;
			}

			// XXX TRANSITIONAL
			var contact_admin = ext_get_cfg_param('contact_admin');
			if (typeof contact_admin == 'undefined') {
				// hasn't existed before: default to true
				console.log('** contact_admin: INIT true');
				ext_set_cfg_param('contact_admin', true);
				update_cfg = true;
			}

			// setup observed typical I/Q DC offsets, admin can fine-tune with IQ display extension
			var DC_offset_I = ext_get_cfg_param('DC_offset_I');
			if (typeof DC_offset_I == 'undefined' || DC_offset_I == 0) {
				// hasn't existed before
				console.log('** DC_offset_I: INIT');
				ext_set_cfg_param('DC_offset_I', 5.0e-02);
				update_cfg = true;
			}

			var DC_offset_Q = ext_get_cfg_param('DC_offset_Q');
			if (typeof DC_offset_Q == 'undefined' || DC_offset_Q == 0) {
				// hasn't existed before
				console.log('** DC_offset_Q: INIT');
				ext_set_cfg_param('DC_offset_Q', 5.0e-02);
				update_cfg = true;
			}

			// XXX TRANSITIONAL
			if ((transition_flags & tflags.WF_SM_CAL2) == 0) {
				ext_set_cfg_param('waterfall_cal', WATERFALL_CALIBRATION_DEFAULT);
				ext_set_cfg_param('S_meter_cal', SMETER_CALIBRATION_DEFAULT);
				console.log("** forcing S-meter and waterfall cal to CALIBRATION_DEFAULT");
				transition_flags |= tflags.WF_SM_CAL2;
				update_flags = true;
				update_cfg = true;
			}
	
			// XXX TRANSITIONAL
			if ((transition_flags & tflags.INACTIVITY) == 0) {
				if (ext_get_cfg_param('inactivity_timeout_mins') == 30) {
					console.log("** resetting old 30 minutes default inactivity timeout");
					ext_set_cfg_param('inactivity_timeout_mins', 0);
				}
				transition_flags |= tflags.INACTIVITY;
				update_flags = true;
				update_cfg = true;
			}
	
			// XXX SECURITY
			// This is really bad because you're updating the server cfg from a user connection.
			// Should only ever be doing this from an admin connection with password validation.
			// We're doing this here because it's currently difficult to modify JSON on the server-side.
			if (update_flags)
				ext_set_cfg_param('transition_flags', transition_flags);
			if (update_cfg)
				cfg_save_json(ws);
			break;

		case "down":
			kiwi_down(param[1], comp_ctr);
			break;

		case "comp_ctr":
			comp_ctr = param[1];
			break;
	}
}

function kiwi_geolocate()
{
	var jsonp_cb = "callback_ipinfo";
	//kiwi_ajax('http://freegeoip.net/json/?callback='+jsonp_cb, true, null, 0, jsonp_cb, function() { setTimeout('kiwi_geolocate();', 1000); } );
	kiwi_ajax('http://ipinfo.io/json/?callback='+jsonp_cb, true, null, 0, jsonp_cb, function() { setTimeout('kiwi_geolocate();', 1000); } );
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

function ajax_msg_gps(acquiring, tracking, good, fixes, adc_clock, adc_clk_corr)
{
	html("id-msg-gps").innerHTML = 'GPS: acquire '+(acquiring? 'yes':'pause')+', track '+tracking+', good '+good+', fixes '+ fixes.toUnits();
	if (adc_clk_corr)
		html("id-msg-gps").innerHTML += ', ADC clock '+adc_clock.toFixed(6)+' ('+ adc_clk_corr.toUnits()  +' avgs)';
}

function ajax_admin_stats(audio_dropped, underruns, seq_errors)
{
	var el = html_id('id-msg-status');
	if (el) {
		el.innerHTML = 'Errors: '+ audio_dropped +' dropped, '+ underruns +' underruns, '+ seq_errors +' sequence';
	}
}


//debug

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
