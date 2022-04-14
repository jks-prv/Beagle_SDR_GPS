// Copyright (c) 2016 John Seamons, ZL/KF6VO

// TODO
//		input range validation
//		NTP status?

var admin = {
   long_running: false,
   is_multi_core: false,
   reg_status: {},
   
   pie_size: 25,
   
   _last_: 0
};


////////////////////////////////
// status
////////////////////////////////

function status_html()
{
   var s2 = admin_sdr_mode?
		('<hr>' +
         w3_div('id-msg-errors w3-container') + 
         w3_div('w3-container w3-section',
            w3_inline('',
               w3_div('', 'Realtime response histograms:'),
               w3_button('w3-padding-smaller w3-aqua|margin-left:10px', 'Reset', 'status_dpump_hist_reset_cb')
            ),
            w3_div('w3-container',
               w3_div('id-status-dp-hist'),
               w3_div('id-status-in-hist')
            )
         )
      ) : '';
   
	var s =
      w3_div('id-status w3-hide',
         '<hr>' +
         w3_div('id-problems w3-container') +
         w3_div('id-msg-config w3-container') +
         w3_div('id-msg-gps w3-container') +
         w3_div('id-msg-snr w3-container') +
         '<hr>' +
         w3_div('id-msg-stats-cpu w3-container') +
         w3_div('id-msg-stats-xfer w3-container') +
         s2 +
         '<hr>' +
         w3_div('id-users-list w3-container') +
         '<hr>'
      );

	return s;
}

function status_dpump_hist_reset_cb(id, idx)
{
	ext_send('SET dpump_hist_reset');
}

function status_user_kick_cb(id, idx)
{
   console.log('status_user_kick_cb='+ idx);
	ext_send('SET user_kick='+ idx);
}


////////////////////////////////
// mode
////////////////////////////////

var mode_icon_snd12 = w3_icon('w3-text-blue', 'fa-volume-up', 28) +'&nbsp;';
var mode_icon_snd20 = w3_icon('w3-text-red', 'fa-volume-up', 28) +'&nbsp;';
var mode_icon_fft = w3_icon('w3-text-green', 'fa-bar-chart', 28) +'&nbsp;';
var mode_icon_wf = w3_icon('w3-text-amber', 'fa-area-chart', 28) +'&nbsp;';

function mode_html()
{
   var bw = 235, bwpx = px(bw);
   var pw = 113, pwpx = px(pw);
   var ci = 0;
	var s =
	w3_div('id-mode w3-hide',
		'<hr>',
		w3_div('w3-container',
         w3_div('w3-flex w3-margin-B-8',
            w3_div('w3-text-teal|width:'+ pwpx, ' '),
            w3_div('w3-text-teal w3-center w3-bold|width:'+ bwpx, 'select FPGA mode'),
            w3_div('id-fw-hdr w3-flex w3-margin-left')
         ),
         
         w3_div('',
            w3_div('w3-left',
               w3_div('w3-flex w3-halign-center w3-margin-B-5', '<img src="gfx/kiwi.73x73.jpg" width="73" height="73" />'),
               w3_div('w3-flex w3-halign-center w3-margin-B-5', '<img src="gfx/cowbelly.73x73.jpg" width="73" height="73" />'),
               w3_div('w3-flex', '<img src="gfx/kiwi.derp.113x73.jpg" width="113" height="73" />'),
               admin.is_multi_core? 
                  w3_div('w3-flex', '<img src="gfx/kiwi.derp.113x73.jpg" width="113" height="73" />')
               :
                  ''
            ),
            w3_sidenav('id-fw-nav|width:'+ bwpx +';border-collapse:collapse',
               w3_nav(admin_colors[ci++] +' w3-border w3-padding-xxlarge w3-restart', 'Kiwi classic', kiwi.RX4_WF4, 'firmware_sel_cb', (adm.firmware_sel == kiwi.RX4_WF4)),
               w3_nav(admin_colors[ci++] +' w3-border w3-padding-xxlarge w3-restart', 'More receivers', kiwi.RX8_WF2, 'firmware_sel_cb', (adm.firmware_sel == kiwi.RX8_WF2)),
               w3_nav(admin_colors[ci++] +' w3-border w3-padding-xxlarge w3-restart', 'More bandwidth', kiwi.RX3_WF3, 'firmware_sel_cb', (adm.firmware_sel == kiwi.RX3_WF3)),
               admin.is_multi_core? 
                  w3_nav(admin_colors[ci++] +' w3-border w3-padding-xxlarge w3-restart', 'MCORE rx14_wf0', kiwi.RX14_WF0, 'firmware_sel_cb', (adm.firmware_sel == kiwi.RX14_WF0))
               :
                  ''
            ),
            w3_div('w3-margin-left w3-left',
               w3_div('id-fw-44 w3-flex w3-padding-TB-7'),
               w3_div('id-fw-82 w3-flex w3-padding-TB-7'),
               w3_div('id-fw-22 w3-flex w3-padding-TB-7')
            )
         ),
         
		   w3_div('w3-clear', ' '),      // don't quite understand why this is needed, but it is
		   w3_div('w3-margin-T-16', '<hr>'),

         w3_col_percent('w3-section/ w3-hspace-16',
            w3_div('w3-text-black',
               w3_text('w3-bold w3-margin-B-8 w3-text-teal', 'Trade-offs: receiver channels, audio bandwidth and waterfalls'),

               w3_div('w3-flex w3-valign-center', w3_div('|width:40px', mode_icon_snd12), w3_div('', 'Audio output, 12 kHz max bandwidth')),
               w3_div('w3-flex w3-valign-center', w3_div('|width:40px', mode_icon_snd20), w3_div('', 'Audio output, 20 kHz max bandwidth')),
               w3_div('w3-flex w3-margin-B-8 w3-valign-center', w3_div('|width:40px', mode_icon_wf), w3_div('', 'Tuneable waterfall/spectrum, 30 MHz bandwidth, 14-level zoom')),
               w3_div('w3-flex w3-margin-B-8 w3-valign-center', w3_div('|width:40px', mode_icon_fft), w3_div('', 'Audio FFT display, 12 kHz max bandwidth ')),

               'The original Kiwi FPGA with its 4 tuneable audio/waterfall receiver channels and 12 GPS channels was completely full. ' +
               'But it is now possible to load a different FPGA configuration where 2 of the waterfalls have been traded for ' +
               'adding more audio-only receiver channels. '
            ), 48,
   
            w3_div('w3-text-black', ' '), 4,
   
            w3_div('w3-text-black',
               'Having more receiver channels per Kiwi is especially important with the recently added features that are channel intensive. ' +
               'Namely the TDoA service, WSPR autorun and external connection via the kiwirecorder program for using other software such ' +
               'as WSJT-X and Dream (DRM). When these kinds of connections are made channels rx2 - rx7 will be used first ' +
               'leaving rx0 and rx1 available for normal browser connections where it is desirable to view the waterfall. ' +
               'However rx0 and rx1 will be used last if necessary. The configurable TDoA channel limit still applies.<br><br>' +

               'To compensate for lack of the waterfall/spectrum on the new channels an audio-bandwidth FFT is presented instead. ' +
               'This requires no additional FPGA resources.'
            ), 48
         ),
		   w3_div('w3-margin-T-16', '<hr>'),
		   
         w3_col_percent('w3-section/ w3-hspace-16',
            w3_div('w3-text-black',
               'And now a third option "More bandwidth". The audio bandwidth is increased from 12 to 20 kHz. ' +
               'This supports wide passbands for hi-fidelity listening of AM BCB and SW stations. ' +
               'And also wide IQ bandwidths for external applications processing large parts of the spectrum. '
            ), 48,
   
            w3_div('w3-text-black', ' '), 4,
   
            w3_div('w3-text-black',
               'In exchange the number of channels drops from four to three. It may have to drop to two in the future depending ' +
               'on how stable operation is with three channels.'
            ), 48
         ),
		   w3_div('w3-margin-T-16', '<hr>')
      )
	);
	return s;
}

function mode_focus()
{
   console.log('mode_focus');
   var i, s;
   var iwpx = px(90);
   
   s = '';
   for (i = 0; i < 8; i++) s += w3_div('w3-margin-left w3-bold w3-center|width:'+ iwpx, 'rx'+ i);
   w3_innerHTML('id-fw-hdr', s);

   //var rx12wf = w3_div('w3-margin-left w3-border w3-border-light-blue w3-center|width:'+ iwpx, mode_icon_snd12, mode_icon_fft, '<br>', mode_icon_wf);
   var rx12wf = w3_div('w3-margin-left w3-border w3-border-light-blue w3-center|width:'+ iwpx, mode_icon_snd12, '<br>', mode_icon_wf);
   var rx20wf = w3_div('w3-margin-left w3-border w3-border-light-blue w3-center|width:'+ iwpx, mode_icon_snd20, '<br>', mode_icon_wf);
   
   var rx = w3_div('w3-margin-left w3-border w3-border-light-blue w3-center|width:'+ iwpx, mode_icon_snd12, '<br>', mode_icon_fft);
   
   s = '';
   for (i = 0; i < 4; i++) s += rx12wf;
   //for (i = 4; i < 8; i++) s += w3_div('w3-margin-left|width:50px', '&nbsp;');
   w3_innerHTML('id-fw-44', s);

   s = '';
   for (i = 0; i < 2; i++) s += rx12wf;
   for (i = 2; i < 8; i++) s += rx;
   w3_innerHTML('id-fw-82', s);

   s = '';
   for (i = 0; i < 3; i++) s += rx20wf;
   w3_innerHTML('id-fw-22', s);
}

function firmware_sel_cb_focus(path)
{
   var firmware_sel = +path;
   console.log('firmware_sel_cb_focus path='+ path);
	ext_set_cfg_param('adm.firmware_sel', firmware_sel, true);
}


////////////////////////////////
// control
////////////////////////////////

function control_html()
{
	var s1 =
		'<hr>' +
		w3_half('w3-valign', '',
         w3_div('',
            w3_div('',
               w3_button('w3-aqua w3-margin', 'KiwiSDR server restart', 'control_restart_cb'),
               w3_button('w3-blue w3-margin', 'Beagle reboot', 'control_reboot_cb'),
               w3_button('w3-red w3-margin', 'Beagle power off', 'control_power_off_cb')
            ),
            w3_div('id-confirm w3-valign w3-hide',
               w3_button('w3-green w3-margin', 'Confirm', 'control_confirm_cb'),
               w3_button('w3-yellow w3-margin', 'Cancel', 'control_confirm_cancel_cb')
            )
         ),
			w3_div('w3-container w3-center',
            '<b>Daily restart?</b> ' +
            w3_switch('', 'Yes', 'No', 'adm.daily_restart', adm.daily_restart, 'admin_radio_YN_cb'),
				w3_div('w3-text-black',
					"Set if you're having problems with the server<br>after it has run for a period of time.<br>" +
					"Restart occurs at the same time as updates (0200-0600 UTC)<br> and will wait until there are no connections."
				)
			)
      );

   // Let cfg.ext_api_nchans retain values > rx_chans if it was set when another configuration
   // was used. Just clamp the menu value to the current rx_chans;
	var ext_api_ch = ext_get_cfg_param('ext_api_nchans', -1);
	if (ext_api_ch == -1) ext_api_ch = rx_chans;      // has never been set
	var ext_api_nchans = Math.min(ext_api_ch, rx_chans);
   var ext_api_chans_u = { 0:'none' };
   for (var i = 1; i <= rx_chans; i++)
      ext_api_chans_u[i] = i.toFixed(0);

	var s2 =
		'<hr>' +
		w3_third('w3-container w3-valign', '',
         w3_divs('/w3-center w3-tspace-8',
            w3_inline('w3-halign-space-around/',
               w3_divs('w3-center/w3-margin-T-8',
                  w3_div('', '<b>Enable user<br>connections?</b>'),
                  w3_switch('', 'Yes', 'No', 'adm.server_enabled', adm.server_enabled, 'server_enabled_cb')
               ),
         
               w3_divs('w3-center/w3-margin-T-8',
                  w3_div('', '<b>Close all active<br>user connections</b>'),
                  w3_button('w3-red', 'Kick', 'control_user_kick_cb')
               )
            )
         ),

         w3_divs('/w3-center w3-tspace-8',
            w3_select('w3-width-auto', 'Number of simultaneous channels available<br>for connection by non-Kiwi apps',
               '', 'ext_api_nchans', ext_api_nchans, ext_api_chans_u, 'admin_select_cb'),
            w3_div('w3-text-black',
               'If you want to limit incoming connections from <br> non-Kiwi apps like kiwirecorder set this value. <br>' +
               'This overrides similar value in TDoA extension settings.'
            )
         ),

			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_div('', '<b>Disable waterfalls/spectrum?</b>'),
            w3_switch('', 'Yes', 'No', 'cfg.no_wf', cfg.no_wf, 'admin_radio_YN_cb'),
				w3_text('w3-text-black w3-center',
				   'Set "yes" to save Internet bandwidth by preventing <br>' +
				   'the waterfall and spectrum from being displayed.'
				)
			)
		) +

		w3_div('w3-container w3-margin-top',
			w3_input_get('', 'Reason if disabled', 'reason_disabled', 'reason_disabled_cb', '', 'will be shown to users attempting to connect')
		) +

		w3_divs('w3-margin-top/w3-container',
			'<label><b>Reason HTML preview</b></label>',
			w3_div('id-reason-disabled-preview w3-text-black w3-background-pale-aqua', '')
		);
	
	var n_camp = ext_get_cfg_param('n_camp', -1);
	console.log('rx_chans='+ rx_chans +' n_camp='+ n_camp +' max_camp='+ max_camp);
   var n_camp_u = [ 'disable camping' ];
   for (var i = 1; i <= max_camp; i++)
      n_camp_u[i] = i.toFixed(0);
   var snr_interval_u = [ 'disable', 'hourly', '4 hours', '6 hours', '24 hours' ];
   var snr_interval = [ 0, 1, 4, 6, 24 ];

	var s3 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_div('',
				w3_input_get('', 'Inactivity time limit (min, 0 = no limit)', 'inactivity_timeout_mins', 'admin_int_cb'),
				w3_div('w3-text-black', 'Connections from the local network are exempt.')
			),
			w3_div('',
				w3_input_get('', '24hr per-IP addr time limit (min, 0 = no limit)', 'ip_limit_mins', 'admin_int_cb'),
				w3_div('w3-text-black', 'Connections from the local network are exempt.')
			),
			w3_div('',
				w3_input_get('', 'Time limit exemption password', 'adm.tlimit_exempt_pwd', 'w3_string_set_cfg_cb'),
				w3_div('w3-text-black', 'Password users can give to override time limits.')
			)
		) +

		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
         w3_divs('w3-restart/w3-center w3-tspace-8',
            w3_select('w3-width-auto', 'Number of audio campers per channel', '', 'n_camp', n_camp, n_camp_u, 'admin_select_cb'),
            w3_div('w3-text-black',
               'Reduce this value if your Kiwi is experiencing <br>' +
               'performance problems from too many audio campers.'
            )
         ),
         
			w3_divs('/w3-center w3-tspace-8',
            w3_select('w3-width-auto', 'SNR measurement interval', '', 'cfg.snr_meas_interval_hrs', cfg.snr_meas_interval_hrs, snr_interval_u, 'admin_select_cb'),
				w3_text('w3-text-black w3-center',
				   'Enables automatic sampling of <br>' +
				   'signal-to-noise ratio (SNR) at the specified interval. <br>' +
				   'Access SNR data in JSON format using <br>' +
				   'URL of the form: <i>my_kiwi:8073/snr</i>'
				)
			),
			
			w3_divs('/w3-center w3-tspace-8',
            w3_div('', '<b>Timestamp SNR with local time?</b>'),
            w3_switch('', 'Yes', 'No', 'cfg.snr_local_time', cfg.snr_local_time, 'admin_radio_YN_cb')
         )
		) +
		'<hr>';

   return w3_div('id-control w3-text-teal w3-hide', s1 + (admin_sdr_mode? (s2 + s3) : ''));
}

function control_focus()
{
	w3_innerHTML('id-reason-disabled-preview', admin_preview_status_box('disabled_preview_1', cfg.reason_disabled));
}

function server_enabled_cb(path, idx, first)
{
	idx = +idx;
	var enabled = (idx == 0);
	//console.log('server_enabled_cb: first='+ first +' enabled='+ enabled);

	if (!first) {
		ext_send('SET server_enabled='+ (enabled? 1:0));
	}
	
	admin_bool_cb(path, enabled, first);
}

function control_user_kick_cb(id, idx)
{
	ext_send('SET user_kick=-1');
}

function reason_disabled_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	w3_el('id-reason-disabled-preview').innerHTML = admin_preview_status_box('disabled_preview_2', cfg.reason_disabled);
}

var pending_restart = false;
var pending_reboot = false;
var pending_power_off = false;

function control_restart_cb()
{
	pending_restart = true;
	w3_show_block('id-confirm');
}

function control_reboot_cb()
{
	pending_reboot = true;
	w3_show_block('id-confirm');
}

function control_power_off_cb()
{
	pending_power_off = true;
	w3_show_block('id-confirm');
}

function control_confirm_cb()
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

