/*
    The MIT License (MIT)

    Copyright (c) 2016-2024 Kari Karvonen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

var ant_sw = {
   
   // set on both sides
   ver: '',
   n_ant: 8,

   // only set on admin side
   backend: -1,
   backend_s: null,
   backends_s: null,
   can_mix: true,
   ip_or_url: null,
   exantennas: 0,    // to avoid console.log spam on timer updates
   first_time: true,
   
   // only set on user side
   focus: false,
   isConfigured: false,
   running: false,
   buttons_locked: false,
   buttons_selected: [],
   chan_that_switched_ant: null,

   denymixing: 0,
   thunderstorm: 0,
   notify_timeout: null,
   url_ant: null,
   url_idx: -1,
   desc_lc: [],
   last_offset: -1,
   last_high_side: -1,
   
   // ordering for backward compatibility with cfg.ant_switch.denyswitching
   EVERYONE: 0,
   LOCAL_CONN: 1,
   LOCAL_OR_PWD: 2,
   denyswitching: 0,
   deny_s: [ 'everyone', 'local connections only', 'local connections or user password only' ],

   // deny reason
   DENY_NONE: 0,
   DENY_SWITCHING: 1,
   DENY_MULTIUSER: 2,
   denied_permission: false,
   denied_because_multiuser: false,
   
   MODE_NO_OFFSET: 0,
   MODE_OFFSET: 1,
   MODE_HI_INJ: 2,
   mode_s: [ 'no offset', 'offset', 'hi side inj' ],
   
   _last_: 0
};

function ant_switch_log(s)
{
   //console.log(s);
}

function ant_switch_disabled()
{
   return (!ant_sw.isConfigured || !cfg.ant_switch.enable);
}

function ant_switch_view()
{
   ant_switch_log('ant_switch_view ant_sw.focus='+ ant_sw.focus);
   if (!ant_sw.focus) return;    // don't bring into view unless focused

   // wait for RF tab render
   keyboard_shortcut_nav('rf');
   var Hrf;
   w3_do_when_cond(
      function() {
         Hrf = w3_el('id-optbar-rf').clientHeight;
         //console.log('Hrf='+ Hrf);
         return (Hrf != 0);
      },
      function() {
         var Hopt = kiwi.OPTBAR_CONTENT_HEIGHT;
         var Hant = w3_el('id-optbar-rf-antsw').clientHeight + /* w3-margin-B-8 */ 8;
         pct = w3_clamp(kiwi_round(1 - (Hant - Hopt) / (Hrf - Hopt), 2), 0, 1);
         //console_nv('ant_switch view', {Hant}, {Hrf}, {pct});    // 160 233
         w3_scrollTo('id-optbar-content', pct);
      }, null,
      200
   );
   // REMINDER: w3_do_when_cond() returns immediately
   
   var p = ext_param();    // will return URL param only once
   if (isNonEmptyString(p)) {
	   ant_switch_log('ant_switch_view: EXT param = <'+ p +'>');
	   ant_sw.url_ant = p.split(',');
	   ant_sw.url_idx = -1;
      ant_switch_prompt_query();
   }

}

function ant_switch_msg(param)
{
   var rv = true;
   ant_switch_log('ant_switch_msg '+ param.join('='));
   
   switch (param[0]) {
      case "antsw_backend_ver":
         var ver = param[1].split('.');
         if (ver.length == 2 && (ver[0] || ver[1])) {
            ant_sw.ver = param[1];
            console.log('ant_switch: backend v'+ ant_sw.ver);
         }
         break;

      case "antsw_channels":
         var n_ch = +param[1];
         if (n_ch >= 1) {
            ant_sw.n_ant = n_ch;
            console.log('ant_switch: channels='+ n_ch);
            ant_switch_buttons_setup();
         }
         break;

      case "antsw_AntennasAre":
         ant_switch_process_reply(param[1]);
         break;

      case "antsw_AntennaDenySwitching":
         var deny_reason = +param[1];
         // breakdown deny_reason into denied_permission and denied_because_multiuser
         ant_sw.denied_permission = (deny_reason != ant_sw.DENY_NONE)? true : false;
         ant_sw.denied_because_multiuser = (deny_reason == ant_sw.DENY_MULTIUSER);
         ant_switch_showpermissions();
         break;

      case "antsw_AntennaDenyMixing":
         if (param[1] == 1) ant_sw.denymixing = 1; else ant_sw.denymixing = 0;
         ant_switch_showpermissions(); 
         break;

      case "antsw_Thunderstorm":
         if (param[1] == 1) {
            ant_sw.thunderstorm = 1;
            ant_sw.denied_permission = true;
         } else {
             ant_sw.thunderstorm = 0;
         }
         ant_switch_showpermissions();
         break;
      
      default:
         console.log('ant_switch_msg: UNKNOWN CMD '+ param[0]);
         rv = false;
         break;
   }
   return rv;
}

