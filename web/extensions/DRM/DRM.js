// Copyright (c) 2019 John Seamons, ZL/KF6VO

var drm = {
   ext_name: 'DRM',     // NB: must match DRM.cpp:DRM_ext.name
   first_time: true,
   locked: 0,
   wrong_srate: false,

   h: 200,
   w_lhs: 75,
   w_msg: 600,
   w_msg2: 450,
   w_graph: 500,
   pad_graph: 10,

   n_menu: 5,
   menu0: W3_SELECT_SHOW_TITLE,
   menu1: W3_SELECT_SHOW_TITLE,
   menu2: W3_SELECT_SHOW_TITLE,
   menu3: W3_SELECT_SHOW_TITLE,
   menu4: W3_SELECT_SHOW_TITLE,
   
   run: 0,
   is_stopped: 0,
   freq_s: '',
   freq: 0,
   display: 0,
   display_s: [ 'graph', 'IQ', 'FAC', 'SDC', 'MSC' ],
   IQ: 1,
   FAC: 2,
   SDC: 3,
   MSC: 4,
   last_canvas: 0,
   monitor: 0,
   monitor_s: [ 'DRM', 'source' ],
   
   last_occ: -1,
   last_sdc: -1,
   last_msc: -1,
   
   DRM_DAT_IQ: 0,
   
   EAudCod: [ 'AAC', 'OPUS', 'RESERVED', 'xHE_AAC', '' ],
   AAC: 0,
   xHE_AAC: 3,

   europe: {
      'BBC\\Worldservice':       [ 3955, 15620 ],
      'Radio\\France Intl':      [ 3965, 6175 ],
      'Voice of\\Russia':        [ 5925, 6025, 6110, 6125, 7435, 9590, 9625, 9800, 11620, 11860, 15325 ],
      'Radio\\Romania Intl':     [ 5940, 5955, 6025, 6030, 6040, 6175, 7220, 7235, 7350, 9490, 9820, 13730 ],
      'Funklust\\(bitXpress)':   [ 15785 ],
   },

   middle_east_africa: {
      'All India\\Radio':        [ 6100, 7550, 9950 ],
      'Radio Kuwait':            [ 11970, 13650, 15110, 15540 ],
      'Voice of\\Nigeria':       [ 15120 ],
   },

   menu_India_MW: 2,
   india_MW: {
      'Ahmedabad':               [ 855 ],
      'Ajmer':                   [ 612 ],
      'Bengaluru':               [ 621 ],
      'Barmer':                  [ 1467 ],
      'Bikaner':                 [ 1404 ],
      'Chennai A':               [ 729 ],
      'Chennai B':               [ 783 ],
      'Chinsurah':               [ 603 ],
      'Delhi A':                 [ 828 ],
      'Delhi B':                 [ 1368 ],
      'Dharwad':                 [ 774 ],
      'Dibrugarh':               [ 576 ],
      'Guwahati B':              [ 1044 ],
      'Hyderabad':               [ 747 ],
      'Itanagar':                [ 684 ],
      'Jabalpur':                [ 810 ],
      'Jalandhar':               [ 882 ],
      'Jammu':                   [ 999 ],
      'Kolkata A':               [ 666 ],
      'Kolkata B':               [ 1017 ],
      'Luknow':                  [ 756 ],
      'Mumbai A':                [ 1053 ],
      'Mumbai B':                [ 567 ],
      'Panaji':                  [ 1296 ],
      'Passighat':               [ 1071 ],
      'Patna':                   [ 630 ],
      'Pune':                    [ 801 ],
      'Rajkot A':                [ 819 ],
      'Rajkot B':                [ 1080 ],
      'Rajkot C':                [ 1071 ],
      'Ranchi':                  [ 558 ],
      'Siliguri':                [ 720 ],
      'Suratgarh':               [ 927 ],
      'Tawang':                  [ 1530 ],
      'Trichirapalli':           [ 945 ],
      'Varanasi':                [ 1251 ],
      'Vijayawada':              [ 846 ],
   },

   asia_pac: {
      'China National\\Radio':   [ 6030, 7360, 9420, 9655, 9870, 11695, 13825, 13850, 15180, 17770, 17800, 17830 ],
      'Chukotka\\Radio':         [ 5935, 6025, 11860, 15325 ],
      'Radio\\New Zealand':      [ 5975, 7285, 7330, 9780, 11690 ],
      'Transworld\\KTWR':        [ 7500, 11995, 13800 ],
   },

   americas: {
      'WINB USA':               [ 7315, 9265, 13690 ],
   },

   last_last: 0
};

