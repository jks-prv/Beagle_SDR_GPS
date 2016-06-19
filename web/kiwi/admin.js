// Copyright (c) 2016 John Seamons, ZL/KF6VO

////////////////////////////////
// status
////////////////////////////////

function status_html()
{
	var s =
	w3_div('id-status w3-hide',
		'<hr>' +
		w3_div('id-problems w3-container', '') +
		w3_div('id-msg-config w3-container', '') +
		w3_div('id-msg-gps w3-container', '') + '<hr>' +
		w3_div('id-info-1 w3-container', '') +
		w3_div('id-info-2 w3-container', '') + '<hr>' +
		w3_div('id-msg-status w3-container', '') + '<hr>' +
		w3_div('id-debugdiv w3-container', '') + '<hr>'
	);
	return s;
}


////////////////////////////////
// config
////////////////////////////////

function config_html()
{
	var s =
	w3_div('id-config w3-hide',
		'<hr>' +
		w3_div('id-msg-config2 w3-container', '') +
		'<hr>' +
		w3_div('w3-container', 'TODO: set ITU 1/2/3 region, set timezone, report errors to kiwisdr.com') +
		'<hr>'
	);
	return s;
}

////////////////////////////////
// webpage
////////////////////////////////

function webpage_html()
{
	var s =
	w3_div('id-webpage w3-hide',
		w3_half('', 'w3-container',
			admin_input('Page title', 'index_html_params.PAGE_TITLE', 64, 'webpage_string_cb'),
			admin_input('Title', 'index_html_params.RX_TITLE', 64, 'webpage_string_cb')
		) +
		'<hr>' +
		w3_half('', 'w3-container',
			admin_input('Location', 'index_html_params.RX_LOC', 64, 'webpage_string_cb'),
			admin_input('Grid', 'index_html_params.RX_QRA', 64, 'webpage_string_cb')
		) +
		w3_half('', 'w3-container',
			admin_input('ASL', 'index_html_params.RX_ASL', 64, 'webpage_string_cb'),
			admin_input('Map', 'index_html_params.RX_GMAP', 64, 'webpage_string_cb')
		) +
		'<hr>' +
		w3_half('', 'w3-container',
			admin_input('Photo file', 'index_html_params.RX_PHOTO_FILE', 64, 'webpage_string_cb'),
			admin_input('Photo height', 'index_html_params.RX_PHOTO_HEIGHT', 64, 'webpage_string_cb')
		) +
		w3_half('', 'w3-container',
			admin_input('Photo title', 'index_html_params.RX_PHOTO_TITLE', 64, 'webpage_string_cb'),
			admin_input('Photo description', 'index_html_params.RX_PHOTO_DESC', 64, 'webpage_string_cb')
		)
	);
	return s;
}

function webpage_string_cb(el, val)
{
	admin_string_cb(el, val);
	ws_admin.send('SET reload_index_params');
}


////////////////////////////////
// sdr.hu
//		stop/start register_SDR_hu task
////////////////////////////////