function ant_switch_buttons_setup()
{
   var antdesc = [];
   var i;
   for (i = 1; i <= ant_sw.n_ant; i++) {
      antdesc[i] = ext_get_cfg_param_string('ant_switch.ant'+ i +'desc', '', EXT_NO_SAVE);
   }
   
   ant_switch_log('ant_switch: Antenna configuration ant_sw.n_ant='+ ant_sw.n_ant);
   var buttons_html = '';
   var n_ant = 0;
   var backend = (cfg.ant_switch && isNonEmptyString(cfg.ant_switch.backend))? cfg.ant_switch.backend : '';
   if (backend != '') {
      ant_switch_log('ant_switch: backend='+ backend);
      for (i = 1; i <= ant_sw.n_ant; i++) {
         // don't show antenna buttons without descriptions (i.e. configured)
         var s = antdesc[i]? antdesc[i] : '';
         if (isNonEmptyString(s)) {
            var locked = ant_sw.buttons_locked? ' w3-disabled' : '';
            var highlight = ant_sw.buttons_selected[i]? ' w3-selection-green' : '';
            buttons_html += w3_div('w3-valign w3-margin-T-8',
               w3_button('id-antsw-btn w3-flex-noshrink w3-padding-smaller w3-round-6px'+ locked + highlight,
                  'Antenna '+ i, 'ant_switch_select_antenna_cb', i),
               w3_div('w3-margin-L-8', s)
            );
            n_ant++;
         }
         ant_sw.desc_lc[i] = s.toLowerCase();
         ant_switch_log('ant_switch: Antenna '+ i +': '+ s);

         if (!ant_sw.isConfigured) {
            if (isNonEmptyString(s))
            ant_sw.isConfigured = true;
         }
      }
   }
   w3_innerHTML('id-antsw-user', cfg.ant_switch.enable? buttons_html : '');
   ant_switch_view();
   ant_switch_log('ant_switch_buttons_setup DONE');
}

// called when cfg reloaded
function ant_switch_user_refresh()
{
   if (!ant_sw.running) return;
   ant_switch_log('ant_switch_user_refresh');
   ant_switch_prompt_query();
}

function ant_switch_user_init()
{
   ant_sw.last_offset = kiwi.freq_offset_kHz;
   ant_sw.denyswitching = ext_get_cfg_param('ant_switch.denyswitching', ant_sw.EVERYONE, EXT_NO_SAVE);
   ant_sw.denymixing = ext_get_cfg_param('ant_switch.denymixing', '', EXT_NO_SAVE)? 1:0;
   ant_sw.thunderstorm = ext_get_cfg_param('ant_switch.thunderstorm', '', EXT_NO_SAVE)? 1:0;

   ant_switch_log('ant_switch: Antenna g: Ground all antennas');
   var controls_html =
      w3_div('id-antsw-controls w3-margin-right w3-text-white',
         w3_inline('w3-gap-12/',
            w3_div('w3-text-aqua', '<b>Antenna switch</b>'),
            w3_div('w3-text-white', 'by Kari Karvonen'),
            w3_button('id-antsw-help-btn w3-btn-right w3-green w3-small w3-padding-small', 'help', 'ant_switch_help')
         ),
         w3_div('w3-margin-LR-8',
            w3_div('id-antsw-display-selected w3-margin-T-4'),
            w3_div('id-antsw-display-permissions'),
            w3_div('id-antsw-user w3-margin-B-8')
         )
      );

   w3_innerHTML('id-optbar-rf-antsw', controls_html);
   
   ant_sw.running = true;
   ant_switch_log('SET antsw_init');
   ext_send('SET antsw_init');
   ant_switch_buttons_setup();
   ant_switch_prompt_query();

	var p = kiwi_url_param('ant');
   console.log('ant_switch_user_init p='+ p);
   if (isArg(p)) {
      if (isNonEmptyString(p)) {
         console.log('ant_switch_user_init: URL param = <'+ p +'>');
         ant_sw.url_ant = p.split(',');
	      ant_sw.url_idx = -1;
      }
      ant_switch_focus();
      ant_switch_view();
   }

   ant_switch_log('SET antsw_check_set_default');
   ext_send('SET antsw_check_set_default');
}