function DRM_main()
{
	ext_switch_to_client(drm.ext_name, drm.first_time, drm_recv);		// tell server to use us (again)
	if (!drm.first_time)
		drm_lock_setup();
	drm.first_time = false;
}

function drm_recv(data)
{
   var i, s;
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var d = new Uint8Array(data, 5);
		var len = d.length-1;
		if (isUndefined(drm.canvas_IQ)) return;
      var ct = drm.canvas_IQ.ctx;
      var w = drm.w_IQ;
      var h = drm.h_IQ;
		var color = [];
		
		switch (cmd) {
		
		/*
		case drm.DRM_DAT_FAC:
         ct.fillStyle = 'white';
         ct.fillRect(0,0, w,h);
         ct.fillStyle = 'grey';
         ct.fillRect(0,h/2, w,1);
         ct.fillRect(w/2,0, 1,h);
         color.push('green');
         // fall through

		case drm.DRM_DAT_SDC:
         color.push('red');
         // fall through

		case drm.DRM_DAT_MSC:
         color.push('blue');
         ct.fillStyle = color.shift();
      */
      
		case drm.DRM_DAT_IQ:
         ct.fillStyle = 'white';
         ct.fillRect(0,0, w,h);
         ct.fillStyle = 'grey';
         ct.fillRect(0,h/2, w,1);
         ct.fillRect(w/2,0, 1,h);
         
         if (drm.display == drm.IQ || drm.display == drm.FAC) {
            ct.fillStyle = 'green';
            for (i = 0; i < 64; i++) {
               var x = d[i*2]   / 255.0 * w;
               var y = d[i*2+1] / 255.0 * h;
               ct.fillRect(x,y, 2,2);
            }
         }
         i = 64;
         
         if (drm.display == drm.IQ || drm.display == drm.SDC) {
            ct.fillStyle = 'red';
            for (; i < 64+256; i++) {
               var x = d[i*2]   / 255.0 * w;
               var y = d[i*2+1] / 255.0 * h;
               ct.fillRect(x,y, 2,2);
            }
         }
         i = 64+256;

         if (drm.display == drm.IQ || drm.display == drm.MSC) {
            ct.fillStyle = 'blue';
            for (; i < 64+256+2048; i++) {
               var x = d[i*2]   / 255.0 * w;
               var y = d[i*2+1] / 255.0 * h;
               ct.fillRect(x,y, 2,2);
            }
         }
         
		   return;
		
		}

		console.log('drm_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('drm_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('drm_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				kiwi_load_js(['pkgs/js/graph.js', 'pkgs/js/sprintf/sprintf.js'], 'drm_lock_setup');
				break;
			
			case "locked":
			   var p = +param[1];
			   if (p == -1) {
			      drm.wrong_srate = true;
			      drm.locked = 0;
			   } else
			   if (kiwi.is_BBAI) {
			      drm.drm_chan = p;
			      drm.locked = p? 0:1;
			   } else {
			      drm.locked = p;
			   }
            drm_controls_setup();
				break;
			
			case "drm_status_cb":
			   if (!drm.run)
			      break;

			   var deco;
			   try {
			      deco = decodeURIComponent(param[1]);
			   } catch(ex) {
			      console.log('DRM decodeURIComponent(param) FAIL:');
			      console.log(ex);
			      console.log(param[1]);
			      break;
			   }

			   var o;
			   try {
			      o = JSON.parse(deco);
			   } catch(ex) {
			      console.log('DRM JSON.parse(deco) FAIL:');
			      console.log(ex);
			      console.log(deco);
			      break;
			   }

            drm_all_status(o.io, o.time, o.frame, o.FAC, o.SDC, o.MSC);

			   w3_innerHTML('id-drm-if_level', 'IF Level: '+ o.if.toFixed(1) +' dB');
			   if (o.if < 0 && o.if > -100) {
			      if (isUndefined(drm.last_if)) drm.last_if = o.if;
			      drm.last_if = graph_plot(o.if, { line: drm.last_if, color: 'green' });
			   }

			   w3_innerHTML('id-drm-snr', o.snr? ('SNR: '+ o.snr.toFixed(1) +' dB') : '');
			   if (o.snr > 0) {
			      if (isUndefined(drm.last_snr)) drm.last_snr = o.snr;
			      drm.last_snr = graph_plot(o.snr, { line: drm.last_snr });
			   }

			   if (isDefined(o.mod)) {
			      i = o.mod;
			      if (i >= 0 && i <= 3)
			         w3_color('id-drm-'+ ['A','B','C','D'][i], 'black', 'lime');

			      i = o.occ;
			      if (i >= 0 && i <= 5) {
			         if (drm.last_occ != i) {
			            w3_color('id-drm-occ-'+ ['4.5','5','9','10','18','20'][drm.last_occ], '', '');
			         }
			         w3_color('id-drm-occ-'+ ['4.5','5','9','10','18','20'][i], 'black', 'lime');
			         drm.last_occ = i;
			      }

			      i = o.sdc;
			      if (i >= 0 && i <= 1) {
			         if (drm.last_sdc != i) {
			            w3_color('id-drm-sdc-'+ ['4','16'][drm.last_sdc], '', '');
			         }
			         w3_color('id-drm-sdc-'+ ['4','16'][i], 'black', 'lime');
			         drm.last_sdc = i;
			      }

			      i = o.msc;
			      if (i > 2) i = 2;
			      if (i >= 1 && i <= 2) {
			         if (drm.last_msc != i) {
			            w3_color('id-drm-msc-'+ ['16','64'][drm.last_msc-1], '', '');
			         }
			         w3_color('id-drm-msc-'+ ['16','64'][i-1], 'black', 'lime');
			         drm.last_msc = i;
			      }

			      if (o.mod >= 0 && o.mod <= 3)
                  w3_innerHTML('id-drm-desc',
                     [
                        'Local/regional use',
                        'Medium range use, multipath resistant',
                        'Long distance use, multipath/doppler resistant',
                        'Resistance to large delay/doppler spread'
                     ][o.mod]
                  );
			   } else {
               drm_reset_status();
            }
			   
			   w3_show_hide('id-drm-mode', isDefined(o.mod));
			   
			   var codec = drm.AAC;
			   o.svc.forEach(function(ao, i) {
			      i++;
               //if (ao.cur && i == 2) ao.ac = drm.xHE_AAC;
			      s = i;
			      if (ao.id) {
                  s += ' '+ drm.EAudCod[ao.ac] +' ('+ ao.id +') '+ ao.lbl + (ao.ep? (' UEP ('+ ao.ep.toFixed(1) +'%) ') : ' EEP ') +
                     (ao.ad? 'Audio ':'Data ') + ao.br.toFixed(2) +' kbps';
               }
               var el = w3_el('id-drm-svc-'+ i);
			      el.innerHTML = s;
			      w3_color(el, null, ao.cur? 'mediumSlateBlue' : '');
			      if (ao.cur && ao.ac < 4)   // glitches to = 4 during acquisition
			         codec = ao.ac;
			   });
            w3_show('id-drm-svcs');
            if (codec != drm.AAC) console.log('codec='+ codec);
            w3_innerHTML('id-drm-error',
               (codec != drm.AAC)? ('WARNING: '+ ((codec == drm.xHE_AAC)? 'xHE_AAC ':'') +'codec not supported yet -- audio will be bad') : '<br>');
            w3_color('id-drm-error', 'white', 'red', (codec != drm.AAC));

            if (o.msg)
			      w3_innerHTML('id-drm-msgs', o.msg);
			   break;

			case "drm_bar_pct":
			   if (!drm.run)
			      break;

			   var pct = w3_clamp(parseInt(param[1]), 0, 100);
			   //if (pct > 0 && pct < 3) pct = 3;    // 0% and 1% look weird on bar
			   var el = w3_el('id-drm-bar');
			   if (el) el.style.width = pct +'%';
			   break;

			case "annotate":
			   switch (+param[1]) {
               case 0: graph_annotate('lime'); break;
               case 1: graph_annotate('gold'); break;
               case 2: graph_annotate('red'); break;
               case 3: graph_annotate('blue'); break;
            }
			   break;
			
			default:
				console.log('drm_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function drm_reset_status()
{
   //console.log('drm_reset_status');
   drm_all_status(4,4,4,4,4,4);
   w3_innerHTML('id-drm-if_level', '');
   w3_innerHTML('id-drm-snr', '');
   w3_hide('id-drm-mode');
   for (var i = 0; i <= 3; i++)
      w3_color('id-drm-'+ ['A','B','C','D'][i], '', '');
   w3_innerHTML('id-drm-error', '');
   w3_hide('id-drm-svcs');
   w3_innerHTML('id-drm-msgs', '');
}

function drm_status(id, v)
{
   var color;
   switch (v) {
      case 0: color = 'lime'; break;
      case 1: color = 'yellow'; break;
      case 2: color = 'red'; break;
      case 3: default: color = 'red'; break;
      case 4: color = 'grey'; break;
   }
   w3_color('id-drm-status-'+ id, color);
}

function drm_all_status(io, time, frame, FAC, SDC, MSC)
{
   drm_status('io', io);
   drm_status('time', time);
   drm_status('frame', frame);
   drm_status('fac', FAC);
   drm_status('sdc', SDC);
   drm_status('msc', MSC);
}

function drm_lock_setup()
{
   ext_send('SET lock_set');
}

function drm_saved_mode()
{
   drm.saved_mode = ext_get_mode();
   console.log('drm_save_mode FIRST saved_mode='+ drm.saved_mode);
   if (drm.saved_mode == 'drm')
      drm.saved_mode = ext_get_prev_mode();     // if 'DRM" button clicked use previously set mode
   console.log('drm_save_mode FINAL saved_mode='+ drm.saved_mode);
}

function drm_controls_setup()
{
   drm_saved_mode();
   console.log('drm_controls_setup saved_mode='+ drm.saved_mode);
   drm.saved_passband = ext_get_passband();
   
   drm.is_stopped = 0;
   console.log('drm_controls_setup is_stopped='+ drm.is_stopped);

   // URL params that need to be setup before controls instantiated
	var p = drm.url_params = ext_param();
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         //console.log('DRM param1 <'+ a +'>');
         var a1 = a.split(':');
         a1 = a1[a1.length-1].toLowerCase();
         var r;
         if ((r = w3_ext_param('debug', a)).match) {
            var debug = r.has_value? r.num : 0;
            ext_send('SET debug='+ debug);
         }
      });
   }

   var controls_inner, data_html;
   
   if (drm.wrong_srate) {
      controls_inner =
            w3_text('w3-medium w3-text-css-yellow',
               'Currently, DRM does not support Kiwis configured for 20 kHz wide channels.'
            );
      data_html = null;
   } else
   
   if (drm.locked == 0) {
      controls_inner =
         kiwi.is_BBAI?
            w3_text('w3-medium w3-text-css-yellow',
               sprintf('Limited to %d active instances.', drm.drm_chan)
            )
         :
            w3_text('w3-medium w3-text-css-yellow',
               'Requires exclusive use of the Kiwi.<br>' +
               'While the DRM decoder is used there can be no other active connections.<br>' +
               'Please try again when this is the only connection.'
            );
      data_html = null;
   } else {
      var svcs = 'Services:<br>';
      for (var i=1; i <= 4; i++) {
         svcs +=
            w3_inline('',
               w3_checkbox('w3-margin-R-8', '', 'drm.svc'+ i, (i == 1), 'drm_svcs_cbox_cb'),
               w3_div('id-drm-svc-'+ i +'|width:100%')
            );
      }
   
      var sblk = function(id) { return id +' '+ w3_icon('id-drm-status-'+ id.toLowerCase(), 'fa-square', 16, 'grey') +' &nbsp;&nbsp;'; };
      var tblk = function(id) { return w3_span('id-drm-'+ id +' cl-drm-blk', id); };
      var oblk = function(id) { return w3_span('id-drm-occ-'+ id +' cl-drm-blk', id); };
      var qsdc = function(id) { return w3_span('id-drm-sdc-'+ id +' cl-drm-blk', id); };
      var qmsc = function(id) { return w3_span('id-drm-msc-'+ id +' cl-drm-blk', id); };

      var twidth = drm.w_lhs + drm.w_msg + drm.w_graph;
      data_html =
         time_display_html('drm') +
         
         w3_div(sprintf('id-drm-data w3-relative w3-no-scroll|width:%dpx; height:%dpx; background-color:black;', twidth, drm.h),
            w3_div(sprintf('id-drm-plot w3-absolute|width:%dpx; height:%dpx; left:%dpx;', drm.w_graph, drm.h, drm.w_lhs + drm.w_msg),
               w3_canvas('id-drm-graph', drm.w_graph, drm.h, { padding:drm.pad_graph }),
               w3_canvas('id-drm-IQ w3-hide', drm.w_graph, drm.h, { padding:drm.pad_graph })
            ),
            w3_div(sprintf('id-drm-console-msg w3-margin-T-8 w3-small w3-text-white w3-absolute|left:%dpx; width:%dpx; height:%dpx; overflow-x:hidden;',
               drm.w_lhs, drm.w_msg, drm.h),
               w3_div('id-drm-status', sblk('IO') + sblk('Time') + sblk('Frame') + sblk('FAC') + sblk('SDC') + sblk('MSC')),
               w3_inline('w3-halign-space-between|width:200px/',
                  w3_div('id-drm-if_level'),
                  w3_div('id-drm-snr')
               ),
               w3_div('id-drm-mode w3-hide',
                  w3_inline('/w3-show-inline',
                     'DRM mode ', tblk('A'), tblk('B'), tblk('C'), tblk('D'),
                     '&nbsp;&nbsp;&nbsp; Chan '+ oblk('4.5'), oblk('5'), oblk('9'), oblk('10'), oblk('18'), oblk('20'), ' kHz',
                     '&nbsp;&nbsp;&nbsp; SDC ', qsdc('4'), qsdc('16'), ' QAM',
                     '&nbsp;&nbsp;&nbsp; MSC ', qmsc('16'), qmsc('64'), ' QAM'
                  ),
                  w3_div('id-drm-desc')
               ),
               w3_div('id-drm-error|width:'+ px(drm.w_msg2)),
               w3_div('id-drm-svcs w3-hide|width:'+ px(drm.w_msg2), svcs),
               '<br><br>',
               w3_div(sprintf('id-drm-msgs|width:%dpx', drm.w_msg))
            )
         ) +

         w3_div('id-drm-options w3-display-right w3-text-white|top:230px; right:0px; width:200px; height:200px',
            w3_select('|color:red', '', 'display', 'drm.display', drm.display, drm.display_s, 'drm_display_cb')
         );

      controls_inner =
         w3_inline('w3-halign-space-between w3-margin-T-8/',
            w3_select_hier('w3-text-red', 'Europe', 'select', 'drm.menu0', drm.menu0, drm.europe, 'drm_pre_select_cb'),
            w3_select_hier('w3-text-red', 'Middle East, Africa', 'select', 'drm.menu1', drm.menu1, drm.middle_east_africa, 'drm_pre_select_cb'),
            w3_select_hier('w3-text-red', 'India MW', 'select', 'drm.menu2', drm.menu2, drm.india_MW, 'drm_pre_select_cb'),
            w3_select_hier('w3-text-red', 'Asia/Pacific', 'select', 'drm.menu3', drm.menu3, drm.asia_pac, 'drm_pre_select_cb'),
            w3_select_hier('w3-text-red', 'Americas', 'select', 'drm.menu4', drm.menu4, drm.americas, 'drm_pre_select_cb')
         ) +

         w3_inline('w3-margin-T-8/w3-margin-between-16',
            w3_button('w3-padding-smaller', 'Next', 'drm_next_prev_cb', 1),
            w3_button('w3-padding-smaller', 'Prev', 'drm_next_prev_cb', -1),
            w3_button('id-drm-stop-button w3-padding-smaller w3-pink', 'Stop', 'drm_stop_start_cb'),
            w3_button('w3-padding-smaller w3-css-yellow', 'Reset', 'drm_reset_cb'),
            w3_button('w3-padding-smaller w3-aqua', 'Test 1', 'drm_test_cb', 1),
            w3_button('w3-padding-smaller w3-aqua', 'Test 2', 'drm_test_cb', 2),
            //w3_select('w3-margin-left|color:red', '', 'monitor', 'drm.monitor', drm.monitor, drm.monitor_s, 'drm_monitor_cb')
            w3_div('id-drm-bar-container w3-progress-container w3-round-large w3-white w3-hide|width:220px; height:16px',
               w3_div('id-drm-bar w3-progressbar w3-round-large w3-light-green|width:'+ 50 +'%', '&nbsp;')
            )
         );
   }

	var controls_html =
		w3_div('id-drm-controls w3-text-white',
			w3_divs('w3-container/',
            w3_col_percent('',
				   w3_div('w3-medium w3-text-aqua', '<b>Digital Radio Mondiale (DRM30) decoder</b>'), 60,
					w3_div('', 'Based on <b><a href="https://sourceforge.net/projects/drm/" target="_blank">Dream 2.2.1</a></b>'), 40
				),
				
            w3_col_percent('w3-margin-T-4/',
               w3_div('id-drm-station w3-text-css-yellow', '&nbsp;'), 60,
               w3_div('', 'Schedules: ' +
                  '<a href="https://www.drm.org/what-can-i-hear/broadcast-schedule-2/" target="_blank">drm.org</a> ' +
                  '<a href="http://www.hfcc.org/drm" target="_blank">hfcc.org</a>'), 40
            ),
            
            controls_inner
         )
      );
   
	ext_panel_show(controls_html, data_html, null);
	ext_set_controls_width_height(700, 150);

	if (drm.locked == 0) return;

	time_display_setup('drm');

	drm.graph = w3_el('id-drm-graph');
	drm.graph.ctx = drm.graph.getContext("2d");
   graph_init(drm.graph, { dBm:0, speed:1, averaging:false });
	graph_mode('auto');
	//graph_mode('fixed', 30, -30);
	graph_clear();
	//graph_marker(30);
	
	drm.canvas_IQ = w3_el('id-drm-IQ');
	drm.canvas_IQ.ctx = drm.canvas_IQ.getContext("2d");
	drm.w_IQ = drm.w_graph - drm.pad_graph*2;
	drm.h_IQ = drm.h - drm.pad_graph*2;

	if (drm.url_params) {
      var freq = parseFloat(drm.url_params);
      if (freq) {
         // select matching menu item frequency
         var found = false;
         for (var i = 0; i < drm.n_menu; i++) {
            var menu = 'drm.menu'+ i;
            w3_select_enum(menu, function(option) {
               //console.log('CONSIDER '+ parseFloat(option.innerHTML));
               if (parseFloat(option.innerHTML) == freq) {
                  drm_pre_select_cb(menu, option.value, false);
                  found = true;
               }
            });
            if (found) break;
         }
      }
   }

   // URL params that need to be setup after controls instantiated
	var p = drm.url_params;
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         //console.log('DRM param1 <'+ a +'>');
         var a1 = a.split(':');
         a1 = a1[a1.length-1].toLowerCase();
         var r;
         if (w3_ext_param('help', a).match) {
            extint_help_click();
         } else
         if ((r = w3_ext_param('test', a)).match) {
            var test = r.has_value? r.num : 1;
            test = w3_clamp(test, 1, 2, 1);
            drm_station('Test Recording '+ test);
            w3_show('id-drm-bar-container');
            drm_test(test);
           
         }
      });
   }

   drm_run(1);
   // done after drm_run() so correct drm.saved_{mode,passband} is set
   drm_set_freq(ext_get_freq_kHz());
}