function sdr_hu_html()
{
	var s =
	w3_div('id-sdr_hu w3-hide',
		'<hr>' +
		w3_div('w3-container w3-restart w3-text-teal',
				'<b>Display your KiwiSDR on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a>?</b> ' +
				w3_radio_btn('sdr_hu_register', 'Yes', cfg.sdr_hu_register? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('sdr_hu_register', 'No', cfg.sdr_hu_register? 0:1, 'admin_radio_YN_cb')
		) +

		'<hr>' +
		w3_half('', 'w3-container', admin_input('Name', 'rx_name', 64, 'admin_string_cb'),
			admin_input('Location', 'rx_location', 64, 'admin_string_cb')) +

		w3_half('', 'w3-container', admin_input('Device', 'rx_device', 64, 'admin_string_cb'),
			admin_input('Antenna', 'rx_antenna', 64, 'admin_string_cb')) +

		w3_third('', 'w3-container', admin_input('Grid', 'rx_grid', 32, 'admin_string_cb'),
			admin_input('GPS', 'rx_gps', 64, 'admin_string_cb'),
			admin_input('ASL (feet)', 'rx_asl', 32, 'admin_int_cb')) +

		w3_half('', 'w3-container', admin_input('Server URL', 'server_url', 64, 'admin_string_cb'),
			admin_input('Admin email', 'admin_email', 64, 'admin_string_cb')) +

		w3_div('w3-container', admin_input('API key', 'api_key', 64, 'admin_string_cb', 'from sdr.hu/register process'))
	);
	return s;
}


////////////////////////////////
// dx
////////////////////////////////

function dx_html()
{
	var s =
	w3_div('id-dx w3-hide',
		'<hr>' +
		w3_div('w3-container', 'TODO: dx list editing...') +
		'<hr>'
	);
	return s;
}


////////////////////////////////
// network
////////////////////////////////

function network_html()
{
	var s =
	w3_div('id-network w3-hide',
		'<hr>' +
		w3_col_percent('w3-container', 'w3-restart',
			admin_input('Port', 'port', 32, 'admin_int_cb'), 20
		) +
		'<hr>' +
		w3_div('id-net-config w3-container', '') +
		'<hr>' +
		w3_div('w3-container', 'TODO: throttle #chan MB/dy GB/mo, static-ip/nm/gw, hostname') +
		'<hr>'
	);
	return s;
}


////////////////////////////////
// update
//		auto reload when build finished?
////////////////////////////////

function update_html()
{
	var s =
	w3_div('id-update w3-hide',
		'<hr>' +
		w3_div('id-msg-update w3-container', '') +
		'<hr>' +
		w3_half('w3-container', 'w3-text-teal',
			w3_div('',
					'<b>Automatically check for software updates?</b> ' +
					w3_radio_btn('update_check', 'Yes', cfg.update_check? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('update_check', 'No', cfg.update_check? 0:1, 'admin_radio_YN_cb')
			),
			w3_div('',
					'<b>Automatically install software updates?</b> ' +
					w3_radio_btn('update_install', 'Yes', cfg.update_install? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('update_install', 'No', cfg.update_install? 0:1, 'admin_radio_YN_cb')
			)
		) +
		'<hr>' +
		w3_half('w3-container', 'w3-text-teal',
			w3_div('w3-vcenter',
				'<b>Check for software update </b> ' +
				w3_radio_btn('update-check', 'Check now', 0, 'update_check_now_cb', 'w3-margin')
			),
			w3_div('w3-vcenter',
				'<b>Force software build </b> ' +
				w3_radio_btn('update-build', 'Build now', 0, 'update_build_now_cb', 'w3-margin')
			)
		) +
		'<hr>' +
		w3_div('w3-container', 'TODO: alt github name') +
		'<hr>'
	);
	return s;
}

function update_check_now_cb(id, idx)
{
	ws_admin.send('SET force_check=1 force_build=0');
	setTimeout('w3_radio_unhighlight('+ quoted(id) +')', 500);
	users_need_ver_update();
}

function update_build_now_cb(id, idx)
{
	ws_admin.send('SET force_check=1 force_build=1');
	setTimeout('w3_radio_unhighlight('+ quoted(id) +')', 500);
	w3_class(html_id('id-reboot'), 'w3-show');
}


////////////////////////////////
// GPS
//		tracking tasks aren't stopped when !enabled
////////////////////////////////

function gps_html()
{
	var s =
	w3_div('id-gps w3-hide',
		w3_div('w3-section w3-container w3-text-teal',
				'<b>Enable GPS?</b> ' +
				w3_radio_btn('enable_gps', 'Yes', cfg.enable_gps? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('enable_gps', 'No', cfg.enable_gps? 0:1, 'admin_radio_YN_cb')
		) +

		w3_div('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue',
			'<table id="id-gps-ch" class="w3-table w3-striped"> </table>'
		) +

		w3_div('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue',
			'<table id="id-gps-info" class="w3-table"> </table>'
		)
	);
	return s;
}

var gps_interval;

function gps_focus(id)
{
	// only get updates while the gps tab is selected
	ws_admin.send("SET gps_update");
	gps_interval = setInterval('ws_admin.send("SET gps_update")', 1000);
}

function gps_blur(id)
{
	kiwi_clearInterval(gps_interval);
}

var gps = { };

var SUBFRAMES = 5;
var max_rssi = 1;

var refresh_icon = '<i class="fa fa-refresh"></i>';

var sub_colors = [ 'w3-red', 'w3-green', 'w3-blue', 'w3-yellow', 'w3-orange' ];

function gps_update(p)
{
	var i;
	var gps_json = decodeURIComponent(p);
	gps = JSON.parse(gps_json);
	var s;

	var el = html("id-gps-ch");
	s =
		'<th>ch</th>'+
		'<th>acq</th>'+
		'<th>PRN</th>'+
		'<th>SNR</th>'+
		'<th>gain</th>'+
		'<th>hold</th>'+
		'<th>wdog</th>'+
		'<th class="w3-center">err</th>'+
		'<th class="w3-center">subframe</th>'+
		'<th>novfl</th>'+
		'<th style="width:50%">RSSI</th>'+
		'';

	for (var cn=0; cn < gps.ch.length; cn++) {
		var ch = gps.ch[cn];

		if (ch.rssi > max_rssi)
			max_rssi = ch.rssi;
	
		s += '<tr>';
			s += '<td class="w3-right-align">'+ cn +'</td>';
			s += '<td class="w3-center">'+ ((cn == gps.FFTch)? refresh_icon:'') +'</td>';
			s += '<td class="w3-right-align">'+ (ch.prn? ch.prn:'') +'</td>';
			s += '<td class="w3-right-align">'+ (ch.snr? ch.snr:'') +'</td>';
			s += '<td class="w3-right-align">'+ (ch.rssi? ch.gain:'') +'</td>';
			s += '<td class="w3-right-align">'+ (ch.hold? ch.hold:'') +'</td>';
			s += '<td class="w3-right-align">'+ (ch.rssi? ch.wdog:'') +'</td>';
			
			s += '<td>';
			s += '<span class="w3-tag '+ (ch.unlock? 'w3-red':'w3-white') +'">U</span>';
			s += '<span class="w3-tag '+ (ch.parity? 'w3-red':'w3-white') +'">P</span>';
			s += '</td>';
	
			s += '<td>';
			for (i = SUBFRAMES-1; i >= 0; i--) {
				var sub_color;
				if (ch.sub_renew & (1<<i)) {
					sub_color = 'w3-grey';
				} else {
					sub_color = (ch.sub & (1<<i))? sub_colors[i]:'w3-white';
				}
				s += '<span class="w3-tag '+ sub_color +'">'+ (i+1) +'</span>';
			}
			s += '</td>';
	
			s += '<td class="w3-right-align">'+ (ch.novfl? ch.novfl:'') +'</td>';
			
			s += '<td> <div class="w3-progress-container w3-round-xlarge w3-white">';
				var pct = ((ch.rssi / max_rssi) * 100).toFixed(0);
				s += '<div class="w3-progressbar w3-round-xlarge w3-light-green" style="width:'+ pct +'%">';
					s += '<div class="w3-container w3-text-white">'+ ch.rssi +'</div>';
				s += '</div>';
			s += '</div> </td>';
		s += '</tr>';
	}
	
	el.innerHTML = s;

	el = html("id-gps-info");
	s =
		'<th>acq</th>'+
		'<th>tracking</th>'+
		'<th>good</th>'+
		'<th>fixes</th>'+
		'<th>run</th>'+
		'<th>TTFF</th>'+
		'<th>GPS time</th>'+
		'<th>ADC clock</th>'+
		'<th>lat</th>'+
		'<th>lon</th>'+
		'<th>alt</th>'+
		'<th>map</th>'+
		'<tr>'+
			'<td>'+ (gps.acq? 'yes':'no') +'</td>'+
			'<td>'+ (gps.track? gps.track:'') +'</td>'+
			'<td>'+ (gps.good? gps.good:'') +'</td>'+
			'<td>'+ (gps.fixes? gps.fixes.toUnits():'') +'</td>'+
			'<td>'+ gps.run +'</td>'+
			'<td>'+ (gps.ttff? gps.ttff:'') +'</td>'+
			'<td>'+ (gps.gpstime? gps.gpstime:'') +'</td>'+
			'<td>'+ gps.adc_clk.toFixed(6) +' ('+ gps.adc_corr.toUnits() +') </td>'+
			'<td>'+ (gps.lat? gps.lat:'') +'</td>'+
			'<td>'+ (gps.lat? gps.lon:'') +'</td>'+
			'<td>'+ (gps.lat? gps.alt:'') +'</td>'+
			'<td>'+ (gps.lat? gps.map:'') +'</td>'+
		'</tr>'+
		'';
	el.innerHTML = s;
}


////////////////////////////////
// log
////////////////////////////////

function log_html()
{
	var s =
	w3_div('id-log w3-hide',
		'<hr>' +
		w3_div('w3-container', 'TODO: log...') +
		'<hr>'
	);
	return s;
}


////////////////////////////////
// security
////////////////////////////////

function security_html()
{
	var s =
	w3_div('id-security w3-container w3-hide',
		w3_col_percent('w3-section w3-border w3-vcenter', 'w3-text-teal',
			w3_div('w3-container',
				'<b>User auto-login from local net<br>even if password set?</b>'
			), 25,

			w3_div('w3-container w3-text-teal',
				w3_radio_btn('user_auto_login', 'Yes', cfg.user_auto_login? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('user_auto_login', 'No', cfg.user_auto_login? 0:1, 'admin_radio_YN_cb')
			), 25,

			w3_div('w3-container',
				admin_input('User password', 'user_password', 64, 'admin_string_cb',
					'No password set: unrestricted Internet access to SDR')
			), 50
		) +
		
		w3_col_percent('w3-section w3-border w3-vcenter', 'w3-text-teal',
			w3_div('w3-container',
				'<b>Admin auto-login from local net<br>even if password set?</b> '
			), 25,

			w3_div('w3-container w3-text-teal',
				w3_radio_btn('admin_auto_login', 'Yes', cfg.admin_auto_login? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('admin_auto_login', 'No', cfg.admin_auto_login? 0:1, 'admin_radio_YN_cb')
			), 25,

			w3_div('w3-container',
				admin_input('Admin password', 'admin_password', 64, 'admin_string_cb',
					'No password set: no admin access from Internet allowed')
			), 50
		) +
		
		//w3_div('id-security-json w3-section w3-border', '')
		''
	);
	return s;
}

function security_focus(id)
{
	//html_id('id-security-json').innerHTML = w3_div('w3-padding w3-scroll', JSON.stringify(cfg));
}

////////////////////////////////
// admin
////////////////////////////////

var cfg = { };

function cfg_save_json()
{
	var s = encodeURIComponent(JSON.stringify(cfg));
	ws_admin.send('SET save='+ s);
}

function w3_restart_cb()
{
	w3_class(html_id('id-restart'), 'w3-show');
}

function admin_restart()
{
	ws_admin.send('SET restart');
}

function admin_input(label, el, size, cb)
{
	var placeholder = (arguments.length > 4)? arguments[4] : null;
	var cur_val = eval('cfg.'+ el);
	//console.log('admin_input: el='+ el +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input(label, el, cur_val, size, cb, placeholder);
}

function admin_int_cb(el, val)
{
	console.log('admin_int_cb: el='+ el +' val='+ val);
	eval('cfg.'+ el +' = '+ val.toString());
	cfg_save_json();
}

function admin_bool_cb(el, val)
{
	console.log('admin_bool_cb: el='+ el +' val='+ val);
	eval('cfg.'+ el +' = '+ (val? 'true':'false'));
	cfg_save_json();
}

function admin_string_cb(el, val)
{
	// FIXME: embedded "
	eval('cfg.'+ el +' = \"'+ val +'\"');
	//console.log('admin_string_cb: el='+ el +' val='+ val +' check='+ eval('cfg.'+ el).toString());
	cfg_save_json();
}

// translate radio button yes/no index to bool value
function admin_radio_YN_cb(id, idx)
{
	admin_bool_cb(id, idx? 0:1);
}

var ws_admin;

function admin_interface()
{
	ws_admin = open_websocket("ADM", timestamp, admin_recv);
}

function admin_draw()
{
	var admin = html("id-admin");
	admin.innerHTML =
		'<header class="w3-container w3-teal"><h5>Admin interface</h5></header>' +
		'<ul class="w3-navbar w3-border w3-light-grey">' +
			w3_navdef('status', 'Status', 'w3-hover-red') +
			w3_nav('config', 'Config', 'w3-hover-blue') +
			w3_nav('webpage', 'Webpage', 'w3-hover-black') +
			w3_nav('sdr_hu', 'sdr.hu', 'w3-hover-aqua') +
			w3_nav('dx', 'DX list', 'w3-hover-pink') +
			w3_nav('update', 'Update', 'w3-hover-yellow') +
			w3_nav('network', 'Network', 'w3-hover-green') +
			w3_nav('gps', 'GPS', 'w3-hover-orange') +
			w3_nav('log', 'Log', 'w3-hover-grey') +
			w3_nav('security', 'Security', 'w3-hover-indigo') +
		'</ul>' +

		w3_div('id-restart w3-vcenter w3-hide',
			'<header class="w3-container w3-red"><h5>Restart required for changes to take effect</h5></header>' +
			w3_radio_btn('admin-restart', 'Restart', 0, 'admin_restart', 'w3-override-cyan w3-margin')
		) +
		
		w3_div('id-reboot w3-vcenter w3-hide',
			'<header class="w3-container w3-blue"><h5>Server will reboot after build</h5></header>'
		) +
		
		status_html() +
		config_html() +
		webpage_html() +
		sdr_hu_html() +
		dx_html() +
		network_html() +
		update_html() +
		gps_html() +
		log_html() +
		security_html() +
		'';

	users_update();

	//admin.style.top = admin.style.left = '10px';
	var i1 = html('id-info-1');
	var i2 = html('id-info-2');
	//i1.style.position = i2.style.position = 'static';
	//i1.style.fontSize = i2.style.fontSize = '100%';
	//i1.style.color = i2.style.color = 'white';
	visible_block('id-admin', 1);
	
	setTimeout(function() { setInterval(function() { ws_admin.send("SET keepalive") }, 5000) }, 5000);
	setTimeout(function() { setInterval(update_TOD, 1000); }, 1000);
}

// after open_websocket(), server will download cfg state to us, then send init message
function admin_recv(data)
{
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");
	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");
		switch (param[0]) {

			case "load":
				var cfg_json = decodeURIComponent(param[1]);
				cfg = JSON.parse(cfg_json);
				break;

			case "init":
				rx_chans = rx_chan = param[1];
				//console.log("ADMIN init rx_chans="+rx_chans);
				
				if (rx_chans == -1) {
					var admin = html("id-admin");
					admin.innerHTML =
						'<header class="w3-container w3-red"><h5>Admin interface</h5></header>' +
						'<p>To use the new admin interface you must edit the configuration ' +
						'parameters from your current kiwi.config/kiwi.cfg into kiwi.config/kiwi.json</p>';
				} else {
					admin_draw();
					users_init();
				}
				break;

			case "gps_update":
				gps_update(param[1]);
				break;

			default:
				console.log('ADMIN UNKNOWN: '+ param[0] +'='+ param[1]);
				break;
		}
	}
}
