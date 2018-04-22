// Copyright (c) 2016 John Seamons, ZL/KF6VO

// TODO
//		input range validation
//		NTP status?


////////////////////////////////
// status
////////////////////////////////

function status_html()
{
   var s2 = admin_sdr_mode?
		('<hr>' +
		w3_divs('id-msg-errors w3-container') + 
		w3_divs('id-status-dpump-resets w3-container') + 
		w3_divs('w3-container', '',
		   w3_div('id-status-dpump-hist w3-show-inline-block') +
         w3_button('w3-aqua|margin-left:10px', 'Reset', 'status_dpump_hist_reset_cb')
      )) : '';
   
	var s =
	w3_divs('id-status w3-hide', '',
		'<hr>' +
		w3_divs('id-problems w3-container') +
		w3_divs('id-msg-config w3-container') +
		w3_divs('id-msg-gps w3-container') +
		'<hr>' +
		w3_divs('id-msg-stats-cpu w3-container') +
		w3_divs('id-msg-stats-xfer w3-container') +
		s2 +
      '<hr>' +
		w3_divs('id-debugdiv w3-container')
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
// control
////////////////////////////////

function control_html()
{
	var s1 =
		'<hr>' +
		w3_half('w3-vcenter', '',
         w3_div('',
            w3_div('',
               w3_button('w3-aqua w3-margin', 'KiwiSDR server restart', 'admin_restart_cb'),
               w3_button('w3-blue w3-margin', 'Beagle reboot', 'admin_reboot_cb'),
               w3_button('w3-red w3-margin', 'Beagle power off', 'admin_power_off_cb')
            ),
            w3_divs('id-confirm w3-vcenter w3-hide', '',
               w3_button('w3-css-yellow w3-margin', 'Confirm', 'admin_confirm_cb')
            )
         ),
			w3_div('w3-container w3-center',
            '<b>Daily restart?</b> ' +
            w3_switch('', 'Yes', 'No', 'adm.daily_restart', adm.daily_restart, 'admin_radio_YN_cb'),
				w3_divs('', 'w3-text-black',
					"Set if you're having problems with the server<br>after it has run for a period of time.<br>" +
					"Restart occurs at the same time as updates (0200-0600 UTC)<br> and will wait until there are no connections."
				)
			)
      );

	var s2 =
		'<hr>' +
		w3_half('', 'w3-container w3-vcenter',
			w3_div('',
            '<b>Enable user connections?</b> ' +
            w3_switch('', 'Yes', 'No', 'adm.server_enabled', adm.server_enabled, 'server_enabled_cb')
			),
			w3_div('',
				'<b>Close all active user connections </b> ' +
				w3_button('w3-red', 'Kick', 'control_user_kick_cb')
			)
		) +
		w3_divs('w3-container w3-margin-top', '',
			w3_input_get_param('Reason if disabled', 'reason_disabled', 'reason_disabled_cb', '', 'will be shown to users attempting to connect')
		) +
		w3_divs('w3-margin-top', 'w3-container',
			'<label><b>Reason HTML preview</b></label>',
			w3_divs('', 'id-reason-disabled-preview w3-text-black w3-background-pale-aqua', '')
		);
	
	var s3 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('w3-restart', '',
				w3_input_get_param('Inactivity time limit (min, 0 = no limit)', 'inactivity_timeout_mins', 'admin_int_zero_cb'),
				w3_divs('', 'w3-text-black', 'Connections from the local network are exempt.')
			),
			w3_divs('w3-restart', '',
				w3_input_get_param('24hr per-IP addr time limit (min, 0 = no limit)', 'ip_limit_mins', 'admin_int_zero_cb'),
				w3_divs('', 'w3-text-black', 'Connections from the local network are exempt.')
			),
			w3_divs('', '',
				w3_input_get_param('Time limit exemption password', 'tlimit_exempt_pwd', 'w3_string_set_cfg_cb'),
				w3_divs('', 'w3-text-black', 'Password users can give to override time limits.')
			)
		);

   return w3_divs('id-control w3-text-teal w3-hide', '', s1 + (admin_sdr_mode? (s2 + s3) : ''));
}

function control_focus()
{
	w3_el('id-reason-disabled-preview').innerHTML = admin_preview_status_box(cfg.reason_disabled);
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
	w3_el('id-reason-disabled-preview').innerHTML = admin_preview_status_box(cfg.reason_disabled);
}


////////////////////////////////
// connect
////////////////////////////////

// cfg.{sdr_hu_dom_sel, sdr_hu_dom_name, sdr_hu_dom_ip, server_url}
// for now server_url and sdr_hu_dom_name are kept equal when changed

var connect_dom_sel = { NAM:0, DUC:1, PUB:2, SIP:3, REV:4 };
var duc_update_i = { 0:'5 min', 1:'10 min', 2:'15 min', 3:'30 min', 4:'60 min' };
var duc_update_v = { 0:5, 1:10, 2:15, 3:30, 4:60 };

function connect_html()
{
   // transition code: default cfg.sdr_hu_dom_name with previous value of cfg.server_url
   if (ext_get_cfg_param('sdr_hu_dom_sel') == null) {
      ext_set_cfg_param('sdr_hu_dom_sel', 0);
      var server_url = ext_get_cfg_param('server_url');
      if (server_url == 'kiwisdr.example.com') {
         server_url = '';
	      ext_set_cfg_param('cfg.server_url', server_url, true);
      }
      ext_set_cfg_param('sdr_hu_dom_name', server_url, true);
   }

   var ci = 0;
   var s1 =
		w3_div('w3-vcenter',
			'<header class="w3-container w3-yellow"><h5>' +
			'If you are not able to make an incoming connection from the Internet to your Kiwi because ' +
			'of problems <br> with your router or Internet Service Provider (ISP) then please consider using the KiwiSDR ' +
			'<a href="http://proxy.kiwisdr.com" target="_blank">reverse proxy service</a>.' +
			'</h5></header>'
		) +
		
      '<hr>' +
		w3_divs('id-warn-ip w3-vcenter w3-margin-B-8 w3-hide', '', '<header class="w3-container w3-yellow"><h5>' +
			'Warning: Using an IP address in the Kiwi connect name will work, but if you switch to using a domain name later on<br>' +
			'this will cause duplicate entries on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a> ' +
			'See <a href="http://kiwisdr.com/quickstart#id-sdr_hu-dup" target="_blank">kiwisdr.com/quickstart</a> for more information.' +
			'</h5></header>'
		) +
		
      w3_divs('w3-container', 'w3-tspace-8',
         w3_label('', 'What domain name or IP address will people use to connect to your KiwiSDR?<br>' +
            'If you are listing on sdr.hu this information will be part of your entry.<br>' +
            'Click one of the five options below and enter any additional information:<br><br>'),
         
         // (n/a anymore) w3-static because w3-sidenav does a position:fixed which is no good here at the bottom of the page
         // w3-left to get float:left to put the input fields on the right side
         // w3-sidenav-full-height to match the height of the w3_input() on the right side
		   '<nav class="id-admin-nav-dom w3-sidenav w3-static w3-left w3-border w3-sidenav-full-height w3-margin-R-16 w3-light-grey">' +
		      w3_nav(admin_colors[ci++], 'Domain Name', 'connect_dom_nam', 'connect_dom_nam', (cfg.sdr_hu_dom_sel == connect_dom_sel.NAM)) +
		      w3_nav(admin_colors[ci++], 'DUC Domain', 'connect_dom_duc', 'connect_dom_duc', (cfg.sdr_hu_dom_sel == connect_dom_sel.DUC)) +
		      w3_nav(admin_colors[ci++], 'Reverse Proxy', 'connect_dom_rev', 'connect_dom_rev', (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)) +
		      w3_nav(admin_colors[ci++], 'Public IP', 'connect_dom_pub', 'connect_dom_pub', (cfg.sdr_hu_dom_sel == connect_dom_sel.PUB)) +
		      w3_nav(admin_colors[ci++], 'Specified IP', 'connect_dom_sip', 'connect_dom_sip', (cfg.sdr_hu_dom_sel == connect_dom_sel.SIP)) +
		   '</nav>',
		   
		   w3_divs('', 'w3-padding-L-16',
            w3_div('w3-show-inline-block|width:70%;', w3_input_get_param('', 'sdr_hu_dom_name', 'connect_dom_name_cb', '',
               'Enter domain name that you will point to Kiwi public IP address, e.g. kiwisdr.my_domain.com '+
               '(don\'t include port number)')),
            w3_div('id-connect-duc-dom w3-padding-TB-8'),
            w3_div('id-connect-rev-dom w3-padding-TB-8'),
            w3_div('id-connect-pub-ip w3-padding-TB-8'),
            w3_div('w3-show-inline-block|width:70%;', w3_input_get_param('', 'sdr_hu_dom_ip', 'connect_dom_ip_cb', '',
               'Enter known public IP address of the Kiwi (don\'t include port number)'))
         ),
         
		   w3_div('w3-margin-T-16', 
            w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', '', 'connect-url-text') +
			   w3_div('id-connect-url w3-show-inline-block w3-text-black w3-background-pale-aqua')
         )
      );

   var s2 =
		'<hr>' +
		w3_div('',
         w3_div('w3-container w3-vcenter',
            '<header class="w3-container w3-yellow"><h6>' +
            'Please read these instructions before use: ' +
            '<a href="http://kiwisdr.com/quickstart/index.html#id-net-duc" target="_blank">dynamic DNS update client (DUC)</a>' +
            '</h6></header>'
         ),

			w3_col_percent('w3-text-teal', 'w3-container',
			   w3_div('w3-text-teal w3-bold', 'Dynamic DNS update client (DUC) configuration'), 50,
				w3_div('w3-text-teal w3-bold w3-center w3-light-grey', 'Account at noip.com'), 50
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_div(), 50,
				w3_input_get_param('Username or email', 'adm.duc_user', 'w3_string_set_cfg_cb', '', 'required'), 25,
				w3_input_get_param('Password', 'adm.duc_pass', 'w3_string_set_cfg_cb', '', 'required'), 25
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_div('w3-center',
					'<b>Enable DUC at startup?</b><br>' +
					w3_divs('', '',
						w3_switch('', 'Yes', 'No', 'adm.duc_enable', adm.duc_enable, 'connect_DUC_enabled_cb')
					)
				), 20,
				
				w3_div('w3-center',
				   w3_select('', 'Update', '', 'adm.duc_update', adm.duc_update, duc_update_i, 'admin_select_cb')
				), 10,
				
				w3_divs('', 'w3-center w3-tspace-8',
					w3_button('w3-aqua', 'Click to (re)start DUC', 'connect_DUC_start_cb'),
					w3_divs('', 'w3-text-black',
						'After changing username or password click to test changes.'
					)
				), 20,
				
				w3_input_get_param('Host', 'adm.duc_host', 'connect_DUC_host_cb', '', 'required'), 50
			),
			
			w3_div('w3-container',
            w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'Status:') +
				w3_div('id-net-duc-status w3-show-inline-block w3-text-black w3-background-pale-aqua', '')
			)
		);

   var s3 =
		'<hr>' +
		w3_div('',
         w3_div('w3-container w3-vcenter',
            '<header class="w3-container w3-yellow"><h6>' +
            'Please read these instructions before use: ' +
            '<a href="http://proxy.kiwisdr.com" target="_blank">reverse proxy service</a>' +
            '</h6></header>'
         ),

			w3_col_percent('w3-text-teal', 'w3-container',
			   w3_div('w3-text-teal w3-bold', 'Reverse proxy configuration'), 50,
				w3_div('w3-text-teal w3-bold w3-center w3-light-grey', 'Account at kiwisdr.com'), 50
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_div(), 50,
				w3_input_get_param('User key', 'adm.rev_user', 'w3_string_set_cfg_cb', '', 'required'), 50
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_divs('', 'w3-center w3-tspace-8',
					w3_button('w3-aqua', 'Click to (re)register', 'connect_rev_register_cb'),
					w3_divs('', 'w3-text-black',
						'After changing user key or<br>host name click to register proxy.'
					)
				), 50,
				
				w3_div('',
               w3_div('w3-show-inline-block|width:40%;', w3_input_get_param('Host name', 'adm.rev_host', 'connect_rev_host_cb', '', 'required')) +
               w3_div('id-connect-rev-url w3-show-inline-block', '.proxy.kiwisdr.com')
            ), 50
			),
			
			w3_div('w3-container',
            w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'Status:') +
				w3_div('id-connect-rev-status w3-show-inline-block w3-text-black w3-background-pale-aqua', '')
			)
		) +
		'<hr>';

	return w3_divs('id-connect w3-text-teal w3-hide', '', s1 + s2 + s3);
}