function drm_run(run)
{
   if (run == drm.run) return;

   if (run) {
      drm_saved_mode();
      console.log('drm_run saved_mode='+ drm.saved_mode);
      //kiwi_trace();
      drm.saved_passband = ext_get_passband();
   }

   //console.log('drm_run run='+ run);
   //kiwi_trace();
   drm.run = run;
   ext_send('SET run='+ drm.run);
}

function drm_test(test)
{
   console.log('drm_test test='+ test);
   ext_send('SET test='+ test);
}

function drm_stop()
{
   drm_test(0);
   drm_run(0);
   drm_reset_status();
   drm_station('');
   w3_hide('id-drm-bar-container');
   
   if (isDefined(drm.saved_passband))
      ext_set_passband(drm.saved_passband.low, drm.saved_passband.high);
   if (isDefined(drm.saved_mode)) {
      console.log('drm_stop RESTORE saved_mode='+ drm.saved_mode);
      ext_set_mode(drm.saved_mode);
   }
}

function drm_start()
{
   drm_run(1);
   ext_set_mode('drm');
   drm.is_stopped = 0;     // for when called by drm_pre_select_cb()
   w3_button_text('id-drm-stop-button', 'Stop', 'w3-pink', 'w3-green');
}

function drm_stop_start_cb(path, cb_param)
{
   console.log('drm_stop_start_cb ENTRY is_stopped='+ drm.is_stopped);
   drm.is_stopped ^= 1;
   
   if (drm.is_stopped) {
      console.log('$drm_stop_start_cb: do stop, show start');
      drm_stop();
      w3_button_text(path, 'Start', 'w3-green', 'w3-pink');
   } else {
      console.log('$drm_stop_start_cb: do start, show stop');
      drm_start();
   }
}

