// Copyright (c) 2016 John Seamons, ZL/KF6VO

// TODO
//		input range validation
//		NTP status?


////////////////////////////////
// status
////////////////////////////////

function status_html()
{
	var s =
	w3_divs('id-status w3-hide', '',
		'<hr>' +
		w3_divs('id-problems w3-container', '') +
		w3_divs('id-msg-config w3-container', '') +
		w3_divs('id-msg-gps w3-container', '') + '<hr>' +
		w3_divs('id-info-1 w3-container', '') +
		w3_divs('id-info-2 w3-container', '') + '<hr>' +
		w3_divs('id-msg-status w3-container', '') + '<hr>' +
		w3_divs('id-debugdiv w3-container', '') + '<hr>' +
		w3_divs('w3-vcenter', '',
			w3_btn('KiwiSDR server restart', 'admin_restart_cb', 'w3-override-cyan w3-margin'),
			w3_btn('Debian reboot', 'admin_reboot_cb', 'w3-override-blue w3-margin'),
			w3_btn('Beagle power off', 'admin_power_off_cb', 'w3-override-red w3-margin')
		) +
		w3_divs('id-confirm w3-vcenter w3-hide', '',
			w3_btn('Confirm', 'admin_confirm_cb', 'w3-override-yellow w3-margin')
		)
	);
	return s;
}


////////////////////////////////
// config
////////////////////////////////

var ITU_region_i = { 0:'R1: Europe, Africa', 1:'R2: North & South America', 2:'R3: Asia / Pacific' };

var AM_BCB_chan_i = { 0:'9 kHz', 1:'10 kHz' };

var max_freq_i = { 0:'30 MHz', 1:'32 MHz' };

function config_html()
{
	var init_mode = getVarFromString('cfg.init.mode');
	if (init_mode == null || init_mode == undefined) {
		init_mode = 0;
	} else {
		init_mode++;
	}
	
	var init_ITU_region = getVarFromString('cfg.init.ITU_region');
	if (init_ITU_region == null || init_ITU_region == undefined) {
		init_ITU_region = 0;
	} else {
		init_ITU_region++;
	}
	
	var init_AM_BCB_chan = getVarFromString('cfg.init.AM_BCB_chan');
	if (init_AM_BCB_chan == null || init_AM_BCB_chan == undefined) {
		init_AM_BCB_chan = 0;
	} else {
		init_AM_BCB_chan++;
	}
	
	var max_freq = getVarFromString('cfg.max_freq');
	if (max_freq == null || max_freq == undefined) {
		max_freq = 0;
	} else {
		max_freq++;
	}
	
	var s =
	w3_divs('id-config w3-restart w3-hide', '',
		'<hr>' +

		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			admin_input('Initial frequency (kHz)', 'init.freq', 'config_num_cb'),
			w3_divs('', 'w3-center',
				w3_select('Initial mode', 'select', 'init.mode', init_mode, modes_u, 'config_select_cb')
			),
			admin_input('Initial zoom (0-11)', 'init.zoom', 'config_num_cb')
		) +

		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			admin_input('Initial waterfall min (dBFS, fully zoomed-out)', 'init.min_dB', 'config_num_cb'),
			admin_input('Initial waterfall max (dBFS)', 'init.max_dB', 'config_num_cb'),
			w3_divs('', 'w3-center',
				w3_select('Initial AM BCB channel spacing', 'select', 'init.AM_BCB_chan', init_AM_BCB_chan, AM_BCB_chan_i, 'config_select_cb')
			)
		) +

		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			admin_input('Inactivity timeout (minutes, 0 = no limit)', 'inactivity_timeout_mins', 'config_num_cb'),
			w3_divs('', 'w3-center w3-tspace-8',
				w3_select('ITU region', 'select', 'init.ITU_region', init_ITU_region, ITU_region_i, 'config_select_cb'),
				w3_divs('', 'w3-text-black',
					'Configures LW/NDB, MW and amateur band allocations, etc.'
				)
			),
			w3_divs('', 'w3-center w3-tspace-8',
				w3_select('Max receiver frequency', 'select', 'max_freq', max_freq, max_freq_i, 'config_select_cb'),
				w3_divs('', 'w3-text-black')
			)
		) +

		'<hr>' +
		w3_divs('w3-container', '', 'TODO: set timezone, report errors to kiwisdr.com, ...') +
		'<hr>'
	);
	return s;
}