function control_confirm_cancel_cb()
{
	w3_hide('id-confirm');
}


////////////////////////////////
// connect
////////////////////////////////

var connect = {
   focus: 0,
   timeout: null
};

// REMEMBER: cfg.server_url is what's used in kiwisdr.com registration
// cfg.sdr_hu_dom_sel is just the selector of connect_dom_sel (e.g. cfg.sdr_hu_dom_sel == connect_dom_sel.NAM)
// Both these are cfg parameters stored in kiwi.json, so don't get confused.

// cfg.{sdr_hu_dom_sel(num), sdr_hu_dom_name(str), sdr_hu_dom_ip(str), server_url(str)}

var connect_dom_sel = { NAM:0, DUC:1, PUB:2, SIP:3, REV:4 };
var duc_update_i = { 0:'5 min', 1:'10 min', 2:'15 min', 3:'30 min', 4:'60 min' };
var duc_update_v = { 0:5, 1:10, 2:15, 3:30, 4:60 };

function connect_html()
{
   // remove old references to kiwisdr.example.com so empty field message shows
   if (ext_get_cfg_param('server_url') == 'kiwisdr.example.com')
      ext_set_cfg_param('cfg.server_url', '', true);
   if (ext_get_cfg_param('sdr_hu_dom_name') == 'kiwisdr.example.com')
      ext_set_cfg_param('cfg.sdr_hu_dom_name', '', true);

   var ci = 0;
   var s1 =
		w3_div('w3-valign',
			'<header class="w3-container w3-yellow"><h5>' +
			'If you are not able to make an incoming connection from the Internet to your Kiwi because ' +
			'of problems <br> with your router or Internet Service Provider (ISP) then please consider using the KiwiSDR ' +
			'<a href="http://proxy.kiwisdr.com" target="_blank">reverse proxy service</a>.' +
			'</h5></header>'
		) +
		
      '<hr>' +
      w3_divs('w3-container/w3-tspace-8',
         w3_label('w3-bold', 'What domain name or IP address will people use to connect to your KiwiSDR?<br>' +
            'If you are listing on rx.kiwisdr.com this information will be part of your entry.<br>' +
            'Click one of the five options below and enter any additional information:<br><br>'),
         
         // (n/a anymore) w3-static because w3-sidenav does a position:fixed which is no good here at the bottom of the page
         // w3-left to get float:left to put the input fields on the right side
         // w3-sidenav-full-height to match the height of the w3_input() on the right side
		   w3_sidenav('id-admin-nav-dom w3-margin-R-16 w3-restart',
		      w3_nav(admin_colors[ci++] +' w3-border', 'Domain Name', 'connect_dom_nam', 'connect_dom_nam', (cfg.sdr_hu_dom_sel == connect_dom_sel.NAM)),
		      w3_nav(admin_colors[ci++] +' w3-border', 'DUC Domain', 'connect_dom_duc', 'connect_dom_duc', (cfg.sdr_hu_dom_sel == connect_dom_sel.DUC)),
		      w3_nav(admin_colors[ci++] +' w3-border', 'Reverse Proxy', 'connect_dom_rev', 'connect_dom_rev', (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)),
		      w3_nav(admin_colors[ci++] +' w3-border', 'Public IP', 'connect_dom_pub', 'connect_dom_pub', (cfg.sdr_hu_dom_sel == connect_dom_sel.PUB)),
		      w3_nav(admin_colors[ci++] +' w3-border', 'Specified IP', 'connect_dom_sip', 'connect_dom_sip', (cfg.sdr_hu_dom_sel == connect_dom_sel.SIP))
		   ),
		   
		   w3_divs('w3-padding-L-16/w3-padding-T-1',
            w3_div('w3-show-inline-block|width:70%;', w3_input_get('', '', 'sdr_hu_dom_name', 'connect_dom_name_cb', '',
               'Enter domain name that you will point to Kiwi public IP address, e.g. kiwisdr.my_domain.com (don\'t include port number)')),
            w3_div('id-connect-duc-dom w3-padding-TB-8'),
            w3_div('id-connect-rev-dom w3-padding-TB-8'),
            w3_div('id-connect-pub-ip w3-padding-TB-8'),
            w3_div('w3-show-inline-block|width:70%;', w3_input_get('', '', 'sdr_hu_dom_ip', 'connect_dom_ip_cb', '',
               'Enter known public IP address of the Kiwi (don\'t include port number)'))
         ),
         
		   w3_div('w3-margin-T-16', 
            w3_label('id-connect-url-text-label w3-show-inline-block w3-margin-R-16 w3-text-teal') +
			   w3_div('id-connect-url w3-show-inline-block w3-text-black w3-background-pale-aqua')
         )
      );

   var s2 =
		'<hr>' +
      w3_div('w3-container w3-text-teal|width:80%',
         w3_input_get('', 'Next Kiwi URL redirect', 'adm.url_redirect', 'connect_url_redirect_cb'),
         w3_div('w3-text-black',
            'Use this setting to get multiple Kiwis to respond to a single URL.<br>' +
            'When all the channels of this Kiwi are busy further connection attempts ' +
            'will be redirected to the above URL.<br>' +
            'Example: Your Kiwi is known as "mykiwi.com:8073". ' +
            'Configure another Kiwi to use port 8074 and be known as "mykiwi.com:8074".<br>' +
            'On the port 8073 Kiwi set the above field to "http://mykiwi.com:8074".<br>' +
            'On the port 8074 Kiwi leave the above field blank.<br>' +
            'Configure the port 8074 Kiwi as normal (i.e. router port open, dynamic DNS, proxy etc.)<br><br>' +
            '<b>CAUTION:</b> Do not create a cycle by redirecting Kiwis like: A -> B -> A &nbsp;' +
            'Always create a chain that stops redirecting at the end: A -> B <br>' +
            'This prevents the browser from going into a loop when all channels on all Kiwis are full.'
         )
		);

   var s3 =
		'<hr>' +
		w3_divs('/w3-tspace-8',
         w3_div('w3-container w3-valign',
            '<header class="w3-container w3-yellow"><h6>' +
            'Please read these instructions before use: ' +
            '<a href="http://kiwisdr.com/quickstart/index.html#id-net-duc" target="_blank">dynamic DNS update client (DUC)</a>' +
            '</h6></header>'
         ),

			w3_col_percent('w3-text-teal/w3-container',
			   w3_div('w3-text-teal w3-bold', 'Dynamic DNS update client (DUC) configuration'), 50,
				w3_div('w3-text-teal w3-bold w3-center w3-light-grey', 'Account at noip.com'), 50
			),
			
			w3_col_percent('w3-text-teal/w3-container',
				w3_div(), 50,
				w3_input_get('', 'Username or email', 'adm.duc_user', 'w3_string_set_cfg_cb', '', 'required'), 25,
				w3_input_get('', 'Password', 'adm.duc_pass', 'w3_string_set_cfg_cb', '', 'required'), 25
			),
			
			w3_col_percent('w3-text-teal/w3-container',
				w3_div('w3-center',
					'<b>Enable DUC at startup?</b><br>' +
					w3_switch('w3-margin-T-8', 'Yes', 'No', 'adm.duc_enable', adm.duc_enable, 'connect_DUC_enabled_cb')
				), 20,
				
				w3_div('w3-center',
				   w3_select('w3-width-auto', 'Update', '', 'adm.duc_update', adm.duc_update, duc_update_i, 'admin_select_cb')
				), 10,
				
				w3_div('w3-center w3-tspace-8',
					w3_button('w3-aqua', 'Click to (re)start DUC', 'connect_DUC_start_cb'),
					w3_div('w3-text-black',
						'After changing username or password click to test changes.'
					)
				), 20,
				
				w3_input_get('', 'Host', 'adm.duc_host', 'connect_DUC_host_cb', '', 'required'), 50
			),
			
			w3_div('w3-container',
            w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'Status:') +
				w3_div('id-net-duc-status w3-show-inline-block w3-text-black w3-background-pale-aqua', '')
			)
		);

   var s4 =
      '<hr>' +
      w3_divs('/w3-tspace-8',
         w3_div('w3-container w3-valign',
            '<header class="w3-container w3-yellow"><h6>' +
            'Please read these instructions before use: ' +
            '<a href="http://proxy.kiwisdr.com" target="_blank">reverse proxy service</a>' +
            '</h6></header>'
         ),

         w3_text('id-proxy-menu w3-margin-left w3-valign w3-nopad w3-width-min w3-red w3-hide',
            'When adding or making changes to the proxy user key or host name fields <br>' +
            'you must click the re-register button below AND select "Reverse proxy" <br>' +
            'in the menu at the top of the page.'
         ),

			w3_col_percent('w3-text-teal/w3-container',
			   w3_div('w3-text-teal w3-bold', 'Reverse proxy configuration'), 50,
				w3_div('id-proxy-hdr w3-text-teal w3-bold w3-center w3-light-grey', 'Proxy information for proxy.kiwisdr.com'), 50
			),
			
			w3_col_percent('w3-text-teal/w3-container',
				w3_div(), 50,
				w3_input_get('', 'User key (see instructions)', 'adm.rev_user', 'connect_rev_user_cb', '', 'required'
				), 50
			),
			
			w3_col_percent('w3-text-teal/w3-container',
				w3_div('w3-center w3-tspace-8',
					w3_button('w3-aqua', 'Click to (re)register', 'connect_rev_register_cb'),
					w3_div('w3-text-black',
						'After changing user key or<br>host name click to register proxy.'
					)
				), 50,
				
			w3_div('',
               w3_div('w3-show-inline-block|width:60%;',
                  w3_input_get('', 'Host name (your choice, see instructions)',
                     'adm.rev_host', 'connect_rev_host_cb', '', 'required'
                  )
               ) +
               w3_div('id-connect-proxy_server w3-show-inline-block')
            ), 50
			),
			
			w3_div('w3-container',
            w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'Status:') +
				w3_div('id-connect-rev-status w3-show-inline-block w3-text-black w3-background-pale-aqua', '')
			)
		) +
		'<hr>';

	return w3_div('id-connect w3-text-teal w3-hide', s1 + s2 + s3 + s4);
}

function connect_focus()
{
    connect.focus = 1;
    connect_update_url();
	w3_el('id-proxy-hdr').innerHTML = 'Proxy information for '+ adm.proxy_server;
    ext_send('SET DUC_status_query');
	
   w3_hide('id-proxy-menu');
	if (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)
	   ext_send('SET rev_status_query');
}

function connect_blur()
{
   connect.focus = 0;
}

function connect_update_url()
{
   var ok, ok_color;
   
   ok = (adm.duc_host && adm.duc_host != '');
   ok_color = ok? 'w3-background-pale-aqua' : 'w3-override-yellow';
	w3_el('id-connect-duc-dom').innerHTML = 'Use domain name from DUC configuration below: ' +
	   w3_div('w3-show-inline-block w3-text-black '+ ok_color, ok? adm.duc_host : '(none currently set)');

   ok = (adm.rev_host && adm.rev_host != '');
   ok_color = ok? 'w3-background-pale-aqua' : 'w3-override-yellow';
   var rev_host_fqdn = ok? (adm.rev_host +'.'+ adm.proxy_server) : '(none currently set)';
	w3_el('id-connect-rev-dom').innerHTML = 'Use domain name from reverse proxy configuration below: ' +
	   w3_div('w3-show-inline-block w3-text-black '+ ok_color, rev_host_fqdn);
	w3_el('id-connect-proxy_server').innerHTML = '.'+ adm.proxy_server;

   ok = config_net.pub_ip;
   ok_color = ok? 'w3-background-pale-aqua' : 'w3-override-yellow';
	w3_el('id-connect-pub-ip').innerHTML = 'Public IP address detected by Kiwi: ' +
	   w3_div('w3-show-inline-block w3-text-black '+ ok_color, ok? config_net.pub_ip : '(no public IP address detected)');

   var host = decodeURIComponent(cfg.server_url);
   var host_and_port = host;
   
   //console.log('connect_update_url: sdr_hu_dom_sel='+ cfg.sdr_hu_dom_sel +' REV='+ connect_dom_sel.REV +' host='+ host_and_port +' port_ext='+ adm.port_ext);

   if (cfg.sdr_hu_dom_sel != connect_dom_sel.REV) {
      host_and_port += ':'+ adm.port_ext;
      w3_set_label('Based on above selection, and external port from Network tab, the URL to connect to your Kiwi is:', 'connect-url-text');
   } else {
      host_and_port += ':8073';
      if (adm.port_ext != 8073)
         host_and_port += ' (proxy always uses port 8073 even though your external port is '+ adm.port_ext +')';
      w3_set_label('Based on the above selection the URL to connect to your Kiwi is:', 'connect-url-text');
   }
   
   ok = (host != '');
   w3_color('id-connect-url', null, ok? 'hsl(180, 100%, 95%)' : '#ffeb3b');   // w3-background-pale-aqua : w3-override-yellow
   w3_el('id-connect-url').innerHTML = ok? ('http://'+ host_and_port) : '(incomplete information)';
}

function connect_dom_nam_focus()
{
   console.log('connect_dom_nam_focus server_url='+ cfg.sdr_hu_dom_name);
	ext_set_cfg_param('cfg.server_url', cfg.sdr_hu_dom_name, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.NAM, true);
	connect_update_url();
}

function connect_dom_duc_focus()
{
   console.log('connect_dom_duc_focus server_url='+ adm.duc_host);
	ext_set_cfg_param('cfg.server_url', adm.duc_host, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.DUC, true);
	connect_update_url();
}

function connect_dom_rev_focus()
{
   var dom = (adm.rev_host == '')? '' : (adm.rev_host +'.'+ adm.proxy_server);
   console.log('connect_dom_rev_focus server_url='+ dom);
	ext_set_cfg_param('cfg.server_url', dom, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.REV, true);
	connect_update_url();
}

function connect_dom_pub_focus()
{
   console.log('connect_dom_pub_focus server_url='+ config_net.pub_ip);
	ext_set_cfg_param('cfg.server_url', config_net.pub_ip, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.PUB, true);
	connect_update_url();
}

function connect_dom_sip_focus()
{
   console.log('connect_dom_sip_focus server_url='+ cfg.sdr_hu_dom_ip);
	ext_set_cfg_param('cfg.server_url', cfg.sdr_hu_dom_ip, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.SIP, true);
	connect_update_url();
}

function connect_dom_name_cb(path, val, first)
{
	connect_remove_port(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.NAM)     // if currently selected option update the value
      connect_dom_nam_focus();
}

function connect_dom_ip_cb(path, val, first)
{
	connect_remove_port(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.SIP)     // if currently selected option update the value
      connect_dom_sip_focus();
}

function connect_remove_port(el, s, first)
{
	var state = { bad:0, number:1, alpha:2, remove:3 };
	var st = state.bad;
	
	s = s.replace('http://', '');
	var sl = s.length;
	
	for (var i = sl-1; i >= 0; i--) {
		var c = s.charAt(i);
		if (c >= '0' && c <= '9') {
			st = state.number;
			continue;
		}
		if (c == ':') {
			if (st == state.number)
				st = state.remove;
			break;
		}
		st = state.alpha;
		if (c == ']')
		   break;      // start of escaped ipv6 with embedded ':'s
	}
	
	if (st == state.remove) {
		s = s.substr(0,i);
	}
	
	w3_string_set_cfg_cb(el, s, first);
	admin_set_decoded_value(el);
}


// URL chain

function connect_url_redirect_cb(path, val, first)
{
	if (val != '' && !val.startsWith('http://')) val = 'http://'+ val;
	w3_string_set_cfg_cb(path, val, first);
	w3_set_value('id-'+ path, val);
}


// DUC

function connect_DUC_enabled_cb(path, idx, first)
{
	idx = +idx;
	var enabled = (idx == 0);
	//console.log('connect_DUC_enabled_cb: first='+ first +' enabled='+ enabled);

	if (!first) {
		//?(enabled? 1:0);
	}
	
	admin_bool_cb(path, enabled, first);
}

function connect_DUC_start_cb(id, idx)
{
	// decode stored json values because we recode below to encode spaces of composite string
	var s = '-u '+ sq(decodeURIComponent(adm.duc_user)) +' -p '+ sq(decodeURIComponent(adm.duc_pass)) +
	   ' -H '+ sq(decodeURIComponent(adm.duc_host)) +' -U '+ duc_update_v[adm.duc_update];
	console.log('start DUC: '+ s);
	w3_innerHTML('id-net-duc-status', 'Getting status from DUC server...');
	ext_send('SET DUC_start args='+ encodeURIComponent(s));
}

function connect_DUC_host_cb(path, val, first)
{
   w3_string_set_cfg_cb(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.DUC)     // if currently selected option update the value
      connect_dom_duc_focus();
   else
      connect_update_url();
}

function connect_DUC_status_cb(status)
{
	status = +status;
	console.log('DUC_status='+ status);
	var s;
	
	switch (status) {
		case 0:   s = 'DUC started successfully'; break;
		case 100: s = 'Incorrect username or password'; break;
		case 101: s = 'No hosts defined on your account at noip.com; please correct and retry'; break;
		case 102: s = 'Please specify a host'; break;
		case 103: s = 'Host given isn\'t defined on your account at noip.com; please correct and retry'; break;
		case 300: s = 'DUC start failed'; break;
		case 301: s = 'DUC enabled and running'; break;
		default:  s = 'DUC internal error: '+ status; break;
	}
	
	w3_el('id-net-duc-status').innerHTML = s;
}