function drm_set_freq(freq)
{
   drm.freq = freq;
   var zoom = ext_get_zoom();
   if (zoom < 5) zoom = 5; else
   if (zoom > 9) zoom = 9;
   ext_tune(drm.freq, 'drm', ext_zoom.ABS, zoom);
   ext_set_passband(-5000, 5000);
   ext_tune(drm.freq, 'drm', ext_zoom.ABS, zoom);  // set again to get correct freq given new passband
}

function drm_reset_cb(path, val, first)
{
   console.log('drm_reset_cb');
   ext_send('SET reset');
}

function drm_test_cb(path, val, first)
{
   console.log('drm_test_cb '+ val);
   drm_station('Test Recording '+ val);
   drm_run(0);
   drm_start();
   drm_test(val);
   ext_set_mode('drm');
   w3_show('id-drm-bar-container');
   graph_annotate('magenta');
}

function drm_svcs_cbox_cb(path, checked, first)
{
   var which = path.substr(-1,1);
   console.log('drm_svcs_cbox_cb path='+ path +' checked='+ checked +' which='+ which);
   if (first) return;
   for (var i = 1; i <= 4; i++)
      w3_checkbox_set('drm.svc'+ i, false);
   w3_checkbox_set(path, true);
   ext_send('SET svc='+ which);
}