function config_num_cb(el, val)
{
	console.log('config_num '+ el +'='+ val);
	val = parseInt(val);
	if (isNaN(val)) {
		val = 0;
		w3_set_value(el, val);
	}
	admin_num_cb(el, val);
}

function config_select_cb(menu_path, i)
{
	console.log('config_select i='+ i +' cfg.'+ menu_path);
	if (i != 0) {
		setVarFromString('cfg.'+ menu_path, i-1);
			cfg_save_json(admin_ws);
	}
}


////////////////////////////////
// webpage
////////////////////////////////

function webpage_html()
{
	var s =
	w3_divs('id-webpage w3-restart w3-text-teal w3-hide', '',
		'<hr>' +
		w3_divs('w3-margin-bottom', 'w3-container',
			w3_input('Status', 'status_msg', '', 'webpage_status_cb')
		) +
		w3_divs('', 'w3-container',
			'<label><b>Status HTML preview</b></label>',
			w3_divs('', 'id-webpage-status-preview w3-text-black', '')
		) +
		
		'<hr>' +
		w3_half('', 'w3-container',
			w3_input('Page title', 'index_html_params.PAGE_TITLE', '', 'webpage_string_cb'),
			w3_input('Title', 'index_html_params.RX_TITLE', '', 'webpage_string_cb')
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_input('Location', 'index_html_params.RX_LOC', '', 'webpage_string_cb'),
			w3_input('Grid square (4 or 6 char) ', 'index_html_params.RX_QRA', '', 'webpage_string_cb', null, null, w3_idiv('id-webpage-grid-check cl-admin-check w3-green'))
		) +
		w3_half('', 'w3-container',
			w3_input('Altitude (ASL meters)', 'index_html_params.RX_ASL', '', 'webpage_string_cb'),
			w3_input('Map (Google format) ', 'index_html_params.RX_GMAP', '', 'webpage_string_cb', null, null, w3_idiv('id-webpage-map-check cl-admin-check w3-green'))
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_input('Photo file (e.g. kiwi.config/my_photo.jpg)', 'index_html_params.RX_PHOTO_FILE', '', 'webpage_string_cb'),
			w3_input('Photo height (must always be 350 pixels for now)', 'index_html_params.RX_PHOTO_HEIGHT', '', 'webpage_string_cb')
		) +
		w3_half('', 'w3-container',
			w3_input('Photo title', 'index_html_params.RX_PHOTO_TITLE', '', 'webpage_string_cb'),
			w3_input('Photo description', 'index_html_params.RX_PHOTO_DESC', '', 'webpage_string_cb')
		) +
		
		w3_divs('w3-margin-bottom', 'w3-container', '')		// bottom gap for better scrolling look
	);
	return s;
}

function webpage_status_cb(el, val)
{
	admin_string_cb(el, val);
	html('id-webpage-status-preview').innerHTML = decodeURIComponent(cfg.status_msg);
}

