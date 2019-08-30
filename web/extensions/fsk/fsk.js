// Copyright (c) 2017 John Seamons, ZL/KF6VO

var fsk = {
   ext_name: 'fsk',     // NB: must match fsk.c:fsk_ext.name
   first_time: true,
   
   lhs: 150,
   tw: 1024,
   th: 200,
   x: 0,
   last_y: [],
   
   n_menu:     4,
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   menu3:      -1,
   header: null,
   menu_sel: '',
   test_mode: false,
   
   freq: 0,
   cf: 1000,
   shift: 170,
   shift_i: 1,
   SHIFT_CUSTOM_IDX: 9,
   shift_cval: 0,
   shift_custom: false,
   baud: 50,
   baud_i: 2,
   BAUD_CUSTOM_IDX: 8,
   baud_cval: 0,
   baud_custom: false,
   baud_mult: 1,
   framing: '5N1.5',
   inverted: 1,
   encoding: 'ITA2',
   
   scope: 0,
   run: 0,
   single: 0,
   decim: 8,
   
   show_framing: 0,
   fr_sample: 0,
   fr_bpw: 5,
   fr_phase: 0,
   fr_bpd: 0,
   fr_shift: false,
   fr_w: 6,
   fr_h: 6,
   fr_s: 2,
   
   sample_count: 0,
   edge: 0,
   
   CHU_offset: 2.125,
   
   last_last: 0
};

function fsk_main()
{
	ext_switch_to_client(fsk.ext_name, fsk.first_time, fsk_recv);		// tell server to use us (again)
	if (!fsk.first_time)
		fsk_controls_setup();
	fsk.first_time = false;
}

