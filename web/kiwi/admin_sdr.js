// Copyright (c) 2016 John Seamons, ZL/KF6VO

// TODO
//		input range validation
//		NTP status?


////////////////////////////////
// config
////////////////////////////////

var ITU_region_i = { 0:'R1: Europe, Africa', 1:'R2: North & South America', 2:'R3: Asia / Pacific' };

var AM_BCB_chan_i = { 0:'9 kHz', 1:'10 kHz' };

var max_freq_i = { 0:'30 MHz', 1:'32 MHz' };

var SPI_clock_i = { 0:'48 MHz', 1:'24 MHz' };

function config_html()
{
	kiwi_get_init_settings();		// make sure defaults exist
	
	var init_mode = ext_get_cfg_param('init.mode', 0);
	var init_AM_BCB_chan = ext_get_cfg_param('init.AM_BCB_chan', 0);
	var init_ITU_region = ext_get_cfg_param('init.ITU_region', 0);
	var max_freq = ext_get_cfg_param('max_freq', 0);
	
	var s1 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get_param('Initial frequency (kHz)', 'init.freq', 'admin_float_zero_cb'),
			w3_divs('', 'w3-center',
				w3_select('', 'Initial mode', '', 'init.mode', init_mode, modes_u, 'admin_select_cb')
			),
			w3_input_get_param('Initial zoom (0-11)', 'init.zoom', 'admin_int_zero_cb')
		) +

		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get_param('Initial waterfall min (dBFS, fully zoomed-out)', 'init.min_dB', 'admin_int_zero_cb'),
			w3_input_get_param('Initial waterfall max (dBFS)', 'init.max_dB', 'admin_int_zero_cb'),
			w3_divs('', 'w3-center',
				w3_select('', 'Initial AM BCB channel spacing', '', 'init.AM_BCB_chan', init_AM_BCB_chan, AM_BCB_chan_i, 'admin_select_cb')
			)
		);

   var s2 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('w3-restart', '',
				w3_input_get_param('Frequency scale offset (kHz)', 'freq_offset', 'admin_int_zero_cb'),
				w3_divs('', 'w3-text-black',
					'Adds offset to frequency scale. <br> Useful when using a frequency converter, e.g. <br>' +
					'set to 116000 kHz when 144-148 maps to 28-32 MHz.'
				)
			),
			w3_input_get_param('S-meter calibration (dB)', 'S_meter_cal', 'admin_int_zero_cb'),
			w3_input_get_param('Waterfall calibration (dB)', 'waterfall_cal', 'admin_int_zero_cb')
		) +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('', 'w3-center w3-tspace-8',
				w3_select('', 'ITU region', '', 'init.ITU_region', init_ITU_region, ITU_region_i, 'admin_select_cb'),
				w3_divs('', 'w3-text-black',
					'Configures LW/NDB, MW and <br> amateur band allocations, etc.'
				)
			),
			w3_divs('w3-restart', 'w3-center w3-tspace-8',
				w3_select('', 'Max receiver frequency', '', 'max_freq', max_freq, max_freq_i, 'admin_select_cb'),
				w3_divs('', 'w3-text-black')
			),
			w3_divs('w3-restart', 'w3-center w3-tspace-8',
				w3_select_get_param('', 'SPI clock', '', 'SPI_clock', SPI_clock_i, 'admin_select_cb', 0),
				w3_divs('', 'w3-text-black',
					'Set to 24 MHz to reduce interference <br> on 2 meters (144-148 MHz).'
				)
			)
		);

   var s3 =
		'<hr>' +
		w3_div('w3-container',
         w3_div('w3-vcenter',
            '<header class="w3-container w3-yellow"><h6>' +
            'To manually adjust/calibrate the ADC clock (e.g. when there is no GPS signal for automatic calibration) follow these steps:' +
            '</h6></header>'
         ),
         
         w3_label('w3-text-teal',
            '<ul>' +
               '<li>Open a normal user connection to the SDR</li>' +
               '<li>Tune to a time station or other accurate signal and zoom all the way in</li>' +
               '<li>Higher frequency shortwave stations are better because they will show more offset than LF/VLF stations</li>' +
               '<li>Click exactly on the signal carrier line in the waterfall</li>' +
               '<li>On the right-click menu select the <i>cal ADC clock (admin)</i> entry</li>' +
               '<li>You may have to give the admin password if not already authenticated</li>' +
               '<li>The adjustment is calculated and the carrier on the waterfall should move to the nearest 1 kHz marker</li>' +
               '<li>Use the fine-tuning controls on the IQ extension panel if necessary</li>' +
            '</ul>'
         ),

         w3_label('w3-text-teal',         
            'You can fine-tune after the above steps as follows:' +
            '<ul>' +
               '<li>Open IQ display extension</li>' +
               '<li>Set the receive frequency to the exact nominal carrier (e.g. 15000 kHz for WWV)</li>' +
               '<li>Press the <i>AM 40 Hz</i> button</li>' +
               '<li>Adjust the gain until you see a point rotating in a circle</li>' +
               '<li>Use the <i>Fcal</i> buttons to slow the rotation as much as possible</li>' +
               '<li>A full rotation in less than two seconds is good calibration</li>' +
            '</ul>'
         )
      ) +
		'<hr>';

	return w3_divs('id-config w3-hide', '', s1 + s2 + s3);
}