function drm_monitor_cb(path, idx, first)
{
   if (first) return;
   var monitor = drm.monitor_s[idx];
   console.log('drm_monitor_cb idx='+ idx +' monitor='+ monitor);
   ext_send('SET monitor='+ idx);
}

function drm_display_cb(path, idx, first)
{
   drm.display = +idx;
   var canvas = (+idx)? 1:0;
   w3_color('id-drm-plot', null, idx? 'black' : 'mediumBlue');
   w3_hide('id-drm-'+ drm.display_s[drm.last_canvas]);
   w3_show('id-drm-'+ drm.display_s[canvas]);
   drm.last_canvas = canvas;
}

function drm_pre_select_cb(path, idx, first)
{
   if (first) return false;
   var rv = false;
	idx = +idx;
	var menu_n = parseInt(path.split('drm.menu')[1]);
   //console.log('drm_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);
   drm.last_disabled = false;

	w3_select_enum(path, function(option) {
	   //console.log('drm_pre_select_cb opt.val='+ option.value +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled) {
	      if (drm.last_disabled == false)
	         drm.station_id = [];
	      if (option.value != -1)
	         drm.station_id.push(option.innerHTML);
	   }
	   drm.last_disabled = option.disabled
	   
	   if (option.value == idx) {
	      drm.freq_s = option.innerHTML;
	      //console.log('drm_pre_select_cb MATCH opt.val='+ option.value +' freq_s='+ drm.freq_s);
	      drm_set_freq(parseFloat(drm.freq_s));

         // if called directly instead of from menu callback, select menu item
         w3_select_value(path, option.value);

         drm_station(drm.station_id.join(' ') + ((menu_n == drm.menu_India_MW)? ', India':''));
         drm_start();
         rv = true;
	   }
	});

   // reset other menus
   for (var i = 0; i < drm.n_menu; i++) {
      if (i != menu_n)
         w3_select_value('drm.menu'+ i, -1);
   }
   
   return rv;
}

