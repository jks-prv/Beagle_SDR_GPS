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
			w3_btn('Beagle reboot', 'admin_reboot_cb', 'w3-override-blue w3-margin'),
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
	kiwi_get_init_settings();		// make sure defaults exist
	
	var init_mode = ext_get_cfg_param('init.mode', 0);
	var init_AM_BCB_chan = ext_get_cfg_param('init.AM_BCB_chan', 0);
	var init_ITU_region = ext_get_cfg_param('init.ITU_region', 0);
	var max_freq = ext_get_cfg_param('max_freq', 0);
	
	var s =
	w3_divs('id-config w3-hide', '',
		'<hr>' +

		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get_param('Initial frequency (kHz)', 'init.freq', 'config_float_cb'),
			w3_divs('', 'w3-center',
				w3_select('Initial mode', '', 'init.mode', init_mode, modes_u, 'config_select_cb')
			),
			w3_input_get_param('Initial zoom (0-11)', 'init.zoom', 'config_int_cb')
		) +

		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get_param('Initial waterfall min (dBFS, fully zoomed-out)', 'init.min_dB', 'config_int_cb'),
			w3_input_get_param('Initial waterfall max (dBFS)', 'init.max_dB', 'config_int_cb'),
			w3_divs('', 'w3-center',
				w3_select('Initial AM BCB channel spacing', '', 'init.AM_BCB_chan', init_AM_BCB_chan, AM_BCB_chan_i, 'config_select_cb')
			)
		) +

		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('w3-restart', '',
				w3_input_get_param('Inactivity timeout (minutes, 0 = no limit)', 'inactivity_timeout_mins', 'config_int_cb')
			),
			w3_input_get_param('S-meter calibration (dB)', 'S_meter_cal', 'config_int_cb'),
			w3_input_get_param('Waterfall calibration (dB)', 'waterfall_cal', 'config_int_cb')
		) +
		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_divs('', 'w3-center w3-tspace-8',
				w3_select('ITU region', '', 'init.ITU_region', init_ITU_region, ITU_region_i, 'config_select_cb'),
				w3_divs('', 'w3-text-black',
					'Configures LW/NDB, MW and amateur band allocations, etc.'
				)
			),
			w3_divs('w3-restart', 'w3-center w3-tspace-8',
				w3_select('Max receiver frequency', '', 'max_freq', max_freq, max_freq_i, 'config_select_cb'),
				w3_divs('', 'w3-text-black')
			),
			''
		) +

		'<hr>' +
		w3_divs('w3-container', '', 'TODO: set timezone, report errors to kiwisdr.com, ...') +
		'<hr>'
	);
	return s;
}

function config_int_cb(path, val)
{
	//console.log('config_int '+ path +'='+ val);
	val = parseInt(val);
	if (isNaN(val)) {
		val = 0;
		w3_set_value(path, val);
	}
	admin_int_cb(path, val);
}

function config_float_cb(path, val)
{
	//console.log('config_float '+ path +'='+ val);
	val = parseFloat(val);
	if (isNaN(val)) {
		val = 0;
		w3_set_value(path, val);
	}
	admin_float_cb(path, val);
}

function config_select_cb(path, idx, first)
{
	//console.log('config_select_cb idx='+ idx +' path='+ path);
	idx = +idx;
	if (idx != -1) {
		var save = first? false : true;
		ext_set_cfg_param(path, idx, save);
	}
}


////////////////////////////////
// channels
////////////////////////////////