// reverse proxy

function connect_rev_usage()
{
   w3_show('id-proxy-menu');
   w3_scrollDown('id-kiwi-container');
}

function connect_rev_register_cb(id, idx)
{
   if (adm.rev_user == '' || adm.rev_host == '')
      return connect_rev_status_cb(100);
   
   connect_rev_usage();
   kiwi_clearTimeout(connect.timeout);
	w3_innerHTML('id-connect-rev-status', 'Getting status from proxy server...');
	var s = 'user='+ adm.rev_user +' host='+ adm.rev_host;
	console.log('start rev: '+ s);
	ext_send('SET rev_register '+ s);
}

function connect_rev_user_cb(path, val, first)
{
   connect_rev_usage();
   w3_clearInnerHTML('id-connect-rev-status');
   w3_string_set_cfg_cb(path, val, first);
}

function connect_rev_host_cb(path, val, first)
{
   connect_rev_usage();
   w3_clearInnerHTML('id-connect-rev-status');
   w3_string_set_cfg_cb(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)     // if currently selected option update the value
      connect_dom_rev_focus();
   else
      connect_update_url();
}

function connect_rev_status_cb(status)
{
   if (!connect.focus) return;
	status = +status;
	console.log('rev_status='+ status);
	var s;
	
	if (status >= 0 && status <= 99 && cfg.sdr_hu_dom_sel == connect_dom_sel.REV) {
      connect_dom_rev_focus();
   }
	
	switch (status) {
		case   0: s = 'Existing account, registration successful'; break;
		case   1: s = 'New account, registration successful'; break;
		case   2: s = 'Updating host name, registration successful'; break;
		case 100: s = 'User key or host name field blank'; break;
		case 101: s = 'User key invalid. Did you email your user/API key to support@kiwisdr.com as per the instructions?'; break;
		case 102: s = 'Host name already in use; please choose another and retry'; break;
		case 103: s = 'Invalid characters in user key or host name field (use a-z, 0-9, -, _)'; break;
		case 200: s = 'Reverse proxy enabled and running'; break;
		case 201: s = 'Reverse proxy enabled and pending'; break;
		case 900: s = 'Problem contacting proxy server; please check Internet connection'; break;
		case 901: s = 'Proxy server returned invalid status data?'; break;
		default:  s = 'Reverse proxy internal error: '+ status; break;
	}
	
	w3_innerHTML('id-connect-rev-status', s);
	
	// if pending keep checking
	if (status == 201) {
	   connect.timeout = setTimeout(function() { ext_send('SET rev_status_query'); }, 5000);
	}
}


////////////////////////////////
// all in admin_sdr.js
// config
// webpage
// public
// dx
////////////////////////////////


////////////////////////////////
// update
//		auto reload page when build finished?
////////////////////////////////

function update_html()
{
	var s =
	w3_div('id-update w3-hide',
		'<hr>' +
		w3_div('id-msg-update w3-container') +

		'<hr>' +
		w3_div('w3-margin-bottom',
         w3_half('w3-container', 'w3-text-teal',
				w3_div('',
                  '<b>Automatically check for software updates?</b> ' +
                  w3_switch('', 'Yes', 'No', 'adm.update_check', adm.update_check, 'admin_radio_YN_cb')
            ),
				w3_div('',
                  '<b>Automatically install software updates?</b> ' +
                  w3_switch('', 'Yes', 'No', 'adm.update_install', adm.update_install, 'admin_radio_YN_cb')
            )
         ),
         w3_half('w3-container w3-tspace-16', 'w3-text-teal',
            w3_div('',
               w3_select('/w3-label-inline/w3-width-auto', 'After a restart', '', 'adm.restart_update', adm.restart_update, restart_update_u, 'admin_select_cb')
            ),
            w3_div('',
               w3_select('/w3-label-inline/w3-width-auto', 'After an update', '', 'adm.update_restart', adm.update_restart, update_restart_u, 'admin_select_cb')
            )
         )
		) +

		w3_half('w3-container', 'w3-text-teal',
			w3_div('w3-valign',
				'<b>Check for software update </b> ' +
				w3_button('w3-aqua w3-margin', 'Check now', 'update_check_now_cb')
			),
			w3_div('w3-valign',
				'<b>Force software build </b> ' +
				w3_button('w3-aqua w3-margin', 'Build now', 'update_build_now_cb')
			)
		) +

		'<hr>' +
		w3_inline('w3-halign-space-between w3-margin-bottom w3-text-teal w3-restart/w3-container',
         w3_divs('w3-tspace-8',
               w3_div('', '<b>Disable recent changes?</b>'),
               w3_switch('', 'Yes', 'No', 'disable_recent_changes', cfg.disable_recent_changes, 'admin_radio_YN_cb'),
               w3_text('w3-text-black', 'Currently:<br><ul><li>The Firefox audio hang workaround.</li></ul>')
         ),
         ''
      ) +

		'<hr>' +
		w3_div('w3-container', 'TODO: alt github name') +
		'<hr>'
	);
	return s;
}

var restart_update_u = { 0: 'install updates', 1: 'delay updates until overnight' };
var update_restart_u = { 0: 'restart server', 1: 'reboot Beagle' };

function update_check_now_cb(id, idx)
{
	ext_send('SET force_check=1 force_build=0');
	w3_el('msg-update').innerHTML = w3_icon('', 'fa-refresh fa-spin', 20);
}

function update_build_now_cb(id, idx)
{
	ext_send('SET force_check=1 force_build=1');
	if (adm.update_restart == 0)
	   w3_show_block('id-build-restart');
	else
	   w3_show_block('id-build-reboot');
}


////////////////////////////////
// backup
////////////////////////////////

function backup_html()
{
	var s =
	w3_div('id-backup w3-hide',
		'<hr>',
		w3_div('w3-section w3-text-teal w3-bold', 'Backup complete contents of KiwiSDR by writing Beagle filesystem onto a user provided micro-SD card'),
		w3_div('w3-container w3-text w3-red', 'WARNING: after SD card is written immediately remove from Beagle.<br>Otherwise on next reboot Beagle will be re-flashed from SD card.'),
		'<hr>',
		w3_third('w3-container', 'w3-valign',
			w3_button('w3-aqua w3-margin', 'Click to write micro-SD card', 'backup_sd_write'),

			w3_div('',
				w3_div('id-progress-container w3-progress-container w3-round-large w3-gray w3-show-inline-block',
					w3_div('id-progress w3-progressbar w3-round-large w3-light-green w3-width-zero',
						w3_div('id-progress-text w3-container')
					)
				),
				
            w3_div('w3-margin-T-8',
               w3_div('id-progress-time w3-show-inline-block') +
               w3_div('id-progress-icon w3-show-inline-block w3-margin-left')
            )
			),

			w3_div('id-sd-status class-sd-status')
		),
		'<hr>',
		w3_div('id-output-msg w3-container w3-text-output w3-scroll-down w3-small w3-margin-B-16')
	);
	return s;
}

function backup_focus()
{
	w3_el('id-progress-container').style.width = px(300);
	w3_el('id-output-msg').style.height = px(300);
}

var sd_progress, sd_progress_max = 6*60;		// measured estimate -- in secs (varies with SD card write speed)
var backup_sd_interval;
var backup_refresh_icon = w3_icon('', 'fa-refresh fa-spin', 20);

function backup_sd_write(id, idx)
{
	var el = w3_el('id-sd-status');
	el.innerHTML = "writing the micro-SD card...";

	w3_el('id-progress-text').innerHTML = w3_el('id-progress').style.width = '0%';

	sd_progress = -1;
	backup_sd_progress();
	backup_sd_interval = setInterval(backup_sd_progress, 1000);

	w3_el('id-progress-icon').innerHTML = backup_refresh_icon;

	ext_send("SET microSD_write");
}

function backup_sd_progress()
{
	sd_progress++;
	var pct = ((sd_progress / sd_progress_max) * 100).toFixed(0);
	if (pct <= 95) {	// stall updates until we actually finish in case SD is writing slowly
		w3_el('id-progress-text').innerHTML = w3_el('id-progress').style.width = pct +'%';
	}
	var secs = (sd_progress % 60).toFixed(0).leadingZeros(2);
	var mins = Math.floor(sd_progress / 60).toFixed(0);
	w3_el('id-progress-time').innerHTML = mins +':'+ secs;
}

function backup_sd_write_done(err)
{
	var el = w3_el('id-sd-status');
	var msg = err? ('FAILED error '+ err.toString()) : 'WORKED';
	if (err == 1) msg += '<br>No SD card inserted?';
	if (err == 15) msg += '<br>rsync I/O error';
	el.innerHTML = msg;
	el.style.color = err? 'red':'lime';

	if (!err) {
		// force to max in case we never made it during updates
		w3_el('id-progress-text').innerHTML = w3_el('id-progress').style.width = '100%';
	}
	kiwi_clearInterval(backup_sd_interval);
	w3_el('id-progress-icon').innerHTML = '';
}


////////////////////////////////
// network
////////////////////////////////

var network = {
   auto_nat_color:   null,
   show_updating: true,
   
   ip_blacklist_file_base: 'kiwisdr.com/ip_blacklist/ip_blacklist3.cjson',
   ip_blacklist_check_mtime: true,
   
   ethernet_speed_s: [ '100 Mbps', '10 Mbps' ],
   ethernet_mtu_s: [ '1500 (default)', '1440', '1400' ]
};

function network_html()
{
   network.ip_blacklist_file = 'http://'+ network.ip_blacklist_file_base;
   network.ip_blacklist_file_SSL = kiwi_SSL() + network.ip_blacklist_file_base;
   network.ip_blacklist_file_SSL_mtime = kiwi_SSL() + network.ip_blacklist_file_base +'.mtime';

   var commit_use_static = ext_get_cfg_param('adm.ip_address.commit_use_static');
   console.log('commit_use_static='+ commit_use_static);
   
   // on reload use last committed value in case commit transaction never completed
   if (commit_use_static == undefined) commit_use_static = false;    // default to DHCP if there has never been a commit
   ext_set_cfg_param('adm.ip_address.use_static', commit_use_static, EXT_SAVE)
   w3_switch_set_value('adm.ip_address.use_static', commit_use_static? w3_SWITCH_NO_IDX : w3_SWITCH_YES_IDX);
   
   // check once per admin page load
   if (network.ip_blacklist_check_mtime) {
      network.ip_blacklist_double_fault = false;
      //kiwi_ajax(network.ip_blacklist_file_SSL_mtime, 'network_blacklist_mtime_cb', 0, -2000);
      kiwi_ajax(network.ip_blacklist_file_SSL_mtime, 'network_blacklist_mtime_cb', 0, 10000);
      network.ip_blacklist_check_mtime = false;
   }
   
	var s1 =
		w3_div('id-net-auto-nat-msg w3-valign w3-hide') +

		w3_div('id-net-need-update w3-valign w3-margin-T-8 w3-hide',
			w3_button('w3-yellow', 'Are you sure? Click to update interface DHCP/static IP configuration', 'network_dhcp_static_update_cb')
		) +

		'<hr>' +
		w3_div('id-net-reboot w3-container',
			w3_inline('w3-halign-space-between w3-margin-bottom w3-text-teal/',
			   w3_divs('w3-valign w3-flex-col w3-restart/w3-tspace-6',
					w3_input_get('', 'Internal port', 'adm.port', 'admin_int_cb'),
					w3_input_get('', 'External port', 'adm.port_ext', 'admin_int_cb')
				),
				w3_divs('id-net-ssl-vis w3-hide/ w3-center w3-restart',
					'<b>Enable HTTPS/SSL on<br>network connections?</b><br>',
					w3_switch_get_param('id-net-ssl w3-margin-T-8', 'Yes', 'No', 'adm.use_ssl', true, false, 'network_use_ssl_cb')
				),
				w3_divs('w3-center',
					'<b>Auto add NAT rule<br>on firewall / router?</b><br>',
					w3_switch('w3-margin-T-8', 'Yes', 'No', 'adm.auto_add_nat', adm.auto_add_nat, 'network_auto_nat_cb')
				),
				w3_div('w3-center',
						'<b>IP address<br>(only static IPv4 for now)</b><br> ' +
						w3_switch_get_param('w3-margin-T-8', 'DHCP', 'Static', 'adm.ip_address.use_static', 0, false, 'network_use_static_cb')
				),
            w3_divs('w3-center/',
               w3_select('w3-width-auto', 'Ethernet interface speed', '', 'ethernet_speed', cfg.ethernet_speed, network.ethernet_speed_s, 'network_ethernet_speed'),
               w3_div('w3-text-black',
                  'Select 10 Mbps to reduce <br> Ethernet spurs. Try changing <br> while looking at waterfall.')
            ),
            w3_divs('w3-center/',
               w3_select('w3-width-auto', 'Ethernet interface MTU', '', 'ethernet_mtu', cfg.ethernet_mtu, network.ethernet_mtu_s, 'network_ethernet_mtu'),
               w3_div('w3-text-black',
                  'Select 1440 when having <br> connection problems <br> using 4G networks.')
            )
			),
			
			w3_div('id-net-ssl-container w3-restart w3-hide',
            w3_inline('w3-halign-space-between w3-margin-bottom w3-text-teal/',
               w3_input_get('', 'Local port (HTTP)', 'adm.port_http_local', 'admin_int_cb')
            )
         ),
			
			w3_div('id-net-static w3-hide',
			   w3_div('',
               w3_third('w3-margin-B-8 w3-text-teal', 'w3-container',
                  w3_input_get('', 'IP address (n.n.n.n where n = 0..255)', 'adm.ip_address.ip', 'network_ip_address_cb', ''),
                  w3_input_get('', 'Netmask (n.n.n.n where n = 0..255)', 'adm.ip_address.netmask', 'network_netmask_cb', ''),
                  w3_input_get('', 'Gateway (n.n.n.n where n = 0..255)', 'adm.ip_address.gateway', 'network_gw_address_cb', '')
               ),
               w3_third('w3-margin-B-8 w3-text-teal', 'w3-container',
                  w3_div('id-network-check-ip w3-green'),
                  w3_div('id-network-check-nm w3-green'),
                  w3_div('id-network-check-gw w3-green')
               ),
               w3_third('w3-valign w3-margin-bottom w3-text-teal', 'w3-container',
                  w3_input_get('', 'DNS-1 (n.n.n.n where n = 0..255)', 'adm.ip_address.dns1', 'w3_string_set_cfg_cb', ''),
                  w3_input_get('', 'DNS-2 (n.n.n.n where n = 0..255)', 'adm.ip_address.dns2', 'w3_string_set_cfg_cb', ''),
                  w3_div('',
                     w3_label('', '<br>') +     // makes the w3-valign above work for button below
                     w3_button('w3-show-inline w3-aqua', 'Use well-known public DNS servers', 'net_public_dns_cb')
                  )
               )
            ),
            w3_text('w3-margin-left w3-text-black', 'If DNS fields are blank the DNS servers specified by your router\'s DHCP will be used.')
			)
		);
	
	var s2 =
		'<hr>' +
		w3_div('id-net-config w3-container') +

		'<hr>' +
		w3_half('w3-container', '',
         w3_div('',
            w3_div('', 
               w3_label('w3-show-inline w3-bold w3-text-teal', 'Check if your external router port is open:') +
               w3_button('w3-show-inline w3-aqua|margin-left:10px', 'Check port open', 'net_port_open_cb')
            ),
            'Does kiwisdr.com successfully connect to your Kiwi using these URLs?<br>' +
            'If both respond "NO" then check the NAT port mapping on your router.<br>' +
            'If first responds "NO" and second "YES" then domain name of the first<br>' +
            'isn\'t resolving to the ip address of the second. Check DNS.',
            w3_div('', 
               w3_label('id-net-check-port-dom-q w3-show-inline-block w3-margin-LR-16 w3-text-teal') +
               w3_div('id-net-check-port-dom-s w3-show-inline-block w3-text-black w3-background-pale-aqua')
            ),
            w3_div('', 
               w3_label('id-net-check-port-ip-q w3-show-inline-block w3-margin-LR-16 w3-text-teal') +
               w3_div('id-net-check-port-ip-s w3-show-inline-block w3-text-black w3-background-pale-aqua')
            )
         ),
         w3_div('w3-center',
            w3_label('w3-bold w3-text-teal', 'Register this Kiwi on my.kiwisdr.com<br>on each reboot?<br>'),
            w3_switch('w3-margin-T-8 w3-margin-B-8', 'Yes', 'No', 'adm.my_kiwi', adm.my_kiwi, 'admin_radio_YN_cb'),
            w3_text('w3-block w3-center w3-text-black',
               'Registering on <a href="http://my.kiwisdr.com" target="_blank">my.kiwisdr.com</a> allows the local ip address of Kiwis <br>' +
               'to be easily discovered. Set to "no" if you don\'t want your Kiwi <br>' +
               'sending information to kiwisdr.com. Defaults to "yes".'
            )
         )
      );

   var s3 =
		'<hr>' +
      w3_div('w3-container w3-text-teal',
         w3_inline('w3-valign w3-halign-space-between/',
            w3_div('',
               w3_text('w3-bold w3-text-teal', 'IP address blacklist'),
               w3_text('w3-text-black w3-show-block',
                  'IP addresses/ranges listed here are blocked <br>' +
                  'from accessing your Kiwi (via Linux iptables). <br>' +
                  'Use CIDR notation for ranges, e.g. CIDR "ip/24" <br>' +
                  'is equivalent to netmask "255.255.255.0" <br>' +
                  'More information at the ' +
                  w3_link('w3-link-darker-color', 'http://forum.kiwisdr.com/index.php?p=/discussion/2352/call-for-ip-address-blacklist-contributions/p1', 'Kiwi forum') +'.'
               )
            ),
            
            w3_divs('/w3-center w3-tspace-8',
                  w3_div('',
                     w3_div('w3-margin-T-16', '<b>Download IP address blacklist?</b>'),
                     w3_inline('w3-valign w3-halign-space-evenly w3-margin-T-10/',
                        w3_button('id-ip-blacklist-download w3-aqua', 'Download', 'network_download_button_cb'),
                        w3_text('id-ip-blacklist-new w3-text-red w3-hide', 'New blacklist available'),
                        w3_button('w3-aqua', 'Clear', 'network_download_clear_cb')
                     )
                  )
               ,
               w3_text('w3-text-black w3-center',
                  'Downloads a standard blacklist definition from ' +
                  w3_link('w3-link-darker-color', network.ip_blacklist_file, 'kiwisdr.com') +
                  '<br>A local, writeable blacklist can be entered below.'
               )
            ),
            
            w3_div('w3-center w3-margin-B-24',
               '<b>Prevent multiple connections from<br>the same IP address?</b><br>',
               w3_switch('w3-margin-T-8 w3-margin-B-8', 'Yes', 'No', 'adm.no_dup_ip', adm.no_dup_ip, 'admin_radio_YN_cb')
            )
         ),
         
         
         w3_inline('w3-valign w3-margin-top/', 
            w3_text('w3-text-teal', 'Blacklist status:'),
            w3_div('id-ip-blacklist-status w3-margin-left w3-text-black w3-background-pale-aqua', '')
         ),
         
         w3_textarea_get_param('w3-margin-T-16//w3-light-grey|width:100%|readonly',
            'Downloaded blacklist (read-only)',
            'adm.ip_blacklist', 8, 100, '', ''
         ),
         
         w3_textarea_get_param('w3-margin-T-32//w3-input-any-change|width:100%',
            w3_div('w3-flex',
               w3_text('w3-bold  w3-text-teal', 'Local blacklist (writeable)'),
               w3_text('w3-text-black w3-margin-left',
                  'Press enter(return) key while positioned at end of text to submit data.<br>' +
                  'Always add whitelist entries ("+" character before ip address) after corresponding ip range, ' +
                  'e.g. 1.2.3.0/24 +1.2.3.22'
               )
            ),
            'adm.ip_blacklist_local', 8, 100, 'network_user_blacklist_cb', ''
         )
      ) +

      '<hr>' +
      w3_half('w3-margin-bottom w3-text-teal', 'w3-container',
         w3_div('w3-restart',
            w3_input_get('id-proxy-server', 'Proxy server hostname', 'adm.proxy_server', 'network_proxy_server_cb'),
            w3_div('w3-text-black',
               'Change <b>only</b> if you have implemented a private proxy server. <br>' +
               'Set to "proxy.kiwisdr.com" for the default proxy service.'
            )
         ),
         w3_div()
      ) +
      '<hr>';

	return w3_div('id-network w3-hide', s1 + s2 + s3);
}

