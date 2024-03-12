// Copyright (c) 2016-2023 John Seamons, ZL4VO/KF6VO

var mfg = {
   w: 800,
   h: 725,
   th: 230,

   ver_maj: 0,
   ver_min: 0,
   
   serno: 0,
   next_serno: -1,
   
   dna: '',
   seed_phrase_ok: false,
   
   model_i: 1,    // KiwiSDR 2
   model: 2,
   cur_model: -1,
   MODEL_BIAS: 1,
   model_s: [ 'KiwiSDR 1', 'KiwiSDR 2' ],
   
   pie_size: 25,

   __last__: null
};

function mfg_main()
{
}

function mfg_msg(param)
{
   //console.log('mfg_msg: '+ param[0]);
   
   switch (param[0]) {

         case "keepalive":
				//console.log('MFG keepalive');
		      kiwi_clearTimeout(mfg.keepalive_timeoout);
            mfg.keepalive_rx_time = Date.now();
            mfg.keepalive_timeoout = setTimeout(function() {
         
               // in case the timeout somehow didn't get cancelled (which shouldn't happen)
               var diff = Date.now() - mfg.keepalive_rx_time;
               if (diff > 75000)
                  mfg_close();
            }, 90000);
				break;
   }
}

function mfg_close()
{
   // don't show message if reload countdown running
   kiwi_clearTimeout(mfg.keepalive_timeoout);
   if (kiwi.no_reopen_retry) {
	      w3_hide('id-kiwi-msg-container');      // in case password entry panel is being shown
         w3_show_block('id-kiwi-container');
         wait_then_reload_page(0, 'Server has closed connection.', 'mfg');
   } else
   if (isUndefined(adm.mfg_keepalive) || adm.mfg_keepalive == true) {
      if (!mfg.reload_rem) {
	      w3_hide('id-kiwi-msg-container');      // in case password entry panel is being shown
         w3_show_block('id-kiwi-container');
         //kiwi_show_msg('Server has closed connection.');
         //if (dbgUs) console.log('mfg close'); else
            wait_then_reload_page(60, 'Server has closed connection. Will retry.', 'mfg');
      }
   }
}

function kiwi_ws_open(conn_type, cb, cbp)
{
	return open_websocket(conn_type, cb, cbp, mfg_msg, mfg_recv, null, mfg_close);
}

function mfg_html()
{
	var s =
      w3_div(sprintf('w3-container w3-section w3-dark-grey|width:%dpx;height:%dpx', mfg.w, mfg.h),
         w3_div('w3-margin-bottom w3-text-aqua',
            '<h4><strong>Manufacturing interface</strong>&nbsp;&nbsp;' +
            '<small>(software version v'+ mfg.ver_maj +'.'+ mfg.ver_min +')</small></h4>'
         ),

         // status
         w3_text('id-ee-status w3-bold w3-yellow w3-padding-LR-4 w3-padding-TB-0'),
         '<br>',
         w3_text('id-ee-msg w3-bold w3-margin-T-8 w3-padding-LR-4 w3-padding-TB-0'),
         '<hr>',
      
         // write serno/model and register proxy key
         w3_div('id-mfg-not-kiwi1 w3-width-full w3-hide', 
            w3_col_percent('w3-valign/',
               w3_button('id-key-write w3-margin-T-8 w3-text-white', '', 'mfg_key_write_cb'), 45,
               '&nbsp;', 15,
            
               w3_input('id-mfg-seed//w3-margin-3 w3-padding-small|width:300px', w3_label('w3-bold', 'Seed phrase ') +
                  w3_div('id-mfg-seed-check cl-admin-check w3-btn w3-round-large w3-margin-B-8'),
                  'id-seed-phrase', '', 'mfg_set_seed_cb'
               )
            ),
            '<hr>',
         ),

         // write serno/model
         w3_col_percent('',
            w3_input('id-seq-input//w3-margin-T-4 w3-padding-small|width:70px', 'write EEPROM with serial number:',
               'mfg.seq', 21000, 'mfg_seq_set_cb'), 45,
            '&nbsp;', 15,

            w3_select('', 'write EEPROM with model:', '', 'mfg.model_i', mfg.model_i, mfg.model_s, 'mfg_model_cb')
         ),
         '<hr>',

         w3_col_percent('w3-valign/',
            w3_button('id-sn-write-k1 w3-margin-T-8 w3-text-white', '', 'mfg_sn_write_k1_cb'), 45,
            '&nbsp;', 15,
            
            w3_button('id-mfg-restart w3-aqua', 'restart and<br>load admin page', 'mfg_restart_cb'), 25,
            w3_button('id-mfg-power-off w3-css-magenta', 'halt and<br>power off', 'mfg_power_off_cb')
         ),
         '<hr>',

      /*
         w3_div('w3-valign',
            w3_button('w3-aqua', 'Click to write micro-SD card', 'sd_backup_click_cb'),

            w3_div('w3-margin-L-64',
               w3_div('id-sd-progress-container w3-progress-container w3-round-large w3-css-lightGray w3-show-inline-block',
                  w3_div('id-sd-progress w3-progressbar w3-round-large w3-light-green w3-width-zero',
                     w3_div('id-sd-progress-text w3-container')
                  )
               ),
            
               w3_inline('w3-margin-T-8/',
                  w3_div('id-sd-backup-time'),
                  w3_div('id-sd-backup-icon w3-margin-left'),
                  w3_div('id-sd-backup-msg w3-margin-left')
               )
            )
         ),
         '<hr>',
      */

         w3_div('id-output-msg class-mfg-status w3-scroll-down w3-small')
      );
	
	var el = w3_innerHTML('id-mfg', s);
	el.style.top = el.style.left = '10px';

	//w3_width_height('id-sd-progress-container', mfg.th);
	w3_width_height('id-output-msg', null, mfg.th);

   sd_backup_init();
   sd_backup_focus();
}