function connect_focus()
{
   connect_update_url();
	ext_send('SET DUC_status_query');
	ext_send('SET rev_status_query');
}

var connect_rev_server = -1;

function connect_update_url()
{
	w3_el('id-connect-duc-dom').innerHTML = 'Use domain name from DUC configuration below: ' +
	   w3_div('w3-show-inline-block w3-text-black w3-background-pale-aqua',
	      ((adm.duc_host && adm.duc_host != '')? adm.duc_host : '(none currently set)')
	   );

   var server = (connect_rev_server == -1)? '' : connect_rev_server;
   var server_url = '.proxy'+ server +'.kiwisdr.com';
	w3_el('id-connect-rev-dom').innerHTML = 'Use domain name from reverse proxy configuration below: ' +
	   w3_div('w3-show-inline-block w3-text-black w3-background-pale-aqua',
	      ((adm.rev_host && adm.rev_host != '')? (adm.rev_host + server_url) : '(none currently set)')
	   );
	w3_el('id-connect-rev-url').innerHTML = server_url;

	w3_el('id-connect-pub-ip').innerHTML = 'Public IP address detected by Kiwi: ' +
	   w3_div('w3-show-inline-block w3-text-black w3-background-pale-aqua',
	      (config_net.pub_ip? config_net.pub_ip : '(no public IP address detected)')
	   );

   var host = decodeURIComponent(cfg.server_url);
   var host_and_port = host;
   
   if (cfg.sdr_hu_dom_sel != connect_dom_sel.REV) {
      host_and_port += ':'+ config_net.pub_port;
      w3_set_label('Based on above selection, and external port from Network tab, the URL to connect to your Kiwi is:', 'connect-url-text');
   } else {
      host_and_port += ':8073';
      w3_set_label('Based on above selection the URL to connect to your Kiwi is:', 'connect-url-text');
   }
   
   w3_el('id-connect-url').innerHTML =
      (host == '')? '(incomplete information)' : ('http://'+ host_and_port);
}

