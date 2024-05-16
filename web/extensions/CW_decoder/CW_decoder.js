// Copyright (c) 2018 John Seamons, ZL4VO/KF6VO

var cw = {
   ext_name: 'CW_decoder',    // NB: must match cw_decoder.cpp:cw_decoder_ext.name
   first_time: true,
   pboff: -1,
   wspace: true,
   test: 0,
   
   sig_color_auto: 'black',
   sig_color_pos: 'black',
   sig_color_neg: 'red',

   threshold_dB: 47,
   is_auto_thresh: 0,
   auto_thresh_dB: 0,
   threshold_fixed_color: 'red',
   threshold_auto_color: 'cyan',
   
   is_auto_wpm: true,
   auto_wpm: 0,
   fixed_wpm: 0,
   default_wpm: 10,
   training_interval: 100,
   training_interval_s: [ 100, 50 ],

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, cols: 135 },

   log_mins: 0,
   log_interval: null,
   log_txt: '',

   last_last: 0
};

function CW_decoder_main()
{
	ext_switch_to_client(cw.ext_name, cw.first_time, cw_decoder_recv);		// tell server to use us (again)
	if (!cw.first_time)
		cw_decoder_controls_setup();
	cw.first_time = false;
}

function cw_decoder_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('cw_decoder_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('cw_decoder_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('cw_decoder_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				kiwi_load_js('pkgs/js/graph.js', 'cw_decoder_controls_setup');
				break;

			case "cw_chars":
				cw_decoder_output_chars(param[1]);
				break;

			case "cw_wpm":
			   if (cw.is_auto_wpm) {
			      cw.auto_wpm = +param[1];
				   w3_innerHTML('id-cw-auto-wpm', '<x0>'+ cw.auto_wpm +'</x0> <b>WPM</b>');
				}
				break;

			case "cw_train":
			   var el = w3_el('id-cw-train-count');
			   if (!el) break;
			   var p = +param[1];
			   if (p < 0)
			      w3_innerHTML(el, 'error '+ (-p) +'/4');
			   else
			      w3_innerHTML(el, 'train '+ p +'/'+ cw.training_interval);
			   w3_background_color(el, (p < 0)? 'orange':'lime');
            w3_show_hide(el, p != 0);
            w3_show_hide('id-cw-train-select', p == 0);
				break;
			
			case "cw_plot":
			   var a = param[1].split(',');
			   var p = +a[0];
			   if (!isNumber(p)) break;
			   var polarity = +a[1];
			   var auto_th = +a[2];
			   //auto_th = 0;   //jksx
			   if (cw.is_auto_thresh) graph_threshold(cw.gr, auto_th, cw.threshold_auto_color);
			   graph_plot(cw.gr, p, { color: polarity? (cw.is_auto_thresh? cw.sig_color_pos : cw.sig_color_auto) : cw.sig_color_neg});
			   break;

			case "bar_pct":
			   if (cw.test) {
               var pct = w3_clamp(parseInt(param[1]), 0, 100);
               //if (pct > 0 && pct < 3) pct = 3;    // 0% and 1% look weird on bar
               var el = w3_el('id-cw-bar');
               if (el) el.style.width = pct +'%';
               //console.log('bar_pct='+ pct);
            }
			   break;

			case "test_done":
			   //cw.test = 0;
			   console.log('test done');
			   break;

			default:
				console.log('cw_decoder_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function cw_decoder_output_chars(c)
{
   cw.console_status_msg_p.s = c;      // NB: already encoded on C-side
   cw.log_txt += kiwi_remove_escape_sequences(kiwi_decodeURIComponent('CW', c));

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-cw-console-msgs', 'id-cw-console-msg', cw.console_status_msg_p);
}

function cw_decoder_controls_setup()
{
	var p = ext_param();
	if (p) {
      p = p.split(',');
      var do_test = false, do_help = false;
      p.forEach(
         function(a, i) {
            console.log('p'+ i +'='+ a);
            var r;
            if (i == 0 && isNumber(a)) {
               cw.pboff_locked = a;
               console.log('CW pboff_locked='+ cw.pboff_locked);
            } else
            if ((r = w3_ext_param('wpm', a)).match) {
               if (isNumber(r.num)) {
                  console.log('WPM='+ r.num);
                  cw.is_auto_wpm = false;
                  cw.fixed_wpm = r.num;
               }
            } else
            if ((r = w3_ext_param('auto', a)).match) {
               cw.is_auto_thresh = 1;
               if (isNumber(r.num)) {
                  // debug: set threshold value used in auto mode instead of default
                  cw.auto_thresh_dB = r.num;
               }
            } else
            if (w3_ext_param('test', a).match) {
               do_test = true;
            } else
            if (w3_ext_param('help', a).match) {
               do_help = true;
            }
         }
      );
   }
	
   var data_html =
      time_display_html('cw') +
      
      w3_div('id-cw-data|left:150px; width:1044px; height:300px; overflow:hidden; position:relative; background-color:mediumBlue;',
         '<canvas id="id-cw-canvas" width="1024" height="180" style="position:absolute; padding: 10px"></canvas>',
			w3_div('id-cw-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|top:200px; width:1024px; height:100px; position:absolute; overflow-x:hidden;',
			   '<pre><code id="id-cw-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-cw-controls w3-text-white',
			w3_divs('',
            w3_col_percent('',
               w3_div('',
				      w3_div('w3-medium w3-text-aqua', '<b>CW decoder</b>')
				   ), 32,
					w3_div('', 'Loftur Jonasson, TF3LJ, <br> <b><a href="https://github.com/df8oe/UHSDR" target="_blank">UHSDR project</a></b> &copy; 2016'
					), 50
				),
				
				w3_inline('w3-margin-T-10/w3-margin-between-16',
               w3_inline('w3-round-large w3-padding-small w3-text-white w3-grey|width:146px;height:58px/',
                  w3_div('|width:65px',
                     w3_input('id-cw-fixed-wpm w3-up-down w3-margin-R-8 w3-hide/w3-label-not-bolx/|padding:0;width:auto|size=3', 'WPM', 'cw.fixed_wpm', cw.fixed_wpm, 'cw_decoder_wpm_cb'),
                     w3_inline('id-cw-auto-wpm', '<x0>0</x0> <b>WPM</b>')
                  ),
                  w3_checkbox('w3-margin-L-16/w3-label-inline w3-label-not-bold/', 'auto', 'cw.is_auto_wpm', cw.is_auto_wpm, 'cw_decoder_auto_wpm_cb')
               ),
               w3_div('',
                  w3_inline('',
                     w3_button('id-cw-train-button w3-css-lime w3-padding-smaller', 'Train', 'cw_train_cb', 0),
                     w3_select('id-cw-train-select w3-margin-L-16 w3-text-red w3-margin-T-4 w3-width-auto w3-hide', '', 'interval',
                        'cw.training_interval', cw.training_interval, cw.training_interval_s, 'cw_training_interval_cb'),
                     w3_div('id-cw-train-count w3-margin-L-16 w3-padding-small w3-text-black w3-hide', 'train')
                  ),
                  w3_inline('',
                     w3_button('w3-margin-T-8 w3-padding-smaller', 'Clear', 'cw_clear_button_cb', 0),
                     w3_checkbox('w3-margin-L-16 w3-margin-T-8/w3-label-inline w3-label-not-bold/', 'word space corr', 'cw.wspace', true, 'cw_decoder_wsc_cb')
                  )
               )
            ),
            
				w3_inline('w3-margin-T-10/w3-margin-between-16',
               w3_inline('w3-round-large w3-padding-small w3-text-white w3-grey|width:146px;height:58px/',
                  w3_input('id-cw-fixed-thresh w3-up-down/w3-label-not-bolx/|padding:0;width:auto|size=4', 'Threshold', 'cw.threshold_dB', cw.threshold_dB, 'cw_decoder_threshold_cb'),
                  w3_checkbox('w3-margin-L-8/w3-label-inline w3-label-not-bold/', 'auto', 'cw.is_auto_thresh', cw.is_auto_thresh, 'cw_decoder_auto_thresh_cb')
               ),
               w3_div('',
                  w3_inline('',
                     w3_button('id-cw-test w3-padding-smaller w3-green', 'Test', 'cw_test_cb', 0),
                     w3_div('id-cw-bar-container w3-margin-L-16 w3-progress-container w3-round-large w3-white w3-hide|width:70px; height:16px',
                        w3_div('id-cw-bar w3-progressbar w3-round-large w3-light-green|width:0%', '&nbsp;')
                     )
                  ),
                  w3_inline('',
                     w3_button('id-cw-log w3-margin-T-8 w3-padding-smaller w3-purple', 'Log', 'cw_log_cb'),
                     w3_input('id-cw-log-mins w3-margin-L-16/w3-label-not-bold/|padding:0;width:auto|size=4',
                        'log min', 'cw.log_mins', cw.log_mins, 'cw_log_mins_cb')
                  )
               )
				)
			)
		);
	
	cw.saved_setup = ext_save_setup();
	ext_set_mode('cw');
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('cw');

	cw.canvas = w3_el('id-cw-canvas');
	cw.canvas.ctx = cw.canvas.getContext("2d");

   cw.gr = graph_init(cw.canvas, { dBm:0, speed:1, averaging:true });
	//graph_mode(cw.gr, 'auto');
	graph_mode(cw.gr, 'fixed', 55-10, 30+5);
	graph_clear(cw.gr);

   ext_set_data_height(300);
	ext_set_controls_width_height(380, 200);
	
   ext_send('SET cw_start='+ cw.training_interval);
	cw.pboff = -1;
	CW_decoder_environment_changed();

	cw_decoder_auto_wpm_cb('', cw.is_auto_wpm);     // prevent content flash when URL "wpm:N" used
   cw_clear_button_cb();
	if (do_test) cw_test_cb();
	if (do_help) ext_help_click();
}

function CW_decoder_environment_changed(changed)
{
   // detect passband offset change and inform C-side so detection filter can be adjusted
   var pboff = Math.abs(ext_get_passband_center_freq() - ext_get_carrier_freq());
   if (cw.pboff != pboff) {
      var first_and_locked = (cw.pboff == -1 && cw.pboff_locked);
      var pbo = first_and_locked? cw.pboff_locked : pboff;
	   if (first_and_locked || !cw.pboff_locked) {
         console.log('CW ENV new pbo='+ pbo);
	      ext_send('SET cw_pboff='+ pbo);
	   }
      cw.pboff = pboff;
   }
}

function cw_clear_button_cb(path, idx, first)
{
   if (first) return;
   cw.console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-cw-console-msgs', 'id-cw-console-msg', cw.console_status_msg_p);
   cw.log_txt = '';
}

function cw_log_mins_cb(path, val)
{
   cw.log_mins = w3_clamp(+val, 0, 24*60, 0);
   console.log('cw_log_mins_cb path='+ path +' val='+ val +' log_mins='+ cw.log_mins);
	w3_set_value(path, cw.log_mins);

   kiwi_clearInterval(cw.log_interval);
   if (cw.log_mins != 0) {
      console.log('CW logging..');
      cw.log_interval = setInterval(function() { cw_log_cb(); }, cw.log_mins * 60000);
   }
}

function cw_log_cb()
{
   var txt = new Blob([cw.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = kiwi_timestamp_filename('CW.', '.log.txt');
   document.body.appendChild(a);
   console.log('cw_log: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);
}

function cw_decoder_set_threshold()
{
   var auto = cw.is_auto_thresh? 1:0;
   ext_send('SET cw_auto_thresh='+ auto);
   var th_dB = 0;
   if (!auto) th_dB = cw.threshold_dB;
   if (auto && cw.auto_thresh_dB != 0) th_dB = cw.auto_thresh_dB;
	if (th_dB) ext_send('SET cw_threshold='+ Math.pow(10, th_dB/10).toFixed(0));
}

function cw_decoder_set_wpm(wpm)
{
   // because setting wpm calls CwDecode_Init() the threshold must be setup immediately afterwards
   ext_send('SET cw_wpm='+ wpm +','+ cw.training_interval);
   cw_decoder_set_threshold();
}

function cw_decoder_auto_wpm_cb(path, checked, first)
{
   if (first) return;
   console.log('cw_decoder_auto_wpm_cb path='+ path +' checked='+ checked +' fixed_wpm='+ cw.fixed_wpm +' training_interval='+ cw.training_interval);
   cw.is_auto_wpm = checked;
   var el_fixed = w3_el('id-cw-fixed-wpm');
   var el_auto = w3_el('id-cw-auto-wpm');
   w3_disable('id-cw-train-button', !checked);
   
   if (checked) {
      w3_hide2(el_fixed, true);
      w3_innerHTML(el_auto, '<x0>0</x0> <b>WPM</b>');
      w3_hide2(el_auto, false);
      cw_decoder_set_wpm(0);
   } else {
      // setup fixed wpm to last auto wpm if it never had a value
      w3_hide2(el_auto, true);
      var el_fixed_inp = w3_el('id-cw.fixed_wpm');
      var v = el_fixed_inp.value;
      console.log('fixed_inp.val=<'+ v +'> auto_wpm='+ cw.auto_wpm);
      if (isUndefined(v) || v == '' || v == 0) {
         cw.fixed_wpm = cw.auto_wpm? cw.auto_wpm : cw.default_wpm;
         el_fixed_inp.value = cw.fixed_wpm;
      }
      w3_hide2(el_fixed, false);
      cw_decoder_set_wpm(cw.fixed_wpm);
   }
   cw.pboff = -1;
   CW_decoder_environment_changed();
}

function cw_decoder_wpm_cb(path, val)
{
   console.log('cw_decoder_wpm_cb path='+ path +' val='+ val);
   wpm = +val;
   if (isNumber(wpm)) {
      cw.fixed_wpm = wpm;
      cw_decoder_set_wpm(wpm);
      cw.pboff = -1;
      CW_decoder_environment_changed();
   }
}

function cw_decoder_wsc_cb(path, checked, first)
{
   //if (first) return;
   ext_send('SET cw_wsc='+ (checked? 1:0));
}

function cw_decoder_auto_thresh_cb(path, checked, first)
{
   //if (first) return;
   console.log('cw_decoder_auto_thresh_cb path='+ path +' checked='+ checked);
   cw.is_auto_thresh = checked? 1:0;
   w3_checkbox_set(path, checked);     // for benefit of direct callers
   w3_disable('id-cw-fixed-thresh', checked);
   if (!cw.is_auto_thresh)
	   graph_threshold(cw.gr, cw.threshold_dB, cw.threshold_fixed_color);
	cw_decoder_set_threshold();
}

function cw_decoder_threshold_cb(path, val)
{
	var threshold_dB = parseFloat(val);
	if (!threshold_dB || isNaN(threshold_dB)) return;
   console.log('cw_decoder_threshold_cb path='+ path +' val='+ val +' threshold_dB='+ threshold_dB);
	w3_num_cb(path, threshold_dB);
	cw.threshold_dB = threshold_dB;
	graph_threshold(cw.gr, cw.threshold_dB, cw.threshold_fixed_color);
	cw_decoder_set_threshold();
}

function cw_train_cb(path, idx, first)
{
   if (first) return;
   console.log('cw_train_cb training_interval='+ cw.training_interval);
   ext_send('SET cw_train='+ cw.training_interval);
	cw.pboff = -1;
	CW_decoder_environment_changed();
   console.log('cw_train_cb threshold_dB='+ Math.pow(10, cw.threshold_dB/10).toFixed(0));
   cw_decoder_set_threshold();
}

function cw_training_interval_cb(path, idx, first)
{
   if (first) return;
   cw.training_interval = cw.training_interval_s[+idx];
   console_nv('cw_training_interval_cb', {path}, {idx}, 'cw.training_interval');
}

function cw_test_cb()
{
   console_nv('cw_test_cb', 'cw.test');
   cw.test = cw.test? 0:1;    // if already running stop if clicked again
   w3_colors('id-cw-test', '', 'w3-red', cw.test);
	w3_el('id-cw-bar').style.width = '0%';
   w3_show_hide('id-cw-bar-container', cw.test);
   ext_send('SET cw_test='+ cw.test);
}

function CW_decoder_blur()
{
	ext_set_data_height();     // restore default height
	ext_send('SET cw_stop');
   kiwi_clearInterval(cw.log_interval);
	ext_restore_setup(cw.saved_setup);
}

// called to display HTML for configuration parameters in admin interface
function CW_decoder_config_html()
{
   ext_config_html(cw, 'cw', 'CW', 'CW decoder configuration');
}

function CW_decoder_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'CW decoder help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               '<br>Use the CWN mode with its narrow passband to maximize the signal/noise ratio. <br>' +
               'The decoder doesn\'t do very well with weak or fading signals. <br><br>' +
         
               'Turning off <x1>auto</x1> WPM training and using a manual setting works best for CW signals ' +
               'that are intermittent. The <x1>interval</x1> menu sets the length of the training period. <br><br>' +
         
               'Adjust the <x1>threshold</x1> value so the red line in the signal level display is just under the <br>' +
               'average value of the signal peaks. Or use the <x1>auto</x1> mode.<br><br>' +
         
               'The <x1>word space corr</x1> checkbox sets the algorithm used to determine word space corrections.<br><br>' +

               'URL parameters: <br>' +
               w3_text('|color:orange', 'wpm:<i>num</i> &nbsp; auto') +
               '<br> wpm sets a fixed value instead of the default auto WPM training. <br>' +
               'auto will automatically determine the threshold value instead of the fixed default.' +
               ''
            )
         );
      confirmation_show_content(s, 600, 350);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}