function network_ssl_container_init()
{
   var show = dbgUs && (debian_ver >= 10);
   w3_hide2('id-net-ssl-vis', !show);

   var use_ssl = show && adm.use_ssl;
   var s = '';
   if (use_ssl) s = ' (HTTPS)';
   w3_innerHTML('id-adm.port-label', 'Internal port'+ s);
   w3_innerHTML('id-adm.port_ext-label', 'External port'+ s);
   w3_hide2('id-net-ssl-container', !use_ssl);
}

function network_use_ssl_cb(path, idx, first)
{
   if (first) return;
	var use_ssl = (+idx == 0);
	console.log('network_use_ssl_cb use_ssl='+ use_ssl);
   admin_bool_cb(path, use_ssl);
   network_ssl_container_init();
}

function network_download_button_cb(id, idx, first)
{
   if (first) return;
   w3_innerHTML('id-ip-blacklist-status', 'updating..'+ w3_icon('w3-margin-left', 'fa-refresh fa-spin', 20));
   //kiwi_ajax(network.ip_blacklist_file_SSL, 'network_download_blacklist_cb', 0, -2000);
   kiwi_ajax(network.ip_blacklist_file_SSL, 'network_download_blacklist_cb', 0, 10000);
}

function network_download_clear_cb(id, idx, first)
{
   network_ip_blacklist_cb('adm.ip_blacklist', '');
   w3_int_set_cfg_cb('adm.ip_blacklist_mtime', 0);
}

function network_user_blacklist_cb(path, val)
{
   //console.log('network_user_blacklist_cb val='+ val);
   network_ip_blacklist_cb('adm.ip_blacklist_local', val);
}

function network_download_blacklist_cb(bl)
{
   var fault = false;
   
   if (!bl) {
      console.log('network_download_blacklist_cb: bl='+ bl);
      fault = true;
   } else
   
   if (bl.AJAX_error) {
      console.log('network_download_blacklist_cb: '+ bl.AJAX_error);
      console.log(bl);
      fault = true;
   } else
   if (!isArray(bl)) {
      console.log('network_download_blacklist_cb: not array');
      console.log(bl);
      fault = true;
   }
   
   if (fault) {
      if (network.ip_blacklist_double_fault) {
         console.log('network_download_blacklist_cb: default blacklist fetch FAILED');
         console.log(bl);
         w3_innerHTML('id-ip-blacklist-status', 'download failed: could not contact kiwisdr.com or find default file');
         return;
      } else {
         w3_innerHTML('id-ip-blacklist-status', 'download failed, using default..'+ w3_icon('w3-margin-left', 'fa-refresh fa-spin', 20));
         network.show_updating = false;
      }
      
      // load the default blacklist if unable to contact kiwisdr.com
      var url = kiwi_url_origin() +'/kiwi/ip_blacklist.default.cjson';
      console.log('network_download_blacklist_cb: using default station list '+ url);
      network.ip_blacklist_double_fault = true;
      kiwi_ajax(url, 'network_download_blacklist_cb', 0, /* timeout */ 10000);
      return;
   }
   
   network.ip_blacklist_double_fault = false;
   //console.log('network_download_blacklist_cb:');
   //console.log(bl);
   
   var bl_debug = false;
   var n = [];
   bl.forEach(function(s, i) {
      if (!isString(s)) return;
      var a = s.split('/');

      var nmd;
      if (a.length == 1)
         nmd = 32;
      else
         nmd = +a[1];
      var nm = (~((1<<(32-nmd))-1)) & 0xffffffff;
      //console.log(a[0]);
      //console.log('nm '+ nmd +' '+ nm.toHex(8));
      
      // NB: if a[0] begins with '+' due to being a whitelist entry, kiwi_inet4_d2h() still works
      // because the first component of the ip address string is just treated as a positive number
      var ip1 = kiwi_inet4_d2h(a[0]);
      var ip = ip1 & nm;
      if (ip1 != ip)
         console.log('ip/netmask mismatch: '+ kiwi_ip_str(ip1) +'|'+ ip1.toHex(8) +' '+
            kiwi_ip_str(ip) +'|'+ ip.toHex(8) +' '+ nm.toHex(8) +'/'+ nmd);

      n.push( { ip: ip, nm: nm, nmd: nmd, whitelist: (a[0][0] == '+')? 1:0, del: 0 } );
   });
   
   // sort largest netmask first
   n.sort(function(a, b) {
      return a.nm - b.nm;
   });
   
   if (bl_debug) n.forEach(function(o, i) {
      console.log('sorted: #'+ i +' '+ (o.whitelist? '+':'') + kiwi_ip_str(o.ip) +'|'+ o.ip.toHex(8) +' '+ o.nm.toHex(8) +'/'+ o.nmd);
   });
   
   // remove duplicates
   var nn = kiwi_dup_array(n);
   if (bl_debug) console.log('blacklist: #'+ nn.length +' entries pre duplicate check');
   n.forEach(function(oi, i) {
      var stop = false;
      nn.forEach(function(oj, j) {
         if (stop || j <= i || oj.ip == 0) return;
         if ((oj.ip & oi.nm) == oi.ip && (!oj.whitelist && !oi.whitelist)) {
            console.log('blacklist: #'+ j +' '+ kiwi_ip_str(oj.ip) +'|'+ oj.ip.toHex(8) +' '+ oj.nm.toHex(8) +'/'+ oj.nmd +
               ' is a subset of #'+
               i +' '+ kiwi_ip_str(oi.ip) +'|'+ oi.ip.toHex(8) +' '+ oi.nm.toHex(8) +'/'+ oi.nmd);
            oj.del = 1;
            stop = true;
         }
      });
   });
   
   var nnn = [];
   nn.forEach(function(o, i) { if (!o.del) nnn.push(o); });
   
   console.log('blacklist: #'+ nnn.length +' entries post duplicate check');
   if (bl_debug) nnn.forEach(function(o, i) {
      console.log('final: #'+ i +' '+ (o.whitelist? '+':'') +
         kiwi_ip_str(o.ip) +'|'+ o.ip.toHex(8) +' '+ o.nm.toHex(8) +'/'+ o.nmd + (o.del? ' DELETE':''));
   });

   var ip_bl_s = '';
   nnn.forEach(function(o, i) {
      ip_bl_s = ip_bl_s +' '+ (o.whitelist? '+':'') + kiwi_ip_str(o.ip) +'/'+ o.nmd;
   });
   
   network_ip_blacklist_cb('adm.ip_blacklist', ip_bl_s);
   network.show_updating = true;

   // silently fail if kiwisdr.com can't be contacted for the mtime check
   //kiwi_ajax(network.ip_blacklist_file_SSL_mtime, 'network_blacklist_mtime_cb', 1, -2000);
   kiwi_ajax(network.ip_blacklist_file_SSL_mtime, 'network_blacklist_mtime_cb', 1, 10000);
}

function network_blacklist_mtime_cb(mt, update)
{
   var fault = false;
   
   if (!isArg(mt)) {
      console.log('network_blacklist_mtime_cb: mt='+ mt);
      fault = true;
   } else
   
   if (isObject(mt) && mt.AJAX_error && mt.AJAX_error == 'timeout') {
      console.log('network_blacklist_mtime_cb: TIMEOUT');
      fault = true;
   }

   if (fault) {
      console.log(mt);
      return;
   }
   //console.log(mt);
   
   //if (dbgUs) console.log(mt);
   var mtime = parseInt(mt);
   if (dbgUs) console.log('network_blacklist_mtime_cb: '+ (update? 'UPDATE' : 'AVAIL') +
      ' mtime='+ mtime +' adm.ip_blacklist_mtime='+ adm.ip_blacklist_mtime);
   
   if (update) {
      // new blacklist downloaded -- update file mtime into our configuration
      w3_remove_then_add('id-ip-blacklist-download', 'w3-red', 'w3-aqua');
      w3_hide2('id-ip-blacklist-new', true);
      w3_int_set_cfg_cb('adm.ip_blacklist_mtime', mtime);
   } else {
      // update download button ui if new blacklist available
      if (adm.ip_blacklist_mtime != mtime) {
         w3_remove_then_add('id-ip-blacklist-download', 'w3-aqua', 'w3-red');
         w3_hide2('id-ip-blacklist-new', false);
      }
   }
}

function network_proxy_server_cb(path, val)
{
   val = val.trim();
   if (val == '') {
      val = 'proxy.kiwisdr.com';
      w3_set_value('id-proxy-server', val);
   }
	w3_string_set_cfg_cb(path, val);
}

function network_ip_blacklist_cb(path, val)
{
   //console.log('network_ip_blacklist_cb path='+ path +' val='+ val);
   
	var re = /([^,;\s]+)/gm;
	var ar = val.match(re);
	//console.log(ar);
	if (ar == null) ar = [];

   var s = '';
   ar.forEach(function(v) {
      s += v +' ';
   });
   w3_set_value(path, s);
   w3_string_set_cfg_cb(path, s);

   if (path.endsWith('ip_blacklist_local')) {
	   network.ip_blacklist_local = ar;
	   
	   // make sure network.ip_blacklist is valid
	   ar = decodeURIComponent(adm.ip_blacklist).match(re);
	   if (ar == null) ar = [];
	   network.ip_blacklist = ar;
	} else {
	   network.ip_blacklist = ar;

	   // make sure network.ip_blacklist_local is valid
	   ar = decodeURIComponent(adm.ip_blacklist_local).match(re);
	   if (ar == null) ar = [];
	   network.ip_blacklist_local = ar;
	}

   ext_send('SET network_ip_blacklist_clear');
   if (network.show_updating) {
      w3_innerHTML('id-ip-blacklist-status', 'updating..'+ w3_icon('w3-margin-left', 'fa-refresh fa-spin', 20));
   }
   
   network.seq = 0;
	network.ip_address_error = false;
	console.log('ip_blacklist:');
	console.log(network.ip_blacklist);
	console.log('ip_blacklist_local:');
	console.log(network.ip_blacklist_local);
	network_ip_blacklist_send( {idx:0, type:0} );
}

function network_ip_blacklist_send(p)
{
   if (p.type == 0) {
      if (p.idx == network.ip_blacklist.length) {
         network_ip_blacklist_send( {idx:0, type:1} );
      } else {
         ext_send('SET network_ip_blacklist='+ encodeURIComponent(network.ip_blacklist[p.idx]));
         setTimeout(function() { network_ip_blacklist_send( {idx:p.idx+1, type:0} ); }, 250);   // rate limit
      }
   } else {
      if (p.idx == network.ip_blacklist_local.length) {
         ext_send('SET network_ip_blacklist_enable');
      } else {
         ext_send('SET network_ip_blacklist='+ encodeURIComponent(network.ip_blacklist_local[p.idx]));
         setTimeout(function() { network_ip_blacklist_send( {idx:p.idx+1, type:1} ); }, 250);   // rate limit
      }
   }
}

function network_ip_blacklist_status(status, ip)
{
	console.log('network_ip_blacklist_status #'+ network.seq +' status='+ status +' ip='+ ip);
	network.seq++;
	if (status == 0) return;
	network.ip_address_error = true;
   w3_innerHTML('id-ip-blacklist-status', 'ip address error: '+ dq(ip));
}

function network_ethernet_speed(path, idx, first)
{
   idx = +idx;
	//console.log('network_ethernet_speed path='+ path +' idx='+ idx +' first='+ first);
   if (first) return;
   admin_select_cb(path, idx, first)
}

function network_ethernet_mtu(path, idx, first)
{
   idx = +idx;
	//console.log('network_ethernet_mtu path='+ path +' idx='+ idx +' first='+ first);
   if (first) return;
   admin_select_cb(path, idx, first)
}

function network_port_open_init()
{
   // proxy always uses port 8073
   w3_do_when_rendered('id-net-check-port-dom-q', function() {
      var el = w3_el('id-net-check-port-dom-q');
      var port = (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)? 8073 : adm.port_ext;
      el.innerHTML =
         (cfg.server_url != '')?
            'http://'+ cfg.server_url +':'+ port +' :' :
            '(incomplete information -- on "connect" tab please use a valid setting in menu) :';


      w3_el('id-net-check-port-dom-s').innerHTML = '';
      w3_el('id-net-check-port-ip-s').innerHTML = '';
   });
   
   w3_do_when_cond(function() { return isArg(config_net.pvt_ip); }, function() {
      w3_el('id-net-check-port-ip-q').innerHTML =
         'http://'+ config_net.pvt_ip +':'+ adm.port_ext +' :';
   });
}

function network_focus()
{
   network_static_init();
	network_port_open_init();
	network_ssl_container_init();
	network.status_interval = kiwi_setInterval(network_auto_nat_status_poll, 1000);
}

function network_blur()
{
	kiwi_clearInterval(network.status_interval);
}

function network_auto_nat_status_poll()
{
	ext_send('SET auto_nat_status_poll');
}

function network_auto_nat_cb(path, idx, first)
{
   if (first) return;
   idx = +idx;
	var auto_nat = (idx == 0)? 1:0;
	//console.log('network_auto_nat_cb: path='+ path +' auto_nat='+ auto_nat);

   admin_radio_YN_cb(path, idx);
   ext_send('SET auto_nat_set');    // server inspects adm.auto_add_nat to add or delete NAT
}

function network_check_port_status_cb(status)
{
   console.log('network_check_port_status_cb status='+ status.toHex());
   if (status < 0) {
      w3_el('id-net-check-port-dom-s').innerHTML = 'Error checking port status';
      w3_el('id-net-check-port-ip-s').innerHTML = 'Error checking port status';
   } else {
      var dom_status = status & 0xf0;
      var ip_status = status & 0x0f;
      w3_el('id-net-check-port-dom-s').innerHTML = dom_status? 'NO' : 'YES';
      w3_el('id-net-check-port-ip-s').innerHTML = ip_status? 'NO' : 'YES';
   }
}