function connect_dom_nam_focus()
{
   console.log('connect_dom_nam_focus server_url='+ cfg.sdr_hu_dom_name);
	ext_set_cfg_param('cfg.server_url', cfg.sdr_hu_dom_name, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.NAM, true);
	connect_update_url();
   w3_hide('id-warn-ip');
}

function connect_dom_duc_focus()
{
   console.log('connect_dom_duc_focus server_url='+ adm.duc_host);
	ext_set_cfg_param('cfg.server_url', adm.duc_host, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.DUC, true);
	connect_update_url();
   w3_hide('id-warn-ip');
}

function connect_dom_rev_focus()
{
   var server = (connect_rev_server == -1)? '' : connect_rev_server;
   var dom = (adm.rev_host == '')? '' : adm.rev_host + '.proxy'+ server +'.kiwisdr.com';
   console.log('connect_dom_rev_focus server_url='+ dom);
	ext_set_cfg_param('cfg.server_url', dom, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.REV, true);
	connect_update_url();
   w3_hide('id-warn-ip');
}

function connect_dom_pub_focus()
{
   console.log('connect_dom_pub_focus server_url='+ config_net.pub_ip);
	ext_set_cfg_param('cfg.server_url', config_net.pub_ip, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.PUB, true);
	connect_update_url();
	//w3_show_block('id-warn-ip');
}