function ant_switch_focus()
{
   //console.log('### ant_switch_focus');
   ant_sw.focus = true;
}

function ant_switch_blur()
{
   //console.log('### ant_switch_blur');
   ant_sw.focus = false;
}

function ant_switch_display_update(ant) {
   w3_innerHTML('id-antsw-display-selected', ant);
}

function ant_switch_select_antenna(ant) {
   ant_switch_log('ant_switch: switching to antenna '+ ant);
   ant_sw.chan_that_switched_ant = rx_chan;
   ext_send('SET antsw_SetAntenna='+ ant);
}

function ant_switch_select_antenna_cb(path, val, first) {
   ant_switch_log('ant_switch_select_antenna_cb: path='+ path +'first='+ first);
   ant_switch_select_antenna(val);
}

function ant_switch_prompt_query() {
   ant_switch_log('ant_switch_prompt_query');
   ext_send('SET antsw_GetAntenna');
}

// called from receiving server/backend: antsw_AntennasAre=
function ant_switch_process_reply(ant_selected_antenna) {
   var need_to_inform = false;
   ant_switch_log('ant_switch_process_reply ant_selected_antenna='+ ant_selected_antenna);
   
   ant_switch_buttons_setup();
   
   ant_sw.denyswitching = ext_get_cfg_param('ant_switch.denyswitching', ant_sw.EVERYONE, EXT_NO_SAVE);

   if (ant_switch_disabled()) {
      ant_switch_display_update('Antenna switch is disabled.');
      return;
   }

   if (ant_sw.exantennas != ant_selected_antenna) {
      // antenna changed.
      need_to_inform = true;
      ant_sw.exantennas = ant_selected_antenna;
      
      // need to re-evaluate auto aperture because antenna might have significantly different gain
      //console.log('ant_switch_process_reply: waterfall_maxmin_cb');
      w3_call('waterfall_maxmin_cb');
   }
   
   if (ant_selected_antenna == 'g') {
      if (need_to_inform) ant_switch_log('ant_switch: all antennas grounded');
      ant_switch_display_update('All antennas are grounded.');
   } else {
      if (need_to_inform) ant_switch_log('ant_switch: antenna '+ ant_selected_antenna +' in use');
      ant_switch_display_update('Selected antennas are now: '+ ant_selected_antenna);
   }
   
   // update highlight
   var selected_antennas_list = ant_selected_antenna.split(',');
   var re=/^Antenna ([1]?[0-9]+)/i;

   w3_els('id-antsw-btn',
      function(el, i) {
         if (!el.textContent.match(re)) return;
         w3_unhighlight(el);
         var antN = el.textContent.parseIntEnd();
         ant_sw.buttons_selected[antN] = false;
         //console.log('A'+ antN);
         if (!isArray(selected_antennas_list)) return;
         if (selected_antennas_list.indexOf(antN.toString()) < 0) return;  // not currently selected
         //console.log('T');
         ant_sw.buttons_selected[antN] = true;
         w3_highlight(el);
      
         // Check for frequency offset and high-side injection change
         // but only when one antenna is selected and mixing is disabled.
         // Also check if curl command needs to be issued.
         if (ant_sw.denymixing && selected_antennas_list.length == 1) {
            var s = 'ant_switch.ant'+ antN;
            var mode = ext_get_cfg_param(s +'mode', '', EXT_NO_SAVE);
            var offset = ext_get_cfg_param(s +'offset', '', EXT_NO_SAVE);
            offset = +offset;
            if (!isNumber(offset)) offset = 0;

            if (mode != ant_sw.MODE_NO_OFFSET && offset != -1) {
               console.log('ant='+ antN +' CHECK setting mode='+ mode +' offset='+ offset +' last='+ ant_sw.last_offset);
               if (offset != ant_sw.last_offset) {
                  ext_send('SET antsw_freq_offset='+ offset);
                  console.log('ant='+ antN +' SET antsw_freq_offset='+ offset);
                  ant_sw.last_offset = offset;
               }
   
               var high_side = ext_get_cfg_param(s +'high_side', '', EXT_NO_SAVE);
               if (high_side != ant_sw.last_high_side) {
                  var hs = high_side? 1:0;
                  ext_send('SET antsw_high_side='+ hs);
                  console.log('ant='+ antN +' SET antsw_high_side='+ hs);
                  ant_sw.last_high_side = high_side;
               }
            } else {
               console.log('ant='+ antN +' not setting offset');
            }

            if (rx_chan === ant_sw.chan_that_switched_ant) {
               var cmd = ext_get_cfg_param(s +'cmd', '', EXT_NO_SAVE);
               console.log('antsw cmd=<'+ cmd +'>');
               if (!ant_sw.first_time && cmd != '') ext_send('SET antsw_curl_cmd='+ encodeURIComponent(cmd));
               ant_sw.chan_that_switched_ant = null;
            }
         }
      }
   );

   // process optional URL antenna list (includes multiuser feature)
   // switching denial is processed on server side to implement ant_sw.LOCAL_OR_PWD
   if (ant_sw.url_ant && ant_sw.url_ant.length > 0) {
   
      // The reason there is no "forEach" on ant_sw.url_ant in this code is a bit subtle.
      // ant_switch_select_antenna() must be called, and the reply handled, on possibly multiple
      // antennas in ant_sw.url_ant[].
      // So one at a time is done each with a ant_sw.url_ant.shift()
      // The call to ant_switch_select_antenna() causes an immediate callback to this routine.
      
      // If mixing allowed start by deselecting all antennas (backends may have memory of last antenna(s) used).
      if (ant_sw.url_idx == -1 ) {
         ant_sw.url_idx = 0;
         if (!ant_sw.denymixing) {
            ant_switch_select_antenna('g');
            return;
         }
      }
      
      // only allow first antenna if mixing denied
      //console.log('ant_switch: URL url_idx='+ ant_sw.url_idx +' denymixing='+ ant_sw.denymixing);
      if (ant_sw.url_idx == 0 || !ant_sw.denymixing) {
         var ant = decodeURIComponent(ant_sw.url_ant.shift());
         //console.log('ant_switch: URL ant = <'+ ant +'>');
         if (ant == 'help') {
            ant_switch_help();
         } else {
            var n = parseInt(ant);
            if (!(!isNaN(n) && n >= 1 && n <= ant_sw.n_ant)) {
               if (ant == '') {
                  n = 0;
               } else {
                  // try to match on antenna descriptions
                  ant = ant.toLowerCase();
                  for (n = 1; n <= ant_sw.n_ant; n++) {
                     //console.log('ant_switch: CONSIDER '+ n +' <'+ ant +'> <'+ ant_sw.desc_lc[n] +'>');
                     if (ant_sw.desc_lc[n].indexOf(ant) != -1)
                        break;
                  }
               }
            }
            if (n >= 1 && n <= ant_sw.n_ant)
               ant_switch_select_antenna(n);    // this causes antenna query to re-occur immediately
            ant_sw.url_idx++;
         }
      }
   }
   
   ant_sw.first_time = false;
}