function net_port_open_cb()
{
   w3_el('id-net-check-port-dom-s').innerHTML = w3_icon('', 'fa-refresh fa-spin', 20);
   w3_el('id-net-check-port-ip-s').innerHTML = w3_icon('', 'fa-refresh fa-spin', 20);
	ext_send('SET check_port_open');
}

function network_dhcp_static_update_cb(path, idx)
{
   var use_static = adm.ip_address.use_static;
	if (use_static) {
      ext_send('SET dns dns1=x'+ encodeURIComponent(adm.ip_address.dns1) +' dns2=x'+ encodeURIComponent(adm.ip_address.dns2));
      ext_send('SET static_ip='+ kiwi_ip_str(network_ip) +' static_nm='+ kiwi_ip_str(network_nm) +' static_gw='+ kiwi_ip_str(network_gw));
	} else {
		ext_send('SET use_DHCP');
	}

   ext_set_cfg_param('adm.ip_address.commit_use_static', use_static, EXT_SAVE)
   w3_hide('id-net-need-update');
   
   if (debian_ver <= 9)    // Debian 10 and above use connmanctl which has immediate effect (no reboot required)
      w3_reboot_cb();      // show reboot button after confirm button pressed
   else
		admin_wait_then_reload(10, 'Waiting for configuration change');
}

function network_static_init()
{
   w3_do_when_rendered('id-net-static', function() {
		var use_static = ext_get_cfg_param('adm.ip_address.use_static', false);
		network_use_static_cb('adm.ip_address.use_static', use_static, /* first */ true);
	});
}