function drm_station(s)
{
   drm.last_station = s;
   if (s == '') s = '&nbsp;';
   w3_el('id-drm-station').innerHTML = '<b>'+ s +'</b>';
   graph_annotate('magenta');
}

function drm_environment_changed(changed)
{
   // reset all frequency menus when frequency etc. is changed by some other means (direct entry, WF click, etc.)
   // but not for changed.zoom, changed.resize etc.
   var dsp_freq = ext_get_freq()/1e3;
   var mode = ext_get_mode();
   //console.log('DRM ENV drm.freq='+ drm.freq +' dsp_freq='+ dsp_freq);
   if (drm.freq != dsp_freq || mode != 'drm') {
      for (var i = 0; i < drm.n_menu; i++) {
         w3_select_value('drm.menu'+ i, -1);
      }
      drm.menu_sel = '';
      drm_station('');
   }
}

function drm_next_prev_cb(path, np, first)
{
	np = +np;
	//console.log('drm_next_prev_cb np='+ np);
	
   // if any menu has a selection value then select next/prev (if any)
   var prev = 0, capture_next = 0, captured_next = 0, captured_prev = 0;
   var menu;
   
   for (var i = 0; i < drm.n_menu; i++) {
      menu = 'drm.menu'+ i;
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
      drm_pre_select_cb(menu, val, false);
   }
}