function channels_html()
{
	var s =
	w3_divs('id-channels w3-hide', '',
		'<hr>' +

		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			'foo',
			'bar',
			'baz'
		)
	);
	return s;
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
			w3_input('Grid square (4 or 6 char) ', 'index_html_params.RX_QRA', '', 'webpage_input_grid', null, null, w3_inline('id-webpage-grid-check cl-admin-check w3-green'))
		) +
		w3_half('', 'w3-container',
			w3_input('Altitude (ASL meters)', 'index_html_params.RX_ASL', '', 'webpage_string_cb'),
			w3_input('Map (Google format) ', 'index_html_params.RX_GMAP', '', 'webpage_input_map', null, null, w3_inline('id-webpage-map-check cl-admin-check w3-green'))
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_divs('', '',
				w3_label('Photo file', 'photo-file'),
				'<input id="id-photo-file" type="file" accept="image/*" onchange="webpage_photo_file_upload()"/>',
				w3_divs('', 'id-photo-error', '')
			),
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

function webpage_input_grid(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_grid();
}

function webpage_update_check_grid()
{
	var grid = ext_get_cfg_param('index_html_params.RX_QRA');
	w3_el_id('webpage-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function webpage_input_map(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_map();
}

function webpage_update_check_map()
{
	var map = ext_get_cfg_param('index_html_params.RX_GMAP');
	w3_el_id('webpage-map-check').innerHTML = '<a href="http://google.com/maps/place/'+ map +'" target="_blank">check map</a>';
}

function webpage_photo_uploaded(obj)
{
	var rc = obj.r;
	
	//console.log('## webpage_photo_uploaded rc='+ rc);
	if (rc == 0) {
		// server restart not needed, effect is immediate on reload or next connection
		webpage_string_cb('index_html_params.RX_PHOTO_FILE', 'kiwi.config/photo.upload');
	}
	
	var el = w3_el_id('photo-error');
	var e;
	
	switch (rc) {
	case 0:
		e = 'Upload successful';
		break;
	case 1:
		e = 'Not an image file?';
		break;
	case 2:
		e = 'Unable to determine file type';
		break;
	case 3:
		e = 'File too large';
		break;
	default:
		e = 'Undefined error?';
		break;
	}
	
	el.innerHTML = e;
	w3_class(el, (rc == 0)? 'w3-text-green' : 'w3-text-red');
	w3_show_block(el);
}

function webpage_photo_file_upload()
{
	var browse = w3_el_id('photo-file');
	browse.innerHTML = 'Uploading...';
	var file = browse.files[0];
	var fdata = new FormData();
	fdata.append('photo', file, file.name);
	//console.log(file);

	var el = w3_el_id('photo-error');
	w3_hide(el);
	w3_unclass(el, 'w3-text-red');
	w3_unclass(el, 'w3-text-green');

	kiwi_ajax_send(fdata, '/PIX', 'webpage_photo_uploaded');
}

function webpage_status_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
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
	admin_set_decoded_value('index_html_params.RX_PHOTO_HEIGHT');
	admin_set_decoded_value('index_html_params.RX_PHOTO_TITLE');
	admin_set_decoded_value('index_html_params.RX_PHOTO_DESC');

	webpage_update_check_grid();
	webpage_update_check_map();
}

function webpage_string_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	ext_send('SET reload_index_params');
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
			'Warning: Using an IP address as the server name will work, but if you switch to using a domain name later on<br>' +
			'this will cause duplicate entries on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a> ' +
			'See <a href="http://kiwisdr.com/quickstart#id-sdr_hu-dup" target="_blank">kiwisdr.com/quickstart</a> for more information.' +
			'</h5></header>'
		) +
		
		'<hr>' +
		w3_half('', '',
			w3_divs('w3-container w3-restart', '',
					'<b>Display your KiwiSDR on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a>?</b> ' +
					w3_radio_btn('Yes', 'adm.sdr_hu_register', adm.sdr_hu_register? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('No', 'adm.sdr_hu_register', adm.sdr_hu_register? 0:1, 'admin_radio_YN_cb')
			),
			w3_divs('w3-container', '',
					'<b>Display contact email link on KiwiSDR main page?</b> ' +
					w3_radio_btn('Yes', 'contact_admin', cfg.contact_admin? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('No', 'contact_admin', cfg.contact_admin? 0:1, 'admin_radio_YN_cb')
			)
		) +

		'<hr>' +
		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Name', 'rx_name', '', 'w3_string_set_cfg_cb'),
			w3_input('Location', 'rx_location', '', 'w3_string_set_cfg_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Device', 'rx_device', '', 'w3_string_set_cfg_cb'),
			w3_input('Antenna', 'rx_antenna', '', 'w3_string_set_cfg_cb')
		) +

		w3_third('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Grid square (4 or 6 char) ', 'rx_grid', '', 'sdr_hu_input_grid', null, null, w3_inline('id-sdr_hu-grid-check cl-admin-check w3-green')),
			w3_input('Location (lat, lon) ', 'rx_gps', '', 'sdr_hu_check_gps', null, null,
				w3_inline('id-sdr_hu-gps-check cl-admin-check w3-green') + ' ' +
				w3_inline('id-sdr_hu-gps-set cl-admin-check w3-blue w3-pointer w3-hide', '', 'set from GPS')
			),
			w3_input_get_param('Altitude (ASL meters)', 'rx_asl', 'admin_int_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Server domain name or IP address (e.g. kiwisdr.my_domain.com) ', 'server_url', '', 'sdr_hu_remove_port'),
			w3_input('Admin email', 'admin_email', '', 'w3_string_set_cfg_cb')
		) +

		w3_divs('w3-container w3-restart', '', w3_input('API key', 'adm.api_key', '', 'w3_string_set_cfg_cb', 'from sdr.hu/register process'))
	);
	return s;
}