function network_use_static_cb(path, idx, first)
{
	idx = +idx;
	//console.log('network_use_static_cb idx='+ idx);
	var dhcp = (idx == 0);
	
	// only show IP fields if in static mode
	w3_hide2('id-net-static', dhcp);

	//console.log('network_use_static_cb: first='+ first +' dhcp='+ dhcp);

	// when mode is changed decide if update button needs to appear
	if (!first) {
		if (dhcp) {
			w3_show_block('id-net-need-update');
		} else {
			network_show_update(false);	// show based on prior static info (if any)
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
		var el = w3_el(id);
		var check = network_ip_nm_check(val_str, ip), check2 = true;
		
		if (check == true && check_func != undefined) {
			check2 = check_func(val_str, ip);
		}
	
		if (check == false || check2 == false) {
			el.innerHTML = 'bad '+ name +' entered';
			w3_remove(el, 'w3-green');
			w3_add(el, 'w3-red');
		} else {
			el.innerHTML = name +' okay, check: '+ ip.a +'.'+ ip.b +'.'+ ip.c +'.'+ ip.d;
			w3_remove(el, 'w3-red');
			w3_add(el, 'w3-green');
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
		w3_el('network-check-nm').innerHTML += ' (/'+ network_nm.nm +')';
}

function network_gw_address_cb(path, val, first)
{
	network_show_check('network-check-gw', 'gateway', path, val, network_gw, first);
}

function net_public_dns_cb(id, idx)
{
	w3_string_set_cfg_cb('adm.ip_address.dns1', '1.1.1.1');
	w3_set_value('adm.ip_address.dns1', '1.1.1.1');
	w3_string_set_cfg_cb('adm.ip_address.dns2', '8.8.8.8');
	w3_set_value('adm.ip_address.dns2', '8.8.8.8');
}


////////////////////////////////
// GPS
//		tracking tasks aren't stopped when !enabled
////////////////////////////////

var pin = {
    green: w3_div('cl-leaflet-marker cl-legend-marker|background-color:lime'),
      red: w3_div('cl-leaflet-marker cl-legend-marker|background-color:red'),
   yellow: w3_div('cl-leaflet-marker cl-legend-marker|background-color:yellow')
};

var _gps = {
   leaflet: true,
   gps_map_loaded: false,
   pkgs_maps_js: [ 'pkgs_maps/pkgs_maps.js', 'pkgs_maps/pkgs_maps.css' ],
   gmap_js: ['http://maps.googleapis.com/maps/api/js?key='],

   RSSI:0, AZEL:1, POS:2, MAP:3, IQ:4,
   IQ_data: null,
   iq_ch: 0,
   map_init: 0,
   map_needs_height: 0,
   map_locate: 0,
   map_mkr: [],
   legend_sep: w3_inline('', pin.green, 'Navstar/QZSS only', pin.yellow, 'Galileo only', pin.red, 'all sats'),
   legend_all: w3_inline('', pin.green, 'all sats (Navstar/QZSS/Galileo)')
};

//var E1B_offset_i = [ '-1', '-3/4', '-1/2', '-1/4', '0', '+1/4', '+1/2', '+3/4', '+1' ];

function gps_html()
{
	var s =
	w3_div('id-gps w3-hide|line-height:1.5',
	   w3_inline('w3-valign w3-halign-space-between w3-margin-T-16/',
         w3_div('w3-valign w3-text-teal',
            //w3_div('w3-show-inline w3-margin-right w3-small', '<b>Enable<br>GPS?</b>') +
            //w3_switch('w3-show-inline w3-padding-smaller', 'Yes', 'No', 'adm.enable_gps', adm.enable_gps, 'admin_radio_YN_cb')
            
            w3_text('w3-text-teal w3-bold w3-small', 'Acquire'),
            w3_div('w3-flex-col w3-valign-start w3-margin-L-4',
               w3_checkbox('w3-label-inline w3-label-not-bold w3-small/w3-small', 'Navstar', 'adm.acq_Navstar', adm.acq_Navstar, 'gps_acq_cb'),
               w3_inline('',
                  w3_checkbox('w3-label-inline w3-label-not-bold w3-small/w3-small', 'QZSS', 'adm.acq_QZSS', adm.acq_QZSS, 'gps_acq_cb'),
                  w3_checkbox('w3-label-inline w3-label-not-bold w3-small w3-margin-left/w3-small/', 'Priority', 'adm.QZSS_prio', adm.QZSS_prio, 'gps_acq_cb')
               ),
               w3_checkbox('w3-label-inline w3-label-not-bold w3-small/w3-small', 'Galileo', 'adm.acq_Galileo', adm.acq_Galileo, 'gps_acq_cb')
            )
         ),

         w3_div('w3-valign w3-text-teal',
            //w3_div('w3-show-inline w3-margin-right w3-small', '<b>Always<br>acquire?</b>') +
            //w3_switch('w3-show-inline w3-padding-smaller', 'Yes', 'No', 'adm.always_acq_gps', adm.always_acq_gps, 'admin_radio_YN_cb')
            w3_checkbox('w3-label-inline w3-small/w3-small', 'Acquire<br>if Kiwi<br>busy? [n]', 'adm.always_acq_gps', adm.always_acq_gps, 'w3_bool_set_cfg_cb')
         ),

         w3_div('w3-valign w3-text-teal',
            w3_checkbox('w3-label-inline w3-small/w3-small', 'Set date<br>from GPS?', 'adm.gps_set_date', adm.gps_set_date, 'w3_bool_set_cfg_cb')
         ),

         w3_div('w3-valign w3-text-teal',
            //w3_div('w3-show-inline w3-margin-right w3-small', '<b>Include<br>alerted?</b>') +
            //w3_switch('w3-show-inline w3-padding-smaller', 'Yes', 'No', 'adm.include_alert_gps', adm.include_alert_gps, 'admin_radio_YN_cb')
            w3_checkbox('w3-label-inline w3-small/w3-small', 'Include<br>alerted sats in<br>solutions? [n]', 'adm.include_alert_gps', adm.include_alert_gps, 'w3_bool_set_cfg_cb')
         ),

         w3_div('w3-valign w3-text-teal',
            //w3_div('w3-show-inline w3-margin-right w3-small', '<b>Include<br>Galileo?</b>') +
            //w3_switch('w3-show-inline w3-padding-smaller', 'Yes', 'No', 'adm.include_E1B', adm.include_E1B, 'admin_radio_YN_cb')
            w3_checkbox('w3-label-inline w3-small/w3-small', 'Include<br>Galileo in<br>solutions? [y]', 'adm.include_E1B', adm.include_E1B, 'w3_bool_set_cfg_cb')
         ),

         w3_div('w3-valign w3-text-teal',
            //w3_div('w3-show-inline w3-margin-right w3-small', '<b>Kalman<br>filter?</b>') +
            //w3_switch('w3-show-inline w3-padding-smaller', 'Yes', 'No', 'adm.use_kalman_position_solver', adm.use_kalman_position_solver, 'admin_radio_YN_cb')
            w3_checkbox('w3-label-inline w3-small/w3-small', 'Use<br>Kalman<br>filter? [y]', 'adm.use_kalman_position_solver', adm.use_kalman_position_solver, 'w3_bool_set_cfg_cb')
         ),

         /*
         w3_div('w3-valign w3-text-teal',
            w3_div('w3-show-inline w3-margin-right w3-small', '<b>Plot<br>Galileo?</b>') +
            w3_switch('w3-show-inline w3-padding-smaller', 'Yes', 'No', 'adm.plot_E1B', adm.plot_E1B, 'admin_radio_YN_cb')
         ),
         */

         /*
         w3_div('w3-valign w3-text-teal',
            w3_div('w3-show-inline w3-margin-right w3-small', '<b>E1B<br>offset</b>') +
				w3_select('w3-text-red w3-width-auto', '', 'chips', 'adm.E1B_offset', adm.E1B_offset, E1B_offset_i, 'gps_E1B_offset_cb')
         ),
         */

         /*
         w3_div('w3-valign w3-text-teal',
            w3_div('w3-show-inline w3-margin-right w3-small', '<b>E1B<br>gain</b>') +
            w3_select('w3-margin-L-5 w3-text-red w3-width-auto', '', '', '_gps.gain', 0, '1:12', 'gps_gain_cb')
         ),
         */

         w3_div('w3-valign w3-hcenter w3-text-teal',
            w3_div('w3-margin-right', '<b>Select<br>Graph</b>') +
            w3_radio_button('w3-margin-R-4', 'RSSI', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.RSSI, 'gps_graph_cb'),
            w3_radio_button('w3-margin-R-4', 'Az/El', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.AZEL, 'gps_graph_cb'),
            w3_radio_button('w3-margin-R-4', 'Pos', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.POS, 'gps_graph_cb'),
            w3_radio_button('w3-margin-R-4', 'Map', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.MAP, 'gps_graph_cb'),
            w3_radio_button('', 'IQ', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.IQ, 'gps_graph_cb')
         ),

         w3_divs('w3-hcenter w3-text-teal/w3-center',
            w3_div('id-gps-pos-scale w3-center w3-hide w3-small',
               '<b>Scale</b> ',
               w3_select('w3-margin-L-5 w3-text-red', '', '', '_gps.pos_scale', 10-1, '1:20', 'gps_pos_scale_cb')
            ),
            w3_div('id-gps-iq-ch w3-center w3-hide w3-small',
               '<b>Chan</b> ',
               w3_select('w3-margin-L-5 w3-text-red', '', '', '_gps.iq_ch', 0, '1:12', 'gps_iq_ch_cb')
            )
         )
      ) +

	   w3_div('w3-valign',
         w3_div('id-gps-loading-maps w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue|width:100%',
            'loading map...'
         ),
         w3_div('id-gps-channels w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue|width:100%',
            w3_table('id-gps-ch w3-table-6-8 w3-striped')
         ),
         w3_div('id-gps-azel-container w3-hide',
            w3_div('w3-hcenter w3-relative',
               '<img id="id-gps-azel-graph" src="gfx/gpsEarth.png" width="400" height="400" style="position:absolute; top:-2px" />',
               '<canvas id="id-gps-azel-canvas" width="400" height="400" style="position:absolute"></canvas>'
            )
         ),
         w3_div('id-gps-map-container',
            w3_div('||id="id-gps-map"', ''),
            w3_div('id-gps-map-legend w3-small w3-margin-left', '')
         )
		) +

		w3_div('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue',
			w3_table('id-gps-info w3-table-6-8')
		)
	);
	return s;
}

function gps_acq_cb(path, val, first)
{
   if (first) return;
   console.log('gps_acq_cb path='+ path +' val='+ val);
   w3_bool_set_cfg_cb(path, val);
}

function gps_graph_cb(id, idx)
{
   idx = +idx;
   //console.log('gps_graph_cb idx='+ idx);
   admin_int_cb(id, idx);
   ext_send('SET gps_IQ_data_ch='+ ((idx == _gps.IQ)? _gps.iq_ch:0));

   w3_show_hide('id-gps-pos-scale', idx == _gps.POS);
   w3_show_hide('id-gps-iq-ch', idx == _gps.IQ);
   
   // id-gps-channels and id-gps-map-container are separated like this because the id-gps-ch inside id-gps-channels
   // is being updated all the time (@ 1 Hz) and we don't want the map to be effected by this.
   // But the id-gps-map-container needs to have its height set somehow. So we set the map_needs_height flag and
   // let the height be set the next time id-gps-ch is rendered.
   
   var el_ch = w3_el('id-gps-channels');
   if (idx == _gps.AZEL || idx == _gps.MAP) {
      el_ch.style.width = '65%';
      w3_el('id-gps-azel-container').style.width = '35%';
      w3_el('id-gps-map-container').style.width = '35%';
      _gps.map_needs_height = 1;
   } else {
      el_ch.style.width = '100%';
   }
   w3_show_hide('id-gps-azel-container', idx == _gps.AZEL);
   w3_show_hide('id-gps-map-container', idx == _gps.MAP);
   
   gps_update_admin_cb();     // redraw immediately to keep display from glitching
   if (idx == _gps.AZEL) ext_send("SET gps_az_el_history");
}

function gps_E1B_offset_cb(path, idx, first)
{
   idx = +idx;
	if (idx == -1)
	   idx = 4;
	//console.log('gps_E1B_offset_cb idx='+ idx +' path='+ path+' first='+ first);
}

function gps_pos_scale_cb(path, idx, first)
{
   idx = +idx;
	if (idx == -1 || first)
	   idx = 10-1;
	_gps.pos_scale = idx + 1;
	//console.log('gps_pos_scale_cb idx='+ idx +' path='+ path+' first='+ first +' pos_scale='+ _gps.pos_scale);
}

function gps_iq_ch_cb(path, idx, first)
{
   idx = +idx;
	if (idx == -1 || first)
	   idx = 0;
	_gps.iq_ch = idx + 1;  // channel # is biased at 1 so zero indicates "off" (no sampling)
	//console.log('gps_iq_ch_cb idx='+ idx +' path='+ path+' first='+ first +' iq_ch='+ _gps.iq_ch);
   ext_send('SET gps_IQ_data_ch='+ _gps.iq_ch);
   _gps.IQ_data = null;    // blank display until new data arrives
}

function gps_gain_cb(path, idx, first)
{
   idx = +idx;
	if (idx == -1 || first)
	   idx = 0;
	_gps.gain = idx + 1;
	//console.log('gps_gain_cb idx='+ idx +' path='+ path+' first='+ first +' gain='+ _gps.gain);
   ext_send('SET gps_gain='+ _gps.gain);
}

var gps_interval, gps_azel_interval;
var gps_has_lat_lon, gps_az_el_history_running = false;

function gps_schedule_azel()
{
   //console.log('gps_schedule_azel running='+ gps_az_el_history_running);
   if (gps_az_el_history_running == false) {
      gps_az_el_history_running = true;
      ext_send("SET gps_az_el_history");
      gps_azel_interval = setInterval(function() {ext_send("SET gps_az_el_history");}, 60000);
   }
}

function gps_focus(id)
{
   if (!_gps.gps_map_loaded) {
      kiwi_load_js(_gps.leaflet? _gps.pkgs_maps_js : _gps.gmap_js, 'gps_focus2');
      _gps.gps_map_loaded = true;
   } else {
      gps_focus2(id);
   }
}

function gps_focus2(id)
{
   w3_hide('id-gps-loading-maps');
   gps_schedule_azel();
   
	// only get updates while the gps tab is selected
	ext_send("SET gps_update");
	gps_interval = setInterval(function() {ext_send("SET gps_update");}, 1000);
	gps_graph_cb('adm.rssi_azel_iq', adm.rssi_azel_iq);
}

function gps_blur(id)
{
	kiwi_clearInterval(gps_interval);
	kiwi_clearInterval(gps_azel_interval);
   gps_az_el_history_running = false;
	ext_send("SET gps_IQ_data_ch=0");
}

var gps_nsamp;
var gps_nsats;
var gps_now;
var gps_prn;
var gps_az = null;
var gps_el = null;
var gps_qzs3_az = 0;
var gps_qzs3_el = 0;
var gps_shadow_map = null;

function gps_az_el_history_cb(obj)
{
   gps_nsats = obj.n_sats;
   gps_nsamp = obj.n_samp;
   gps_now = obj.now;
   gps_prn = new Array(gps_nsats);
   gps_az = new Array(gps_nsamp*gps_nsats); gps_az.fill(0);
   gps_el = new Array(gps_nsamp*gps_nsats); gps_el.fill(0);
   
   var n_sat = obj.sat_seen.length;
   //console.log('gps_nsamp='+ gps_nsamp +' n_sat='+ n_sat +' alen='+ gps_az.length);
   for (var sat_i = 0; sat_i < n_sat; sat_i++) {
      var sat = obj.sat_seen[sat_i];
      gps_prn[sat] = obj.prn_seen[sat_i];
   }

   for (var samp = 0; samp < gps_nsamp; samp++) {
      for (var sat_i = 0; sat_i < n_sat; sat_i++) {
         var obj_i = samp*n_sat + sat_i;
         var az = obj.az[obj_i];
         var el = obj.el[obj_i];

         var sat = obj.sat_seen[sat_i];
         var azel_i = samp*gps_nsats + sat;
         gps_az[azel_i] = az;
         gps_el[azel_i] = el;

         //console.log('samp='+ samp +' sat_i='+ sat_i +' obj_i='+ obj_i +' sat='+ sat +' prn='+ obj.prn_seen[sat_i] +' az='+ az +' el='+ el);
      }
   }

   gps_qzs3_az = obj.qzs3.az;
   gps_qzs3_el = obj.qzs3.el;
   gps_shadow_map = kiwi_dup_array(obj.shadow_map);
   //for (var az=0; az<90; az++) gps_shadow_map[az] = (az < 45)? 0x0000ffff:0xffff0000;
   gps_update_azel();
}

var SUBFRAMES = 5;
var max_rssi = 1;

var refresh_icon = w3_icon('', 'fa-refresh', 20);

var sub_colors = [ 'w3-red', 'w3-green', 'w3-blue', 'w3-yellow', 'w3-orange' ];

var gps_canvas;
var gps_last_good_el = [];
var gps_rssi_azel_iq_s = [ 'RSSI', 'Az/el', 'Position solution map', 'Map', 'LO PLL IQ' ];

function gps_update_admin_cb()
{
   if (!gps) return;

	var s;
	
	s =
		w3_table_row('',
			w3_table_heads('w3-right-align', 'chan', 'acq', '&nbsp;PRN', 'SNR', 'eph age', 'hold', 'wdog'),
			w3_table_heads('w3-center', 'status', 'subframe'),
			w3_table_heads('w3-right-align', 'ov', 'az', 'el'),
			(adm.rssi_azel_iq == _gps.RSSI)? null : w3_table_heads('w3-right-align', 'RSSI'),
         (adm.rssi_azel_iq == _gps.AZEL || adm.rssi_azel_iq == _gps.MAP)? null : 
            w3_table_heads('w3-center|width:35%',
               ((adm.rssi_azel_iq == _gps.IQ && _gps.iq_ch)? ('Channel '+ _gps.iq_ch +' ') : '') + gps_rssi_azel_iq_s[adm.rssi_azel_iq]
            )
		);
	
      for (var cn=0; cn < gps.ch.length; cn++) {
         s += w3_table_row('id-gps-ch-'+ cn, '');
      }

	w3_el("id-gps-ch").innerHTML = s;
	
	var soln_color = (gps.stype == 0)? 'w3-green' : ((gps.stype == 1)? 'w3-yellow':'w3-red');

	for (var cn=0; cn < gps.ch.length; cn++) {
		var ch = gps.ch[cn];

		if (ch.rssi > max_rssi)
			max_rssi = ch.rssi;
		
		var prn = 0;
		var prn_pre = '';
		if (ch.prn != -1) {
		   if (ch.prn_s != 'N') prn_pre = ch.prn_s;
		   prn = ch.prn;
      }
      //if (cn == 3) console.log('ch04 ch.prn='+ ch.prn +' ch.prn_s='+ ch.prn_s +' snr='+ ch.snr);
      
		//var ch_soln_color = (adm.plot_E1B && ch.prn_s == 'E')? 'w3-yellow' : soln_color;
		var ch_soln_color = soln_color;

      var unlock = ch.alert? 'A' : ((ch.ACF == 1)? '+' : ((ch.ACF == 2)? '-':'U'));
	
		var cells =
			w3_table_cells('w3-right-align', cn+1) +
			w3_table_cells('w3-center', (cn == gps.FFTch)? refresh_icon:'') +
			w3_table_cells('w3-right-align',
				prn? (prn_pre + prn):'',
				ch.snr? ch.snr:''
				//ch.rssi? ch.gain:'',
			) +
			w3_table_cells('w3-right-align'+ (ch.old? ' w3-text-red w3-bold':''), ch.age) +
			w3_table_cells('w3-right-align',
				ch.hold? ch.hold:'',
				ch.rssi? ch.wdog:''
			) +
			w3_table_cells('w3-center',
				'<span class="w3-tag '+ (ch.alert? ((ch.alert == 1)? 'w3-red':'w3-green') : (ch.unlock? 'w3-yellow':'w3-white')) +'">' +
				   unlock +'</span>' +
				'<span class="w3-tag '+ (ch.parity? 'w3-yellow':'w3-white') +'">P</span>' +
				'<span class="w3-tag '+ (ch.soln? ch_soln_color : 'w3-white') +'">S</span>'
			);
	
		var sub = '';
		var has_subframes = false;
		for (var i = SUBFRAMES-1; i >= 0; i--) {
			var sub_color;
			if (ch.sub_renew & (1<<i)) {
				sub_color = 'w3-grey';
			} else {
			   var subframe = ch.sub & (1<<i);
				sub_color = subframe? sub_colors[i]:'w3-white';
				if (subframe) has_subframes = true;
			}
			sub += '<span class="w3-tag '+ sub_color +'">'+ (i+1) +'</span>';
		}
		cells +=
			w3_table_cells('w3-center', sub);
	
      cells +=
         w3_table_cells('w3-right-align',
            ch.novfl? ch.novfl:'',
            ch.el? ch.az:'',
            ch.el? ch.el:'',
            (adm.rssi_azel_iq == _gps.RSSI)? null : (ch.rssi? ch.rssi : '')
         );

	   if (adm.rssi_azel_iq == _gps.RSSI) {
         var pct = ((ch.rssi / max_rssi) * 100).toFixed(0);
         var color = has_subframes? 'w3-light-green' : 'w3-red';
         cells +=
            w3_table_cells('',
               w3_div('w3-progress-container w3-round-xlarge w3-white',
                  w3_div('w3-progressbar w3-round-xlarge '+ color +'|width:'+ pct +'%',
                     w3_div('w3-container w3-text-white', ch.rssi)
                  )
               )
            );
      } else
	   if (adm.rssi_azel_iq != _gps.RSSI || adm.rssi_azel_iq != _gps.AZEL || adm.rssi_azel_iq != _gps.MAP) {
         if (cn == 0) {
            cells +=
               w3_table_cells('|vertical-align:top;position:relative;|rowspan='+ gps.ch.length,
                  w3_div('w3-hcenter',
                     '<canvas id="id-gps-canvas" width="400" height="400" style="position:absolute; z-index:2; pointer-events:none"></canvas>'
                  )
               );
         }
      }

		w3_el('id-gps-ch-'+ cn).innerHTML = cells;
	}

	s =
		w3_table_row('',
			w3_table_heads('', 'acq', 'track', 'good', 'fixes', 'f/min', 'run', 'TTFF', 'UTC offset',
				'ADC clock', 'lat', 'lon', 'alt', 'map')
		) +
		
		w3_table_row('',
			w3_table_cells('',
				gps.acq? 'yes':'paused',
				gps.track? gps.track:'',
				gps.good? gps.good:'',
				gps.fixes? gps.fixes.toUnits():'',
				gps.fixes_min,
				gps.run,
				gps.ttff? gps.ttff:'',
			//	gps.gpstime? gps.gpstime:'',
				gps.utc_offset? gps.utc_offset:'',
				gps.adc_clk.toFixed(6) +' ('+ gps.adc_corr.toUnits() +')',
				gps.lat? gps.lat:'',
				gps.lat? gps.lon:'',
				gps.lat? gps.alt:'',
				gps.lat? gps.map:''
			)
		);
	w3_el("id-gps-info").innerHTML = s;

   gps_canvas = w3_el('id-gps-canvas');
   if (gps_canvas == null) return;
   gps_canvas.ctx = gps_canvas.getContext("2d");
   var ctx = gps_canvas.ctx;
   
   if (adm.rssi_azel_iq == _gps.RSSI) return;

   if (_gps.map_needs_height) {
      var h = css_style_num(w3_el('id-gps-channels'), 'height');
      if (h) {
         w3_el('id-gps-azel-container').style.height = px(h - 24);
         w3_el('id-gps-map').style.height = px(h - 24);
         _gps.a = enc(gps.a);
         _gps.map_needs_height = 0;
      }
   }
      

   ////////////////////////////////
   // MAP
   ////////////////////////////////

   if (adm.rssi_azel_iq == _gps.MAP) {

      if (!_gps.map_init && !_gps.map_needs_height) {
         if (_gps.leaflet) {
            var map_tiles;
            maxZoom = 19;
            var server_e = { MapTiler_Vector:0, MapTiler_Raster_512:1, MapTiler_Raster_256:2, OSM_Raster:3 };
            var server = server_e.OSM_Raster;

            // MapTiler vector tiles using LeafletGL/MapBoxGL
            if (server == server_e.MapTiler_Vector) {
               map_tiles = function(map_style) {
                  return L.mapboxGL({
                     attribution: '<a href="https://www.maptiler.com/license/maps/" target="_blank">&copy; MapTiler</a> <a href="https://www.openstreetmap.org/copyright" target="_blank">&copy; OpenStreetMap contributors</a>',
                     accessToken: 'not-needed',
                     style: 'https://api.maptiler.com/maps/'+ map_style +'/style.json'+ _gps.a
                  });
               }
            }

            // MapTiler 512/256 px raster tiles
            if (server == server_e.MapTiler_Raster_512 || server == server_e.MapTiler_Raster_256) {
               var slash_256 = (server == server_e.MapTiler_Raster_256)? '/256':'';
               map_tiles = function(map_style) {
                  return L.tileLayer(
                     (map_style == 'hybrid')?
                        'https://api.maptiler.com/maps/'+ map_style + slash_256 +'/{z}/{x}/{y}{r}.jpg'+ _gps.a
                     :
                        'https://api.maptiler.com/maps/'+ map_style + slash_256 +'/{z}/{x}/{y}.png'+ _gps.a, {
                     tileSize: (server == server_e.MapTiler_Raster_256)? 256 : 512,
                     zoomOffset: (server == server_e.MapTiler_Raster_256)? 0 : -1,
                     attribution: '<a href="https://www.maptiler.com/copyright/" target="_blank">&copy; MapTiler</a> <a href="https://www.openstreetmap.org/copyright" target="_blank">&copy; OpenStreetMap contributors</a>',
                     crossOrigin: true
                  });
               }
            }

            // OSM raster tiles
            if (server == server_e.OSM_Raster) {
               map_tiles = function() {
                  maxZoom = 18;
                  return L.tileLayer(
                     'https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                     tileSize: 256,
                     zoomOffset: 0,
                     attribution: '<a href="https://www.openstreetmap.org/copyright" target="_blank">&copy; OpenStreetMap contributors</a>',
                     crossOrigin: true
                  });
               }
            }

            var sat_map = map_tiles('hybrid');
            _gps.map = L.map('id-gps-map',
               {
                  maxZoom: maxZoom,
                  minZoom: 1,
               }
            ).setView([0, 0], 1);
            sat_map.addTo(_gps.map);

            // MapTiler map choices
            if (server != server_e.OSM_Raster) {
               L.control.layers(
                  {
                     'Satellite': sat_map,
                     'Basic': map_tiles('basic'),
                     'Bright': map_tiles('bright'),
                     'Positron': map_tiles('positron'),
                     'Street': map_tiles('streets'),
                     'Topo': map_tiles('topo')
                  },
                  null
               ).addTo(_gps.map);
            }

         } else {
            var latlon = new google.maps.LatLng(0, 0);
            var map_div = w3_el('id-gps-map');
            _gps.map = new google.maps.Map(map_div,
               {
                  zoom: 1,
                  center: latlon,
                  navigationControl: false,
                  mapTypeControl: false,
                  streetViewControl: false,
                  mapTypeId: google.maps.MapTypeId.SATELLITE
               });
         }
         _gps.map_init = 1;
      }
      
      w3_innerHTML('id-gps-map-legend', adm.plot_E1B? _gps.legend_sep : _gps.legend_all);

      if (!_gps.MAP_data || !_gps.map_init) return;
         
      if (!_gps.map_locate) {
         if (_gps.leaflet) {
            _gps.map.setView([_gps.MAP_data.ref_lat, _gps.MAP_data.ref_lon], 15, { duration: 0, animate: false });
         } else {
            var latlon = new google.maps.LatLng(_gps.MAP_data.ref_lat, _gps.MAP_data.ref_lon);
            _gps.map.panTo(latlon);
            _gps.map.setZoom(18);
         }
         _gps.map_locate = 1;
      }
      
      var mlen = _gps.MAP_data.MAP.length;
      //console.log('mlen='+ mlen);
      for (var j=0; j < mlen; j++) {
         var mp = _gps.MAP_data.MAP[j];
         //console.log(mp);
         var color = (mp.nmap == 0)? (_gps.leaflet? 'lime':'green') : ((mp.nmap == 1)? 'red':'yellow');
         if (_gps.leaflet) {
            var icon =
               L.divIcon({
                  className: "fooLM",
                  iconAnchor: [12, 12],
                  labelAnchor: [-6, 0],
                  popupAnchor: [0, -36],
                  html: '<span class="cl-leaflet-marker" style="background-color:'+ color +';"/>',
               });
            var mkr = L.marker([mp.lat, mp.lon], { 'icon':icon, 'opacity':1.0 });
            mkr.addTo(_gps.map);
            _gps.map_mkr.push(mkr);
            while (_gps.map_mkr.length > 12) {
               _gps.map_mkr.shift().remove();
            }
         } else {
            var latlon = new google.maps.LatLng(mp.lat, mp.lon);
            var mkr = new google.maps.Marker({
               position:latlon,
               //label: mp.nmap? 'G':'N',
               icon: 'http://maps.google.com/mapfiles/ms/icons/'+ color +'-dot.png',
               map:_gps.map
            });
            _gps.map_mkr.push(mkr);
            while (_gps.map_mkr.length > 12) {
               _gps.map_mkr.shift().setMap(null);
            }
         }
      }
      _gps.MAP_data = null;

      return;
   }


   ////////////////////////////////
   // IQ
   ////////////////////////////////

   if (adm.rssi_azel_iq == _gps.IQ) {
      var axis = 400;
      ctx.fillStyle = 'hsl(0, 0%, 90%)';
      ctx.fillRect(0,0, axis, axis);
      
      // crosshairs
      ctx.fillStyle = 'yellow';
      ctx.fillRect(axis/2,0, 1, axis);
      ctx.fillRect(0,axis/2, axis, 1);
      
      if (!_gps.IQ_data) return;
      ctx.fillStyle = 'black';
      var magnify = 8;
      var scale = (axis/2) / 32768.0 * magnify;
      var len = _gps.IQ_data.IQ.length;

      for (var i=0; i < len; i += 2) {
         var I = _gps.IQ_data.IQ[i];
         var Q = _gps.IQ_data.IQ[i+1];
         if (I == 0 && Q == 0) continue;
         var x = Math.round(I*scale + axis/2);
         var y = Math.round(Q*scale + axis/2);
         if (x < 0 || x >= axis) {
            x = (x < 0)? 0 : axis-1;
         }
         if (y < 0 || y >= axis) {
            y = (y < 0)? 0 : axis-1;
         }
         ctx.fillRect(x,y, 2,2);
      }
      
      return;
   }

   
   ////////////////////////////////
   // POS
   ////////////////////////////////

   if (adm.rssi_azel_iq == _gps.POS) {
      var axis = 400;
      ctx.fillStyle = 'hsl(0, 0%, 90%)';
      ctx.fillRect(0,0, axis, axis);
      
      if (!_gps.POS_data) return;
      ctx.fillStyle = 'black';
      var fs = 0.001000 * _gps.pos_scale;
      var scale = (axis/2) / fs;
      var len = _gps.POS_data.POS.length;
      var clamp = 0;

      //ctx.globalAlpha = 0.5;
      ctx.globalAlpha = 1;
      var x0min, x0max, y0min, y0max, x1min, x1max, y1min, y1max;
      x0min = y0min = x1min = y1min = Number.MAX_VALUE;
      x0max = y0max = x1max = y1max = Number.MIN_VALUE;
      for (var i=0; i < len; i += 2) {
         if (!adm.plot_E1B && i >= len/2) break;
         ctx.fillStyle = (i < len/2)? "DeepSkyBlue":"black";
         var lat = _gps.POS_data.POS[i];
         if (lat == 0) continue;
         var lon = _gps.POS_data.POS[i+1];
         lat -= _gps.POS_data.ref_lat;
         lon -= _gps.POS_data.ref_lon;
         var x = Math.round(lon*scale + axis/2);
         var y = Math.round(-lat*scale + axis/2);
         if (x < 0 || x >= axis) {
            x = (x < 0)? 0 : axis-1;
            clamp++;
         }
         if (y < 0 || y >= axis) {
            y = (y < 0)? 0 : axis-1;
            clamp++;
         }
         var bs = 8;
         if (i < len/2) {
            ctx.fillRect(x-2,y-2, 5,5);
            if (x+bs > x0max) x0max = x+bs; else if (x-bs < x0min) x0min = x-bs;
            if (y+bs > y0max) y0max = y+bs; else if (y-bs < y0min) y0min = y-bs;
         } else {
            ctx.fillRect(x,y-2, 1,5);
            ctx.fillRect(x-2,y, 5,1);
            if (x+bs > x1max) x1max = x+bs; else if (x-bs < x1min) x1min = x-bs;
            if (y+bs > y1max) y1max = y+bs; else if (y-bs < y1min) y1min = y-bs;
         }
      }
      //ctx.globalAlpha = 1.0;
      
      // bboxes
      if (x0max >= axis) x0max = axis-1;
      if (x0min < 0) x0min = 0;
      if (y0max >= axis) y0max = axis-1;
      if (y0min < 0) y0min = 0;
      line_stroke(ctx, 0, 1, 'DeepSkyBlue', x0min,y0min, x0max,y0min);
      line_stroke(ctx, 0, 1, 'DeepSkyBlue', x0min,y0max, x0max,y0max);
      line_stroke(ctx, 1, 1, 'DeepSkyBlue', x0min,y0min, x0min,y0max);
      line_stroke(ctx, 1, 1, 'DeepSkyBlue', x0max,y0min, x0max,y0max);

      if (adm.plot_E1B) {
         if (x1max >= axis) x1max = axis-1;
         if (x1min < 0) x1min = 0;
         if (y1max >= axis) y1max = axis-1;
         if (y1min < 0) y1min = 0;
         line_stroke(ctx, 0, 1, 'black', x1min,y1min, x1max,y1min);
         line_stroke(ctx, 0, 1, 'black', x1min,y1max, x1max,y1max);
         line_stroke(ctx, 1, 1, 'black', x1min,y1min, x1min,y1max);
         line_stroke(ctx, 1, 1, 'black', x1max,y1min, x1max,y1max);
      }
      
      // text
      var x = 16;
      var xi = 12;
      var y = axis - 16*2;
      var yi = 18;
      var yf = 4;
      var fontsize = 15;
      ctx.font = fontsize +'px Courier';

      ctx.fillStyle = "DeepSkyBlue";
      ctx.fillRect(x-2,y-2-yf, 5,5);
      x += xi;
      ctx.fillStyle = 'black';
      ctx.fillText((adm.plot_E1B? ' w/o Galileo span: ':'All sats span: ')+
         _gps.POS_data.y0span.toFixed(0).fieldWidth(4) +'m Ylat '+ _gps.POS_data.x0span.toFixed(0).fieldWidth(4) +'m Xlon', x,y);

      if (adm.plot_E1B) {
         x -= xi;
         y += yi;
         ctx.fillStyle = 'black';
         ctx.fillRect(x,y-2-yf, 1,5);
         ctx.fillRect(x-2,y-yf, 5,1);
         x += xi;
         ctx.fillText('with Galileo span: '+ _gps.POS_data.y1span.toFixed(0).fieldWidth(4) +'m Ylat '+ _gps.POS_data.x1span.toFixed(0).fieldWidth(4) +'m Xlon', x,y);
      }

      //if (clamp) console.log('gps POS clamp='+ clamp);
      return;
   }
}

function gps_update_azel()
{
   ////////////////////////////////
   // AZEL
   ////////////////////////////////

   //console.log('gps_update_azel');

   if (adm.rssi_azel_iq != _gps.AZEL || gps_el == null) return;

   var gps_azel_canvas = w3_el('id-gps-azel-canvas');
   if (gps_azel_canvas == null) return;
   gps_azel_canvas.ctx = gps_azel_canvas.getContext("2d");
   var ctx = gps_azel_canvas.ctx;

   var gW = 400;
   var gD = 360;
   var gHD = gD/2;
   var gM = (gW-gD)/2;
   var gO = gHD + gM;
   ctx.clearRect(0, 0, gW, gW);
   
   if (adm.rssi_azel_iq == _gps.AZEL && gps_shadow_map) {
      ctx.fillStyle = "cyan";
      ctx.globalAlpha = 0.1;
      var z = 4;
      var zw = z*2 + 1;
      
      for (var az = 0; az < 360; az++) {
         var az_rad = az * Math.PI / gHD;
         var elm = gps_shadow_map[az];
         for (var n = 0, b = 1; n < 32; n++, b <<= 1) {
            if (elm & b) {
               var el = n/31 * 90;
               var r = (90 - el)/90 * gHD;
               var x = Math.round(r * Math.sin(az_rad));
               var y = Math.round(r * Math.cos(az_rad));
               ctx.fillRect(x+gO-z-1, gO-y-z-1, zw+2, zw+2);
            }
         }
      }
      ctx.globalAlpha = 1;
   }
   
   ctx.strokeStyle = "black";
   ctx.miterLimit = 2;
   ctx.lineJoin = "circle";
   ctx.font = "13px Verdana";

   if (gps_qzs3_el > 0) {
      var az_rad = gps_qzs3_az * Math.PI / gHD;
      var r = (90 - gps_qzs3_el)/90 * gHD;
      var x = Math.round(r * Math.sin(az_rad));
      var y = Math.round(r * Math.cos(az_rad));
      x += gO;
      y = gO - y;
      //console.log('QZS-3 az='+ gps_qzs3_az +' el='+ gps_qzs3_el +' x='+ x +' y='+ y);
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.arc(x,y,10, 0,2*Math.PI);
      ctx.stroke();

      // legend
      x = 12;
      y = 30;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.arc(x,y,10, 0,2*Math.PI);
      ctx.stroke();
      var xo = 16;
      var yo = 4;
      ctx.fillStyle = 'black';
      ctx.lineWidth = 1;
      ctx.fillText('QZS-3', x+xo, y+yo);
   }

   ctx.fillStyle = "black";
   
   // img & canvas alignment target
   //ctx.fillRect(100, 200, 200, 1);
   //ctx.fillRect(200, 100, 1, 200);

   for (var sat = 0; sat < gps_nsats; sat++) gps_last_good_el[sat] = -1;
   
   for (var off = gps_nsamp-10; off >= -1; off--) {
      for (var sat = 0; sat < gps_nsats; sat++) {
         var loff = (off == -1)? gps_last_good_el[sat] : off;
         if (off == -1 && loff == -1) continue;
         var m = gps_now - loff;
         if (m < 0) m += gps_nsamp;
         i = m*gps_nsats + sat;
         var az = gps_az[i];
         var el = gps_el[i];
         if (el == 0) continue;
         gps_last_good_el[sat] = off;
         
         var az_rad = az * Math.PI / gHD;
         var r = (90 - el)/90 * gHD;
         var x = Math.round(r * Math.sin(az_rad));
         var y = Math.round(r * Math.cos(az_rad));

         if (off == -1) {
            var prn = gps_prn[sat];
            var tw = ctx.measureText(prn).width;
            var tof = 8;
            var ty = 5;
            var toff = (az <= 180)? (-tw-tof) : tof;
            ctx.fillStyle = (loff > 1)? "pink" : "yellow";
            ctx.lineWidth = 3;   // rule of thumb: font size 13px /3 = 4.333
            ctx.strokeText(prn, x+toff+gO, gO-y+ty);
            ctx.lineWidth = 1;
            ctx.fillText(prn, x+toff+gO, gO-y+ty);

            var z = 3;
            var zw = z*2 + 1;
            ctx.fillStyle = "black";
            ctx.fillRect(x+gO-z-1, gO-y-z-1, zw+2, zw+2);
            ctx.fillStyle = (loff > 1)? "red" : "yellow";
            ctx.fillRect(x+gO-z, gO-y-z, zw, zw);
         } else {
            ctx.fillStyle = "black";
            ctx.fillRect(x+gO, gO-y, 2, 2);
         }
      }
   }
}


////////////////////////////////
// log
////////////////////////////////

var log = { };
var nlog = 256;

function log_html()
{
	var s =
	w3_div('id-log w3-text-teal w3-hide',
		'<hr>'+
		w3_div('w3-container',
		   w3_inline('w3-valign w3-halign-space-between/',
		      w3_div('',
               w3_label('w3-show-inline', 'KiwiSDR server log (scrollable list, first and last set of messages)'),
               w3_button('w3-aqua|margin-left:10px', 'Dump', 'log_dump_cb'),
               w3_button('w3-blue|margin-left:10px', 'Clear Histogram', 'log_clear_hist_cb')
            ),
				w3_div('',
					'<b>Log connections from local ip addresses?</b> ',
					w3_switch('', 'Yes', 'No', 'adm.log_local_ip', adm.log_local_ip, 'admin_radio_YN_cb')
				)
         ),
			w3_div('id-log-msg w3-margin-T-8 w3-text-output w3-small w3-text-black', '')
		)
	);
	return s;
}

function log_setup()
{
	var el = w3_el('id-log-msg');
	var s = '<pre>';
		for (var i = 0; i < nlog; i++) {
			if (i == nlog/2) s += '<code id="id-log-not-shown"></code>';
			s += '<code id="id-log-'+ i +'"></code>';
		}
	s += '</pre>';
	el.innerHTML = s;

	ext_send('SET log_update=1');
}

function log_dump_cb(id, idx)
{
	ext_send('SET log_dump');
}

function log_clear_hist_cb(id, idx)
{
	ext_send('SET log_clear_hist');
}


function log_resize()
{
	var el = w3_el('id-log-msg');
	if (!el) return;
	var log_height = window.innerHeight - w3_el("id-admin-header-container").clientHeight - 100;
	el.style.height = px(log_height);
}

var log_interval;

function log_focus(id)
{
	log_resize();
	log_update();
	log_interval = setInterval(log_update, 3000);
}

function log_blur(id)
{
	kiwi_clearInterval(log_interval);
}

function log_update()
{
	ext_send('SET log_update=0');
}


////////////////////////////////
// console
////////////////////////////////

// must set "remove_returns" since pty output lines are terminated with \r\n instead of \n alone
// otherwise the \r overwrite logic in kiwi_output_msg() will be triggered
var console_status_msg_p = { scroll_only_at_bottom: true, inline_returns: true, process_return_alone: false, remove_returns: false, ncol: 160 };

function console_html()
{
	var s =
	w3_div('id-console w3-section w3-text-teal w3-hide',
		w3_div('w3-container',
		   w3_div('',
            w3_label('w3-show-inline', 'Beagle Debian console') +
            w3_button('w3-aqua|margin-left:10px', 'Connect', 'console_connect_cb')
         ),
			w3_div('id-console-msg w3-margin-T-8 w3-text-output w3-scroll-down w3-small w3-text-black|background-color:#a8a8a8',
			   '<pre><code id="id-console-msgs"></code></pre>'
			),
         w3_div('w3-margin-top',
            w3_input('', '', 'console_input', '', 'console_input_cb|console_ctrl_cb', 'enter shell command')
         ),
         w3_text('w3-text-black w3-margin-top',
            'Control characters (^C, ^D, ^\\) and empty lines may now be typed directly into shell command field.'
         )
		)
	);
	return s;
}

function console_input_cb(path, val)
{
	//console.log('console_w2c '+ val.length +' <'+ val +'>');
	ext_send('SET console_w2c='+ encodeURIComponent(val +'\n'));
   w3_set_value(path, '');    // erase input field
}

function console_connect_cb()
{
	ext_send('SET console_open');
}

function console_ctrl_cb(c)
{
	ext_send('SET console_ctrl='+ c);
}

function console_setup()
{
}

function console_resize()
{
	var el = w3_el('id-console-msg');
	if (!el) return;
	var console_height = window.innerHeight - w3_el("id-admin-header-container").clientHeight - 200;
	el.style.height = px(console_height);
}

function console_focus(id)
{
	console_resize();
}

function console_blur(id)
{
}


////////////////////////////////
// extensions, in admin_sdr.js
////////////////////////////////


////////////////////////////////
// security
////////////////////////////////

function security_html()
{
   // Let cfg.chan_no_pwd retain values > rx_chans if it was set when another configuration
   // was used. Just clamp the menu value to the current rx_chans;
	var chan_no_pwd = ext_get_cfg_param('chan_no_pwd', 0);
	chan_no_pwd = Math.min(chan_no_pwd, rx_chans - 1);
   var chan_no_pwd_u = { 0:'none' };
   for (var i = 1; i < rx_chans; i++)
      chan_no_pwd_u[i] = i.toFixed(0);

	var s1 =
	/*
		w3_div('w3-valign',
			'<header class="w3-container w3-yellow"><h5>' +
			'Passwords are now stored in an encrypted format. After the page is reloaded the ' +
			'password fields will show "(encrypted)" instead of showing the passwords in the clear. <br>' +
			'As before, you may change passwords at any time and also set an empty password if, for example, ' +
			'you want to allow user connections without needing a password. <br>' +
			'If below "Admin auto-login from local net even if password set" is set to "No", ' +
			'<i>and you forget the admin password</i>, then you\'ll have no way to bring up the admin page. <br>' +
			'In that case the only way to recover is to ssh/PuTTY into Debian on the Beagle and remove the password encryption files manually.' +
			'</h5></header>'
		) +
	*/

		'<hr>' +
		w3_inline_percent('w3-container/w3-hspace-16 w3-text-teal',
			w3_div('',
				w3_div('',
					'<b>User auto-login from local net<br>even if password set?</b><br>',
					w3_switch('w3-margin-T-8', 'Yes', 'No', 'adm.user_auto_login', adm.user_auto_login, 'admin_radio_YN_cb')
				)
			), 25,

			w3_div('w3-center',
				w3_select('w3-width-auto', 'Number of channels<br>not requiring a password<br>even if password set',
					'', 'chan_no_pwd', chan_no_pwd, chan_no_pwd_u, 'admin_select_cb'),
				w3_div('w3-margin-T-8 w3-text-black',
					'Set this and a password to create two sets of channels. ' +
					'Some that have open-access requiring no password and some that are password protected.'
				)
			), 30,

			w3_div(''), 1,

			w3_div('',
				w3_input('w3-encrypted', 'User password', 'adm.user_password', '', 'security_set_upw_cb',
					'No password set: unrestricted Internet access to SDR')
			), 40
		) +

		'<hr>' +
		w3_inline_percent('w3-container/w3-hspace-16 w3-text-teal',
			w3_div('',
				w3_div('',
					'<b>Admin auto-login from local net<br>even if password set?</b><br>',
					w3_switch('w3-margin-T-8', 'Yes', 'No', 'adm.admin_auto_login', adm.admin_auto_login, 'admin_radio_YN_cb')
				)
			), 25,

			w3_div('w3-text-teal',
				''
			), 30,

			w3_div(''), 1,

			w3_div('',
				w3_input('w3-encrypted', 'Admin password', 'adm.admin_password', '', 'security_set_apw_cb',
					'No password set: no admin access from Internet allowed')
			), 40
		);

   var s2 =
		'<hr>' +
		w3_inline_percent('w3-container/w3-hspace-16 w3-text-teal',
			w3_div('',
				w3_div('',
					'<b>Restrict console connections <br> to the local network?</b><br>',
					w3_switch('w3-margin-T-8', 'Yes', 'No', 'adm.console_local', adm.console_local, 'admin_radio_YN_cb')
				)
			), 25,

			w3_div('w3-text-black',
				'Set to "Yes" (default) to restrict use of the console tab ' +
				'to connections from the local network. This is an important ' +
				'security enhancement. You might want to set to "No" if the  ' +
				'Kiwi has difficulty determining your local network address. '
			), 33
		) +

		'<hr>' +
		w3_inline_percent('w3-container/w3-hspace-16 w3-text-teal',
			w3_div('',
				w3_div('',
					'<b>Allow GPS timestamp information <br> to be sent on the network?</b><br>',
					w3_switch('w3-margin-T-8', 'Yes', 'No', 'adm.GPS_tstamp', adm.GPS_tstamp, 'admin_radio_YN_cb')
				)
			), 25,

			w3_div('w3-text-black',
				'Set to "No" to prevent timestamp information from your GPS ' +
				'(assuming it is working) from being used by applications on the Internet ' +
				'such as the TDoA service. You would only do this if you had some concern ' +
				'about your publicly-listed Kiwi participating in these kinds of projects. '
			), 33,

			w3_div('w3-text-black'), 1,

			w3_div('w3-text-black',
				'However we expect most Kiwi owners will want to participate and we encourage ' +
				'you to do so. Your precise GPS location is not revealed by the timestamp information. ' +
				'For more discussion please see the ' +
				w3_link('w3-link-darker-color', 'http://forum.kiwisdr.com/discussion/1218/participation-of-kiwis-in-the-tdoa-process/p1', 'Kiwi forum') +'.'
			), 33
		) +

		'<hr>' +
		//w3_div('id-security-json w3-section w3-border')
		'';

	return w3_div('id-security w3-hide', s1 + s2);
}

function security_focus(id)
{
	admin_set_decoded_value('adm.user_password');
	admin_set_decoded_value('adm.admin_password');
	//w3_el('id-security-json').innerHTML = w3_div('w3-padding w3-scroll', JSON.stringify(cfg));
}

function security_set_upw_cb(path, val, first)
{
   adm.user_pwd_seq = +adm.user_pwd_seq + 1;
   w3_string_set_cfg_cb(path, val, first);
}

function security_set_apw_cb(path, val, first)
{
   adm.admin_pwd_seq = +adm.admin_pwd_seq + 1;
   w3_string_set_cfg_cb(path, val, first);
}


////////////////////////////////
// admin
////////////////////////////////

var admin_colors = [
	'w3-hover-red',
	'w3-hover-blue',
	'w3-hover-purple',
	'w3-hover-black',
	'w3-hover-aqua',
	'w3-hover-pink',
	'w3-hover-yellow',
	'w3-hover-amber',
	'w3-hover-green',
	'w3-hover-orange',
	'w3-hover-grey',
	'w3-hover-lime',
	'w3-hover-indigo',
	'w3-hover-brown',
	'w3-hover-teal',
	'w3-hover-blue-grey'
];

function admin_main()
{
	window.addEventListener("resize", admin_resize);
}

function admin_resize()
{
	log_resize();
	console_resize();
}

function kiwi_ws_open(conn_type, cb, cbp)
{
	return open_websocket(conn_type, cb, cbp, admin_msg, admin_recv, null, admin_close);
}

function admin_draw(sdr_mode)
{
	var ael = w3_el("id-admin");
	var ci = 0;
	
	var s = '';
	if (!sdr_mode) s += w3_nav(admin_colors[ci++], 'GPS', 'gps', 'admin_nav');
	s +=
      w3_nav(admin_colors[ci++], 'Status', 'status', 'admin_nav') +
      w3_nav(admin_colors[ci++], 'Mode', 'mode', 'admin_nav') +
      w3_nav(admin_colors[ci++], 'Control', 'control', 'admin_nav') +
      w3_nav(admin_colors[ci++], 'Connect', 'connect', 'admin_nav');
	if (sdr_mode)
	   s +=
         //w3_nav(admin_colors[ci++], 'Channels', 'channels', 'admin_nav') +
         w3_nav(admin_colors[ci++], 'Config', 'config', 'admin_nav') +
         w3_nav(admin_colors[ci++], 'Webpage', 'webpage', 'admin_nav') +
         w3_nav(admin_colors[ci++], 'Public', 'sdr_hu', 'admin_nav') +
         w3_nav(admin_colors[ci++], 'DX', 'dx', 'admin_nav');
   s += 
      w3_nav(admin_colors[ci++], 'Update', 'update', 'admin_nav') +
      w3_nav(admin_colors[ci++], 'Backup', 'backup', 'admin_nav') +
      w3_nav(admin_colors[ci++], 'Network', 'network', 'admin_nav') +
      (sdr_mode? w3_nav(admin_colors[ci++], 'GPS', 'gps', 'admin_nav') : '') +
      w3_nav(admin_colors[ci++], 'Log', 'log', 'admin_nav') +
      w3_nav(admin_colors[ci++], 'Console', 'console', 'admin_nav') +
      (sdr_mode? w3_nav(admin_colors[ci++], 'Extensions', 'extensions', 'admin_nav') : '') +
      w3_nav(admin_colors[ci++], 'Security', 'security', 'admin_nav');

	ael.innerHTML =
		w3_div('id-admin-header-container',
			'<header class="w3-container w3-teal"><h5>Admin interface</h5></header>' +
			w3_navbar('w3-border w3-light-grey', s) +
	
			w3_divs('id-restart w3-hide/w3-valign',
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Restart required for changes to take effect</h5></header>' +
				w3_div('w3-show-inline-block', w3_button('w3-green w3-margin', 'KiwiSDR server restart', 'admin_restart_now_cb')) +
				w3_div('w3-show-inline-block', w3_button('w3-yellow w3-margin', 'cancel', 'admin_restart_cancel_cb'))
			) +
			
			w3_divs('id-reboot w3-hide/w3-valign',
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Reboot required for changes to take effect</h5></header>' +
				w3_div('w3-show-inline-block', w3_button('w3-green w3-margin', 'Beagle reboot', 'admin_reboot_now_cb')) +
				w3_div('w3-show-inline-block', w3_button('w3-yellow w3-margin', 'cancel', 'admin_reboot_cancel_cb'))
			) +
			
			w3_div('id-build-restart w3-valign w3-hide',
				'<header class="w3-container w3-blue"><h5>Server will restart after build</h5></header>'
			) +

			w3_div('id-build-reboot w3-valign w3-hide',
				'<header class="w3-container w3-red"><h5>Beagle will reboot after build</h5></header>'
			)
		);
	
	if (sdr_mode)
	   s = status_html();
	else
	   s = gps_html() + status_html();

	s +=
		mode_html() +
		control_html() +
		connect_html();

	if (sdr_mode)
	   s +=
         //channels_html() +
		   config_html() +
         webpage_html() +
         kiwi_reg_html() +
         dx_html();

   s +=
		update_html() +
		backup_html() +
		network_html() +
		(sdr_mode? gps_html() : '') +
		log_html() +
		console_html() +
		(sdr_mode? extensions_html() : '') +
		security_html();

	ael.innerHTML += s;
	log_setup();
	console_setup();
	stats_init();

	if (sdr_mode) {
	   users_init( { admin:1 } );
	   //gps_focus();
	} else {
	   gps_focus();
	}

	w3_show_block('id-admin');
	var nav_def = sdr_mode? 'status' : 'gps';
	
	admin.init = true;
      w3_click_nav(kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, nav_def, nav_def, 'last_admin_navbar'), 'admin_nav');
	admin.init = false;
	
	setTimeout(function() { setInterval(status_periodic, 5000); }, 1000);
}

function admin_nav_focus(id, cb_arg)
{
   //console.log('admin_nav_focus id='+ id);
   w3_click_nav(id, id);
   writeCookie('last_admin_navbar', id);
}

function admin_nav_blur(id, cb_arg)
{
   //console.log('admin_nav_blur id='+ id);
   w3_call(id +'_blur');
}

function admin_close()
{
   // don't show message if reload countdown running
   if (!admin.reload_rem && !admin.long_running)
      kiwi_show_msg('Server has closed connection.');
}

// Process replies to our messages sent via ext_send('SET ...')
// As opposed to admin_recv() below that processes unsolicited messages sent from C code.

var gps = null;

function admin_msg(param)
{
   //console.log('admin_msg: '+ param[0]);
   switch (param[0]) {

		case "gps_update_cb":
			//console.log('gps_update_cb='+ param[1]);
         gps = kiwi_JSON_parse('gps_update_cb', decodeURIComponent(param[1]));
         w3_call('gps_update_admin_cb');
			break;

		case "gps_IQ_data_cb":
         _gps.IQ_data = kiwi_JSON_parse('gps_IQ_data_cb', decodeURIComponent(param[1]));
			break;

      case "gps_POS_data_cb":
         _gps.POS_data = kiwi_JSON_parse('gps_POS_data_cb', decodeURIComponent(param[1]));
         break;

      case "gps_MAP_data_cb":
         _gps.MAP_data = kiwi_JSON_parse('gps_MAP_data_cb', decodeURIComponent(param[1]));
         break;

      case "gps_az_el_history_cb":
         var gps_az_el = kiwi_JSON_parse('gps_az_el_history_cb', decodeURIComponent(param[1]));
         if (gps_az_el) w3_call('gps_az_el_history_cb', gps_az_el);
         break;

		case "dx_size":
			dx_size(param[1]);
			break;
		
		case "admin_mkr":
			var mkr = param[1];
			var obj = kiwi_JSON_parse('admin_mkr', mkr);
			if (obj) dx_render(obj);
			break;
		
		case "mkr_search_pos":
		   dx_search_pos_cb(param[1]);
		   break;

		case "keepalive":
		   kiwi_clearTimeout(admin.keepalive_timeoout);
		   admin.keepalive_timeoout = setTimeout(function() {
		      admin_close();
		   }, 30000);
		   break;

		default:
		   return false;
   }
   
   return true;
}

var log_msg_idx, log_msg_not_shown = 0;
var admin_sdr_mode = 1;

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

			case "admin_sdr_mode":
				admin_sdr_mode = (+param[1])? 1:0;
				break;

			case "is_multi_core":
				admin.is_multi_core = true;
				break;

			case "init":
		      // rx_chan == rx_chans for admin connections (e.g. 4 when ch = 0..3 for user connections)
				rx_chans = rx_chan = +param[1];
				//console.log("ADMIN init rx_chans="+rx_chans);
				
				if (rx_chans == -1) {
					w3_innerHTML('id-admin',
						'<header class="w3-container w3-red"><h5>Admin interface</h5></header>' +
						'<p>To use the new admin interface you must edit the configuration ' +
						'parameters from your current kiwi.config/kiwi.cfg into kiwi.config/kiwi.json<br>' +
						'Use the file kiwi.config/kiwi.template.json as a guide.</p>');
				} else {
					admin_draw(admin_sdr_mode);
					ext_send('SET extint_load_extension_configs');
				}
				break;

			case "ext_call":
			   // assumes that '=' is a safe delimiter to split func/param
				var ext_call = decodeURIComponent(param[1]).split('=');
				var ext_func = ext_call[0];
				var ext_param = (ext_call.length > 1)? ext_call[1] : null;
				//console.log('ext_call: func='+ ext_func +' param='+ ext_param);
				w3_call(ext_func, ext_param);
				break;

			case "public_update":
				public_update(param[1]);
				break;

			case "auto_nat":
				var p = +param[1];
				//console.log('auto_nat='+ p);
				var el = w3_el('id-net-auto-nat-msg');
				var msg, color, type = 'add';
				
				switch (p) {
					case 0: break;
					case 1: msg = 'succeeded'; color = 'w3-green'; break;
					case 2: msg = 'no device found'; color = 'w3-orange'; break;
					case 3: msg = 'rule already exists'; color = 'w3-yellow'; break;
					case 4: msg = 'command failed'; color = 'w3-red'; break;
					case 5: msg = 'pending'; color = 'w3-yellow'; break;
					case 6: msg = 'pending'; color = 'w3-yellow'; type = 'delete'; break;
					case 7: msg = 'succeeded'; color = 'w3-green'; type = 'delete'; break;
					default: break;
				}
				
				if (p && el) {
					el.innerHTML = '<header class="w3-container"><h5>Automatic '+ type +' of NAT rule on firewall / router: '+ msg +'</h5></header>';
					w3_remove_then_add(el, network.auto_nat_color, color);
					network.auto_nat_color = color;
					w3_show_block(el);
				}
				break;

			case "log_msg_not_shown":
				log_msg_not_shown = parseInt(param[1]);
				if (log_msg_not_shown) {
					var el = w3_el('id-log-not-shown');
					el.innerHTML = '---- '+ log_msg_not_shown.toString() +' lines not shown ----\n'
				}
				break;

			case "log_msg_idx":
				log_msg_idx = parseInt(param[1]);
				break;

			case "log_msg_save":
				var el = w3_el('id-log-'+ log_msg_idx);
				if (!el) break;
				var el2 = w3_el('id-log-msg');
				var wasScrolledDown = w3_isScrolledDown(el2);
				var s = kiwi_decodeURIComponent('log_msg_save', param[1]).replace(/</g, '&lt;').replace(/>/g, '&gt;');
				el.innerHTML = s;

				// only jump to bottom of updated list if it was already sitting at the bottom
				w3_scrollDown(el2, wasScrolledDown);
				break;

			case "log_update":
				log_update(param[1]);
				break;

			case "microSD_done":
				backup_sd_write_done(parseFloat(param[1]));
				break;

			case "DUC_status":
				connect_DUC_status_cb(parseFloat(param[1]));
				break;

			case "rev_status":
				connect_rev_status_cb(parseFloat(param[1]));
				break;

			case "check_port_status":
				network_check_port_status_cb(parseInt(param[1]));
				break;
				
			case "console_c2w":
		      // kiwi_output_msg() does decodeURIComponent()
		      console_status_msg_p.s = param[1];
		      //console.log('console_c2w:');
		      //console.log(console_status_msg_p);
				kiwi_output_msg('id-console-msgs', 'id-console-msg', console_status_msg_p);
				break;

			case "console_done":
			   console.log('## console_done');
				break;

			case "config_clone_status":
				config_clone_status_cb(parseInt(param[1]));
				break;
				
			case "network_ip_blacklist_status":
			   p = decodeURIComponent(param[1]).split(',')
				network_ip_blacklist_status(parseInt(p[0]), p[1]);
				break;
				
			case "network_ip_blacklist_enabled":
            if (!network.ip_address_error)
               w3_innerHTML('id-ip-blacklist-status', 'updated');
				break;
				
			default:
				console.log('ADMIN UNKNOWN: '+ param[0] +'='+ param[1]);
		}
	}
}