function connect_dom_sip_focus()
{
   console.log('connect_dom_sip_focus server_url='+ cfg.sdr_hu_dom_ip);
	ext_set_cfg_param('cfg.server_url', cfg.sdr_hu_dom_ip, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.SIP, true);
	connect_update_url();
	//w3_show_block('id-warn-ip');
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
	var sl = s.length;
	var state = { bad:0, number:1, alpha:2, remove:3 };
	var st = state.bad;
	
	s = s.replace('http://', '');
	
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
	var s = '-u '+ q(decodeURIComponent(adm.duc_user)) +' -p '+ q(decodeURIComponent(adm.duc_pass)) +
	   ' -H '+ q(decodeURIComponent(adm.duc_host)) +' -U '+ duc_update_v[adm.duc_update];
	console.log('start DUC: '+ s);
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

function connect_rev_register_cb(id, idx)
{
   if (adm.rev_user == '' || adm.rev_host == '')
      return connect_rev_status_cb(100);
   
	w3_el('id-connect-rev-status').innerHTML = '';
	var s = 'user='+ adm.rev_user +' host='+ adm.rev_host;
	console.log('start rev: '+ s);
	ext_send('SET rev_register '+ s);
}

function connect_rev_host_cb(path, val, first)
{
   w3_string_set_cfg_cb(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)     // if currently selected option update the value
      connect_dom_rev_focus();
   else
      connect_update_url();
}

function connect_rev_status_cb(status)
{
	status = +status;
	console.log('rev_status='+ status);
	var s;
	
	if (status >= 0 && status <= 99) {
	   // jks-proxy
      //connect_rev_server = status & 0xf;
      //status = status >> 4;
      connect_dom_rev_focus();
   }
	
	switch (status) {
		case   0: s = 'Existing account, registration successful'; break;
		case   1: s = 'New account, registration successful'; break;
		case   2: s = 'Updating host name, registration successful'; break;
		case 100: s = 'User key or host name field blank'; break;
		case 101: s = 'User key invalid; please contact support@kiwisdr.com'; break;
		case 102: s = 'Host name already in use; please choose another and retry'; break;
		case 103: s = 'Invalid characters in user key or host name field (use a-z, 0-9, -, _)'; break;
		case 200: s = 'Reverse proxy enabled and running'; break;
		case 900: s = 'Problem contacting proxy.kiwisdr.com; please check Internet connection'; break;
		default:  s = 'Reverse proxy internal error: '+ status; break;
	}
	
	w3_el('id-connect-rev-status').innerHTML = s;
}


////////////////////////////////
// network
////////////////////////////////

function network_html()
{
	var s1 =
		w3_divs('id-net-auto-nat w3-vcenter w3-hide', '',
			'<header class="w3-container"><h5 id="id-net-auto-nat-msg">Automatic add of NAT rule on firewall / router: </h5></header>'
		) +

		w3_divs('id-net-need-update w3-vcenter w3-margin-T-8 w3-hide', '',
			w3_button('w3-aqua', 'Are you sure? Click to update interface DHCP/static IP configuration', 'network_dhcp_static_update_cb')
		) +

		'<hr>' +
		w3_divs('id-net-reboot', '',
			w3_col_percent('w3-container w3-margin-bottom w3-text-teal', 'w3-hspace-16',
				w3_divs('', 'w3-restart',
					w3_input_get_param('Internal port', 'adm.port', 'admin_int_null_cb')
				), 24,
				w3_divs('', 'w3-restart',
					w3_input_get_param('External port', 'adm.port_ext', 'admin_int_null_cb')
				), 24,
				w3_divs('w3-center', 'w3-restart',
					'<b>Auto add NAT rule<br>on firewall / router?</b><br>',
					w3_divs('', '',
						w3_switch('', 'Yes', 'No', 'adm.auto_add_nat', adm.auto_add_nat, 'admin_radio_YN_cb')
					)
				), 24,
				w3_divs('w3-center', '',
						'<b>IP address<br>(only static IPv4 for now)</b><br> ' +
						w3_radio_button_get_param('', 'DHCP', 'adm.ip_address.use_static', 0, false, 'network_use_static_cb') +
						w3_radio_button_get_param('', 'Static', 'adm.ip_address.use_static', 1, false, 'network_use_static_cb')
				), 24
			),
			w3_divs('id-net-static w3-hide', '',
				w3_third('w3-margin-B-8 w3-text-teal', 'w3-container',
					w3_input_get_param('IP address (n.n.n.n where n = 0..255)', 'adm.ip_address.ip', 'network_ip_address_cb', ''),
					w3_input_get_param('Netmask (n.n.n.n where n = 0..255)', 'adm.ip_address.netmask', 'network_netmask_cb', ''),
					w3_input_get_param('Gateway (n.n.n.n where n = 0..255)', 'adm.ip_address.gateway', 'network_gw_address_cb', '')
				),
				w3_third('w3-margin-B-8 w3-text-teal', 'w3-container',
					w3_divs('id-network-check-ip w3-green', ''),
					w3_divs('id-network-check-nm w3-green', ''),
					w3_divs('id-network-check-gw w3-green', '')
				),
				w3_third('w3-vcenter w3-margin-bottom w3-text-teal', 'w3-container',
					w3_input_get_param('DNS-1 (n.n.n.n where n = 0..255)', 'adm.ip_address.dns1', 'w3_string_set_cfg_cb', ''),
					w3_input_get_param('DNS-2 (n.n.n.n where n = 0..255)', 'adm.ip_address.dns2', 'w3_string_set_cfg_cb', ''),
					w3_div('',
                  w3_label('', '<br>') +     // makes the w3-vcenter above work for button below
                  w3_button('w3-show-inline w3-aqua', 'Use Google public DNS', 'net_google_dns_cb')
               )
				)
			)
		);
	
	var s2 =
		'<hr>' +
		w3_divs('id-net-config w3-container', '') +

		'<hr>' +
		w3_divs('', 'w3-container',
		   w3_div('', 
            w3_label('w3-show-inline w3-text-teal', 'Check if your external router port is open:') +
            w3_button('w3-show-inline w3-aqua|margin-left:10px', 'Check port open', 'net_port_open_cb')
         ),
         //'Does kiwisdr.com receive a correct reply when checking these URLs used to reach your Kiwi:',
         'Does kiwisdr.com successfully connect to your Kiwi using these URLs?<br>' +
         'If both respond "NO" then check the NAT port mapping on your router.<br>' +
         'If first responds "NO" and second "YES" then domain name of first isn\'t resolving to the ip address of the second. Check DNS.',
		   w3_div('', 
            w3_label('id-net-check-port-dom-q w3-show-inline-block w3-margin-LR-16 w3-text-teal') +
			   w3_div('id-net-check-port-dom-s w3-show-inline-block w3-text-black w3-background-pale-aqua')
         ),
		   w3_div('', 
            w3_label('id-net-check-port-ip-q w3-show-inline-block w3-margin-LR-16 w3-text-teal') +
			   w3_div('id-net-check-port-ip-s w3-show-inline-block w3-text-black w3-background-pale-aqua')
         )
      );

   var s3 =
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: throttle #chan MB/dy GB/mo, hostname') +
		'<hr>';

	// FIXME replace this with general instantiation call from w3_input()
	setTimeout(function() {
		var use_static = ext_get_cfg_param('adm.ip_address.use_static', false);
		network_use_static_cb('adm.ip_address.use_static', use_static, /* first */ true);
	}, 500);
	
	return w3_divs('id-network w3-hide', '', s1 + s2 + s3);
}

function network_focus()
{
	w3_el('id-net-check-port-dom-q').innerHTML =
	   (cfg.server_url != '')?
	      'http://'+ cfg.server_url +':'+ config_net.pub_port +' :' :
	      '(incomplete information) :';
	w3_el('id-net-check-port-ip-q').innerHTML =
	   'http://'+ config_net.pvt_ip +':'+ config_net.pub_port +' :';
   w3_el('id-net-check-port-dom-s').innerHTML = '';
   w3_el('id-net-check-port-ip-s').innerHTML = '';
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
	if (adm.ip_address.use_static) {
		ext_send('SET static_ip='+ kiwi_ip_str(network_ip) +' static_nm='+ kiwi_ip_str(network_nm) +' static_gw='+ kiwi_ip_str(network_gw));
		ext_send('SET dns dns1=x'+ encodeURIComponent(adm.ip_address.dns1) +' dns2=x'+ encodeURIComponent(adm.ip_address.dns2));
	} else {
		ext_send('SET use_DHCP');
	}

	w3_hide('id-net-need-update');
	w3_reboot_cb();		// show reboot button after confirm button pressed
}

function network_use_static_cb(path, idx, first)
{
	idx = +idx;
	//console.log('network_use_static_cb idx='+ idx);
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

function net_google_dns_cb(id, idx)
{
	w3_string_set_cfg_cb('adm.ip_address.dns1', '8.8.8.8');
	w3_set_value('adm.ip_address.dns1', '8.8.8.8');
	w3_string_set_cfg_cb('adm.ip_address.dns2', '8.8.4.4');
	w3_set_value('adm.ip_address.dns2', '8.8.4.4');
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
		w3_divs('', 'w3-margin-bottom',
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
         w3_half('w3-container', 'w3-text-teal',
            w3_div('',
               w3_select('w3-label-inline', 'After update', '', 'adm.update_restart', adm.update_restart, update_restart_u, 'admin_select_cb')
            ),
            ''
         )
		) +
		'<hr>' +
		w3_half('w3-container', 'w3-text-teal',
			w3_divs('w3-vcenter', '',
				'<b>Check for software update </b> ' +
				w3_button('w3-aqua w3-margin', 'Check now', 'update_check_now_cb')
			),
			w3_divs('w3-vcenter', '',
				'<b>Force software build </b> ' +
				w3_button('w3-aqua w3-margin', 'Build now', 'update_build_now_cb')
			)
		) +
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: alt github name') +
		'<hr>'
	);
	return s;
}

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
	w3_divs('id-backup w3-hide', '',
		'<hr>',
		w3_div('w3-section w3-text-teal w3-bold', 'Backup complete contents of KiwiSDR by writing Beagle filesystem onto a user provided micro-SD card'),
		w3_div('w3-container w3-text w3-red', 'WARNING: after SD card is written immediately remove from Beagle.<br>Otherwise on next reboot Beagle will be re-flashed from SD card.'),
		'<hr>',
		w3_third('w3-container', 'w3-vcenter',
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
		'<div id="id-output-msg" class="w3-container w3-text-output w3-scroll-down w3-small w3-margin-B-16"></div>'
	);
	return s;
}

function backup_focus()
{
	w3_el('id-progress-container').style.width = px(300);
	w3_el('id-output-msg').style.height = px(300);
}

var sd_progress, sd_progress_max = 4*60;		// measured estimate -- in secs (varies with SD card write speed)
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
	w3_el('id-progress-time').innerHTML =
		((sd_progress / 60) % 60).toFixed(0) +':'+ (sd_progress % 60).toFixed(0).leadingZeros(2);
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
// GPS
//		tracking tasks aren't stopped when !enabled
////////////////////////////////

var _gps = {
   RSSI:0, AZEL:1, POS:2, MAP:3, IQ:4,
   IQ_data: null,
   iq_ch: 0,
   gmap_init: 0,
   gmap_needs_height: 0,
   gmap_locate: 0,
   gmap_mkr: [],
   legend_sep: 'Pins: green = Navstar/QZSS only, red = all sats, yellow = Galileo only',
   legend_all: 'Pins: green = all sats (Navstar/QZSS/Galileo)'
};

var E1B_offset_i = { 0:'-1', 1:'-3/4', 2:'-1/2', 3:'-1/4', 4:'0', 5:'+1/4', 6:'+1/2', 7:'+3/4', 8:'+1' };

function gps_html()
{
	var s =
	w3_divs('id-gps w3-hide', '',
	   w3_col_percent('w3-vcenter', '',
         w3_div('w3-section w3-container w3-vcenter w3-text-teal',
            w3_div('w3-show-inline w3-margin-right', '<b>Enable<br>GPS?</b>') +
            w3_switch('', 'Yes', 'No', 'adm.enable_gps', adm.enable_gps, 'admin_radio_YN_cb')
         ), 15,

         w3_div('w3-section w3-container w3-vcenter w3-text-teal',
            w3_div('w3-show-inline w3-margin-right', '<b>Always<br>acquire?</b>') +
            w3_switch('w3-show-inline', 'Yes', 'No', 'adm.always_acq_gps', adm.always_acq_gps, 'admin_radio_YN_cb')
         ), 15,

         w3_div('w3-section w3-container w3-vcenter w3-text-teal',
            w3_div('w3-show-inline w3-margin-right', '<b>Include<br>alerted?</b>') +
            w3_switch('w3-show-inline', 'Yes', 'No', 'adm.include_alert_gps', adm.include_alert_gps, 'admin_radio_YN_cb')
         ), 15,

         w3_div('w3-section w3-container w3-vcenter w3-text-teal',
            w3_div('w3-show-inline w3-margin-right', '<b>Plot<br>Galileo?</b>') +
            w3_switch('w3-show-inline', 'Yes', 'No', 'adm.plot_E1B', adm.plot_E1B, 'admin_radio_YN_cb')
         ), 15,

         /*
         w3_div('w3-section w3-container w3-vcenter w3-text-teal',
            w3_div('w3-show-inline w3-margin-right', '<b>Include<br>Galileo?</b>') +
            w3_switch('w3-show-inline', 'Yes', 'No', 'adm.include_E1B', adm.include_E1B, 'admin_radio_YN_cb')
         ), 15,
         */

         /*
         w3_div('w3-section w3-container w3-vcenter w3-text-teal',
            w3_div('w3-show-inline w3-margin-right', '<b>E1B<br>offset</b>') +
				w3_select('|color:red', '', 'chips', 'adm.E1B_offset', adm.E1B_offset, E1B_offset_i, 'gps_E1B_offset_cb')
         ), 15,
         */

         /*
         w3_div('w3-section w3-container w3-vcenter w3-text-teal',
            w3_div('w3-show-inline w3-margin-right', '<b>E1B<br>gain</b>') +
            w3_select('w3-margin-L-5|color:red', '', '', '_gps.gain', 0, '1:12', 'gps_gain_cb')
         ), 15,
         */

         w3_div('w3-section w3-container w3-vcenter w3-hcenter w3-text-teal',
            w3_div('w3-margin-right', '<b>Graph</b>') +
            w3_radio_button('w3-margin-R-4', 'RSSI', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.RSSI, 'gps_graph_cb'),
            w3_radio_button('w3-margin-R-4', 'Az/El', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.AZEL, 'gps_graph_cb'),
            w3_radio_button('w3-margin-R-4', 'Pos', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.POS, 'gps_graph_cb'),
            w3_radio_button('w3-margin-R-4', 'Map', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.MAP, 'gps_graph_cb'),
            w3_radio_button('', 'IQ', 'adm.rssi_azel_iq', adm.rssi_azel_iq == _gps.IQ, 'gps_graph_cb')
         ), 33,

         w3_divs('w3-section w3-container w3-hcenter w3-text-teal', 'w3-center',
            w3_div('id-gps-pos-scale w3-center',
               '<b>Scale</b> ',
               w3_select('w3-margin-L-5|color:red', '', '', '_gps.pos_scale', 10-1, '1:20', 'gps_pos_scale_cb')
            ),
            w3_div('id-gps-iq-ch w3-center',
               '<b>Chan</b> ',
               w3_select('w3-margin-L-5|color:red', '', '', '_gps.iq_ch', 0, '1:12', 'gps_iq_ch_cb')
            )
         ), 7
      ) +

	   w3_div('w3-vcenter',
         w3_div('id-gps-channels w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue|width:100%',
            w3_table('id-gps-ch w3-table w3-striped')
         ),
         w3_div('id-gps-gmap',
            w3_div('id-gps-map', ''),
            w3_div('id-gps-map-legend w3-small w3-margin-left', '')
         )
		) +

		w3_divs('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue', '',
			w3_table('id-gps-info w3-table')
		)
	);
	return s;
}

function gps_graph_cb(id, idx)
{
   idx = +idx;
   //console.log('gps_graph_cb idx='+ idx);
   admin_int_zero_cb(id, idx);
   ext_send('SET gps_IQ_data_ch='+ ((idx == _gps.IQ)? _gps.iq_ch:0));

   w3_show_hide('id-gps-pos-scale', idx == _gps.POS);
   w3_show_hide('id-gps-iq-ch', idx == _gps.IQ);
   
   // id-gps-channels and id-gps-gmap are separated like this because the id-gps-ch inside id-gps-channels
   // is being updated all the time (@ 1 Hz) and we don't want the gmap to be effected by this.
   // But the id-gps-gmap needs to have its height set somehow. So we set the gmap_needs_height flag and
   // let the height be set id-gps-ch the next time it is rendered.
   
   var el_ch = w3_el('id-gps-channels');
   if (idx == _gps.MAP) {
      el_ch.style.width = '65%';
      w3_el('id-gps-gmap').style.width = '35%';
      _gps.gmap_needs_height = 1;
   } else {
      el_ch.style.width = '100%';
   }
   w3_show_hide('id-gps-gmap', idx == _gps.MAP);
   
   gps_update_admin_cb();     // redraw immediately to keep display from glitching
}

function gps_E1B_offset_cb(path, idx, first)
{
   idx = +idx;
	if (idx == -1)
	   idx = 4;
	console.log('gps_E1B_offset_cb idx='+ idx +' path='+ path+' first='+ first);
}

function gps_pos_scale_cb(path, idx, first)
{
   idx = +idx;
	if (idx == -1 || first)
	   idx = 10-1;
	_gps.pos_scale = idx + 1;
	console.log('gps_pos_scale_cb idx='+ idx +' path='+ path+' first='+ first +' pos_scale='+ _gps.pos_scale);
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

function gps_kick_pll_cb(id, ch)
{
	console.log('gps_kick_pll_cb ch='+ ch);
	ext_send('SET gps_kick_pll_ch='+ ch);
}

function gps_gain_cb(path, idx, first)
{
   idx = +idx;
	if (idx == -1 || first)
	   idx = 0;
	_gps.gain = idx + 1;
	console.log('gps_gain_cb idx='+ idx +' path='+ path+' first='+ first +' gain='+ _gps.gain);
   ext_send('SET gps_gain='+ _gps.gain);
}

var gps_interval, gps_azel_interval;
var gps_has_lat_lon, gps_az_el_history_running = false;

function gps_schedule_azel(focus)
{
   var launch = false;
   
   if (focus) {
      gps_az = gps_el = null;    // don't display az/el until we get fresh data
      if (gps && gps.lat) launch = true;
   } else {
      if (!gps.lat) {
         gps_has_lat_lon = false;
      } else
      if (!gps_has_lat_lon && gps.lat) {
         gps_has_lat_lon = true;
         launch = true;
      }
   }
   
   if (launch && gps_az_el_history_running == false) {
      gps_az_el_history_running = true;
      ext_send("SET gps_az_el_history");
      gps_azel_interval = setInterval(function() {ext_send("SET gps_az_el_history");}, 60000);
   }
}

function gps_focus(id)
{
   gps_schedule_azel(true);
   
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
   gps_shadow_map = obj.shadow_map.slice();
   //for (var az=0; az<90; az++) gps_shadow_map[az] = (az < 45)? 0x0000ffff:0xffff0000;
}

var SUBFRAMES = 5;
var max_rssi = 1;

var refresh_icon = w3_icon('', 'fa-refresh', 20);

var sub_colors = [ 'w3-red', 'w3-green', 'w3-blue', 'w3-yellow', 'w3-orange' ];

var gps_canvas;
var gps_last_good_el = [];
var gps_rssi_azel_iq_s = [ 'RSSI', 'Azimuth/elevation shadow map', 'Position solution map', 'Google map', 'LO PLL IQ' ];

function gps_update_admin_cb()
{
   if (!gps) return;

	var s;
	
	s =
		w3_table_row('',
			w3_table_heads('w3-right-align', 'chan', 'acq', '&nbsp;PRN', 'SNR', 'gain', 'hold', 'wdog'),
			w3_table_heads('w3-center', 'status', 'subframe'),
			w3_table_heads('w3-right-align', 'ov', 'az', 'el'),
			(adm.rssi_azel_iq == _gps.RSSI)? null : w3_table_heads('w3-right-align', 'RSSI'),
         (adm.rssi_azel_iq == _gps.IQ)? w3_table_heads('w3-center', 'PLL') : null,
         (adm.rssi_azel_iq == _gps.MAP)? null : 
            w3_table_heads('w3-center|width:35%',
               ((adm.rssi_azel_iq == _gps.IQ && _gps.iq_ch)? ('Channel '+ _gps.iq_ch +' ') : '') + gps_rssi_azel_iq_s[adm.rssi_azel_iq]
            )
		);
	
      for (var cn=0; cn < gps.ch.length; cn++) {
         s += w3_table_row('id-gps-ch-'+ cn, '');
      }

	w3_el("id-gps-ch").innerHTML = s;
	
	soln_color = (gps.soln == 0)? 'w3-green' : ((gps.soln == 1)? 'w3-yellow':'w3-red');

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
      
		//var ch_soln_color = (gps.sep && ch.prn_s == 'E')? 'w3-yellow' : soln_color;
		var ch_soln_color = soln_color;

      var unlock = ch.alert? 'A' : ((ch.ACF == 1)? '+' : ((ch.ACF == 2)? '-':'U'));
	
		var cells =
			w3_table_cells('w3-right-align', cn+1) +
			w3_table_cells('w3-center', (cn == gps.FFTch)? refresh_icon:'') +
			w3_table_cells('w3-right-align',
				prn? (prn_pre + prn):'',
				ch.snr? ch.snr:'',
				ch.rssi? ch.gain:'',
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
		for (var i = SUBFRAMES-1; i >= 0; i--) {
			var sub_color;
			if (ch.sub_renew & (1<<i)) {
				sub_color = 'w3-grey';
			} else {
				sub_color = (ch.sub & (1<<i))? sub_colors[i]:'w3-white';
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

      if (adm.rssi_azel_iq == _gps.IQ) {
         cells +=
            w3_table_cells('w3-right-align',
               (cn >= 4)? '' :
               w3_button('id-kick-pll-'+ cn +' w3-small w3-aqua w3-round-large w3-padding-0 w3-padding-LR-8', 'Kick', 'gps_kick_pll_cb', cn)
            );
      }
      
	   if (adm.rssi_azel_iq == _gps.RSSI) {
         var pct = ((ch.rssi / max_rssi) * 100).toFixed(0);
         cells +=
            w3_table_cells('',
               w3_div('w3-progress-container w3-round-xlarge w3-white',
                  w3_div('w3-progressbar w3-round-xlarge w3-light-green|width:'+ pct +'%',
                     w3_div('w3-container w3-text-white', ch.rssi)
                  )
               )
            );
      } else
	   if (adm.rssi_azel_iq != _gps.RSSI || adm.rssi_azel_iq != _gps.MAP) {
         if (cn == 0) {
            cells +=
               w3_table_cells('|position:relative;|rowspan='+ gps.ch.length,
                  w3_div('w3-hcenter',
                     ((adm.rssi_azel_iq == _gps.AZEL)?
                     '<img id="id-gps-azel-graph" src="gfx/gpsEarth.png" width="400" height="400" style="position:absolute" />':'') +
                     '<canvas id="id-gps-azel-canvas" width="400" height="400" style="position:absolute"></canvas>'
                  )
               );
         }
      }

		w3_el('id-gps-ch-'+ cn).innerHTML = cells;
	}

	s =
		w3_table_row('',
			w3_table_heads('', 'acq', 'tracking', 'good', 'fixes', 'run', 'TTFF', 'UTC offset',
				'ADC clock', 'lat', 'lon', 'alt', 'map')
		) +
		
		w3_table_row('',
			w3_table_cells('',
				gps.acq? 'yes':'paused',
				gps.track? gps.track:'',
				gps.good? gps.good:'',
				gps.fixes? gps.fixes.toUnits():'',
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

   gps_canvas = w3_el('id-gps-azel-canvas');
   if (gps_canvas == null) return;
   gps_canvas.ctx = gps_canvas.getContext("2d");
   var ctx = gps_canvas.ctx;
   
   if (adm.rssi_azel_iq == _gps.RSSI) return;

   if (adm.rssi_azel_iq == _gps.MAP) {

      if (_gps.gmap_needs_height) {
         var h = css_style_num(w3_el('id-gps-channels'), 'height');
         if (h) {
            w3_el('id-gps-map').style.height = px(h - 24);
            _gps.gmap_needs_height = 0;
         }
      }
      
      if (!_gps.gmap_init && !_gps.gmap_needs_height) {
         var latlon = new google.maps.LatLng(0, 0);
         var map_div = w3_el('id-gps-map');
         _gps.gmap = new google.maps.Map(map_div,
            {
               zoom: 1,
               center: latlon,
               navigationControl: false,
               mapTypeControl: false,
               streetViewControl: false,
               mapTypeId: google.maps.MapTypeId.SATELLITE
            });
         _gps.gmap_latlon = latlon;
         _gps.gmap_init = 1;
      }
      
      w3_innerHTML('id-gps-map-legend', adm.plot_E1B? _gps.legend_sep : _gps.legend_all);

      if (!_gps.MAP_data || !_gps.gmap_init) return;
         
      if (!_gps.gmap_locate) {
         var latlon = new google.maps.LatLng(_gps.MAP_data.ref_lat, _gps.MAP_data.ref_lon);
         _gps.gmap.panTo(latlon);
         _gps.gmap.setZoom(18);
         _gps.gmap_locate = 1;
      }
      
      var mlen = _gps.MAP_data.MAP.length;
      //console.log('mlen='+ mlen);
      for (var j=0; j < mlen; j++) {
         var mp = _gps.MAP_data.MAP[j];
         //console.log(mp);
         var latlon = new google.maps.LatLng(mp.lat, mp.lon);
         var color = (mp.nmap == 0)? 'green' : ((mp.nmap == 1)? 'red':'yellow');
         var mkr = new google.maps.Marker({
            position:latlon,
            //label: mp.nmap? 'G':'N',
            icon: 'http://maps.google.com/mapfiles/ms/icons/'+ color +'-dot.png',
            map:_gps.gmap
         });
         _gps.gmap_mkr.push(mkr);
         while (_gps.gmap_mkr.length > 12) {
            var omkr = _gps.gmap_mkr.shift();
            omkr.setMap(null);
         }
      }
      _gps.MAP_data = null;

      return;
   }

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
         if (!gps.sep && i >= len/2) break;
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

      if (gps.sep) {
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
      ctx.fillText((gps.sep? ' w/o Galileo span: ':'All sats span: ')+
         _gps.POS_data.y0span.toFixed(0).fieldWidth(4) +'m Ylat '+ _gps.POS_data.x0span.toFixed(0).fieldWidth(4) +'m Xlon', x,y);

      if (gps.sep) {
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

   gps_schedule_azel(false);

   if (adm.rssi_azel_iq != _gps.AZEL || gps_el == null) return;

   var gW = 400;
   var gD = 360;
   var gHD = gD/2;
   var gM = (gW-gD)/2;
   var gO = gHD + gM;
   
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
            ctx.lineWidth = 3;
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
	w3_divs('id-log w3-text-teal w3-hide', '',
		'<hr>'+
		w3_divs('', 'w3-container',
		   w3_div('',
            w3_label('w3-show-inline', 'KiwiSDR server log (scrollable list, first and last set of messages)') +
            w3_button('w3-aqua|margin-left:10px', 'Dump', 'log_dump_cb') +
            w3_button('w3-blue|margin-left:10px', 'Clear Histogram', 'log_clear_hist_cb')
         ),
			w3_divs('', 'id-log-msg w3-margin-T-8 w3-text-output w3-small w3-text-black', '')
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
var console_status_msg_p = { scroll_only_at_bottom: true, process_return_nexttime: false, remove_returns: true, ncol: 160 };

function console_html()
{
	var s =
	w3_divs('id-console w3-section w3-text-teal w3-hide', '',
		w3_divs('', 'w3-container',
		   w3_div('',
            w3_label('w3-show-inline', 'Beagle Debian console') +
            w3_button('w3-aqua|margin-left:10px', 'Connect', 'console_connect_cb')
         ),
			w3_divs('', 'id-console-msg w3-margin-T-8 w3-text-output w3-scroll-down w3-small w3-text-black',
			   '<pre><code id="id-console-msgs"></code></pre>'
			),
         w3_divs('w3-margin-top', '',
            w3_input('', 'console_input', '', 'console_input_cb', 'enter shell command')
         ),
		   w3_div('w3-margin-top',
            w3_button('w3-yellow', 'Send ^C', 'console_ctrl_C_cb') +
            w3_button('w3-blue|margin-left:10px', 'Send ^D', 'console_ctrl_D_cb') +
            w3_button('w3-red|margin-left:10px', 'Send ^\\', 'console_ctrl_backslash_cb')
         )
		)
	);
	return s;
}

function console_input_cb(path, val)
{
	//console.log('console_w2c='+ val);
	ext_send('SET console_w2c='+ encodeURIComponent(val +'\n'));
   w3_set_value(path, '');    // erase input field
}

function console_connect_cb()
{
	ext_send('SET console_open');
}

function console_ctrl_C_cb()
{
	ext_send('SET console_ctrl_C');
}

function console_ctrl_D_cb()
{
	ext_send('SET console_ctrl_D');
}

function console_ctrl_backslash_cb()
{
	ext_send('SET console_ctrl_backslash');
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


var admin_colors = [
	'w3-hover-red',
	'w3-hover-blue',
	'w3-hover-purple',
	'w3-hover-black',
	'w3-hover-aqua',
	'w3-hover-pink',
	'w3-hover-yellow',
	'w3-hover-khaki',
	'w3-hover-green',
	'w3-hover-orange',
	'w3-hover-grey',
	'w3-hover-lime',
	'w3-hover-indigo',
	'w3-hover-brown',
	'w3-hover-teal'
];


////////////////////////////////
// security
////////////////////////////////

// FIXME: breaks if we someday add more channels
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
						w3_switch('', 'Yes', 'No', 'adm.user_auto_login', adm.user_auto_login, 'admin_radio_YN_cb')
					)
				)
			), 25,

			w3_divs('', 'w3-center',
				w3_select('', 'Number of channels<br>not requiring a password<br>even if password set',
					'', 'chan_no_pwd', chan_no_pwd, chan_no_pwd_u, 'admin_select_cb'),
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
						w3_switch('', 'Yes', 'No', 'adm.admin_auto_login', adm.admin_auto_login, 'admin_radio_YN_cb')
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
	//w3_el('id-security-json').innerHTML = w3_divs('w3-padding w3-scroll', '', JSON.stringify(cfg));
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
	console_resize();
}

function kiwi_ws_open(conn_type, cb, cbp)
{
	return open_websocket(conn_type, cb, cbp, admin_msg, admin_recv);
}

function admin_draw(sdr_mode)
{
	var admin = w3_el("id-admin");
	var ci = 0;
	
	var s;
	if (sdr_mode)
	   s = w3_navdef(admin_colors[ci++], 'Status', 'status');
	   //s = w3_nav(admin_colors[ci++], 'Status', 'status');
	else
	   s = w3_navdef(admin_colors[ci++], 'GPS', 'gps') +
	       w3_nav(admin_colors[ci++], 'Status', 'status');
	s +=
      w3_nav(admin_colors[ci++], 'Control', 'control') +
      w3_nav(admin_colors[ci++], 'Connect', 'connect');
	if (sdr_mode)
	   s +=
         //w3_nav(admin_colors[ci++], 'Channels', 'channels') +
         w3_nav(admin_colors[ci++], 'Config', 'config') +
         w3_nav(admin_colors[ci++], 'Webpage', 'webpage') +
         w3_nav(admin_colors[ci++], 'sdr.hu', 'sdr_hu') +
         w3_nav(admin_colors[ci++], 'DX list', 'dx');
   s += 
      w3_nav(admin_colors[ci++], 'Update', 'update') +
      w3_nav(admin_colors[ci++], 'Backup', 'backup') +
      w3_nav(admin_colors[ci++], 'Network', 'network') +
      (sdr_mode? w3_nav(admin_colors[ci++], 'GPS', 'gps') : '') +
      //(sdr_mode? w3_navdef(admin_colors[ci++], 'GPS', 'gps') : '') +
      w3_nav(admin_colors[ci++], 'Log', 'log') +
      w3_nav(admin_colors[ci++], 'Console', 'console') +
      (sdr_mode? w3_nav(admin_colors[ci++], 'Extensions', 'admin-ext') : '') +
      w3_nav(admin_colors[ci++], 'Security', 'security');

	admin.innerHTML =
		w3_divs('id-admin-header-container', '',
			'<header class="w3-container w3-teal"><h5>Admin interface</h5></header>' +
			'<nav class="w3-navbar w3-border w3-light-grey">' +
			   s +
			'</nav>' +
	
			w3_divs('id-restart w3-hide', 'w3-vcenter',
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Restart required for changes to take effect</h5></header>' +
				w3_div('w3-show-inline-block', w3_button('w3-aqua w3-margin', 'KiwiSDR server restart', 'admin_restart_now_cb'))
			) +
			
			w3_divs('id-reboot w3-hide', 'w3-vcenter',
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Reboot required for changes to take effect</h5></header>' +
				w3_div('w3-show-inline-block', w3_button('w3-blue w3-margin', 'Beagle reboot', 'admin_reboot_now_cb'))
			) +
			
			w3_divs('id-build-restart w3-vcenter w3-hide', '',
				'<header class="w3-container w3-blue"><h5>Server will restart after build</h5></header>'
			) +

			w3_divs('id-build-reboot w3-vcenter w3-hide', '',
				'<header class="w3-container w3-red"><h5>Beagle will reboot after build</h5></header>'
			)
		);
	
	if (sdr_mode)
	   s = status_html();
	else
	   s = gps_html() + status_html();

	s +=
		control_html() +
		connect_html();

	if (sdr_mode)
	   s +=
         //channels_html() +
		   config_html() +
         webpage_html() +
         sdr_hu_html() +
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

	admin.innerHTML += s;
	log_setup();
	console_setup();
	stats_init();

	if (sdr_mode) {
	   users_init(true);
	   //gps_focus();
	} else {
	   gps_focus();
	}

	w3_show_block('id-admin');
	
	setTimeout(function() { setInterval(status_periodic, 5000); }, 1000);
}

// Process replies to our messages sent via ext_send('SET ...')
// As opposed to admin_recv() below that processes unsolicited messages sent from C code.

var gps = null;

function admin_msg(data)
{
   //console.log('admin_msg: '+ param[0]);
   switch (param[0]) {

		case "gps_update_cb":
			//console.log('gps_update_cb='+ param[1]);
			try {
				var gps_json = decodeURIComponent(param[1]);
				gps = JSON.parse(gps_json);
				w3_call('gps_update_admin_cb');
			} catch(ex) {
				console.log('<'+ gps_json +'>');
				console.log('kiwi_msg() gps_update_cb: JSON parse fail');
			}
			break;

		case "gps_IQ_data_cb":
			try {
				var IQ_data = decodeURIComponent(param[1]);
				_gps.IQ_data = JSON.parse(IQ_data);
			   //console.log('gps_IQ_data_cb ch='+ _gps.IQ_data.ch +' len='+ _gps.IQ_data.IQ.length);
			   //console.log(_gps.IQ_data);
			} catch(ex) {
				console.log('<'+ IQ_data +'>');
				console.log('kiwi_msg() gps_IQ_data_cb: JSON parse fail');
			}
			break;

      case "gps_POS_data_cb":
         try {
            var POS_data = decodeURIComponent(param[1]);
            _gps.POS_data = JSON.parse(POS_data);
            //console.log('gps_POS_data_cb len='+ _gps.POS_data.POS.length);
            //console.log(_gps.POS_data);
         } catch(ex) {
            console.log('<'+ POS_data +'>');
            console.log('kiwi_msg() gps_POS_data_cb: JSON parse fail');
         }
         break;

      case "gps_MAP_data_cb":
         try {
            var MAP_data = decodeURIComponent(param[1]);
            _gps.MAP_data = JSON.parse(MAP_data);
            //console.log('gps_MAP_data_cb len='+ _gps.MAP_data.MAP.length);
            //console.log(_gps.MAP_data);
         } catch(ex) {
            console.log('<'+ MAP_data +'>');
            console.log('kiwi_msg() gps_MAP_data_cb: JSON parse fail');
         }
         break;

      case "gps_az_el_history_cb":
         try {
            var gps_az_el_json = decodeURIComponent(param[1]);
            w3_call('gps_az_el_history_cb', JSON.parse(gps_az_el_json));
         } catch(ex) {
            console.log('kiwi_msg() gps_az_el_history_cb: JSON parse fail');
         }
         break;					
   }
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

			case "sdr_mode":
				admin_sdr_mode = +param[1];
				break;

			case "init":
				rx_chans = rx_chan = param[1];
				//console.log("ADMIN init rx_chans="+rx_chans);
				
				if (rx_chans == -1) {
					var admin = w3_el("id-admin");
					admin.innerHTML =
						'<header class="w3-container w3-red"><h5>Admin interface</h5></header>' +
						'<p>To use the new admin interface you must edit the configuration ' +
						'parameters from your current kiwi.config/kiwi.cfg into kiwi.config/kiwi.json<br>' +
						'Use the file kiwi.config/kiwi.template.json as a guide.</p>';
				} else {
					admin_draw(admin_sdr_mode);
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

			case "auto_nat":
				var p = +param[1];
				var el = w3_el('id-net-auto-nat-msg');
				var msg, color;
				
				switch (p) {
					case 0: break;
					case 1: msg = 'succeeded'; color = 'w3-green'; break;
					case 2: msg = 'no device found'; color = 'w3-orange'; break;
					case 3: msg = 'rule already exists'; color = 'w3-yellow'; break;
					case 4: msg = 'command failed'; color = 'w3-red'; break;
					default: break;
				}
				
				if (p) {
					el.innerHTML += msg;
					el = w3_el('id-net-auto-nat');
					w3_add(el, color);
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
				var wasScrolledDown = kiwi_isScrolledDown(el2);
				var s = decodeURIComponent(param[1]).replace(/</g, '&lt;').replace(/>/g, '&gt;');
				el.innerHTML = s;

				// only jump to bottom of updated list if it was already sitting at the bottom
				if (wasScrolledDown) el2.scrollTop = el2.scrollHeight;
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
		      console_status_msg_p.s = param[1];
		      //console.log('console_c2w:');
		      //console.log(console_status_msg_p);
				kiwi_output_msg('id-console-msgs', 'id-console-msg', console_status_msg_p);
				break;

			case "console_done":
			   console.log('## console_done');
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
	w3_el('id-admin-reload-secs').innerHTML = 'Admin page reload in '+ admin_reload_rem + ' secs';
	if (admin_reload_rem > 0) {
		admin_reload_rem--;
		kiwi_draw_pie('id-admin-pie', admin_pie_size, (admin_reload_secs - admin_reload_rem) / admin_reload_secs);
	} else {
		window.location.reload(true);
	}
};

function admin_wait_then_reload(secs, msg)
{
	var admin = w3_el("id-admin");
	var s2;
	
	if (secs) {
		s2 =
			w3_divs('w3-vcenter w3-margin-T-8', 'w3-container',
				w3_div('w3-show-inline-block', kiwi_pie('id-admin-pie', admin_pie_size, '#eeeeee', 'deepSkyBlue')),
				w3_div('w3-show-inline-block',
					w3_div('id-admin-reload-msg'),
					w3_div('id-admin-reload-secs')
				)
			);
	} else {
		s2 =
			w3_divs('w3-vcenter w3-margin-T-8', 'w3-container',
				w3_div('id-admin-reload-msg')
			);
	}
	
	var s = '<header class="w3-container w3-teal"><h5>Admin interface</h5></header>'+ s2;
	//console.log('s='+ s);
	admin.innerHTML = s;
	
	if (msg) w3_el("id-admin-reload-msg").innerHTML = msg;
	
	if (secs) {
		admin_reload_rem = admin_reload_secs = secs;
		setInterval(admin_draw_pie, 1000);
		admin_draw_pie();
	}
}

function admin_restart_now_cb()
{
	ext_send('SET restart');
	admin_wait_then_reload(60, 'Restarting KiwiSDR server');
}

function admin_reboot_now_cb()
{
	ext_send('SET reboot');
	admin_wait_then_reload(90, 'Rebooting Beagle');
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

function admin_int_null_cb(path, val, first)
{
	var v = parseInt(val);
	//console.log('admin_int_null_cb '+ typeof val +' "'+ val +'" '+ v);
	if (isNaN(v)) v = null;

	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function admin_int_zero_cb(path, val)
{
	//console.log('admin_int_zero_cb '+ path +'='+ val);
	val = parseInt(val);
	if (isNaN(val)) {
		val = 0;
		w3_set_value(path, val);
	}
	ext_set_cfg_param(path, val, true);
}

function admin_float_null_cb(path, val, first)
{
	var v = parseFloat(val);
	//console.log('admin_float_cb '+ typeof val +' "'+ val +'" '+ v);
	if (isNaN(v)) v = null;

	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, val, save);
}

function admin_float_zero_cb(path, val)
{
	//console.log('admin_float_zero_cb '+ path +'='+ val);
	val = parseFloat(val);
	if (isNaN(val)) {
		val = 0;
		w3_set_value(path, val);
	}
	ext_set_cfg_param(path, val, true);
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

function admin_preview_status_box(val)
{
	var s = decodeURIComponent(val);
	if (!s || s == '') s = '&nbsp;';
	return s;
}