function ant_switch_lock_buttons(lock) {
   ant_sw.buttons_locked = lock;
   w3_els('id-antsw-btn',
      function(el, i) {
         // Antenna
         var re=/^Antenna ([1]?[0-9]+)/i;
         if (el.textContent.match(re)) {
            w3_disable(el, lock);
         }

         // Ground All
         re=/^Ground all$/i;
         if (el.textContent.match(re)) {
            w3_disable(el, lock);
         }
      }
   );
}

function ant_switch_showpermissions() {
   var s, lock = true;
   if (ant_switch_disabled()) {
      s = '';
   } else
   if (ant_sw.thunderstorm == 1) {
      s = w3_text('w3-text-css-yellow', 'Thunderstorm. Switching is denied.');
   } else
   if (ant_sw.denied_because_multiuser) {
      s = 'Switching denied: Multiple users online.';
   } else
   if (ant_sw.denied_permission) {
      s = 'Switching denied: No permission.';
   } else {
      lock = false;
      if (ant_sw.denymixing) {
         s = 'Switching is allowed. Mixing not allowed.';
      } else {
         s = 'Switching and mixing are allowed.';
      }
   }
   ant_switch_lock_buttons(lock);
   w3_innerHTML('id-antsw-display-permissions', s);
   ant_switch_view();
}