function sdr_hu_input_grid(path, val)
{
	w3_string_set_cfg_cb(path, val);
	sdr_hu_update_check_grid();
}

function sdr_hu_update_check_grid()
{
	var grid = ext_get_cfg_param('rx_grid');
	w3_el_id('sdr_hu-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function sdr_hu_check_gps(path, val, first)
{
	if (val.charAt(0) != '(')
		val = '('+ val;
	if (val.charAt(val.length-1) != ')')
		val = val +')';

	if (val == '(-37.631120, 176.172210)' || val == '(-37.631120%2C%20176.172210)') {
		w3_show_block('id-need-gps');
		w3_flag('rx_gps');
	} else {
		w3_hide('id-need-gps');
		w3_unflag('rx_gps');
	}
	
	w3_string_set_cfg_cb(path, val, first);
	w3_set_value(path, val);
	sdr_hu_update_check_map();
}

function sdr_hu_update_check_map()
{
	var gps = decodeURIComponent(ext_get_cfg_param('rx_gps'));
	gps = gps.substring(1, gps.length-1);		// remove parens
	w3_el_id('sdr_hu-gps-check').innerHTML = '<a href="http://google.com/maps/place/'+ gps +'" target="_blank">check map</a>';
}

function sdr_hu_remove_port(el, s, first)
{
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
		w3_show_block('id-warn-ip');
		w3_flag('server_url');
	} else {
		w3_hide('id-warn-ip');
		w3_unflag('server_url');
	}
	
	w3_string_set_cfg_cb(el, s, first);
	admin_set_decoded_value(el);
}

var sdr_hu_interval;

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
	admin_set_decoded_value('adm.api_key');
	sdr_hu_remove_port('server_url', ext_get_cfg_param('server_url'), /* first */ true);

	// The default in the factory-distributed kiwi.json is the kiwisdr.com NZ location.
	// Detect this and ask user to change it so sdr.hu/map doesn't end up with multiple SDRs
	// defined at the kiwisdr.com location.
	var gps = decodeURIComponent(ext_get_cfg_param('rx_gps'));
	sdr_hu_check_gps('rx_gps', gps, /* first */ true);
	
	sdr_hu_update_check_grid();
	sdr_hu_update_check_map();
	
	w3_el_id('sdr_hu-gps-set').onclick = function() {
		var val = '('+ sdr_hu_gps.lat +', '+ sdr_hu_gps.lon +')';
		w3_set_value('rx_gps', val);
		w3_input_change('rx_gps', 'sdr_hu_check_gps');
	};

	// only get updates while the sdr_hu tab is selected
	ext_send("SET sdr_hu_update");
	sdr_hu_interval = setInterval(function() {ext_send("SET sdr_hu_update");}, 2000);
}

function sdr_hu_blur(id)
{
	kiwi_clearInterval(sdr_hu_interval);
}

var sdr_hu_gps = { };