////////////////////////////////
// channels [not used currently]
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
	var s1 =
		'<hr>' +
		w3_divs('w3-margin-bottom', 'w3-container',
			w3_input('Top bar title', 'index_html_params.RX_TITLE', '', 'webpage_title_cb')
		) +
		w3_divs('', 'w3-container',
			'<label><b>Top bar title HTML preview</b></label>',
			w3_divs('', 'id-webpage-title-preview w3-text-black w3-background-pale-aqua', '')
		) +

		w3_divs('w3-margin-top w3-margin-bottom', 'w3-container',
			w3_input('Owner info (appears in center of top bar)', 'owner_info', '', 'webpage_owner_info_cb')
		) +
		w3_divs('', 'w3-container',
			'<label><b>Owner info HTML preview</b></label>',
			w3_divs('', 'id-webpage-owner-info-preview w3-text-black w3-background-pale-aqua', '')
		) +

		w3_divs('w3-margin-top w3-margin-bottom', 'w3-container',
			w3_input('Status', 'status_msg', '', 'webpage_status_cb')
		) +
		w3_divs('', 'w3-container',
			'<label><b>Status HTML preview</b></label>',
			w3_divs('', 'id-webpage-status-preview w3-text-black w3-background-pale-aqua', '')
		) +
		
		w3_divs('w3-margin-top', 'w3-container',
			w3_input('Window/tab title', 'index_html_params.PAGE_TITLE', '', 'webpage_string_cb')
		);
	
	var s2 =
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_input('Location', 'index_html_params.RX_LOC', '', 'webpage_string_cb'),
			w3_input('Grid square (4 or 6 char) ', 'index_html_params.RX_QRA', '', 'webpage_input_grid', null, null,
			   w3_div('id-webpage-grid-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large')
			)
		) +
		w3_half('', 'w3-container',
			w3_input('Altitude (ASL meters)', 'index_html_params.RX_ASL', '', 'webpage_string_cb'),
         w3_input('Map (Google format or lat, lon) ', 'index_html_params.RX_GMAP', '', 'webpage_input_map', null, null,
            w3_div('id-webpage-map-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large')
         )
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_half('', '',
            w3_divs('', '',
               w3_label('', 'Photo file'),
               '<input id="id-photo-file" type="file" accept="image/*" onchange="webpage_photo_file_upload()"/>',
               w3_divs('', 'id-photo-error', '')
            ),
            w3_checkbox_get_param('w3-restart', 'Photo left margin', 'index_html_params.RX_PHOTO_LEFT_MARGIN', 'admin_bool_cb', true)
         ),
			w3_input('Photo maximum height (pixels)', 'index_html_params.RX_PHOTO_HEIGHT', '', 'webpage_string_cb')
		) +
		w3_half('', 'w3-container',
			w3_input('Photo title', 'index_html_params.RX_PHOTO_TITLE', '', 'webpage_string_cb'),
			w3_input('Photo description', 'index_html_params.RX_PHOTO_DESC', '', 'webpage_string_cb')
		);
		
	var s3 =
		'<hr>' +
      w3_div('w3-container',
         w3_textarea_get_param('|width:100%',
            w3_label('w3-show-inline-block w3-text-teal', 'Additional HTML/Javascript for HTML &lt;head&gt; element', null, ' (e.g. Google analytics)'),
            'index_html_params.HTML_HEAD', 10, 100, 'webpage_string_cb', ''
         )
		) +
		
		w3_divs('w3-margin-bottom', 'w3-container', '');		// bottom gap for better scrolling look

   return w3_divs('id-webpage w3-text-teal w3-hide', '', s1 + s2 + s3);
}

function webpage_input_grid(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_grid();
}