// callback when a control has w3-restart property
function w3_restart_cb()
{
	w3_show_block('id-restart');
	w3_scrollTop('id-kiwi-container');
}

// callback when a control has w3-reboot property
function w3_reboot_cb()
{
	w3_show_block('id-reboot');
	w3_scrollTop('id-kiwi-container');
}

function admin_draw_pie() {
	w3_el('id-admin-reload-secs').innerHTML = 'Admin page reload in '+ admin.reload_rem + ' secs';
	if (admin.reload_rem > 0) {
		admin.reload_rem--;
		kiwi_draw_pie('id-admin-pie', admin.pie_size, (admin.reload_secs - admin.reload_rem) / admin.reload_secs);
	} else {
		window.location.reload(true);
	}
};

function admin_wait_then_reload(secs, msg)
{
	var ael = w3_el("id-admin");
	var s2;
	
	if (secs) {
		s2 =
			w3_divs('w3-valign w3-margin-T-8/w3-container',
				w3_div('w3-show-inline-block', kiwi_pie('id-admin-pie', admin.pie_size, '#eeeeee', 'deepSkyBlue')),
				w3_div('w3-show-inline-block',
					w3_div('id-admin-reload-msg'),
					w3_div('id-admin-reload-secs')
				)
			);
	} else {
		s2 =
			w3_divs('w3-valign w3-margin-T-8/w3-container',
				w3_div('id-admin-reload-msg')
			);
	}
	
	var s = '<header class="w3-container w3-teal"><h5>Admin interface</h5></header>'+ s2;
	//console.log('s='+ s);
	ael.innerHTML = s;
	
	if (msg) w3_el("id-admin-reload-msg").innerHTML = msg;
	
	if (secs) {
		admin.reload_rem = admin.reload_secs = secs;
		setInterval(admin_draw_pie, 1000);
		admin_draw_pie();
	}
}

