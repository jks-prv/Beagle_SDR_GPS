// Copyright (c) 2021-2024 John Seamons, ZL4VO/KF6VO


var wfext = {    // "wf" is already used elsewhere
   sfmt: 'w3-text-red',

   aper_algo: 3,
   aper_algo_s: [ 'IIR', 'MMA', 'EMA', 'off' ],
   aper_algo_e: { IIR:0, MMA:1, EMA:2, OFF:3 },
   aper_param: 0,
   aper_param_s: [ 'gain', 'averages', 'averages', '' ],
   aper_params: {
      IIR_min:0, IIR_max:2, IIR_step:0.1, IIR_def:0.8, IIR_val:undefined,
      MMA_min:1, MMA_max:16, MMA_step:1, MMA_def:2, MMA_val:undefined,
      EMA_min:1, EMA_max:16, EMA_step:1, EMA_def:2, EMA_val:undefined
   },
   
   spb_on: 1,
   spb_color: '#ffffff44',
   spb_color_seq: 0,
   
   tstamp: 0,
   tstamp_f: '',
   tstamp_i: 0,
   tstamp_s: [ 'off', '2s', '5s', '10s', '30s', '1m', '5m', '10m', '30m', '60m', 'custom' ],
   tstamp_v: [ 0, 2, 5, 10, 30, 60, 300, 600, 1800, 3600, -1 ],

   tstamp_tz_s: [ 'UTC', 'local' ],
   
   winf_i: 2,
   winf_s: [ 'Hanning', 'Hamming', 'Blackman-Harris', 'none' ],
   
   interp_i: 3,
   interp_s: [ 'max', 'min', 'last', 'drop samp', 'CMA' ],
   
   cic_comp: true
};

function waterfall_view()
{
   keyboard_shortcut_nav('wf');
   w3_scrollTo('id-optbar-content', 0.65);   // empirically measured
}

function waterfall_controls_setup()
{
	var controls_html =
		w3_div('id-waterfall-controls w3-text-white',
		   w3_divs('w3-margin-T-8',
            w3_text('w3-margin-B-2 w3-text-css-orange', '<b>Aperture auto mode</b>'),

            w3_inline('w3-valign w3-margin-LR-16 w3-gap-16/',
               w3_select('id-wfext-aper-algo '+ wfext.sfmt, '', 'avg', 'wfext.aper_algo', wfext.aper_algo, wfext.aper_algo_s, 'waterfall_aper_algo_cb'),
               w3_div('id-wfext-aper-param w3-width-40pct',
                  w3_slider('id-wfext-aper-param-slider', 'Parameter', 'wfext.aper_param', wfext.aper_param, 0, 10, 1, 'waterfall_aper_param_cb')
               ),
               w3_div('id-wfext-aper-param-field'),
               w3_button('id-wfext-aper-autoscale class-button w3-btn-right w3-selection-green w3-font-11_25px||title="waterfall auto scale"', 'Auto<br>Scale', 'wf_autoscale_cb')
            )
         ),
         
         w3_div('w3-margin-LR-16 w3-margin-T-8',
            w3_text('id-wfext-text'),
            w3_inline('id-wfext-maxmin w3-background-fade w3-text-white w3-small|background:#575757/',
               w3_div('id-wfext-content')
            )
         ),
         
         w3_div('w3-margin-T-8',
            w3_text('w3-margin-B-8 w3-text-css-orange', '<b>Spec RF passband marker</b>'),
            w3_inline('w3-margin-LR-16/w3-hspace-16',
               w3_input('coloris-square//coloris coloris-instance1 coloris-input w3-input-evt||data-coloris',
                  '', 'wfext.spb_color', wfext.spb_color, 'wfext_spb_color_cb'),
               w3_checkbox('w3-label-inline w3-label-not-bold', 'Enable', 'wfext.spb_on', wfext.spb_on, 'wfext_spb_on_cb')
            )
         ),
         
         //w3_hr('w3-margin-10'),
         w3_div('w3-margin-T-8',
            w3_text('w3-margin-B-8 w3-text-css-orange', '<b>Timestamps</b>'),
            w3_inline('w3-margin-LR-16/w3-hspace-16',
               w3_select('id-wfext-tstamp '+ wfext.sfmt, '', '', 'wfext.tstamp_i', wfext.tstamp_i, wfext.tstamp_s, 'wfext_tstamp_cb'),
               w3_input('id-wfext-tstamp-custom|padding:0;width:auto|size=4',
                  '', 'wfext.tstamp_f', wfext.tstamp_f, 'wfext_tstamp_custom_cb'),
               w3_select(wfext.sfmt, '', '', 'wf.ts_tz', wf.ts_tz, wfext.tstamp_tz_s, 'w3_num_cb'),
				   w3_button('w3-padding-smaller w3-aqua', 'Save WF as JPG', 'export_waterfall')
            )
         ),
         
         //w3_hr('w3-margin-10'),
         w3_div('w3-margin-T-8 w3-margin-B-8',
            w3_text('w3-text-css-orange', '<b>Waterfall FFT</b>'),
            w3_inline('w3-margin-LR-16/w3-hspace-16',
               w3_select(wfext.sfmt, '', 'window function', 'wfext.winf_i', wfext.winf_i, wfext.winf_s, 'wfext_winf_cb'),
               w3_select(wfext.sfmt, '', 'interp', 'wfext.interp_i', wfext.interp_i, wfext.interp_s, 'wfext_interp_cb'),
               w3_checkbox('w3-label-inline w3-label-not-bold', 'CIC<br>comp', 'wfext.cic_comp', wfext.cic_comp, 'wfext_cic_comp_cb')
            )
         )
		);

	w3_innerHTML('id-wf-more', controls_html);
	coloris_init();      // NB: has to be after elements using coloris are instantiated
   w3_show_hide('id-wfext-tstamp-custom', false, null, 2);
   waterfall_maxmin_cb();
   //w3_event_listener('wf_cmap', 'id-wf.cmap');
   //w3_event_listener('wf_ts_tz', 'id-wf.ts_tz');
}