function webpage_update_check_grid()
{
	var grid = ext_get_cfg_param('index_html_params.RX_QRA');
	w3_el('webpage-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function webpage_input_map(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_map();
}

function webpage_update_check_map()
{
	var map = ext_get_cfg_param('index_html_params.RX_GMAP');
	w3_el('webpage-map-check').innerHTML = '<a href="https://google.com/maps/place/'+ map +'" target="_blank">check map</a>';
}

function webpage_photo_uploaded(obj)
{
	var rc;
	
	if (obj.AJAX_error != undefined)
		rc = -1;
	else
		rc = obj.r;
	
	//console.log('## webpage_photo_uploaded rc='+ rc);
	if (rc == 0) {
		// server restart not needed, effect is immediate on reload or next connection
		webpage_string_cb('index_html_params.RX_PHOTO_FILE', 'kiwi.config/photo.upload');
	}
	
	var el = w3_el('photo-error');
	var e;
	
	switch (rc) {
	case -1:
		e = 'Communication error';
		break;
	case 0:
		e = 'Upload successful';
		break;
	case 1:
		e = 'Authentication failed';
		break;
	case 2:
		e = 'Not an image file?';
		break;
	case 3:
		e = 'Unable to determine file type';
		break;
	case 4:
		e = 'File too large';
		break;
	default:
		e = 'Undefined error?';
		break;
	}
	
	el.innerHTML = e;
	w3_add(el, (rc == 0)? 'w3-text-green' : 'w3-text-red');
	w3_show_block(el);
}

function webpage_photo_file_upload()
{
	ext_get_authkey(function(key) {
		webpage_photo_file_upload2(key);
	});
}

function webpage_photo_file_upload2(key)
{
	var browse = w3_el('id-photo-file');
	browse.innerHTML = 'Uploading...';
	var file = browse.files[0];
	var fdata = new FormData();
	fdata.append('photo', file, file.name);
	//console.log(file);

	var el = w3_el('photo-error');
	w3_hide(el);
	w3_remove(el, 'w3-text-red');
	w3_remove(el, 'w3-text-green');

	kiwi_ajax_send(fdata, '/PIX?'+ key, 'webpage_photo_uploaded');
}

function webpage_title_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el('id-webpage-title-preview').innerHTML = admin_preview_status_box(cfg.index_html_params.RX_TITLE);
}

function webpage_owner_info_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box(cfg.owner_info);
}

function webpage_status_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	w3_el('id-webpage-status-preview').innerHTML = admin_preview_status_box(cfg.status_msg);
}