function ant_switch_help()
{
   var s = 
      w3_text('w3-medium w3-bold w3-text-aqua', 'Antenna switch help') +
      w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
         w3_div('w3-margin-R-8 w3-margin-bottom',
            'The antenna switch, formerly a Kiwi extension downloaded and installed from the web, ' +
            'has now been integrated into the Kiwi server. Thanks to the author Kari Karvonen, ' +
            'for agreeing to this change. This change allows antennas to be switched while another extension ' +
            'is in use (e.g. FT8). Previously, any open extension was closed when the antenna switch ' +
            'extension was opened.<br><br>' +
            
            'If you had a switch device previously setup the configuration will carryover. ' +
            'See the <x1>Antenna Switch</x1> entry on the admin page, extensions tab ' +
            '(same location as previously). The switch device can be selected from a menu here ' +
            'as well as any IP address or URL the device requires.<br><br>' +
            
            'The antenna switch scripts are now contained in the directory <br>' +
            '<x1>/root/Beagle_SDR_GPS/pkgs/ant_switch</x1><br><br>' +
      
            'From the browser URL the antenna(s) to select can be specified with a parameter, ' +
            'e.g. <x1>my_kiwi:8073/?ant=6</x1> would select antenna #6 ' +
            'and <x1>my_kiwi:8073/?ant=6,3</x1> would select antennas #6 and #3 if antenna mixing ' +
            'is allowed.<br><br>' +
      
            'Instead of an antenna number a name can be specified that matches any ' +
            'case insensitive substring of the antenna description ' +
            'e.g. <x1>my_kiwi:8073/?ant=loop</x1> would match the description "E-W Attic Loop". ' +
            'The first description match wins.<br><br>' +
      
            'The previous <x1>my_kiwi:8073/?ext=ant,...</x1> URL syntax continues to work.' +
            ''
         )
      );
   confirmation_show_content(s, 600, 300);
   w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
}


////////////////////////////////
// admin interface
////////////////////////////////