// because of the inline quoting issue, set value dynamically
function webpage_focus()
{
	admin_set_decoded_value('status_msg');
	html('id-webpage-status-preview').innerHTML = decodeURIComponent(cfg.status_msg);
	admin_set_decoded_value('index_html_params.PAGE_TITLE');
	admin_set_decoded_value('index_html_params.RX_TITLE');
	admin_set_decoded_value('index_html_params.RX_LOC');
	admin_set_decoded_value('index_html_params.RX_QRA');
	admin_set_decoded_value('index_html_params.RX_ASL');
	admin_set_decoded_value('index_html_params.RX_GMAP');
	admin_set_decoded_value('index_html_params.RX_PHOTO_FILE');
	admin_set_decoded_value('index_html_params.RX_PHOTO_HEIGHT');
	admin_set_decoded_value('index_html_params.RX_PHOTO_TITLE');
	admin_set_decoded_value('index_html_params.RX_PHOTO_DESC');

	var grid = getVarFromString('cfg.index_html_params.RX_QRA');
	var el = html_idname('webpage-grid-check');
	el.innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';

	var map = getVarFromString('cfg.index_html_params.RX_GMAP');
	el = html_idname('webpage-map-check');
	el.innerHTML = '<a href="http://google.com/maps/place/'+ map +'" target="_blank">check map</a>';
}

function webpage_string_cb(el, val)
{
	admin_string_cb(el, val);
	admin_ws.send('SET reload_index_params');
}


////////////////////////////////
// sdr.hu
//		stop/start register_SDR_hu task
////////////////////////////////

function sdr_hu_html()
{
	var s =
	w3_divs('id-sdr_hu w3-text-teal w3-hide', '',
		w3_divs('id-need-gps w3-vcenter w3-hide', '',
			'<header class="w3-container w3-yellow"><h5>Warning: GPS location field set to the default, please update</h5></header>'
		) +
		
		w3_divs('id-warn-ip w3-vcenter w3-hide', '', '<header class="w3-container w3-yellow"><h5>' +
			'Warning: Using an IP address as the server name (or leaving it blank) will work, but if you switch to using a domain name later on<br>' +
			'this will cause duplicate entries on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a> ' +
			'See <a href="http://kiwisdr.com/quickstart#id-sdr_hu-dup" target="_blank">kiwisdr.com/quickstart</a> for more information.' +
			'</h5></header>'
		) +
		
		'<hr>' +
		w3_half('', '',
			w3_divs('w3-container w3-restart', '',
					'<b>Display your KiwiSDR on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a>?</b> ' +
					w3_radio_btn('sdr_hu_register', 'Yes', cfg.sdr_hu_register? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('sdr_hu_register', 'No', cfg.sdr_hu_register? 0:1, 'admin_radio_YN_cb')
			),
			w3_divs('w3-container', '',
					'<b>Display contact email link on KiwiSDR main page?</b> ' +
					w3_radio_btn('contact_admin', 'Yes', cfg.contact_admin? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('contact_admin', 'No', cfg.contact_admin? 0:1, 'admin_radio_YN_cb')
			)
		) +

		'<hr>' +
		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Name', 'rx_name', '', 'admin_string_cb'),
			w3_input('Location', 'rx_location', '', 'admin_string_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Device', 'rx_device', '', 'admin_string_cb'),
			w3_input('Antenna', 'rx_antenna', '', 'admin_string_cb')
		) +

		w3_third('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Grid square (4 or 6 char) ', 'rx_grid', '', 'admin_string_cb', null, null, w3_idiv('id-sdr_hu-grid-check cl-admin-check w3-green')),
			w3_input('Location (lat, lon) ', 'rx_gps', '', 'sdr_hu_check_gps', null, null, w3_idiv('id-sdr_hu-gps-check cl-admin-check w3-green')),
			admin_input('Altitude (ASL meters)', 'rx_asl', 'admin_num_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Server domain name or IP address (e.g. kiwisdr.my_domain.com) ', 'server_url', '', 'sdr_hu_remove_port'),
			w3_input('Admin email', 'admin_email', '', 'admin_string_cb')
		) +

		w3_divs('w3-container w3-restart', '', w3_input('API key', 'api_key', '', 'admin_string_cb', 'from sdr.hu/register process'))
	);
	return s;
}