function sdr_hu_update(p)
{
	var i;
	var sdr_hu_json = decodeURIComponent(p);
	//console.log('sdr_hu_json='+ sdr_hu_json);
	sdr_hu_gps = JSON.parse(sdr_hu_json);
	
	// GPS has had a solution, show button
	if (sdr_hu_gps.lat != undefined) {
		w3_show_inline('id-sdr_hu-gps-set');
	}
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
		w3_divs('id-net-need-update w3-vcenter w3-hide', '',
			w3_btn('Are you sure? Click to update interface DHCP/static IP configuration', 'network_dhcp_static_update_cb', 'w3-override-cyan')
		) +

		'<hr>' +
		w3_divs('w3-reboot', '',
			w3_col_percent('w3-container w3-margin-bottom w3-text-teal', '',
				w3_input_get_param('Port', 'adm.port', 'admin_int_cb'), 20,
				w3_divs('w3-center', '',
						'<b>IP address (only IPv4 for now)</b><br> ' +
						w3_radio_btn_get_param('DHCP', 'adm.ip_address.use_static', 0, false, 'network_use_static_cb') +
						w3_radio_btn_get_param('Static', 'adm.ip_address.use_static', 1, false, 'network_use_static_cb')
				), 40
			),
			w3_divs('id-net-static w3-hide', '',
				w3_third('w3-margin-B-8 w3-text-teal', 'w3-container',
					w3_input_get_param('IP address (n.n.n.n where n = 0..255)', 'adm.ip_address.ip', 'network_ip_address_cb', ''),
					w3_input_get_param('Netmask (n.n.n.n where n = 0..255)', 'adm.ip_address.netmask', 'network_netmask_cb', ''),
					w3_input_get_param('Gateway (n.n.n.n where n = 0..255)', 'adm.ip_address.gateway', 'network_gw_address_cb', '')
				),
				w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
					w3_divs('id-network-check-ip w3-green', ''),
					w3_divs('id-network-check-nm w3-green', ''),
					w3_divs('id-network-check-gw w3-green', '')
				)
			)
		) +
		'<hr>' +
		w3_divs('id-msg-config2 w3-container', '') +
		'<hr>' +
		w3_divs('id-net-config w3-container', '') +
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: throttle #chan MB/dy GB/mo, hostname') +
		'<hr>'
	);
	
	// FIXME replace this with general instantiation call from w3_input()
	setTimeout(function() {
		var use_static = ext_get_cfg_param('adm.ip_address.use_static', false);
		network_use_static_cb('adm.ip_address.use_static', use_static, /* first */ true);
	}, 500);
	
	return s;
}