function waterfall_init()
{
   //console.log('waterfall_init audioFFT_active='+ wf.audioFFT_active);
   if (!wf.audioFFT_active) {
      var init_aper = +ext_get_cfg_param('init.aperture', -1, EXT_NO_SAVE);
      var last_aper = kiwi_storeRead('last_aper', (init_aper == -1)? 0 : init_aper);
      if (isArg(wf_auto)) last_aper = wf_auto? 1:0;
      //console.log('waterfall_init: last_aper='+ last_aper +' init_aper='+ init_aper +' wfa='+ wf_auto +' url_tstamp='+ wf.url_tstamp);
      wf_aper_cb('wf.aper', last_aper, false);     // writes 'last_aper' cookie
      w3_show('id-aper-data');
   } else {
      // force aperture manual for audio FFT mode
      wf_aper_cb('wf.aper', false, false);
   }
   
   wfext.spb_on = +kiwi_storeInit('last_spb_on', wfext.spb_on);
   wfext.spb_color = kiwi_storeInit('last_spb_color', wfext.spb_color);
   
   // allow URL param to override
   if (wf_interp != -1) {
      wfext.interp_i = wf_interp % 10;
      wfext.cic_comp = (wf_interp >= 10)? true:false;
   }
   
   if (wf.url_tstamp) wfext.tstamp = wf.url_tstamp;
}