function admin_restart_now_cb()
{
	ext_send('SET restart');
	admin_wait_then_reload(60, 'Restarting KiwiSDR server');
}

function admin_restart_cancel_cb()
{
	w3_hide('id-restart');
}

function admin_reboot_now_cb()
{
	ext_send('SET reboot');
	admin_wait_then_reload(90, 'Rebooting Beagle');
}

function admin_reboot_cancel_cb()
{
	w3_hide('id-reboot');
}

function admin_int_cb(path, val)
{
	//console.log('admin_int_cb '+ path +'='+ val);
	val = parseInt(val);
	if (isNaN(val)) {
	   // put old value back
	   val = ext_get_cfg_param(path);
	} else {
	   ext_set_cfg_param(path, val, true);
	}
   w3_set_value(path, val);   // remove any fractional part from field
}

// limit precision using callback spec: 'admin_float_cb|prec'
function admin_float_cb(path, val, first, cb_a)
{
   var prec = -1;    // default to no precision limiting applied
	//console.log('admin_float_cb '+ path +'='+ val +' cb_a.len='+ cb_a.length);
	if (cb_a && cb_a.length >= 2) {
	   prec = +(cb_a[1]);
	   if (isNaN(prec)) prec = -1;
	   //console.log('admin_float_cb prec='+ prec);
	}
	val = parseFloat(val);
	if (isNaN(val)) {
	   // put old value back
	   val = ext_get_cfg_param(path);
	} else {
	   if (prec != -1) {
         var s = val.toFixed(prec);    // NB: .toFixed() does rounding
         //console.log('admin_float_cb val('+ prec +')='+ s);
         val = +s;
      }
	   ext_set_cfg_param(path, val, true);
	}
   w3_set_value(path, val);   // remove any non-numeric part from field
}

function admin_bool_cb(path, val, first)
{
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	//console.log('admin_bool_cb path='+ path +' val='+ val +' save='+ save);
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
	admin_bool_cb(id, idx? 0:1);     // idx: 0 = 'yes', 1 = 'no'
}

function admin_select_cb(path, idx, first)
{
	//console.log('admin_select_cb idx='+ idx +' path='+ path+' first='+ first);
	idx = +idx;
	if (idx != -1) {
		var save = first? false : true;
		ext_set_cfg_param(path, idx, save);
	}
}

function admin_slider_cb(path, val, done, first)
{
   if (!done || first) return;
	//console.log('admin_slider_cb path='+ path +' val='+ val);
	val = +val;
   ext_set_cfg_param(path, val, true);
}

function admin_preview_status_box(id, val)
{
	var s = kiwi_decodeURIComponent('admin_preview_status_box:'+ id, val);
	if (!s || s == '') s = '&nbsp;';
	return s;
}