function sdr_hu_check_gps(el, val)
{
	if (val == '(-37.631120, 176.172210)' || val == '-37.631120, 176.172210') {
		w3_class(html_id('id-need-gps'), 'w3-show');
		w3_flag('rx_gps');
	} else {
		w3_unclass(html_id('id-need-gps'), 'w3-show');
		w3_unflag('rx_gps');
	}
	
	admin_string_cb(el, val);
}

function sdr_hu_remove_port(el, val)
{
	admin_string_cb(el, val);
	var s = decodeURIComponent(cfg.server_url);
	var sl = s.length
	var state = { looking:0, number:1, okay:2 };
	var st = state.looking;
	for (var i = sl-1; i >= 0; i--) {
		var c = s.charAt(i);
		if (c >= '0' && c <= '9') {
			st = state.number;
			continue;
		}
		if (c == ':') {
			if (st == state.number)
				st = state.okay;
			break;
		}
		st = state.looking;
	}
	if (st == state.okay) {
		s = s.substr(0,i);
	}
	
	sl = s.length;
	st = state.looking;
	for (var i = 0; i < sl; i++) {
		var c = s.charAt(i);
		if (!(c >= '0' && c <= '9') && c != '.')
			st = state.okay;
	}
	if (st != state.okay) {
		w3_class(html_id('id-warn-ip'), 'w3-show');
		w3_flag('server_url');
	} else {
		w3_unclass(html_id('id-warn-ip'), 'w3-show');
		w3_unflag('server_url');
	}
	
	admin_string_cb(el, s);
	admin_set_decoded_value(el);
}

// because of the inline quoting issue, set value dynamically
function sdr_hu_focus()
{
	admin_set_decoded_value('rx_name');
	admin_set_decoded_value('rx_location');
	admin_set_decoded_value('rx_device');
	admin_set_decoded_value('rx_antenna');
	admin_set_decoded_value('rx_grid');
	admin_set_decoded_value('rx_gps');
	admin_set_decoded_value('admin_email');
	admin_set_decoded_value('api_key');
	sdr_hu_remove_port('server_url', getVarFromString('cfg.server_url'));

	// The default in the factory-distributed kiwi.json is the kiwisdr.com NZ location.
	// Detect this and ask user to change it so sdr.hu/map doesn't end up with multiple SDRs
	// defined at the kiwisdr.com location.
	var gps = getVarFromString('cfg.rx_gps');
	if (gps == '(-37.631120%2C%20176.172210)' || gps == '-37.631120%2C%20176.172210') {
		w3_class(html_id('id-need-gps'), 'w3-show');
		w3_flag('rx_gps');
	} else {
		w3_unclass(html_id('id-need-gps'), 'w3-show');
		w3_unflag('rx_gps');
	}
	
	var el = html_idname('sdr_hu-gps-check');
	el.innerHTML = '<a href="http://google.com/maps/place/'+ gps +'" target="_blank">check map</a>';

	var grid = getVarFromString('cfg.rx_grid');
	el = html_idname('sdr_hu-grid-check');
	el.innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}


////////////////////////////////
// dx
////////////////////////////////