function DRM_blur()
{
   console.log('DRM_blur saved_mode='+ drm.saved_mode);
   
   drm_stop();
   if (isUndefined(drm.saved_mode) || drm.saved_mode == 'drm') {
      console.log('DRM_blur FORCE iq');
      ext_set_mode('iq');
   }
   drm.locked = 0;
   ext_send('SET lock_clear');
}

function DRM_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Digital Radio Mondiale (DRM30) decoder help') + '<br><br>' +
         w3_div('w3-scroll-y|height:85%',
            '<hr>' +
            'DRM code from <b>Dream 2.2.1</b> <br>' +
            'Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik <br>' +
            'Copyright (c) 2001-2019 &nbsp;&nbsp;&nbsp;&nbsp;' +
            '<a href="https://sourceforge.net/projects/drm" target="_blank">sourceforge.net/projects/drm</a> <br>' +
            'License: GNU General Public License version 2.0 (GPLv2) <br>' +
            '<hr>' +
            '<b>Fraunhofer FDK AAC Codec Library for Android</b> <br>' +
            '© Copyright  1995 - 2018 Fraunhofer-Gesellschaft zur Förderung der angewandten <br>' +
            'Forschung e.V. All rights reserved. <br>' +
            'For more information visit <a href="http://www.iis.fraunhofer.de/amm" target="_blank">www.iis.fraunhofer.de/amm</a> <br>' +
            '<hr>' +
            '<b>OpenCORE-AMR modifications to Fraunhofer FDK AAC Codec</b> <br>' +
            'Copyright (C) 2009-2011 Martin Storsjo &nbsp;&nbsp;&nbsp;&nbsp;' +
            '<a href="https://sourceforge.net/projects/opencore-amr" target="_blank">sourceforge.net/projects/opencore-amr</a> <br>' +
            'License: Apache License V2.0 <br>' +
            '<hr>' +
            'Features <b>NewsService Journaline(R)</b> decoder technology by <br>' +
            'Fraunhofer IIS, Erlangen, Germany. <br>' +
            'Copyright (c) 2003, 2004 <br>' +
            'For more information visit <a href="http://www.iis.fhg.de/dab" target="_blank">www.iis.fhg.de/dab</a> <br>' +
            'License: GNU General Public License version 2.0 (GPLv2) <br>' +
            '<hr>'
         );
      confirmation_show_content(s, 610, 350);
      w3_el('id-confirmation-container').style.height = '100%';
   }
   return true;
}

// called to display HTML for configuration parameters in admin interface
function DRM_config_html()
{
   ext_config_html(drm, 'DRM', 'DRM', 'DRM configuration');
}