function mfg_recv(data)
{
	var stringData = arrayBufferToString(data);
	params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		param = params[i].split("=");
      console.log('mfg_recv: '+ param[0]);
		
		switch (param[0]) {     // #msg-proc
			case "ver_maj":
				mfg.ver_maj = parseFloat(param[1]);
				break;

			case "ver_min":
				mfg.ver_min = parseFloat(param[1]);
				mfg_html();
				break;

			case "serno":
				mfg.serno = parseFloat(param[1]);
				console.log("MFG next_serno="+ mfg.next_serno + " serno="+ mfg.serno);
				
				mfg_set_button_text();

				w3_innerHTML('id-seq-override', 'next serial number = #'+ mfg.next_serno +'<br>click to override');
				el = w3_remove('id-seq-input', 'w3-visible');
				el.value = mfg.next_serno;
			   mfg_set_status();
				break;
			
			case "next_serno":
				mfg.next_serno = parseFloat(param[1]);
            console.log("MFG next_serno="+ mfg.next_serno);
				break;

			case "model":
			   mfg.cur_model = parseFloat(param[1]);
			   mfg_set_status();
				console.log("MFG cur_model="+ mfg.cur_model);
            console.log('MFG: not-kiwi1 '+ mfg.cur_model);
            //if (mfg.cur_model != 1)
               w3_show('id-mfg-not-kiwi1');
			   break;

			case "microSD_done":
				sd_backup_write_done(parseFloat(param[1]));
				break;

			case "dna":
				mfg.dna = param[1];
				break;

			default:
				console.log('MFG UNKNOWN: '+ param[0] +'='+ param[1]);
				break;
		}
	}
}

function mfg_model_cb(path, idx, first)
{
   mfg.model_i = +idx;
   mfg.model = mfg.model_i + mfg.MODEL_BIAS;
   mfg_set_button_text();
}

function mfg_set_status()
{
   var invalid = (mfg.cur_model == -1 || mfg.serno == -1);
   var status = invalid? '(invalid)' : ('KiwiSDR '+ mfg.cur_model +', #'+ mfg.serno);
   var el = w3_el('id-ee-status');
   w3_innerHTML(el, 'current EEPROM data: '+ status);
   w3_colors(el, 'w3-red', 'w3-yellow', !invalid);
}

function mfg_set_button_text()
{
   var button_k1  = 'click to write EEPROM with:<br>model KiwiSDR 1, #'+ mfg.next_serno;
   var button_k2p = 'click to write EEPROM with:<br>model KiwiSDR '+ mfg.model +', #'+ mfg.next_serno;
   var sn_valid = (mfg.serno > 0);     // write serno = 0 to force invalid for testing
   
   var el = w3_el('id-key-write');
   if (mfg.seed_phrase_ok)
      w3_innerHTML(el, button_k2p +'<br>AND send proxy key to kiwisdr.com');
   else
      w3_innerHTML(el, 'seed phrase incorrect');
   w3_colors(el, 'w3-red', 'w3-green', sn_valid && mfg.seed_phrase_ok);

   el = w3_innerHTML('id-sn-write-k1', button_k1);
   w3_colors(el, 'w3-red', 'w3-green', sn_valid);
   
   w3_el('id-mfg.seq').value = mfg.next_serno;
}