function fsk_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('fsk_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('fsk_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('fsk_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
            kiwi_load_js_dir('extensions/fsk/', ['JNX.js', 'BiQuadraticFilter.js', 'CCIR476.js', 'FSK_async.js'], 'fsk_controls_setup');
				break;

			default:
				console.log('fsk_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function fsk_scope_reset_display()
{
   var ct = fsk.canvas.ctx;
   ct.fillStyle = 'black';
   ct.fillRect(fsk.lhs,0, fsk.tw,fsk.th);
}

function fsk_scope(dv, edge, bit)
{
   if (!fsk.scope || !fsk.run) return;
   fsk.sample_count++;
   fsk.edge |= edge;
   if ((fsk.sample_count & (fsk.decim-1)) != 0) return;
   
   var cv = fsk.canvas;
   var ct = fsk.canvas.ctx;
   var w = cv.width;
   var h = cv.height;
   var x = fsk.lhs + fsk.x;
   var y;

   if (fsk.edge) {
      ct.fillStyle = 'red';
      ct.fillRect(x,0, 1,h*3/4);
      
      if (x-8 > fsk.lhs) {
         ct.fillStyle = 'white';
         ct.font = '8px Courier';
         ct.fillText(bit, x-8, h*3/4+10);
      }
   } else {
      // erase previous (full height if edge marker)
      y = fsk.last_y[fsk.x];
      ct.fillStyle = 'black';
      if (y == -1) {
         ct.fillRect(x,0, 1,h);
      } else {
         ct.fillRect(x,y, 1,1);
      }
      
      // zero reference
      ct.fillStyle = 'yellow';
      ct.fillRect(x,h/2, 1,1);
   }

   //dv /= 5;
   if (dv > 1) dv = 1;
   if (dv < -1) dv = -1;
   y = Math.floor(h/2 + dv*h/4);
   ct.fillStyle = 'lime';
   ct.fillRect(x,y, 1,1);
   fsk.last_y[fsk.x] = fsk.edge? -1:y;
   fsk.edge = 0;

   fsk.x++;
   if (fsk.x >= w) {
      fsk.x = 0;
      if (fsk.single) {
         fsk.run = 0;
      } else {
         // clear text area
         ct.fillStyle = 'black';
         ct.fillRect(fsk.lhs,h*3/4, fsk.tw,16);
      }
   }
}

function fsk_framing_reset()
{
   fsk.fr_bits = [];
   fsk_framing_reset_display();
   fsk.fr_sample = 1;
}

function fsk_framing_reset_display()
{
   var ct = fsk.canvas.ctx;

   fsk.fr_x = fsk.lhs + fsk.fr_s;
   fsk.fr_xi = 0;
   fsk.fr_yi = fsk.fr_h + fsk.fr_s;
   
   fsk.fr_bitn = fsk.fr_bpw;
   fsk.data_msb = 1 << (fsk.fr_bpd - 1);
   fsk.fr_shift = false;
   
   ct.fillStyle = 'lightGray';
   ct.fillRect(fsk.lhs,0, fsk.tw,fsk.th);
   
   if (fsk.fr_bpd) {
      ct.fillStyle = 'red';
      var y = fsk.th - (fsk.fr_bitn - 1*fsk.baud_mult) * fsk.fr_yi - fsk.fr_s;
      ct.fillRect(fsk.lhs,y, fsk.tw,fsk.fr_s);
      y = fsk.th - (fsk.fr_bitn - ((fsk.fr_bpd + 1) * fsk.baud_mult)) * fsk.fr_yi - fsk.fr_s;
      ct.fillRect(fsk.lhs,y, fsk.tw,fsk.fr_s);
   }
}

// called by JNX.output_bit_cb()
function fsk_framing(bit)
{
   if (!fsk.fr_sample) return;
   fsk.fr_bits.push(bit);
   fsk_framing_proc(bit);
}

// FIXME: needs to handle 5N1V mode
function fsk_framing_proc(bit)
{
   var ct = fsk.canvas.ctx;
   var yi = fsk.fr_yi;
   fsk.fr_y = fsk.th - fsk.fr_bitn * yi;
   
   if (fsk.fr_bpd) {
      var c;
      var bm = fsk.baud_mult;
      var d_bpd = fsk.fr_bpd * bm;
      var d_lsb = fsk.fr_bpw - bm;
      var d_msb = fsk.fr_bpw - bm - d_bpd;
      
      if (fsk.fr_bitn == fsk.fr_bpw) fsk.fr_code = 0;
      if (fsk.fr_bitn <= d_lsb && fsk.fr_bitn > d_msb) {
         if (bm == 1 || ((fsk.fr_bitn & 1) == 0)) {
            fsk.fr_code >>= 1;
            fsk.fr_code |= bit? fsk.data_msb : 0;
            //if (fsk.fr_xi < 4) console.log(fsk.fr_xi +' bit='+ bit +' code=0x'+ fsk.fr_code.toString(16) +' data_msb=0x'+ fsk.data_msb.toString(16));
            //console.log(fsk.fr_bitn +'=0x'+ fsk.fr_code.toString(16));
         }
      }
      if (fsk.fr_bitn == 1) {
         var code = fsk.fr_code;
         //if (fsk.fr_xi < 4) console.log(fsk.fr_xi +' DONE code=0x'+ code.toString(16));
         if (code == fsk.encoder.LETTERS) { fsk.fr_shift = false; c = '\u2193'; }   // down arrow
         else
         if (code == fsk.encoder.FIGURES) { fsk.fr_shift = true; c = '\u2191'; }    // up arrow
         else {
            c = fsk.encoder.code_to_char(code, fsk.fr_shift);
         }
         if (c < 0) c = '\u2612';
         //console.log('fr_code=0x'+ code.toString(16) +' sft='+ (fsk.fr_shift? 1:0) +' c=['+ c +']');
         ct.fillStyle = 'black';
         ct.font = '12px Courier';
         ct.fillText(c, fsk.fr_x, fsk.th - fsk.fr_bpw * yi - 14);
      }
   }

   if (bit) {
      ct.fillStyle = 'blue';
      ct.fillRect(fsk.fr_x,fsk.fr_y, fsk.fr_w,fsk.fr_h);
   } else {
      ct.fillStyle = 'yellow';
      ct.fillRect(fsk.fr_x,fsk.fr_y, fsk.fr_w,fsk.fr_h);
   }

   fsk.fr_bitn--;
   if (fsk.fr_bitn == 0) {
      fsk.fr_bitn = fsk.fr_bpw;
      fsk.fr_x += fsk.fr_w + fsk.fr_s;
      fsk.fr_xi++;
      if (fsk.fr_x >= fsk.lhs + fsk.tw - fsk.fr_w - fsk.fr_s) {
         fsk.fr_sample = 0;
      }
   }
}

function fsk_phase()
{
   fsk_framing_reset_display();
   for (var i = 0; i < (fsk.fr_bits.length - fsk.fr_phase); i++) {
      fsk_framing_proc(fsk.fr_bits[i + fsk.fr_phase]);
   }
}

function fsk_baud_error_init()
{
   var hh = fsk.th/2;
   var cv = fsk.canvas;
   var ct = fsk.canvas.ctx;

   ct.fillStyle = 'white';
   ct.font = '14px Verdana';
   ct.fillText('Baud', fsk.lhs/2-15, hh);
   ct.fillText('Error', fsk.lhs/2-15, hh+14);
}

function fsk_baud_error(err)
{
   var max = 8;
   if (err > max) err = max;
   if (err < -max) err = -max;
   var h = Math.round(fsk.th*0.8/2 * err/max);
   //console.log('err='+ err +' h='+ h);

   var bw = 20;
   var bx = fsk.lhs - bw*2;
   var hh = fsk.th/2;
   var cv = fsk.canvas;
   var ct = fsk.canvas.ctx;
   
   ct.fillStyle = 'black';
   ct.fillRect(bx,0, bw,fsk.th);

   if (h > 0) {
      ct.fillStyle = 'lime';
      ct.fillRect(bx,hh-h, bw,h);
   } else {
      ct.fillStyle = 'red';
      ct.fillRect(bx,hh, bw,-h);
   }
}

// must set "remove_returns" since pty output lines are terminated with \r\n instead of \n alone
// otherwise the \r overwrite logic in kiwi_output_msg() will be triggered
var fsk_console_status_msg_p = { scroll_only_at_bottom: true, process_return_nexttime: false, remove_returns: true, ncol: 135 };

function fsk_output_char(s)
{
   if (s == '') return;
   
   if (fsk.framing.includes('EFR')) {
      s = 'EFR '+ fsk.menu_sel + s;
   }
   
   fsk_console_status_msg_p.s = encodeURIComponent(s);
   kiwi_output_msg('id-fsk-console-msgs', 'id-fsk-console-msg', fsk_console_status_msg_p);
}

function fsk_audio_data_cb(samps, nsamps)
{
   fsk.jnx.process_data(samps, nsamps);
}

var fsk_weather = {
   'Germany': [
      {f:'147.3 DDH47',  s: 85, b:50, fr:'5N1.5', i:1, e:'ITA2', cf:500},
      {f:'11039 DDH9',   s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'},
      {f:'14467.3 DDH8', s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'},
      {f:'4583 DDK2',    s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'},
      {f:'7646 DDH7',    s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'},
      {f:'10100.8 DDK9', s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'}
   ]
};

var fsk_maritime = {
   'MSI (safety)': [
      {f:4210,    cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:6314,    cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:8416.5,  cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:12579,   cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:16806.5, cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:19680.5, cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:22376,   cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'}
   ],
   'SVO Athens': [
      {f:12603.5, s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'}
   ],
   'UDK2 Murmansk': [
      {f:6322.5, s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'}
   ],
   'XSQ China': [
      {f:8425.5,  s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'},
      {f:12622.5, s:450, b:50, fr:'5N1.5', i:1, e:'ITA2'}
   ]
};

var fsk_military = {
   'PBB Dutch Navy': [
      {f:2474,   s:850, b:75, fr:'5N1V', i:1, e:'ITA2'},
      {f:4280,   s:850, b:75, fr:'5N1V', i:1, e:'ITA2'},
      {f:6358.5, s:850, b:75, fr:'5N1V', i:1, e:'ITA2'},
      {f:8439,   s:850, b:75, fr:'5N1V', i:1, e:'ITA2'}
   ],
   
   // CIS/BEE 36-50 aka T600, see:
   // github.com/IanWraith/Rivet/wiki/CIS36-50
   // i56578-swl.blogspot.com/2014/10/cis-navy-broadcast-bee36-50.html
   'RDL CIS': [
      {f:18.1, s: 75, b:36, fr:'T600', i:1, e:'ITA2'},
      {f:4582, s:200, b:50, fr:'T600', i:1, e:'ITA2'}
   ]
};

var fsk_ham_utility = {
   'Ham RTTY': [
      {f:'3590 80m',  s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'},
      {f:'7043 40m',  s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'},
      {f:'10143 30m', s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'},
      {f:'14080 20m', s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'},
      {f:'18105 17m', s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'},
      {f:'21080 15m', s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'},
      {f:'24925 12m', s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'},
      {f:'28080 10m', s:170, b:45.45, fr:'5N1.5', i:0, e:'ITA2'}
   ],
   'EFR Teleswitch': [
      {f:'129.1 DCF49', s:340, b:200, fr:'EFR', i:1, e:'ASCII'},
      {f:'135.6 HGA22', s:340, b:200, fr:'EFR', i:1, e:'ASCII'},
      {f:'139 DCF39',   s:340, b:200, fr:'EFR', i:1, e:'ASCII'}
   ],
   'CHU time': [
      {f:3330,  s:200, b:300, fr:'CHU', i:0, e:'ASCII'},
      {f:7850,  s:200, b:300, fr:'CHU', i:0, e:'ASCII'},
      {f:14670, s:200, b:300, fr:'CHU', i:0, e:'ASCII'}
   ]
   
   /*
   'Test mode': [
      {f:'test: lazy dog', s:0, b:0, fr:'', i:0, e:''}
   ]
   */
};

var fsk_menu_s = [ 'Weather', 'Maritime', 'Military', 'Ham/Utility' ];
var fsk_menus = [ fsk_weather, fsk_maritime, fsk_military, fsk_ham_utility ];

var fsk_shift_s = [ 85, 170, 200, 340, 425, 450, 500, 850, 1000, 'custom' ];
var fsk_baud_s = [ 36, 45.45, 50, 75, 100, 150, 200, 300, 'custom' ];
var fsk_framing_s = [ '5N1', '5N1V', '5N1.5', '5N2', '7N1', '8N1', '4/7', 'EFR', 'EFR2', 'CHU', 'T600' ];
var fsk_encoding_s = [ 'ITA2', 'ASCII', 'CCIR476' ];

var fsk_mode_s = [ 'decode', 'scope', 'framing' ];
var fsk_bpd_s = [ 'none', '5', '6', '7', '8' ];
var fsk_decim_s = [ 1, 2, 4, 8, 16, 32 ];

function fsk_controls_setup()
{
	fsk.saved_passband = ext_get_passband();

	fsk.jnx = new JNX();
	fsk.freq = ext_get_freq()/1e3;
	//w3_console_obj(fsk.jnx, 'fsk.jnx');
	fsk.jnx.set_baud_error_cb(fsk_baud_error);
	fsk.jnx.set_output_char_cb(fsk_output_char);

   // URL params that need to be setup before controls instantiated
	var p = fsk.url_params = ext_param();
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         //console.log('FSK param1 <'+ a +'>');
         var a1 = a.split(':');
         a1 = a1[a1.length-1].toLowerCase();
         w3_ext_param_array_match_str(fsk_framing_s, a1, function(i,s) { fsk.framing = s; });
         w3_ext_param_array_match_str(fsk_encoding_s, a1, function(i,s) { fsk.encoding = s; });
         var r;
         if ((r = w3_ext_param('shift', a)).match) {
            if (!isNaN(r.num)) {
               fsk.shift = r.num;
               if (w3_ext_param_array_match_str(fsk_shift_s, r.num.toString(),
                  function(i) {
                     fsk.shift_custom = false;
                     //console.log('FSK param1 shift='+ fsk.shift);
                  })) {
                     ;
               } else {
                  fsk.shift_custom = true;
                  //console.log('FSK param1 shift custom='+ fsk.shift);
               }
            }
         } else
         if ((r = w3_ext_param('baud', a)).match) {
            if (!isNaN(r.num)) {
               fsk.baud = r.num;
               if (w3_ext_param_array_match_str(fsk_baud_s, r.num.toString(),
                  function(i) {
                     fsk.baud_custom = false;
                     //console.log('FSK param1 baud='+ fsk.baud);
                  })) {
                     ;
               } else {
                  fsk.baud_custom = true;
                  //console.log('FSK param1 baud custom='+ fsk.baud);
               }
            }
         } else
         if ((r = w3_ext_param('inverted', a)).match) {
            fsk.inverted = (r.num == 0)? 0:1;
         }
      });
   }

   var data_html =
      time_display_html('fsk') +
      
      w3_div('id-fsk-data|width:'+ px(fsk.lhs+1024) +'; height:200px; overflow:hidden; position:relative; background-color:black;',
         '<canvas id="id-fsk-canvas" width="'+ (fsk.lhs+1024) +'" height="200" style="left:0; position:absolute"></canvas>',
			w3_div('id-fsk-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left:'+ px(fsk.lhs) +'; width:1024px; height:200px; position:relative; overflow-x:hidden;',
			   '<pre><code id="id-fsk-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-fsk-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
            w3_col_percent('',
               w3_div('',
				      w3_div('w3-show-inline-block w3-medium w3-text-aqua', '<b><a href="https://en.wikipedia.org/wiki/Frequency-shift_keying" target="_blank">FSK</a> decoder</b>')
				   ), 50,
					w3_div('', 'From <b><a href="https://arachnoid.com/JNX/index.html" target="_blank">JNX</a></b> by P. Lutus &copy; 2011'), 50
				),
				
            w3_col_percent('',
               w3_div('id-fsk-station w3-text-css-yellow', '&nbsp;'), 50,
               w3_div('', 'Please <a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');">report</a> new stations for menus.'), 50
            ),

				w3_col_percent('',
               w3_select_hier('w3-text-red', 'Weather', 'select', 'fsk.menu0', fsk.menu0, fsk_weather, 'fsk_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Maritime', 'select', 'fsk.menu1', fsk.menu1, fsk_maritime, 'fsk_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Military', 'select', 'fsk.menu2', fsk.menu2, fsk_military, 'fsk_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Ham/Utility', 'select', 'fsk.menu3', fsk.menu3, fsk_ham_utility, 'fsk_pre_select_cb'), 25
            ),

            w3_inline('/w3-margin-between-16',
               w3_inline('',
                  w3_select('|color:red', '', 'shift', 'fsk.shift_i', W3_SELECT_SHOW_TITLE, fsk_shift_s, 'fsk_shift_cb'),
                  w3_input('id-fsk-shift-custom w3-margin-left w3-hide|padding:0;width:auto|size=4', '', 'fsk.shift_cval', fsk.shift_cval, 'fsk_shift_custom_cb')
               ),

               w3_inline('',
                  w3_select('|color:red', '', 'baud', 'fsk.baud_i', W3_SELECT_SHOW_TITLE, fsk_baud_s, 'fsk_baud_cb'),
                  w3_input('id-fsk-baud-custom w3-margin-left w3-hide|padding:0;width:auto|size=4', '', 'fsk.baud_cval', fsk.baud_cval, 'fsk_baud_custom_cb')
               ),

               w3_select('|color:red', '', 'framing', 'fsk.framing', fsk.framing, fsk_framing_s, 'fsk_framing_cb'),

               w3_select('|color:red', '', 'encoding', 'fsk.encoding', fsk.encoding, fsk_encoding_s, 'fsk_encoding_cb'),

               w3_checkbox('w3-label-inline w3-label-not-bold/', 'inverted', 'fsk.inverted', fsk.inverted, 'fsk_inverted_cb')
            ),

            w3_inline('/w3-margin-between-16',
					w3_button('w3-padding-smaller', 'Next', 'fsk_next_prev_cb', 1),
					w3_button('w3-padding-smaller', 'Prev', 'fsk_next_prev_cb', -1),

               w3_select('|color:red', '', 'mode', 'fsk.mode', 0, fsk_mode_s, 'fsk_mode_cb'),

               w3_div('',
                  w3_inline('id-fsk-decode/',
                     w3_button('w3-padding-smaller', 'Clear', 'fsk_clear_cb', 0)
                  ),
   
                  w3_inline('id-fsk-framing w3-hide/w3-margin-between-16',
                     w3_button('w3-padding-smaller', 'Sample', 'fsk_sample_cb', 0),
                     w3_select('|color:red', '', 'bits/word', 'fsk.fr_bpw', 0, '5:15', 'fsk_bpw_cb'),
                     w3_select('|color:red', '', 'phase', 'fsk.fr_phase', 0, '0:15', 'fsk_phase_cb'),
                     w3_select('|color:red', '', 'bits/data', 'fsk.fr_bpw', 0, fsk_bpd_s, 'fsk_bpd_cb')
                  ),
   
                  w3_inline('id-fsk-scope w3-hide/w3-margin-between-16',
                     w3_button('w3-padding-smaller', 'Single', 'fsk_single_cb', 0),
                     w3_select('|color:red', '', 'decim', 'fsk.decim', 3, fsk_decim_s, 'fsk_decim_cb')
                  )
               )
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('fsk');

   // URL params that need to be setup after controls instantiated
	if (fsk.url_params) {
      p = fsk.url_params.split(',');
      p.forEach(function(a, i) {
         //console.log('FSK param2 <'+ a +'>');
         if (w3_ext_param('help', a).match) {
            extint_help_click();
         }
      });
   }

	fsk_setup();

	fsk.canvas = w3_el('id-fsk-canvas');
	fsk.canvas.ctx = fsk.canvas.getContext("2d");
	fsk_baud_error_init();

	ext_set_controls_width_height(650, 200);
	
	// first URL param can be a match in the preset menus
	if (fsk.url_params) {
      var freq = parseFloat(fsk.url_params);
      if (!isNaN(freq)) {
         // select matching menu item frequency
         var found = false;
         for (var i = 0; i < fsk.n_menu; i++) {
            var menu = 'fsk.menu'+ i;
            w3_select_enum(menu, function(option) {
               //console.log('CONSIDER '+ parseFloat(option.innerHTML));
               if (!found && parseFloat(option.innerHTML) == freq) {
                  fsk_pre_select_cb(menu, option.value, false);
                  found = true;
               }
            });
            if (found) break;
         }
      }
   }

   // because w3_input() doesn't yet have instantiation callback
   if (fsk.shift_custom)
      fsk_shift_custom_cb('id-fsk-shift-custom', fsk.shift);
   if (fsk.baud_custom)
      fsk_baud_custom_cb('id-fsk-baud-custom', fsk.baud);

	
	// receive the network-rate, post-decompression, real-mode samples
	ext_register_audio_data_cb(fsk_audio_data_cb);
}

function fsk_setup()
{
	fsk.freq = ext_get_freq()/1e3;
	fsk.baud_mult = fsk.framing.endsWith('.5')? 2:1;
	var baud = fsk.baud * fsk.baud_mult;
   //console.log('FSK SETUP freq='+ fsk.freq +' cf='+ fsk.cf +' shift='+ fsk.shift +' framing='+ fsk.framing +' enc='+ fsk.encoding +' inv='+ fsk.inverted);
	//console.log('FSK SETUP baud: '+ fsk.baud +'*'+ fsk.baud_mult +' = '+ baud);
	fsk.jnx.setup_values(ext_sample_rate(), fsk.cf, fsk.shift, baud, fsk.framing, fsk.inverted, fsk.encoding);
	//console.log('fsk_setup ext_get_freq='+ ext_get_freq()/1e3 +' ext_get_carrier_freq='+ ext_get_carrier_freq()/1e3 +' ext_get_mode='+ ext_get_mode())
   fsk.encoder = fsk.jnx.get_encoding_obj();

   var z = ext_get_zoom();
   ext_tune(fsk.freq, 'cw', ext_zoom.ABS, z);
   var pb_half = Math.max(fsk.shift, fsk.baud) /2;
   var pb_edge = Math.round(((pb_half * 0.2) + 10) / 10) * 10;
   pb_half += pb_edge;
   ext_set_passband(fsk.cf - pb_half, fsk.cf + pb_half);
   ext_tune(fsk.freq, 'cw', ext_zoom.ABS, z);      // set again to get correct freq given new passband
   
   // set matching entries in menus
   var shift = fsk.shift_custom? 'custom' : fsk.shift;
   w3_select_set_if_includes('fsk.shift_i', '\\b'+ shift +'\\b');
   var baud = fsk.baud_custom? 'custom' : fsk.baud;
   w3_select_set_if_includes('fsk.baud_i', '\\b'+ baud +'\\b');
   w3_select_set_if_includes('fsk.framing', '\\b'+ fsk.framing +'\\b');
   w3_select_set_if_includes('fsk.encoding', '\\b'+ fsk.encoding +'\\b');
   w3_checkbox_set('fsk.inverted', fsk.inverted);
   
   fsk_crosshairs(1);
}

function fsk_crosshairs(vis)
{
   var ct = canvas_annotation.ctx;
   ct.clearRect(0,0, window.innerWidth,24);
   
   if (vis && ext_get_zoom() >= 10) {
      var f = ext_get_freq();
      var f_range = get_visible_freq_range();
      var Lpx = scale_px_from_freq(f - fsk.shift/2, f_range);
      var Rpx = scale_px_from_freq(f + fsk.shift/2, f_range);
      //console.log('FSK crosshairs f='+ f +' Lpx='+ Lpx +' Rpx='+ Rpx);
      var d = 3;
      for (var i = 0; i < 6; i++) {
         var y = i*d;
         ct.fillStyle = (i&1)? 'black' : 'white';
         ct.fillRect(Lpx-d,y, d,d);
         ct.fillRect(Rpx-d,y, d,d);
         ct.fillStyle = (i&1)? 'white' : 'black';
         ct.fillRect(Lpx,y, d,d);
         ct.fillRect(Rpx,y, d,d);
      }
   }
}

function fsk_pre_select_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
	var menu_n = parseInt(path.split('fsk.menu')[1]);
   //console.log('fsk_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);

   // find matching object entry in fsk_menus[] hierarchy and set fsk.* parameters from it
	w3_select_enum(path, function(option) {
	   //console.log('fsk_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled) {
	      fsk.header = option.innerHTML;
	   }
	   
	   if (option.value != idx) return;
	   
      fsk.menu_sel = option.innerHTML +' ';
      //console.log('fsk_pre_select_cb opt.val='+ option.value +' menu_sel='+ fsk.menu_sel +' opt.id='+ option.id);

      var id = option.id.split('id-')[1];
      id = id.split('-');
      var i = id[0];
      var j = id[1];
      //console.log('fsk_pre_select_cb i='+ i +' j='+ j);
      var o = w3_obj_seq_el(fsk_menus[menu_n], i);
      //w3_console_obj(o);
      o = w3_obj_seq_el(o, j);
      //w3_console_obj(o);
   
      if (fsk.header == 'Test mode') {
         fsk.test_mode = true;
         console.log('test mode');
      } else {
         fsk.test_mode = false;
         fsk.framing = o.fr;
         fsk.freq = parseFloat(o.f) + ((fsk.framing == 'CHU')? fsk.CHU_offset : 0);
         fsk.cf = o.hasOwnProperty('cf')? o.cf : 1000;
         fsk.shift = o.s;
         fsk.baud = o.b;
         fsk.shift_custom = fsk.baud_custom = false;
         fsk.inverted = o.i;
         fsk.encoding = o.e;
   
         // set freq here because fsk_setup() recalls current freq in case it has been manually tuned
         var z = Math.min(ext_get_zoom(), 12);
         ext_tune(fsk.freq, 'cw', ext_zoom.ABS, z);
         fsk_setup();
   
         // if called directly instead of from menu callback, select menu item
         w3_select_value(path, idx);
      }
   
      w3_el('id-fsk-station').innerHTML =
         '<b>Station: '+ fsk_menu_s[menu_n] +', '+ fsk.header +'</b>';
	});

   // reset other frequency menus
   for (var i = 0; i < fsk.n_menu; i++) {
      if (i != menu_n)
         w3_select_value('fsk.menu'+ i, -1);
   }
}

function fsk_environment_changed(changed)
{
   //w3_console_obj(changed);
   fsk_crosshairs(1);

   // reset all frequency menus when frequency etc. is changed by some other means (direct entry, WF click, etc.)
   // but not for changed.zoom, changed.resize etc.
   var dsp_freq = ext_get_freq()/1e3;
   var mode = ext_get_mode();
   //console.log('FSK ENV fsk.freq='+ fsk.freq +' dsp_freq='+ dsp_freq +' mode='+ mode);
   if (fsk.freq != dsp_freq || mode != 'cw') {
      for (var i = 0; i < fsk.n_menu; i++) {
         w3_select_value('fsk.menu'+ i, -1);
      }
      fsk.menu_sel = '';
      w3_el('id-fsk-station').innerHTML = '&nbsp;';
   }
}

function fsk_shift_cb(path, idx, first)
{
   //if (first) return;
   var shift_s = fsk_shift_s[+idx];
   var custom = (idx == fsk.SHIFT_CUSTOM_IDX);
   //console.log('fsk_shift_cb idx='+ idx +' shift_s='+ shift_s +' custom='+ custom);
   w3_show_hide('id-fsk-shift-custom', custom);
   if (custom) {
      fsk.shift = w3_get_value('id-fsk-shift-custom');
	   fsk.shift_custom = true;
   } else {
      fsk.shift = + shift_s;
	   fsk.shift_custom = false;
   }
   fsk_setup();
}

function fsk_shift_custom_cb(path, val)
{
	var shift = parseFloat(val);
	if (!shift || isNaN(shift) || shift <= 0 || shift > 5000) return;
   //console.log('fsk_shift_custom_cb path='+ path +' val='+ val +' shift='+ shift);
	w3_set_value(path, shift);
	fsk.shift_cval = fsk.shift = shift;
	fsk.shift_custom = true;
	w3_show_hide('id-fsk-shift-custom', true);
   fsk_setup();
}

function fsk_baud_cb(path, idx, first)
{
   //if (first) return;
   var baud_s = fsk_baud_s[+idx];
   var custom = (idx == fsk.BAUD_CUSTOM_IDX);
   //console.log('fsk_baud_cb idx='+ idx +' baud_s='+ baud_s +' custom='+ custom);
   w3_show_hide('id-fsk-baud-custom', custom);
   if (custom) {
      fsk.baud = w3_get_value('id-fsk-baud-custom');
	   fsk.baud_custom = true;
   } else {
      fsk.baud = + baud_s;
	   fsk.baud_custom = false;
   }
   fsk_setup();
}

function fsk_baud_custom_cb(path, val)
{
	var baud = parseFloat(val);
	if (!baud || isNaN(baud) || baud <= 0 || baud > 5000) return;
   //console.log('fsk_baud_custom_cb path='+ path +' val='+ val +' baud='+ baud);
	w3_set_value(path, baud);
	fsk.baud_cval = fsk.baud = baud;
	fsk.baud_custom = true;
	w3_show_hide('id-fsk-baud-custom', true);
   fsk_setup();
}

function fsk_framing_cb(path, idx, first)
{
   if (first) return;
   var framing = fsk_framing_s[idx];
   //console.log('fsk_framing_cb idx='+ idx +' framing='+ framing);
   if (framing != 'custom')
      fsk.framing = framing;
   fsk_setup();
}

function fsk_encoding_cb(path, idx, first)
{
   if (first) return;
   var encoding = fsk_encoding_s[idx];
   //console.log('fsk_encoding_cb idx='+ idx +' encoding='+ encoding);
   if (encoding != 'custom')
      fsk.encoding = encoding;
   fsk_setup();
}

function fsk_inverted_cb(path, checked, first)
{
   if (first) return;
   checked = checked? 1:0;
   //console.log('fsk_inverted_cb checked='+ checked);
   fsk.inverted = checked;
   w3_checkbox_set(path, checked);
   fsk_setup();
}

function fsk_next_prev_cb(path, np, first)
{
	np = +np;
	//console.log('fsk_next_prev_cb np='+ np);
	
   // if any menu has a selection value then select next/prev (if any)
   var prev = 0, capture_next = 0, captured_next = 0, captured_prev = 0;
   var menu;
   
   for (var i = 0; i < fsk.n_menu; i++) {
      menu = 'fsk.menu'+ i;
      var el = w3_el(menu);
      var val = el.value;
      //console.log('menu='+ menu +' value='+ val);
      if (val == -1) continue;
      
      w3_select_enum(menu, function(option) {
	      if (option.disabled) return;
         if (capture_next) {
            captured_next = option.value;
            capture_next = 0;
         }
         if (option.value === val) {
            captured_prev = prev;
            capture_next = 1;
         }
         prev = option.value;
      });
      break;
   }

	//console.log('i='+ i +' captured_prev='+ captured_prev +' captured_next='+ captured_next);
	val = 0;
	if (np == 1 && captured_next) val = captured_next;
	if (np == -1 && captured_prev) val = captured_prev;
	if (val) {
      fsk_pre_select_cb(menu, val, false);
   }
}

function fsk_mode_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
   fsk.decode = fsk.scope = fsk.show_framing = 0;

   switch (idx) {
   
   case 0:  // decode
   default:
      fsk.decode = 1;
      break;

   case 1:  // scope
      fsk.scope = 1;
      fsk.run = 1;
      fsk_scope_reset_display();
      break;

   case 2:  // framing
      fsk.show_framing = 1;
      fsk_framing_reset();
      break;
   }

   fsk.jnx.set_scope_cb(fsk.scope? fsk_scope : null);
   fsk.jnx.set_output_bit_cb(fsk.show_framing? fsk_framing : null);

   w3_show_hide_inline('id-fsk-decode', fsk.decode);
   w3_show_hide_inline('id-fsk-console-msg', fsk.decode);
   w3_show_hide_inline('id-fsk-scope', fsk.scope);
   w3_show_hide_inline('id-fsk-framing', fsk.show_framing);
}

function fsk_clear_cb(path, idx, first)
{
   fsk_console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-fsk-console-msgs', 'id-fsk-console-msg', fsk_console_status_msg_p);
}

function fsk_single_cb(path, idx, first)
{
   if (first) return;
   //console.log('fsk_single_cb single='+ fsk.single +' run='+ fsk.run);
   if (fsk.single) fsk.run = 1;
   fsk.single ^= 1;
   w3_innerHTML(path, fsk.single? 'Run' : 'Single');
}

function fsk_sample_cb(path, idx, first)
{
   if (first) return;
   //console.log('fsk_sample_cb');
   fsk_framing_reset();
}

function fsk_decim_cb(path, idx, first)
{
   if (first) return;
   fsk.decim = [ 1, 2, 4, 8, 16, 32 ] [idx];
   //console.log('fsk_decim_cb idx='+ idx +' decim='+ fsk.decim);
}

function fsk_bpw_cb(path, idx, first)
{
   if (first) return;
   fsk.fr_bpw = [ 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 ] [idx];
   //console.log('fsk_bpw_cb idx='+ idx +' bpw='+ fsk.fr_bpw);
   
   // if bits have been sampled redraw using new bpw
   if (fsk.fr_bits.length) {
      fsk_framing_reset_display();
      for (var i = 0; i < (fsk.fr_bits.length - fsk.fr_phase); i++) {
         fsk_framing_proc(fsk.fr_bits[i + fsk.fr_phase]);
      }
   }
}

function fsk_phase_cb(path, idx, first)
{
   if (first) return;
   fsk.fr_phase = +idx;
   //console.log('fsk_phase_cb idx='+ idx +' phase='+ fsk.fr_phase);
   fsk_phase();
}

function fsk_bpd_cb(path, idx, first)
{
   if (first) return;
   fsk.fr_bpd = [ 0, 5, 6, 7, 8 ] [idx];
   //console.log('fsk_bpd_cb idx='+ idx +' bpd='+ fsk.fr_bpd);
   fsk_phase();
}

function fsk_blur()
{
	ext_unregister_audio_data_cb();
   ext_set_passband(fsk.saved_passband.low, fsk.saved_passband.high);
   fsk_crosshairs(0);
}

function fsk_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'FSK decoder help') +
         '<br>Decoding FSK is not always easy because of the many signal parameters involved. <br>' +
         'Try the stations listed in the menus. Most of these are best heard from Kiwis in Europe. <br><br>' +

         'The frequency shift can be set by zooming in sufficiently, centering the passband between <br>' +
         'the two tones, and selecting a menu shift (or setting a custom shift) so that the <br>' +
         'checkered crosshairs align on the tones. The scope and framing modes are to assist ' +
         'in setting the correct baud rate and framing. <br><br>' +
         
         'URL parameters: <br>' +
         'First parameter can be a frequency matching an entry in station menus. <br>' +
         'shift:<i>num</i> &nbsp; baud:<i>num</i> &nbsp; framing:<i>value</i> &nbsp; encoding:<i>value</i> &nbsp; inverted<i>[:0|1]</i> <br>' +
         'Values are those appearing in their respective menus. <br>' +
         'Any number for shift and baud can be used. Not just the preset values in the menus. <br>' +
         'Keywords are case-insensitive and can be abbreviated. <br>' +
         'So for example this is valid: <i>&ext=fsk,147.3,s:425,b:200,a,i:0</i> <br>' +
         '';
      confirmation_show_content(s, 610, 300);
   }
   return true;
}