function ant_switch_admin_msg(param)
{
   var s;
   ant_switch_log('ant_switch_admin_msg '+ param.join('='));
   
   switch (param[0]) {
      case "antsw_backends":
         ant_sw.backends_s = decodeURIComponent(param[1]).split(' ');
         ant_sw.backends_s.unshift('disable');
         console.log('antsw_backends='+ ant_sw.backends_s);
         w3_innerHTML('id-antsw-backends',
            w3_select('w3-width-auto w3-label-inline', 'Switch device:', '',
               'ant_sw.backend', ant_sw.backend, ant_sw.backends_s, 'ant_switch_backend_cb')
         );
         w3_innerHTML('id-antsw-ip-url',
            w3_input('/w3-label-inline w3-label-not-bold w3-nowrap/w3-padding-small||size=30', 'IP address or URL:',
               'ant_sw.ip_or_url', ant_sw.ip_or_url, 'ant_switch_set_ip_or_url_cb')
         );
         return true;
         
      case "antsw_backend":
         console.log('antsw_backend='+ param[1] +' enable='+ cfg.ant_switch.enable);
         ant_sw.backend_s = param[1];
         
         if (isNonEmptyString(ant_sw.backend_s) && cfg.ant_switch.enable) {
            if (w3_select_set_if_includes('ant_sw.backend', ant_sw.backend_s)) {
               ext_set_cfg_param('ant_switch.backend', ant_sw.backend_s, EXT_SAVE);
               console.log('cfg.ant_switch.backend='+ ant_sw.backend_s +' ant_sw.backend='+ ant_sw.backend);
               w3_hide2('id-antsw-backend-info', false);
               w3_hide2('id-antsw-error', true);
               w3_hide2('id-antsw-container', false);
            }
         } else
         if (!cfg.ant_switch.enable) {
            w3_select_set_if_includes('ant_sw.backend', 'disable');
            w3_hide2('id-antsw-backend-info', true);
            w3_hide2('id-antsw-error', true);
            w3_hide2('id-antsw-container', true);
         }
         ant_switch_users_notify_change();
         return true;
      
      case "antsw_backend_err":
         ant_sw.ver = param[1];
         s = 'Backend script <x1>'+ ant_sw.backend_s +'</x1> function AntSW_ShowBackend() sent bad configuration data!';
         w3_innerHTML('id-antsw-error', s);
         w3_hide2('id-antsw-backend-info', true);
         w3_hide2('id-antsw-error', false);
         w3_hide2('id-antsw-container', true);

         w3_select_set_if_includes('ant_sw.backend', 'disable');
         ext_set_cfg_param('ant_switch.enable', false, EXT_SAVE);
         ant_switch_users_notify_change();
         return true;
         
      case "antsw_ver":
         console.log('antsw_ver='+ param[1]);
         ant_sw.ver = param[1];
         s = 'Backend script v'+ ant_sw.ver;
         if (ant_sw.ver == '0.0') s = '';
         w3_innerHTML('id-antsw-ver', s);
         if (cfg.ant_switch.enable) {
            w3_hide2('id-antsw-backend-info', false);
            w3_hide2('id-antsw-error', true);
            w3_hide2('id-antsw-container', false);
         }
         return true;
         
      case "antsw_nch":
         console.log('antsw_nch='+ param[1]);
         ant_switch_config_html2(+param[1]);
         return true;

      case "antsw_mix":
         ant_switch_log('antsw_mix='+ param[1]);
         ant_sw.can_mix = (param[1] == 'mix')? true : false;
         w3_innerHTML('id-antsw-mix', 'Supports mixing? '+ (ant_sw.can_mix? 'yes' : 'no'));
         w3_disable('id-antsw-mix-switch', !ant_sw.can_mix);
         return true;

      case "antsw_ip_or_url":
         ant_switch_log('antsw_ip_or_url='+ param[1]);
         var ip_url = param[1];
         if (ip_url == '(null)') ip_url = '';
         w3_set_value('id-ant_sw.ip_or_url', ip_url);
         ant_sw.ip_or_url = ip_url;
         return true;
      
	}
	
	return false;
}

function ant_switch_backend_cb(path, val, first)
{
   val = +val;
	ant_switch_log('ant_switch_backend_cb path='+ path +' val='+ val +' cfg.ant_switch.enable='+ cfg.ant_switch.enable +' first='+ first);
	if (first) return;
	ant_sw.backend_s = ant_sw.backends_s[+val];
   ext_set_cfg_param('ant_switch.backend', ant_sw.backend_s, EXT_NO_SAVE);
   var enable = (val != 0);
   ant_switch_log('ADM antsw_SetBackend enable='+ enable);
   ext_set_cfg_param('ant_switch.enable', enable, EXT_SAVE);
	if (enable) {
      ant_switch_log('ADM antsw_SetBackend='+ ant_sw.backend_s);
      ext_send('ADM antsw_SetBackend='+ ant_sw.backend_s);
      ext_send('ADM antsw_GetInfo');
   } else {
         // there won't be a "antsw_backend" returned to do the notification
         ant_switch_users_notify_change();
   }
   if (!enable) {
      w3_hide2('id-antsw-backend-info', true);
      w3_hide2('id-antsw-error', true);
      w3_hide2('id-antsw-container', true);
   }
}

function ant_switch_users_notify_change()
{
   ant_switch_log('ant_switch_users_notify_change');
   kiwi_clearTimeout(ant_sw.notify_timeout);
   ant_sw.notify_timeout = setTimeout(
      function() {
         ant_switch_log('antsw_notify_users');
         ext_send('ADM antsw_notify_users');
      }, 1000
   );
}

function ant_switch_desc_cb(path, val, first)
{
   w3_string_set_cfg_cb(path, val, first);
   ant_switch_users_notify_change();
}

