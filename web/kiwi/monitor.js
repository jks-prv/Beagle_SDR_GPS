// KiwiSDR
//
// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

// TODO
//
// features
//    mute, vol key shortcuts
//    S_meter
//    audio FFT wf/spec?
//

var monitor = {
   init: false
};

function kiwi_queue_cb() {
   kiwi.queued ^= 1;
   w3_innerHTML('id-queue-button', kiwi.queued? 'Exit queue' : 'Enter queue');
   if (!kiwi.queued) w3_innerHTML('id-queue-pos', '');
   msg_send('SET MON_QSET='+ (kiwi.queued? 1:0));
}

function kiwi_monitor()
{
   console.log('n_camp='+ cfg.n_camp);
   var camp_s = cfg.n_camp?
      'To camp (listen) to the audio of an existing connection <br>' +
      'click on one of the channel links below.'
   :
      'No audio camping allowed on this Kiwi.';
   var s =
	   w3_div('',
	      w3_text('w3-text-black', 'All Kiwi channels busy. Click button to wait in queue for an available channel.'),
         w3_inline('w3-margin-L-8 w3-margin-T-4/w3-margin-right w3-valign',
            w3_button('id-queue-button w3-medium w3-padding-smaller w3-green', 'Enter queue', 'kiwi_queue_cb'),
            w3_text('id-queue-pos w3-text-black'),
            w3_button('id-queue-button w3-medium w3-padding-smaller w3-yellow', 'Reload page', 'kiwi_queue_reload_cb')
         ),
         w3_text('id-queue-status w3-margin-T-4 w3-text-black', ''),

         w3_div('id-camp-container',
            w3_hr('w3-margin-16'),
            w3_text('id-monitor-disconnect w3-nobk w3-css-orange w3-width-fit w3-hide', '&nbsp;Channel has disconnected'),
            w3_text('w3-margin-T-4 w3-text-black w3-show-block', camp_s),
            w3_text('id-camp-status w3-margin-T-4 w3-text-black', '')
         ),
         
         w3_hr('w3-margin-16'),
         w3_div('id-users-list w3-margin-T-10')
      );
	kiwi_show_msg(s);
	kiwi.unmuted_color = 'mediumSeaGreen';
	users_init( { monitor:1 } );

   pwd_prot =  chan_no_pwd_true? (rx_chans - chan_no_pwd_true) : 0;
   console.log('rx_chans='+ rx_chans +' chan_no_pwd_true='+ chan_no_pwd_true +' pwd_prot='+ pwd_prot);
   var channels = plural(pwd_prot, ' channel');
   var is_are = (pwd_prot == 1)? 'is':'are';

   if (pwd_prot) {
      s = '<br>Note that '+ pwd_prot + channels +' on this Kiwi '+ is_are +' password protected. <br>' +
         'This is why you may see empty channels you cannot access.';
      w3_innerHTML('id-queue-status', s);
   } else
      w3_innerHTML('id-queue-status', '');

   setInterval(function() {
      msg_send('SET keepalive');
      if (kiwi.queued) msg_send('SET MON_QPOS');
   }, 2500);
   
   ws_any().msg_cb = function(msg_a) {
      var a = msg_a[1]? msg_a[1].split(',') : null;
      var s;
      switch (msg_a[0]) {

         case 'qpos':
            var pos = a[0], waiters = a[1], reload = +a[2];
            
            // remove any camp url param so the reload doesn't come right back here
            if (reload) kiwi_queue_reload_cb();
            s = '';
            if (kiwi.queued) s += 'You are '+ pos +' of '+ waiters +' in queue.';
            w3_innerHTML('id-queue-pos', s);
            break;

         case 'camp':
            var okay = +a[0], rx = +a[1];
            if (okay) {
               if (!monitor.init) {
                  check_suspended_audio_state();
                  monitor.init = true;
               }
               
               s = w3_div('',
                  w3_inline('w3-margin-T-4/w3-margin-right w3-valign',
                     w3_text('id-queue-pos w3-text-black', 'Camping on RX'+ rx),
                     w3_button('w3-medium w3-padding-smaller w3-red', 'Stop', 'kiwi_camp_stop_cb'),
                     w3_slider('w3-margin-L-16/w3-label-inline w3-label-not-bold/', 'Volume', 'kiwi.volume', kiwi.volume, 0, 200, 1, 'setvolume'),
                     w3_div('|background-color: #e0e0e0; padding:2px; border-radius:8px|title="mute"',
                        w3_div('id-mute-no fa-stack|width:28px; color:'+ kiwi.unmuted_color,
                           w3_icon('', 'fa-volume-up fa-stack-2x fa-nudge-up', 24, 'inherit', 'toggle_or_set_mute')
                        ),
                        w3_div('id-mute-yes fa-stack w3-hide|width:28px;color:magenta;',  // hsl(340, 82%, 60%) w3-text-pink but lighter
                           w3_icon('', 'fa-volume-off fa-stack-2x fa-nudge-left fa-nudge-up', 24, '', 'toggle_or_set_mute'),
                           w3_icon('', 'fa-times fa-right', 12, '', 'toggle_or_set_mute')
                        )
                     )
                  ),
                  
                  w3_inline('',
                     w3_div('id-control-smeter w3-margin-T-4'),
                     w3_text('id-monitor-masked w3-margin-left w3-red w3-text-white w3-hide', '&nbsp;Frequency is masked')
                  )
               );
            } else {
               s = 'Too many campers on channel RX'+ rx;
               rx = -1;
            }
            kiwi_camp_update(rx, s);
            if (rx != -1) smeter_init(/* force_no_agc_thresh_smeter */ true);
	         toggle_or_set_mute(0);
            break;
         
         case 'isMasked':
            var masked = (+a[0])? true:false;
            w3_show_hide('id-monitor-masked', masked);
            break;

         case 'camp_disconnect':
            w3_show_hide('id-monitor-masked', false);
            w3_show_hide('id-monitor-disconnect', true);
            console.log('camp_disconnect');
            kiwi_camp_stop_cb();
            break;

         default:
            //console.log('>>> FWD OWRX '+ msg_a[0]);
            owrx_msg_cb(msg_a);
            break;
      }
   };
}

function kiwi_queue_reload_cb()
{
   kiwi_open_or_reload_page({ url:kiwi_remove_search_param(kiwi_url(), 'camp') });
}

function kiwi_camp_update(rx, s)
{
   w3_innerHTML('id-camp-status', s);
   for (var i = 0; i < rx_chans; i++) {
      var el = w3_el('id-user-'+ i);
      w3_remove(el, 'w3-green');
      if (i == rx)
         w3_add(el, 'w3-green');
   }
}

function camp(ch, freq, mode, zoom)
{
   console.log('camp ch'+ ch);
   w3_show_hide('id-monitor-disconnect', false);
   if (cfg.n_camp)
      msg_send('SET MON_CAMP='+ ch);
}

function kiwi_camp_stop_cb()
{
   console.log('camp STOP');
   kiwi_camp_update(-1, '');
   msg_send('SET MON_CAMP=-1');
}