function waterfall_aper_algo_cb(path, idx, first)
{
   if (first) {
      idx = +kiwi_storeRead('last_aper_algo', wfext.aper_algo_e.OFF);
      w3_set_value(path, idx);
   } else {
      idx = +idx;
   }
   //console.log('waterfall_aper_algo_cb ENTER path='+ path +' idx='+ idx +' first='+ first);
   //kiwi_trace('waterfall_aper_algo_cb');
   wf.aper_algo = wfext.aper_algo = +idx;

   if (wfext.aper_algo == wfext.aper_algo_e.OFF) {
      w3_hide(w3_el('id-wfext-aper-param'));
      w3_show(w3_el('id-wfext-aper-autoscale'));
      w3_innerHTML('id-wfext-aper-param-field', 'aperture auto-scale on <br> waterfall pan/zoom only');
      colormap_update();
   } else {
      var f_a = wfext.aper_algo;
      var f_s = wfext.aper_algo_s[f_a];
      var f_p = wfext.aper_params;
      var val = f_p[f_s +'_val'];
      //console.log('waterfall_aper_algo_cb menu='+ f_a +'('+ f_s +') val='+ val);
   
      // update slider to match menu change
      w3_show(w3_el('id-wfext-aper-param'));
      w3_hide(w3_el('id-wfext-aper-autoscale'));
      waterfall_aper_param_cb('id-wfext-aper-param-slider', val, /* done */ true, /* first */ false);
      //console.log('waterfall_aper_algo_cb EXIT path='+ path +' menu='+ f_a +'('+ f_s +') param='+ wfext.aper_param);
   }
   
	kiwi_storeWrite('last_aper_algo', wfext.aper_algo.toString());
   freqset_select();
}

function waterfall_aper_param_cb(path, val, done, first)
{
   if (first) return;
   val = +val;
   //console.log('waterfall_aper_param_cb ENTER path='+ path +' val='+ val +' done='+ done);
   //kiwi_trace('waterfall_aper_param_cb');
   var f_a = wfext.aper_algo;
   var f_s = wfext.aper_algo_s[f_a];
   var f_p = wfext.aper_params;

   if (isUndefined(val) || isNaN(val)) {
      val = f_p[f_s +'_def'];
      //console.log('waterfall_aper_param_cb using default='+ val +'('+ typeof(val) +')');
      var lsf = parseFloat(kiwi_storeRead('last_aper_algo'));
      var lsfp = parseFloat(kiwi_storeRead('last_aper_param'));
      if (lsf == f_a && !isNaN(lsfp)) {
         //console.log('waterfall_aper_param_cb USING READ_COOKIE last_aper_param='+ lsfp);
         val = lsfp;
      }
   }

	wf.aper_param = wfext.aper_param = f_p[f_s +'_val'] = val;
	//console.log('waterfall_aper_param_cb UPDATE slider='+ val +' menu='+ f_a +' done='+ done +' first='+ first);

   // needed because called by waterfall_aper_algo_cb()
   w3_slider_setup('id-wfext-aper-param-slider', f_p[f_s +'_min'], f_p[f_s +'_max'], f_p[f_s +'_step'], val);
   w3_innerHTML('id-wfext-aper-param-field', (f_a == wfext.aper_algo_e.OFF)? '' : (val +' '+ wfext.aper_param_s[f_a]));

   if (done) {
	   //console.log('waterfall_aper_param_cb DONE WRITE_COOKIE last_aper_param='+ val.toFixed(2));
	   kiwi_storeWrite('last_aper_param', val.toFixed(2));
      colormap_update();
      freqset_select();
   }

   //console.log('waterfall_aper_param_cb EXIT path='+ path);
}

function waterfall_maxmin_cb()
{
   var auto = (wf.aper == kiwi.APER_AUTO);
   var max = auto? (wf.auto_maxdb + wf.auto_ceil.val) : maxdb;
   var min = auto? (wf.auto_mindb + wf.auto_floor.val) : mindb;
	//console.log('auto='+ TF(auto) +' min='+ min +' max='+ max);   
   var dyn_range = max - min;
   var total_s = min.toString().positiveWithSign() +','+ max.toString().positiveWithSign();
	if (auto) {
      w3_innerHTML('id-wfext-text', 'Min,max: total = computed + floor,ceil');
      w3_flash_fade('id-wfext-maxmin', 'cyan', 50, 300, '#575757');
      var computed_s = wf.auto_mindb.toString().positiveWithSign() +','+ wf.auto_maxdb.toString().positiveWithSign();
      var floor_ceil_s = wf.auto_floor.val.toString().positiveWithSign() +','+ wf.auto_ceil.val.toString().positiveWithSign();
      w3_innerHTML('id-wfext-content',
         paren(total_s) +'&nbsp;=&nbsp;'+ paren(computed_s) +'&nbsp;+&nbsp;'+ paren(floor_ceil_s) +'&nbsp;&nbsp;[&Delta; '+ dyn_range +' dB]');
   } else {
      w3_innerHTML('id-wfext-text', 'Min,max: aperture manual mode');
      w3_innerHTML('id-wfext-content',
         paren(total_s) +'&nbsp;&nbsp;[&Delta; '+ dyn_range +' dB]');
   }
}