// because of the inline quoting issue, set value dynamically
function webpage_focus()
{
	admin_set_decoded_value('index_html_params.RX_TITLE');
	w3_el('id-webpage-title-preview').innerHTML = admin_preview_status_box(cfg.index_html_params.RX_TITLE);

	admin_set_decoded_value('status_msg');
	w3_el('id-webpage-status-preview').innerHTML = admin_preview_status_box(cfg.status_msg);

	admin_set_decoded_value('index_html_params.PAGE_TITLE');
	admin_set_decoded_value('index_html_params.RX_LOC');
	admin_set_decoded_value('index_html_params.RX_QRA');
	admin_set_decoded_value('index_html_params.RX_ASL');
	admin_set_decoded_value('index_html_params.RX_GMAP');
	admin_set_decoded_value('index_html_params.RX_PHOTO_HEIGHT');
	admin_set_decoded_value('index_html_params.RX_PHOTO_TITLE');
	admin_set_decoded_value('index_html_params.RX_PHOTO_DESC');

	admin_set_decoded_value('owner_info');
	w3_el('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box(cfg.owner_info);

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
////////////////////////////////

function sdr_hu_html()
{
	var s1 =
		w3_divs('id-need-gps w3-vcenter w3-hide', '',
			'<header class="w3-container w3-yellow"><h5>Warning: GPS location field set to the default, please update</h5></header>'
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', '',
			w3_divs('w3-container', '',
					'<b>Display your KiwiSDR on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a>?</b> ' +
					w3_switch('', 'Yes', 'No', 'adm.sdr_hu_register', adm.sdr_hu_register, 'admin_radio_YN_cb')
			),
			w3_divs('w3-container', '',
					'<b>Display owner/admin email link on KiwiSDR main page?</b> ' +
					w3_switch('', 'Yes', 'No', 'contact_admin', cfg.contact_admin, 'admin_radio_YN_cb')
			)
		) +

		w3_div('id-sdr_hu-reg-status-container w3-hide',
         w3_div('w3-container',
            w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'sdr.hu registration status:') +
            w3_div('id-sdr_hu-reg-status w3-show-inline-block w3-text-black w3-background-pale-aqua', '')
         )
      );
      
   var s2 =
		'<hr>' +
		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Name', 'rx_name', '', 'w3_string_set_cfg_cb'),
			w3_input('Location', 'rx_location', '', 'w3_string_set_cfg_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			//w3_input('Device', 'rx_device', '', 'w3_string_set_cfg_cb'),
			w3_input('Admin email', 'admin_email', '', 'w3_string_set_cfg_cb'),
			w3_input('Antenna', 'rx_antenna', '', 'w3_string_set_cfg_cb')
		) +

		w3_third('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Grid square (4/6 char) ', 'rx_grid', '', 'sdr_hu_input_grid', null, null,
				w3_div('id-sdr_hu-grid-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large') + ' ' +
				w3_div('id-sdr_hu-grid-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS')
			),
			w3_div('',
            w3_input('Location (lat, lon) ', 'rx_gps', '', 'sdr_hu_check_gps', null, null,
               w3_div('id-sdr_hu-gps-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large') + ' ' +
               w3_div('id-sdr_hu-gps-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS')
            ),
				w3_div('w3-text-black', 'Format: (nn.nnnnnn, nn.nnnnnn)')
			),
			w3_input_get_param('Altitude (ASL meters)', 'rx_asl', 'admin_int_zero_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
		   w3_div('w3-restart', w3_input('API key', 'adm.api_key', '', 'w3_string_set_cfg_cb', 'from sdr.hu/register process')),
		   ''
		) +
		w3_half('w3-margin-bottom', 'w3-container',
         w3_input_get_param('Coverage frequency low (kHz)', 'sdr_hu_lo_kHz', 'admin_int_zero_cb'),
         w3_input_get_param('Coverage frequency high (kHz)', 'sdr_hu_hi_kHz', 'admin_int_zero_cb')
      );

	return w3_divs('id-sdr_hu w3-text-teal w3-hide', '', s1 + s2);
}

var sdr_hu_interval;

// because of the inline quoting issue, set value dynamically
function sdr_hu_focus()
{
	admin_set_decoded_value('rx_name');
	admin_set_decoded_value('rx_location');
	//admin_set_decoded_value('rx_device');
	admin_set_decoded_value('rx_antenna');
	admin_set_decoded_value('rx_grid');
	admin_set_decoded_value('rx_gps');
	admin_set_decoded_value('admin_email');
	admin_set_decoded_value('adm.api_key');

	// The default in the factory-distributed kiwi.json is the kiwisdr.com NZ location.
	// Detect this and ask user to change it so sdr.hu/map doesn't end up with multiple SDRs
	// defined at the kiwisdr.com location.
	var gps = decodeURIComponent(ext_get_cfg_param('rx_gps'));
	sdr_hu_check_gps('rx_gps', gps, /* first */ true);
	
	sdr_hu_update_check_grid();
	sdr_hu_update_check_map();
	
	w3_el('sdr_hu-grid-set').onclick = function() {
		var val = sdr_hu_status.grid;
		w3_set_value('rx_grid', val);
		w3_input_change('rx_grid', 'sdr_hu_input_grid');
	};

	w3_el('sdr_hu-gps-set').onclick = function() {
		var val = '('+ sdr_hu_status.lat +', '+ sdr_hu_status.lon +')';
		w3_set_value('rx_gps', val);
		w3_input_change('rx_gps', 'sdr_hu_check_gps');
	};

	// only get updates while the sdr_hu tab is selected
	ext_send("SET sdr_hu_update");
	sdr_hu_interval = setInterval(function() {ext_send("SET sdr_hu_update");}, 5000);
}

function sdr_hu_input_grid(path, val)
{
	w3_string_set_cfg_cb(path, val);
	sdr_hu_update_check_grid();
}

function sdr_hu_update_check_grid()
{
	var grid = ext_get_cfg_param('rx_grid');
	w3_el('sdr_hu-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
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
	w3_el('sdr_hu-gps-check').innerHTML = '<a href="https://google.com/maps/place/'+ gps +'" target="_blank">check map</a>';
}

function sdr_hu_blur(id)
{
	kiwi_clearInterval(sdr_hu_interval);
}

var sdr_hu_status = { };

function sdr_hu_update(p)
{
	var i;
	var sdr_hu_json = decodeURIComponent(p);
	//console.log('sdr_hu_json='+ sdr_hu_json);
	sdr_hu_status = JSON.parse(sdr_hu_json);
	
	// sdr.hu registration status
	if (sdr_hu_status.reg != undefined && sdr_hu_status.reg != '') {
	   var el = w3_el('id-sdr_hu-reg-status');
	   el.innerHTML = sdr_hu_status.reg;
		w3_show_inline_block('id-sdr_hu-reg-status-container');
	}
	
	// GPS has had a solution, show buttons
	if (sdr_hu_status.lat != undefined) {
		w3_show_inline_block('id-sdr_hu-grid-set');
		w3_show_inline_block('id-sdr_hu-gps-set');
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

// called by extensions to register extension admin configuration
function ext_admin_config(id, nav_name, ext_html, focus_blur_cb)
{
   // indicate we don't want a callback unless explicitly requested
   if (focus_blur_cb == undefined) focus_blur_cb = null;

	var ci = ext_seq % admin_colors.length;
	w3_el('id-admin-ext-nav').innerHTML +=
		w3_nav(admin_colors[ci] + ((ci&1)? ' w3-css-lightGray':''), nav_name, id, focus_blur_cb);
	ext_seq++;
	w3_el('id-admin-ext-config').innerHTML += ext_html;
}