function network_dhcp_static_update_cb(path, idx)
{
	if (adm.ip_address.use_static)
		ext_send('SET static_ip='+ kiwi_ip_str(network_ip) +' static_nm='+ kiwi_ip_str(network_nm) +' static_gw='+ kiwi_ip_str(network_gw));
	else
		ext_send('SET use_DHCP');

	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function network_use_static_cb(path, idx, first)
{
	idx = +idx;
	var dhcp = (idx == 0);
	
	// only show IP fields if in static mode
	if (!dhcp) {
		w3_show_block('id-net-static');
	} else {
		w3_hide('id-net-static');
	}

	//console.log('network_use_static_cb: first='+ first +' dhcp='+ dhcp);

	// when mode is changed decide if update button needs to appear
	if (!first) {
		if (dhcp) {
			w3_show_block('id-net-need-update');
		} else {
			w3_hide('id-net-need-update');	// will be set to "show" later on if IP fields are okay
		}
	} else {
		// first time, fill-in the fields with the configured values
		network_ip_address_cb('adm.ip_address.ip', adm.ip_address.ip, true);
		network_netmask_cb('adm.ip_address.netmask', adm.ip_address.netmask, true);
		network_gw_address_cb('adm.ip_address.gateway', adm.ip_address.gateway, true);
	}
	
	admin_bool_cb(path, dhcp? 0:1, first);
}

function network_ip_nm_check(val, ip)
{
	var rexp = '^([0-9]*)\.([0-9]*)\.([0-9]*)\.([0-9]*)$';
	var p = new RegExp(rexp).exec(val);
	var a, b, c, d;
	
	if (p != null) {
		//console.log('regexp p='+ p);
		a = parseInt(p[1]);
		a = (a > 255)? Math.NaN : a;
		b = parseInt(p[2]);
		b = (b > 255)? Math.NaN : b;
		c = parseInt(p[3]);
		c = (c > 255)? Math.NaN : c;
		d = parseInt(p[4]);
		d = (d > 255)? Math.NaN : d;
	}
	
	if (p == null || isNaN(a) || isNaN(b) || isNaN(c) || isNaN(d)) {
		ip.ok = false;
	} else {
		ip.ok = true; ip.a = a; ip.b = b; ip.c = c; ip.d = d;
	}
	
	return ip.ok;
}

network_ip = { ok:false, a:null, b:null, c:null, d:null };
network_nm = { ok:false, a:null, b:null, c:null, d:null };
network_gw = { ok:false, a:null, b:null, c:null, d:null };

function network_show_update(first)
{
	//console.log('network_show_update: first='+ first);

	if (!first && network_ip.ok && network_nm.ok && network_gw.ok) {
		//console.log('network_show_update: SHOW');
		w3_show_block('id-net-need-update');
	} else {
		w3_hide('id-net-need-update');
	}
}

function network_show_check(id, name, path, val_str, ip, first, check_func)
{
	
	if (val_str != '') {
		var el = w3_el_id(id);
		var check = network_ip_nm_check(val_str, ip), check2 = true;
		
		if (check == true && check_func != undefined) {
			check2 = check_func(val_str, ip);
		}
	
		if (check == false || check2 == false) {
			el.innerHTML = 'bad '+ name +' entered';
			w3_unclass(el, 'w3-green');
			w3_class(el, 'w3-red');
		} else {
			el.innerHTML = name +' okay, check: '+ ip.a +'.'+ ip.b +'.'+ ip.c +'.'+ ip.d;
			w3_unclass(el, 'w3-red');
			w3_class(el, 'w3-green');
			w3_string_set_cfg_cb(path, val_str, first);
		}

		network_show_update(first);		// when a field is made good decide if update button needs to be shown
	}
}

function network_ip_address_cb(path, val, first)
{
	network_show_check('network-check-ip', 'IP address', path, val, network_ip, first);
}

function network_netmask_cb(path, val, first)
{
	network_nm.nm = -1;
	network_show_check('network-check-nm', 'netmask', path, val, network_nm, first,
		function(val_str, ip) {
			var ip_host = kiwi_inet4_d2h(val_str);
			ip.nm = 0;		// degenerate case: no one-bits in netmask at all
			for (var i = 0; i < 32; i++) {
				if (ip_host & (1<<i)) {		// first one-bit hit
					ip.nm = 32-i;
					for (; i < 32; i++) {
						if ((ip_host & (1<<i)) == 0) {
							ip.nm = -1;		// rest of bits weren't ones like they're supposed to be
							ip.ok = false;
							return false;
						}
					}
				}
			}
			ip.ok = true;
			return true;
		});

	if (network_nm.nm != -1)
		w3_el_id('network-check-nm').innerHTML += ' (/'+ network_nm.nm +')';
}

function network_gw_address_cb(path, val, first)
{
	network_show_check('network-check-gw', 'gateway', path, val, network_gw, first);
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
					w3_radio_btn('Yes', 'adm.update_check', adm.update_check? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('No', 'adm.update_check', adm.update_check? 0:1, 'admin_radio_YN_cb')
			),
			w3_divs('', '',
					'<b>Automatically install software updates?</b> ' +
					w3_radio_btn('Yes', 'adm.update_install', adm.update_install? 1:0, 'admin_radio_YN_cb') +
					w3_radio_btn('No', 'adm.update_install', adm.update_install? 0:1, 'admin_radio_YN_cb')
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
	ext_send('SET force_check=1 force_build=0');
	setTimeout(function() {w3_radio_unhighlight(id);}, w3_highlight_time);
	w3_el_id('msg-update').innerHTML = 'Checking <i class="fa fa-refresh fa-spin"></i>';
	ext_send('SET CHECK_UPDATE');
}

function update_build_now_cb(id, idx)
{
	ext_send('SET force_check=1 force_build=1');
	setTimeout(function() {w3_radio_unhighlight(id);}, w3_highlight_time);
	w3_show_block('id-build-restart');
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
				w3_radio_btn('Yes', 'adm.enable_gps', adm.enable_gps? 1:0, 'admin_radio_YN_cb') +
				w3_radio_btn('No', 'adm.enable_gps', adm.enable_gps? 0:1, 'admin_radio_YN_cb')
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
	ext_send("SET gps_update");
	gps_interval = setInterval(function() {ext_send("SET gps_update");}, 1000);
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

var log = { };
var nlog = 256;

function log_html()
{
	var s =
	w3_divs('id-log w3-text-teal w3-hide', '',
		'<hr>'+
		w3_divs('', 'w3-container',
			'<label><b>KiwiSDR server log (scrollable list, first and last set of messages)</b></label>',
			w3_divs('', 'id-log-msg w3-margin-T-16 w3-text-output w3-small w3-text-black', '')
		)
	);
	return s;
}

function log_setup()
{
	var el = w3_el_id('id-log-msg');
	var s = '<pre>';
		for (var i = 0; i < nlog; i++) {
			if (i == nlog/2) s += '<code id="id-log-not-shown"></code>';
			s += '<code id="id-log-'+ i +'"></code>';
		}
	s += '</pre>';
	el.innerHTML = s;

	ext_send('SET log_update=1');
}

function log_resize()
{
	if (!log_selected) return;
	var log_height = window.innerHeight - w3_el_id("id-admin-header-container").clientHeight - 100;
	var el = w3_el_id('id-log-msg');
	el.style.height = px(log_height);
}

var log_selected = false;
var log_interval;

function log_focus(id)
{
	log_selected = true;
	log_resize();
	log_update();
	log_interval = setInterval(log_update, 3000);
}

function log_blur(id)
{
	kiwi_clearInterval(log_interval);
	log_selected = false;
}

function log_update()
{
	ext_send('SET log_update=0');
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
	'w3-hover-red',
	'w3-hover-blue',
	'w3-hover-purple',
	'w3-hover-black',
	'w3-hover-aqua',
	'w3-hover-pink',
	'w3-hover-yellow',
	'w3-hover-green',
	'w3-hover-orange',
	'w3-hover-grey',
	'w3-hover-lime',
	'w3-hover-indigo',
	'w3-hover-brown'
];

function ext_admin_config(id, nav_name, ext_html)
{
	var ci = ext_seq % ext_colors.length;
	w3_el_id('id-admin-ext-nav').innerHTML +=
		w3_anchor('nav-ext', id, nav_name, ext_colors[ci] + ((ci&1)? ' w3-lighter-grey':''), false);
	ext_seq++;
	w3_el_id('id-admin-ext-config').innerHTML += ext_html;
}


////////////////////////////////
// security
////////////////////////////////

var chan_no_pwd_u = { 0:'none', 1:'1', 2:'2', 3:'3' };

function security_html()
{
	var chan_no_pwd = ext_get_cfg_param('chan_no_pwd', 0);

	var s =
	w3_divs('id-security w3-hide', '',
		'<hr>' +
		w3_col_percent('w3-container w3-vcenter', 'w3-hspace-16 w3-text-teal',
			w3_divs('', '',
				w3_divs('', '',
					'<b>User auto-login from local net<br>even if password set?</b><br>',
					w3_divs('w3-margin-T-8', '',
						w3_radio_btn('Yes', 'adm.user_auto_login', adm.user_auto_login? 1:0, 'admin_radio_YN_cb') +
						w3_radio_btn('No', 'adm.user_auto_login', adm.user_auto_login? 0:1, 'admin_radio_YN_cb')
					)
				)
			), 25,

			w3_divs('', 'w3-center',
				w3_select('Number of channels<br>not requiring a password<br>even if password set',
					'', 'chan_no_pwd', chan_no_pwd, chan_no_pwd_u, 'config_select_cb'),
				w3_divs('', 'w3-margin-T-8 w3-text-black',
					'Set this and a password to create a two sets of channels, ' +
					'some that have open-access requiring no password and some that are password protected.'
				)
			), 25,

			w3_divs('', '',
				w3_input('User password', 'adm.user_password', '', 'w3_string_set_cfg_cb',
					'No password set: unrestricted Internet access to SDR')
			), 50
		) +
		'<hr>' +
		w3_col_percent('w3-container w3-vcenter', 'w3-hspace-16 w3-text-teal',
			w3_divs('', '',
				w3_divs('', '',
					'<b>Admin auto-login from local net<br>even if password set?</b><br>',
					w3_divs('w3-margin-T-8', '',
						w3_radio_btn('Yes', 'adm.admin_auto_login', adm.admin_auto_login? 1:0, 'admin_radio_YN_cb') +
						w3_radio_btn('No', 'adm.admin_auto_login', adm.admin_auto_login? 0:1, 'admin_radio_YN_cb')
					)
				)
			), 25,

			w3_divs('w3-text-teal', '',
				''
			), 25,

			w3_divs('', '',
				w3_input('Admin password', 'adm.admin_password', '', 'w3_string_set_cfg_cb',
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
	admin_set_decoded_value('adm.user_password');
	admin_set_decoded_value('adm.admin_password');
	//w3_el_id('id-security-json').innerHTML = w3_divs('w3-padding w3-scroll', '', JSON.stringify(cfg));
}


////////////////////////////////
// admin
////////////////////////////////

function admin_main()
{
	window.addEventListener("resize", admin_resize);
}

function admin_resize()
{
	log_resize();
}

function kiwi_ws_open(conn_type, cb, cbp)
{
	return open_websocket(conn_type, cb, cbp, null, admin_recv);
}

function admin_draw()
{
	var admin = html("id-admin");
	admin.innerHTML =
		w3_divs('id-admin-header-container', '',
			'<header class="w3-container w3-teal"><h5>Admin interface</h5></header>' +
			'<ul class="w3-navbar w3-border w3-light-grey">' +
				w3_navdef('admin-nav', 'status', 'Status', 'w3-hover-red') +
				w3_nav('admin-nav', 'config', 'Config', 'w3-hover-blue') +
				//w3_nav('admin-nav', 'channels', 'Channels', 'w3-hover-purple') +
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
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Restart required for changes to take effect</h5></header>' +
				w3_inline('', '', w3_btn('KiwiSDR server restart', 'admin_restart_now_cb', 'w3-override-cyan w3-margin'))
			) +
			
			w3_divs('id-reboot w3-vcenter w3-hide', '',
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Reboot required for changes to take effect</h5></header>' +
				w3_inline('', '', w3_btn('Beagle reboot', 'admin_reboot_now_cb', 'w3-override-blue w3-margin'))
			) +
			
			w3_divs('id-build-restart w3-vcenter w3-hide', '',
				'<header class="w3-container w3-blue"><h5>Server will restart after build</h5></header>'
			)
		) +
		
		status_html() +
		config_html() +
		//channels_html() +
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

	log_setup();
	users_init();
	stats_init();

	//admin.style.top = admin.style.left = '10px';
	var i1 = html('id-info-1');
	var i2 = html('id-info-2');
	//i1.style.position = i2.style.position = 'static';
	//i1.style.fontSize = i2.style.fontSize = '100%';
	//i1.style.color = i2.style.color = 'white';
	visible_block('id-admin', 1);
	
	setTimeout(function() { setInterval(update_TOD, 1000); }, 1000);
}

var log_msg_idx, log_msg_not_shown = 0;

// after calling admin_main(), server will download cfg and adm state to us, then send 'init' message
function admin_recv(data)
{
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	//console.log('admin_recv: '+ stringData);

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		//console.log('admin_recv: '+ param[0]);
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
					ext_send('SET extint_load_extension_configs');
				}
				break;

			case "ext_config_html":
				var ext_name = decodeURIComponent(param[1]);
				//console.log('ext_config_html name='+ ext_name);
				w3_call(ext_name +'_config_html');
				break;

			case "sdr_hu_update":
				sdr_hu_update(param[1]);
				break;

			case "gps_update":
				gps_update(param[1]);
				break;

			case "log_msg_not_shown":
				log_msg_not_shown = parseInt(param[1]);
				if (log_msg_not_shown) {
					var el = w3_el_id('id-log-not-shown');
					el.innerHTML = '---- '+ log_msg_not_shown.toString() +' lines not shown ----\n'
				}
				break;

			case "log_msg_idx":
				log_msg_idx = parseInt(param[1]);
				break;

			case "log_msg_save":
				var el = w3_el_id('id-log-'+ log_msg_idx);
				if (!el) break;
				var el2 = w3_el_id('id-log-msg');
				var wasScrolledDown = (el2.scrollTop == el2.scrollTopMax);
				var s = decodeURIComponent(param[1]).replace(/</g, '&lt;').replace(/>/g, '&gt;');
				el.innerHTML = s;
				if (wasScrolledDown) el2.scrollTop = el2.scrollHeight;
				break;

			case "log_update":
				log_update(param[1]);
				break;

			default:
				console.log('ADMIN UNKNOWN: '+ param[0] +'='+ param[1]);
				break;
		}
	}
}

// callback when a control has w3-restart property
function w3_restart_cb()
{
	w3_show_block('id-restart');
}

// callback when a control has w3-reboot property
function w3_reboot_cb()
{
	w3_show_block('id-reboot');
}

var pending_restart = false;
var pending_reboot = false;
var pending_power_off = false;

function admin_restart_cb()
{
	pending_restart = true;
	w3_show_block('id-confirm');
}

function admin_reboot_cb()
{
	pending_reboot = true;
	w3_show_block('id-confirm');
}

function admin_power_off_cb()
{
	pending_power_off = true;
	w3_show_block('id-confirm');
}

var admin_pie_size = 25;
var admin_reload_secs, admin_reload_rem;

function admin_draw_pie() {
	html('id-admin-reload-secs').innerHTML = 'Admin page reload in '+ admin_reload_rem + ' secs';
	if (admin_reload_rem > 0) {
		admin_reload_rem--;
		kiwi_draw_pie('id-admin-pie', admin_pie_size, (admin_reload_secs - admin_reload_rem) / admin_reload_secs);
	} else {
		window.location.reload(true);
	}
};

function admin_wait_then_reload(secs, msg)
{
	var admin = html("id-admin");
	var s2;
	
	if (secs) {
		s2 =
			w3_divs('w3-vcenter w3-margin-T-8', 'w3-container',
				w3_inline('', '', kiwi_pie('id-admin-pie', admin_pie_size, '#eeeeee', 'deepSkyBlue')),
				w3_inline('', '',
					w3_divs('id-admin-reload-msg', ''),
					w3_divs('id-admin-reload-secs', '')
				)
			);
	} else {
		s2 =
			w3_divs('w3-vcenter w3-margin-T-8', 'w3-container',
				w3_divs('id-admin-reload-msg', '')
			);
	}
	
	var s = '<header class="w3-container w3-teal"><h5>Admin interface</h5></header>'+ s2;
	//console.log('s='+ s);
	admin.innerHTML = s;
	
	if (msg) html("id-admin-reload-msg").innerHTML = msg;
	
	if (secs) {
		admin_reload_rem = admin_reload_secs = secs;
		setInterval(admin_draw_pie, 1000);
		admin_draw_pie();
	}
}

function admin_restart_now_cb()
{
	ext_send('SET restart');
	admin_wait_then_reload(30, 'Restarting KiwiSDR server');
}

function admin_reboot_now_cb()
{
	ext_send('SET reboot');
	admin_wait_then_reload(60, 'Rebooting Beagle');
}

function admin_confirm_cb()
{
	if (pending_restart) {
		admin_restart_now_cb();
	} else
	if (pending_reboot) {
		admin_reboot_now_cb();
	} else
	if (pending_power_off) {
		ext_send('SET power_off');
		admin_wait_then_reload(0, 'Powering off Beagle');
	}
}

function admin_int_cb(path, val, first)
{
	var v = parseInt(val);
	//console.log('admin_int_cb '+ typeof val +' "'+ val +'" '+ v);
	if (isNaN(v)) v = null;

	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function admin_float_cb(path, val, first)
{
	var v = parseFloat(val);
	//console.log('admin_float_cb '+ typeof val +' "'+ val +'" '+ v);
	if (isNaN(v)) v = null;

	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, val, save);
}

function admin_bool_cb(path, val, first)
{
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, val? true:false, save);
}

function admin_set_decoded_value(path)
{
	var val = ext_get_cfg_param(path);
	//console.log('admin_set_decoded_value: path='+ path +' val='+ val);
	w3_set_decoded_value(path, val);
}

// translate radio button yes/no index to bool value
function admin_radio_YN_cb(id, idx)
{
	admin_bool_cb(id, idx? 0:1);
}