function wfext_spb_color_cb(path, val, first, cbp)
{
   //console.log('wfext_spb_color_cb val='+ val +' cpb='+ cbp);
   w3_string_cb(path, val);
   kiwi_storeWrite('last_spb_color', val);
   wfext.spb_color_seq++;
}

function wfext_spb_on_cb(path, checked, first)
{
   if (first) return;
   w3_bool_cb(path, checked, first);
   kiwi_storeWrite('last_spb_on', checked? 1:0);
   wfext.spb_color_seq++;
}

function wfext_tstamp_cb(path, idx, first)
{
   //if (first) return;
   //console.log('wfext_tstamp_cb TOP first='+ first);
   wfext.tstamp_i = idx = +idx;
   w3_set_value(path, idx);      // for benefit of direct callers
   var tstamp_s = wfext.tstamp_s[idx];
   var isCustom = (tstamp_s == 'custom');
   var el_custom = w3_el('id-wfext-tstamp-custom');

   if (first) {
      if (!wf.url_tstamp) {
         if (isCustom) wfext_tstamp_custom_cb(el_custom, wfext.tstamp);
         return;
      }
      //console.log('wfext_tstamp_cb HAVE url_tstamp='+ wf.url_tstamp);
      var stop = false;
      w3_select_enum(path, function(el, idx) {
         if (stop || el.innerHTML != 'custom') return;
         //console.log('wfext_tstamp_cb MATCH url_tstamp='+ wf.url_tstamp);
         wfext_tstamp_custom_cb('id-wfext-tstamp-custom', wf.url_tstamp);
         wfext_tstamp_cb(path, el.value);
         stop = true;
      });
      wf.url_tstamp = 0;   // don't do on subsequent extension opens
      return;
   }

   //console.log('wfext_tstamp_cb idx='+ idx +' tstamp_s='+ tstamp_s +' isCustom='+ isCustom);
   w3_show_hide(el_custom, isCustom, null, 2);

   if (isCustom) {
	   wfext_tstamp_custom_cb(el_custom, w3_get_value(el_custom));
   } else {
      wfext.tstamp = wfext.tstamp_v[idx];
   }
}

function wfext_tstamp_custom_cb(path, val)
{
   //console.log('wfext_tstamp_custom_cb val='+ val);
   wfext.tstamp = w3_clamp(+val, 2, 60*60, 2);
   w3_set_value(path, wfext.tstamp);
   w3_show_block(path);
}

function wfext_winf_cb(path, idx, first)
{
   //if (first) return;    // NB: commented out so default from wfext.winf_i will be sent to server
   w3_num_cb(path, idx, first);
	wf_send('SET window_func='+ (+idx));
}

function wfext_interp_cb(path, idx, first)
{
   if (first) return;
   w3_num_cb(path, idx, first);
   idx = +idx + (wfext.cic_comp? 10:0);
	wf_send('SET interp='+ idx);
	//console.log('wfext_interp_cb idx='+ idx);
}

function wfext_cic_comp_cb(path, checked, first)
{
   if (first) return;
   w3_bool_cb(path, checked, first);
   wfext_interp_cb('', wfext.interp_i);
}

/*
function waterfall_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Waterfall Help') +
         '<br><br>' +
         w3_div('w3-text-css-orange', '<b>Aperture auto mode</b>') +
         'xxx' +

         '<br><br>' +
         w3_div('w3-text-css-orange', '<b>Waterfall timestamps</b>') +
         'xxx' +
         '';
      confirmation_show_content(s, 610, 350);
   }
   return true;
}
*/