function ant_switch_set_ip_or_url_cb(path, val, first)
{
	ant_switch_log('ant_switch_set_ip_or_url_cb: path='+ path +' val='+ val +' first='+ first);
	if (first) return;
   ant_sw.ip_or_url = val;
   w3_set_value('id-ant_sw.ip_or_url', ant_sw.ip_or_url);
   ant_switch_log('ADM antsw_SetIP_or_URL='+ ant_sw.ip_or_url);
   ext_send('ADM antsw_SetIP_or_URL='+ ant_sw.ip_or_url);
}

function ant_denyswitch_cb(path, val, first) {
	ant_switch_log('ant_denyswitch_cb path='+ path +' val='+ val +'('+ w3_switch_s(val) +') first='+ first);
	w3_int_set_cfg_cb(path, val, first);
   ant_switch_users_notify_change();
}

// mix, multiuser, thunderstorm
function ant_switch_cb(path, val, first) {
	ant_switch_log('ant_switch_cb path='+ path +' val='+ val +'('+ w3_switch_s(val) +') first='+ first);
	admin_radio_YN_cb(path, val);
   ant_switch_users_notify_change();
}

function ant_switch_cmd_cb(path, val, first)
{
   w3_string_set_cfg_cb(path, val, first);
}

function ant_switch_config_html2(n_ch)
{
   if (n_ch) ant_sw.n_ant = n_ch;
   ant_switch_log('ant_switch_config_html2 n_ch='+ n_ch);
   var s = '';
         /*
         w3_inline_percent('w3-margin-T-16 w3-valign-center/',
            '', 52.65,
            w3_checkbox_get_param('w3-defer//w3-label-inline', 'Default ground antenna', 'ant_switch.ant_ground_default', 'admin_bool_cb', false)
         );
         */

   for (var i = 1; i <= ant_sw.n_ant; i++) {
      s +=
         w3_inline_percent('w3-margin-T-16 w3-valign-center/',
            w3_input_get('w3-defer', 'Antenna '+ i +' description', 'ant_switch.ant'+ i +'desc', 'ant_switch_desc_cb', ''), 50,
            '&nbsp;', 3,
            w3_checkbox_get_param('w3-defer//w3-label-inline', 'Default<br>antenna', 'ant_switch.ant'+ i +'default', 'admin_bool_cb', false), 7,
            '&nbsp;', 3,
            w3_select('', 'Mode', '', 'ant_switch.ant'+ i +'mode', cfg.ant_switch['ant'+ i +'mode'], ant_sw.mode_s, 'admin_select_cb'), 10,
            '&nbsp;', 1,
            w3_input_get('w3-defer//', 'Frequency scale offset (kHz)', 'ant_switch.ant'+ i +'offset', 'w3_int_set_cfg_cb', 0)
         ) +

         w3_inline_percent('w3-margin-T-16 w3-valign-center/',
            '', 53,
            w3_input_get('w3-defer//', 'cURL command arguments', 'ant_switch.ant'+ i +'cmd', 'ant_switch_cmd_cb', '')
         );
   }
   w3_innerHTML('id-antsw-list', s);
}