function dx_html()
{
	var s =
	w3_divs('id-dx w3-hide', '',
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: dx list editing...') +
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
	w3_divs('id-network w3-hide', '',
		'<hr>' +
		w3_col_percent('w3-container w3-text-teal', 'w3-restart',
			admin_input('Port', 'port', 'admin_num_cb'), 20
		) +
		'<hr>' +
		w3_divs('id-msg-config2 w3-container', '') +
		'<hr>' +
		w3_divs('id-net-config w3-container', '') +
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: throttle #chan MB/dy GB/mo, static-ip/nm/gw, hostname') +
		'<hr>'
	);
	return s;
}


////////////////////////////////
// update
//		auto reload page when build finished?
////////////////////////////////

function update_html()
{
	var s =
	w3_divs('id-update w3-hide', '',
		'<hr>' +
		w3_divs('id-msg-update w3-container', '') +
		'<hr>' +
		w3_half('w3-container', 'w3-text-teal',
			w3_divs('', '',
					'<b>Automatically check for software updates?</b> ' +
					w3_radio_btn('update_check', 'Yes', cfg.update_check? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('update_check', 'No', cfg.update_check? 0:1, 'admin_radio_YN_cb')
			),
			w3_divs('', '',
					'<b>Automatically install software updates?</b> ' +
					w3_radio_btn('update_install', 'Yes', cfg.update_install? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('update_install', 'No', cfg.update_install? 0:1, 'admin_radio_YN_cb')
			)
		) +
		'<hr>' +
		w3_half('w3-container', 'w3-text-teal',
			w3_divs('w3-vcenter', '',
				'<b>Check for software update </b> ' +
				w3_btn('Check now', 'update_check_now_cb', 'w3-margin')
			),
			w3_divs('w3-vcenter', '',
				'<b>Force software build </b> ' +
				w3_btn('Build now', 'update_build_now_cb', 'w3-margin')
			)
		) +
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: alt github name') +
		'<hr>'
	);
	return s;
}

function update_check_now_cb(id, idx)
{
	admin_ws.send('SET force_check=1 force_build=0');
	setTimeout('w3_radio_unhighlight('+ q(id) +')', w3_highlight_time);
	users_need_ver_update();
}

function update_build_now_cb(id, idx)
{
	admin_ws.send('SET force_check=1 force_build=1');
	setTimeout('w3_radio_unhighlight('+ q(id) +')', w3_highlight_time);
	w3_class(html_id('id-build-restart'), 'w3-show');
}


////////////////////////////////
// GPS
//		tracking tasks aren't stopped when !enabled
////////////////////////////////

function gps_html()
{
	var s =
	w3_divs('id-gps w3-hide', '',
		w3_divs('w3-section w3-container w3-text-teal', '',
				'<b>Enable GPS?</b> ' +
				w3_radio_btn('enable_gps', 'Yes', cfg.enable_gps? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('enable_gps', 'No', cfg.enable_gps? 0:1, 'admin_radio_YN_cb')
		) +

		w3_divs('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue', '',
			'<table id="id-gps-ch" class="w3-table w3-striped"> </table>'
		) +

		w3_divs('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue', '',
			'<table id="id-gps-info" class="w3-table"> </table>'
		)
	);
	return s;
}

var gps_interval;

function gps_focus(id)
{
	// only get updates while the gps tab is selected
	admin_ws.send("SET gps_update");
	gps_interval = setInterval('admin_ws.send("SET gps_update")', 1000);
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
			'<td>'+ (gps.acq? 'yes':'paused') +'</td>'+
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
	w3_divs('id-log w3-hide', '',
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: log...') +
		'<hr>'
	);
	return s;
}


////////////////////////////////
// extensions
////////////////////////////////

function extensions_html()
{
	var s =
	w3_divs('id-admin-ext w3-hide w3-section', '',
		'<nav class="id-admin-ext-nav w3-sidenav w3-light-grey"></nav>' +
		w3_divs('id-admin-ext-config', '')
	);
	return s;
}

var ext_seq = 0;
var ext_colors = [
	'w3-hover-blue',
	'w3-hover-red',
	'w3-hover-black',
	'w3-hover-aqua',
	'w3-hover-pink',
	'w3-hover-yellow',
	'w3-hover-green',
	'w3-hover-orange',
	'w3-hover-grey',
	'w3-hover-lime',
	'w3-hover-indigo'
];

function ext_admin_config(id, nav_name, ext_html)
{
	var ci = ext_seq % ext_colors.length;
	html_id('id-admin-ext-nav').innerHTML +=
		w3_anchor('nav-ext', id, nav_name, ext_colors[ci] + ((ci&1)? ' w3-lighter-grey':''));
	ext_seq++;
	html_id('id-admin-ext-config').innerHTML += ext_html;
}


////////////////////////////////
// security
////////////////////////////////

function security_html()
{
	var s =
	w3_divs('id-security w3-hide', '',
		'<hr>' +
		w3_col_percent('w3-container w3-vcenter', 'w3-text-teal',
			w3_divs('', '',
				'<b>User auto-login from local net<br>even if password set?</b>'
			), 25,

			w3_divs('w3-text-teal', '',
				w3_radio_btn('user_auto_login', 'Yes', cfg.user_auto_login? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('user_auto_login', 'No', cfg.user_auto_login? 0:1, 'admin_radio_YN_cb')
			), 25,

			w3_divs('', '',
				w3_input('User password', 'user_password', '', 'admin_string_cb',
					'No password set: unrestricted Internet access to SDR')
			), 50
		) +
		'<hr>' +
		w3_col_percent('w3-container w3-vcenter', 'w3-text-teal',
			w3_divs('', '',
				'<b>Admin auto-login from local net<br>even if password set?</b> '
			), 25,

			w3_divs('w3-text-teal', '',
				w3_radio_btn('admin_auto_login', 'Yes', cfg.admin_auto_login? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('admin_auto_login', 'No', cfg.admin_auto_login? 0:1, 'admin_radio_YN_cb')
			), 25,

			w3_divs('', '',
				w3_input('Admin password', 'admin_password', '', 'admin_string_cb',
					'No password set: no admin access from Internet allowed')
			), 50
		) +
		'<hr>' +
		//w3_divs('id-security-json w3-section w3-border', '')
		''
	);
	return s;
}

function security_focus(id)
{
	admin_set_decoded_value('user_password');
	admin_set_decoded_value('admin_password');
	//html_id('id-security-json').innerHTML = w3_divs('w3-padding w3-scroll', '', JSON.stringify(cfg));
}


////////////////////////////////
// admin
////////////////////////////////

// callback when an input has w3-restart property
function w3_restart_cb()
{
	w3_class(html_id('id-restart'), 'w3-show');
}

var pending_restart = false;
var pending_reboot = false;
var pending_power_off = false;

function admin_restart_cb()
{
	pending_restart = true;
	w3_class(html_id('id-confirm'), 'w3-show');
}

function admin_reboot_cb()
{
	pending_reboot = true;
	w3_class(html_id('id-confirm'), 'w3-show');
}

function admin_power_off_cb()
{
	pending_power_off = true;
	w3_class(html_id('id-confirm'), 'w3-show');
}

function admin_confirm_cb()
{
	if (pending_restart) admin_ws.send('SET restart');
	if (pending_reboot) admin_ws.send('SET reboot');
	if (pending_power_off) admin_ws.send('SET power_off');
}

function admin_restart_now_cb()
{
	admin_ws.send('SET restart');
}

function admin_input(label, el, cb)
{
	var placeholder = (arguments.length > 3)? arguments[3] : null;
	//console.log('admin_input: cfg.'+ el);
	//console.log(cfg);
	var cur_val = getVarFromString('cfg.'+ el);
	if (cur_val == null || cur_val == undefined) {		// scope or parameter doesn't exist, create it
		cur_val = null;	// create as null in json
		// parameter hasn't existed before or hasn't been set (empty field)
		console.log('admin_input: creating el='+ el +' cur_val='+ cur_val);
		setVarFromString('cfg.'+ el, cur_val);
		cfg_save_json(admin_ws);
	} else {
		cur_val = decodeURIComponent(cur_val);
	}
	//console.log('admin_input: el='+ el +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input(label, el, cur_val, cb, placeholder);
}

function admin_num_cb(el, val)
{
	console.log('admin_num_cb '+ typeof val +' "'+ val +'" '+ parseInt(val));
	var v = parseInt(val);
	if (isNaN(v)) v = null;
	setVarFromString('cfg.'+el, v);
	cfg_save_json(admin_ws);
}

function admin_bool_cb(el, val)
{
	setVarFromString('cfg.'+el, val? true:false);
	cfg_save_json(admin_ws);
}

function admin_string_cb(el, val)
{
	console.log('admin_string_cb '+ typeof val +' "'+ val +'"');
	setVarFromString('cfg.'+el, encodeURIComponent(val.toString()));
	cfg_save_json(admin_ws);
}

function admin_set_decoded_value(path)
{
	w3_set_decoded_value(path, getVarFromString('cfg.'+ path));
}

// translate radio button yes/no index to bool value
function admin_radio_YN_cb(id, idx)
{
	admin_bool_cb(id, idx? 0:1);
}

var admin_ws;

function admin_main()
{
	admin_ws = open_websocket("ADM", timestamp, admin_recv);
}

function admin_draw()
{
	var admin = html("id-admin");
	admin.innerHTML =
		'<header class="w3-container w3-teal"><h5>Admin interface</h5></header>' +
		'<ul class="w3-navbar w3-border w3-light-grey">' +
			w3_navdef('admin-nav', 'status', 'Status', 'w3-hover-red') +
			w3_nav('admin-nav', 'config', 'Config', 'w3-hover-blue') +
			w3_nav('admin-nav', 'webpage', 'Webpage', 'w3-hover-black') +
			w3_nav('admin-nav', 'sdr_hu', 'sdr.hu', 'w3-hover-aqua') +
			w3_nav('admin-nav', 'dx', 'DX list', 'w3-hover-pink') +
			w3_nav('admin-nav', 'update', 'Update', 'w3-hover-yellow') +
			w3_nav('admin-nav', 'network', 'Network', 'w3-hover-green') +
			w3_nav('admin-nav', 'gps', 'GPS', 'w3-hover-orange') +
			w3_nav('admin-nav', 'log', 'Log', 'w3-hover-grey') +
			w3_nav('admin-nav', 'admin-ext', 'Extensions', 'w3-hover-lime') +
			w3_nav('admin-nav', 'security', 'Security', 'w3-hover-indigo') +
		'</ul>' +

		w3_divs('id-restart w3-vcenter w3-hide', '',
			'<header class="w3-idiv w3-container w3-red"><h5>Restart required for changes to take effect</h5></header>' +
			w3_idiv('', w3_btn('KiwiSDR server restart', 'admin_restart_now_cb', 'w3-override-cyan w3-margin'))
		) +
		
		w3_divs('id-build-restart w3-vcenter w3-hide', '',
			'<header class="w3-container w3-blue"><h5>Server will restart after build</h5></header>'
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
		extensions_html() +
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
	
	setTimeout(function() { setInterval(function() { admin_ws.send("SET keepalive") }, 5000) }, 5000);
	setTimeout(function() { setInterval(update_TOD, 1000); }, 1000);
}

// after open_websocket(), server will download cfg state to us, then send init message
function admin_recv(data)
{
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		console.log('admin_recv: '+ param[0]);
		switch (param[0]) {

			case "init":
				rx_chans = rx_chan = param[1];
				//console.log("ADMIN init rx_chans="+rx_chans);
				
				if (rx_chans == -1) {
					var admin = html("id-admin");
					admin.innerHTML =
						'<header class="w3-container w3-red"><h5>Admin interface</h5></header>' +
						'<p>To use the new admin interface you must edit the configuration ' +
						'parameters from your current kiwi.config/kiwi.cfg into kiwi.config/kiwi.json<br>' +
						'Use the file kiwi.config/kiwi.template.json as a guide.</p>';
				} else {
					admin_draw();
					users_init();
					admin_ws.send('SET extint_load_extension_configs');
				}
				break;

			case "ext_config_html":
				var ext_name = decodeURIComponent(param[1]);
				console.log('ext_config_html name='+ ext_name);
				w3_call(ext_name +'_config_html', null);
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