function mfg_key_ajax_cb(reply)
{
   console.log('mfg_key_ajax_cb: reply='+ reply);
   console.log(reply);

   var m = '', err = false;
   if (isObject(reply) && reply.AJAX_error) {
      if (reply.AJAX_error == 'status') reply.AJAX_error = reply.status;
      switch (reply.AJAX_error) {
         case 'timeout':   m = 'Timeout contacting proxy.kiwisdr.com'; break;
         case 403:         m = 'DANGER!!! Wrong software installed. Contact jks@kiwisdr.com immediately.'; break;
         default:          m = 'Unknown error reply "'+ reply.AJAX_error +'"'; break;
      }
      m = 'FAILED: '+ m;
      err = true;
   } else {
      m = 'SUCCESS proxy.kiwisdr.com updated with proxy key for serial number: '+ mfg.next_serno;
      m += '<br>Admin password: '+ reply;
      ext_set_cfg_param('adm.admin_password', reply);
      ext_set_cfg_param('adm.rev_user', mfg.auto_key);
      ext_set_cfg_param('adm.rev_auto_user', mfg.auto_key);
      ext_set_cfg_param('adm.rev_auto_host', mfg.next_serno.toString());
      ext_set_cfg_param('adm.rev_auto', true, EXT_SAVE);
      ext_set_cfg_param('cfg.sdr_hu_dom_sel', kiwi.REV, EXT_SAVE);
   }
   w3_innerHTML('id-ee-msg', m);
   w3_colors('id-ee-msg', 'w3-green', 'w3-red', err);
}

function mfg_key_write_cb()
{
   if (!mfg.seed_phrase_ok) return;
   console.log('mfg_key_write_cb mfg.serno='+ mfg.serno +' kiwi.model='+ kiwi.model);
   w3_clearInnerHTML('id-ee-msg');

   var hash_key = mfg.next_serno +'_'+ mfg.dna;
   var keygen = Sha256.hash(hash_key +'_'+ mfg.seed_phrase_hash);
   mfg.auto_key = keygen.slice(0,29);
	ext_send('SET eeprom_write=1 serno='+ mfg.next_serno +' model='+ mfg.model +' key='+ mfg.auto_key);
   
   // setup frpc.ini file
   ext_send('SET rev_config user='+ mfg.auto_key +' host='+ mfg.next_serno);

   var url = 'http://proxy.kiwisdr.com/?auth=eeb3f5150bdfb6dcfb6dcfb6dcfb6dcdc4fceeb3' +
      '&u='+ hash_key +'_'+ keygen;
   kiwi_ajax(url, 'mfg_key_ajax_cb', 0, 10000);
   
   /*
   var request = kiwi_GETrequest('proxy', 'http://proxy.kiwisdr.com/', {debug:1});
   kiwi_GETrequest_param(request, "auth", 'eeb3f5150bdfb6dcfb6dcfb6dcfb6dcdc4fceeb3');
   kiwi_GETrequest_param(request, "u", hash_key +'_'+ keygen);
   kiwi_GETrequest_submit(request, { gc: 1 } );
   */
}

function mfg_sn_write_k1_cb()
{
	ext_send('SET eeprom_write=0 serno='+ mfg.next_serno +' model=1');
}

function mfg_set_seed_cb(path, val, first)
{
   if (first) return;
   mfg.seed_phrase_hash = Sha256.hash(val);
   //console.log('mfg_set_seed_cb: <'+ val +'> '+ mfg.seed_phrase_hash);
   mfg.seed_phrase_ok = mfg.seed_phrase_hash.startsWith('e989cca6');
   var el = w3_el('id-mfg-seed-check');
   w3_innerHTML(el, mfg.seed_phrase_ok? 'Correct' : 'Incorrect');
   w3_colors(el, 'w3-red', 'w3-green', mfg.seed_phrase_ok);
   mfg_set_button_text();
}

function mfg_seq_set_cb(path, val, first)
{
	val = parseFloat(val).toFixed(0);
	val = val || 21000;
   console.log('mfg_seq_set_cb: next_serno='+ val);
	if (val >= 0 && val <= 99999999) {
	   mfg.next_serno = val;
		ext_send("SET set_serno="+ mfg.next_serno);
		mfg_set_button_text();
	}
}

function mfg_restart_cb()
{
	var el = w3_innerHTML('id-mfg-restart', 'restarting...');
	w3_background_color(el, 'red');
	kiwi.reload_url = kiwi_SSL() + mfg.serno +'.proxy.kiwisdr.com/admin';
	console.log(kiwi.reload_url);
   wait_then_reload_page(60, 'Reconnecting to admin page.', 'mfg');
	ext_send("SET mfg_restart");
}

function mfg_power_off_cb()
{
	var el = w3_innerHTML('id-mfg-power-off', 'halting and<br>powering off...');
	w3_background_color(el, 'red');
	ext_send("SET mfg_power_off");
}