function ant_switch_config_html()
{
   // All of the w3-defer cfg saves here, and in ant_switch_config_html2(),
   // get resolved when the first ant_switch_users_notify_change() occurs.
   // That routine does a time deferred (1 second) call to ext_set_cfg_param(EXT_SAVE)
   // which will save all the accumulated changes.
   
   ant_sw.denyswitching = ext_get_cfg_param('ant_switch.denyswitching', ant_sw.EVERYONE, EXT_NO_SAVE);
   if (ant_sw.denyswitching == '') ant_sw.denyswitching = ant_sw.EVERYONE;

   ext_admin_config('ant_switch', 'Antenna switch',
      w3_div('id-antsw w3-text-teal', '<b>Antenna switch configuration</b>' + '<hr>' +
         w3_div('',
            w3_inline('w3-gap-32/',
               w3_div('id-antsw-backends', '<b>Loading...</b>'),
               w3_inline('id-antsw-backend-info w3-hide w3-gap-32/',
                  w3_div('id-antsw-ver'),
                  w3_div('id-antsw-mix'),
                  w3_div('id-antsw-ip-url')
               ),
               w3_div('id-antsw-error w3-hide')
            ),

            w3_div('id-antsw-container w3-hide',
               w3_div('',
                  '<br>If antenna switching is denied then users cannot switch antennas. <br>' +
                  'Admin can always switch antennas, either from a user connection on the local network, or from the <br>' +
                  'admin console tab using the script: <x1>/root/Beagle_SDR_GPS/pkgs/ant_switch/ant-switch-frontend</x1> <br><br>' +
                  'The last menu option allows anyone connecting using a password to switch antennas <br>' +
                  '(i.e. time limit exemption password on the admin page control tab, not the user login password). <br>' +
                  'Other connections made without passwords are denied.'
               ),
               w3_select('w3-width-auto w3-label-inline w3-margin-T-8|color:red', 'Allow antenna switching by:', '',
                  'ant_switch.denyswitching', ant_sw.denyswitching, ant_sw.deny_s, 'ant_denyswitch_cb'),

               w3_div('w3-margin-T-16','If antenna mixing is denied then users can select only one antenna at time.'),
               w3_switch_label_get_param('id-antsw-mix-switch w3-defer w3-margin-T-8/w3-label-inline w3-label-left/', 'Deny antenna mixing?',
                  'Yes', 'No', 'ant_switch.denymixing', true, false, 'ant_switch_cb'),

               w3_div('w3-margin-T-16','If multiuser is denied then antenna switching is disabled when more than one user is online.'),
               w3_switch_label_get_param('w3-defer/w3-label-inline w3-label-left w3-margin-T-8', 'Deny multiuser switching?',
                  'Yes', 'No', 'ant_switch.denymultiuser', true, false, 'ant_switch_cb'),

               w3_div('w3-margin-T-16','If thunderstorm mode is activated, all antennas and forced to ground and switching is disabled.'),
               w3_switch_label_get_param('w3-defer/w3-label-inline w3-label-left w3-margin-T-8', 'Enable thunderstorm mode?',
                  'Yes', 'No', 'ant_switch.thunderstorm', true, false, 'ant_switch_cb'),

               w3_div('w3-margin-T-16',
                  'The <x1>Default antenna</x1> checkbox below define which single antenna is initially selected. <br>' +
                  'And also when no users are connected if the switch below is set to <x1>Yes</x1>. <br>' +
                  '"Users" includes all connections including FT8/WSPR autorun and kiwirecorder (e.g. wsprdaemon). <br>' +
                  'If multiple are checked only the first encountered is used.'
               ),
               w3_switch_label_get_param('w3-defer/w3-label-inline w3-label-left w3-margin-T-8',
                  'Switch to default antenna when no users connected?',
                  'Yes', 'No', 'ant_switch.default_when_no_users', true, true, 'admin_radio_YN_cb'),

               w3_div('w3-margin-T-16',
                  'Grounds antennas instead of switching to default antenna when no users connected.'
               ),
               w3_switch_label_get_param('w3-defer/w3-label-inline w3-label-left w3-margin-T-8',
                  'Ground antennas when no users connected?',
                  'Yes', 'No', 'ant_switch.ground_when_no_users', true, true, 'admin_radio_YN_cb'),

               w3_div('','<hr><b>Antenna buttons configuration</b><br>'),
               w3_col_percent('w3-margin-T-16/',
                  'Leave antenna description field empty to hide antenna button from users. <br>' +
                  'For two-line descriptions use break sequence &lt;br&gt; between lines.', 50,
                  '&nbsp;', 3,
               
                  w3_link('w3-link-darker-color', 'http://kiwisdr.com/info/#id-antsw', 'Click here') +
                  ' for info about <br>' +
                  'cURL command field.', 20,
                  '&nbsp;', 1,

                  'Overrides frequency scale offset value on<br>' +
                  'config tab when any antenna selected. <br>' +
                  'No effect if antenna mixing enabled. <br>'
               ),

               w3_div('',
                  w3_div('id-antsw-list'),
                  w3_col_percent('w3-margin-T-16/',
                     w3_input_get('w3-defer', 'Antenna switch failure or unknown status decription', 'ant_switch.ant0desc', 'w3_string_set_cfg_cb', ''), 70
                  )
               )
            )
         )
      )
   );

   // delay to lessen impact on other tab updates
   setTimeout(
      function() {
         ext_send('ADM antsw_GetBackends');
         ext_send('ADM antsw_GetInfo');
      }, 3000
   );
}
