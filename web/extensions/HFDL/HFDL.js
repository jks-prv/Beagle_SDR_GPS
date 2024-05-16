
// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

var hfdl = {
   ext_name: 'HFDL',    // NB: must match HFDL.cpp:hfdl_ext.name
   first_time: true,
   test_flight: false,  // requires test audio file to be played
   dataH: 445,
   dataW: 1024,
   ctrlW: 600,
   ctrlH: 185,
   freq: 0,
   sfmt: 'w3-text-red',
   pb: { lo: 300, hi: 2600 },
   
   stations: null,

   bf_cf:  [ 3500,  4670,  5600,  6625,  8900, 10065,  11290,  13310, 15020, 17945, 21965 ],
   bf_z:   [    4,     9,     6,     7,     7,     8,      7,      8,     8,     8,     8 ],
   bf:     [],
   bf_gs:  {},
   bf_color: [
      'pink', 'lightCoral', 'lightSalmon', 'khaki', 'gold', 'yellowGreen', 'lime', 'cyan', 'deepSkyBlue', 'lavender', 'violet'
   ],
   
   SHOW_MSGS: 0,
   SHOW_MAP: 1,
   SHOW_SPLIT: 2,
   show: 0,
   show_s: [ 'messages', 'map', 'split' ],

   ALL: 0,
   DX: 1,
   ACARS: 2,
   SQUITTER: 3,
   dsp: 0,
   dsp_s: [ 'all', 'DX', 'ACARS', 'squitter' ],
   uplink: true,
   downlink: true,
   
   testing: false,
   
   log_mins: 0,
   log_interval: null,
   log_txt: '',
   
   menu_n: 0,
   menu_s: [ ],
   menus: [ ],
   menu_sel: '',

   flights: {},
   prev_flight: null,
   too_old_min: 10,
   flights_visible: true,
   gs_visible: true,

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, cols: 135 }
};

function HFDL_main()
{
	ext_switch_to_client(hfdl.ext_name, hfdl.first_time, hfdl_recv);		// tell server to use us (again)
	if (!hfdl.first_time)
		hfdl_controls_setup();
	hfdl.first_time = false;
}

function hfdl_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		console.log('hfdl_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('hfdl_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('hfdl_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				kiwi_load_js_dir('pkgs_maps/', ['pkgs_maps.js', 'pkgs_maps.css'], 'hfdl_controls_setup');
				break;

			case "lowres_latlon":
			   var latlon = kiwi_decodeURIComponent('', param[1]);
			   if (dbgUs) console.log('lowres_latlon='+ latlon);
			   latlon = JSON.parse(latlon);
			   kiwi_map_add_marker_icon(hfdl.kmap, kmap.ADD_TO_MAP,
			      'kiwi/gfx/kiwi-with-headphones.51x67.png', latlon, [25, 33], 1.0,
			      function(el) {
                  w3_add(el, 'id-hfdl-kiwi-icon w3-hide');
                  el.style.zIndex = 99999;
			      }
			   );
			   break;

			case "bar_pct":
			   if (hfdl.testing) {
               var pct = w3_clamp(parseInt(param[1]), 0, 100);
               //if (pct > 0 && pct < 3) pct = 3;    // 0% and 1% look weird on bar
               var el = w3_el('id-hfdl-bar');
               if (el) el.style.width = pct +'%';
               //console.log('bar_pct='+ pct);
            }
			   break;

			case "test_start":
			   hfdl_test_cb('', 1);
			   break;

			case "test_done":
            if (hfdl.testing) {
               w3_hide('id-hfdl-bar-container');
            }
			   break;

			case "chars":
			   var s = param[1];
            var x = kiwi_decodeURIComponent('', param[1]).split('\n');
            //console.log(x);
            var uplink = x[1].startsWith('Uplink');
            var downlink = x[1].startsWith('Downlink');
            if (!hfdl.uplink && uplink) break;
            if (!hfdl.downlink && downlink) break;
            var flight = null, lat = null, lon = null;

            x.forEach(function(s, i) {
               x[i] = s.trim();
               if (uplink) return;
               var a = x[i].split(':');
               var p0 = a[0];
               var p1 = (a[1] && a[1] != '')? a[1].slice(1) : null;
               if (p0 == 'Flight ID' && p1) flight = p1; else
               if (p0 == 'Lat' && p1) lat = +p1; else
               if (p0 == 'Lon' && p1) lon = +p1;
            });
            if (flight && lat && lon && (lat != 180 || lon != 180)) {
               hfdl_flight_update(flight, lat, lon);
            }

            if (x[3].startsWith('Squitter')) {
               x.forEach(function(a, i) {
                  if (a.startsWith('ID:')) {
                     id = a.split(': ')[1];
                  } else
                  if (a.startsWith('Frequencies in use:')) {
                     a = a.split(': ')[1];
                     if (hfdl.dsp == hfdl.SQUITTER) s += id +': '+ a +'\n';
                     var f = a.split(', ');
                     hfdl_update_AFT(id.split(', ')[1], f);
                  }
               });
               //hfdl_update_AFT('New Zealand', [ '6535.0', '5583.0' ]);
               //hfdl_update_AFT('Alaska', [ '5529.0' ]);
               //hfdl_update_AFT('South Africa', [ '5529.0' ]);
               //hfdl_update_AFT('Ireland', [ '21949.0', '17922.0', '15025.0', '13321.0', '11384.0', '10081.0', '8942.0', '6532.0', '5547.0', '3455.0' ]);

               if (hfdl.test_flight) {
                  hfdl_flight_update('XYZ123', 40, 0);
                  setTimeout(function() { hfdl_flight_update('ABC987', 60, -20); }, 5000);
                  setTimeout(function() { hfdl_flight_update('ZZZ000', 50, 20); }, 60000);
               }
            }

            switch (hfdl.dsp) {
            
               case hfdl.ALL:
                  break;
            
               case hfdl.DX:
                  s = x[0] +'\n'+ x[1].slice(0,-1) +' | '+ x[2];

                  if (x[3].startsWith('Squitter')) {
                     s += ' | Squitter: ';
                     var first = true;
                     x.forEach(function(s1, i) {
                        if (i <= 3 || !s1.startsWith('ID:')) return;
                        var a = s1.split(', ');
                        if (a.length < 2 || a[1] == '') return;
                        s += (first? '' : ', ') + a[1];
                        first = false;
                     });
                     s += '\n';
                     break;
                  }

                  s += ' | '+ x[3];

                  var found = false, acars = false;
                  var ss = '', _type;
                  var src = '';
                  var type_s = [ 'Logon request', 'Logon resume', 'Logon confirm', 'Unnumbered data', 'Unnumbered ack\'ed' ];
                  type_s.forEach(function(type, i) {
                     if (found || !x[4].includes(type)) return;
                     found = true;
                     _type = type;
                     x.forEach(function(s1, j) {
                        //console.log(j +': '+ s1);
                        if (j <= 4) return;
                        var a = s1.split(':');
                        var p0 = a[0];
                        var p1 = (a[1] && a[1] != '')? a[1].slice(1) : null;
                        var p2 = (a[2] && a[2] != '')? a[2].slice(1) : null;

                        //if (p0 == 'ICAO' && p1) ss += ' | ICAO '+ p1; else
                        if (p0 == 'Flight ID' && p1) ss += ' | Flight '+ p1; else
                        if (p0 == 'Lat' && p1) ss += ' | Lat '+ ((+p1).toFixed(4)); else
                        if (p0 == 'Lon' && p1) ss += ' | Lon '+ ((+p1).toFixed(4)); else
                        if (p0 == 'ACARS') acars = 'ACARS data'; else
                        if (p0 == 'Message') acars = 'ACARS message'; else
                        if (uplink && p0 == 'Src GS') src = ' | '+ p1; else    // src is gs for uplinks
                        if (downlink && p1 && p1.endsWith('Flight')) src = ' | Flight '+ p2; else
                           ;
                     });
                  });

                  if (found) s += ' | '+ (acars? (acars + src + ss) : (_type + ss)); else s += ' | '+ x[4];
                  s += '\n';
                  break;
         
               case hfdl.ACARS:
                  var s = '';
                  var src = '';
                  var accum_acars_msgs = false;
                  var acars = '';
                  x.forEach(function(s1, j) {
                     //console.log('ACARS-'+ j +': '+ s1);
                     var a = s1.split(':');
                     var p0 = a[0];
                     var p1 = (a[1] && a[1] != '')? a[1].slice(1) : null;
                     var p2 = (a[2] && a[2] != '')? a[2].slice(1) : null;

                     if (uplink && p0 == 'Src GS') src = p1; else    // src is gs for uplinks
                     if (downlink && p1 && p1.endsWith('Flight')) src = 'Flight '+ p2; else
                     if (p0 == 'Message') { s = x[0] +'\nACARS message from '+ src +'\n'; accum_acars_msgs = true; return; } else
                     if (p0.startsWith('Media Advisory')) accum_acars_msgs = false;
                     
                     if (accum_acars_msgs && s1 != '')
                        acars += ' '+ s1 +'\n';
                  });
                  if (acars != '') s += acars;
                  break;
         
               case hfdl.SQUITTER:
                  if (!x[3].startsWith('Squitter')) { s = ''; break; }
                  s = x[0] +'\nSquitter | '+ x[2] +'\n';
                  var id = '';
                  x.forEach(function(a, i) {
                     if (a.startsWith('ID:')) {
                        id = a.split(': ')[1];
                     } else
                     if (a.startsWith('Frequencies in use:')) {
                        a = a.split(': ')[1];
                        s += id +': '+ a +'\n';
                     }
                  });
                  break;
            }

				if (s != '') hfdl_decoder_output_chars(s);
				break;

			default:
				console.log('hfdl_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function hfdl_decoder_output_chars(c)
{
   hfdl.console_status_msg_p.s = c;     // NB: already encoded on C-side
   hfdl.log_txt += kiwi_remove_escape_sequences(kiwi_decodeURIComponent('HFDL', c));

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-hfdl-console-msgs', 'id-hfdl-console-msg', hfdl.console_status_msg_p);
}

function hfdl_controls_setup()
{
   for (i = 0; i < hfdl.bf_cf.length; i++) {
      hfdl.bf[i] = Math.floor(hfdl.bf_cf[i]/1000);
   }

   var wh = sprintf('width:%dpx; height:%dpx;', hfdl.dataW, hfdl.dataH);
   var cbox = 'w3-label-inline w3-label-not-bold';
   var cbox2 = 'w3-margin-left//'+ cbox;
   
   var data_html =
      time_display_html('hfdl') +

      w3_div('id-hfdl-data w3-display-container|left:0px; '+ wh,

         // re w3-hide: for map to initialize properly it must be initially selected, then it will be hidden if
         // the 'map' URL param was not given.
         w3_div('id-hfdl-msgs w3-hide|'+ wh +'; z-index:1; overflow:hidden; position:absolute;',
            w3_div(sprintf('id-hfdl-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|width:%dpx; position:absolute; overflow-x:hidden;', 1024),
               '<pre><code id="id-hfdl-console-msgs"></code></pre>'
            )
         ),
         w3_div('|'+ wh +' position:absolute; z-index:0|id="id-hfdl-map"')
      ) +
      
      w3_div('id-hfdl-options w3-display-right w3-text-white|top:230px; right:0px; width:250px; height:200px',
         w3_text('w3-text-aqua w3-bold', 'HFDL options'),
         w3_select('w3-margin-T-4 w3-width-auto '+ hfdl.sfmt, '', 'show', 'hfdl.show', hfdl.show, hfdl.show_s, 'hfdl_show_cb'),
         
         w3_button('id-hfdl-show-kiwi w3-margin-T-10 w3-btn w3-small w3-cyan w3-text-css-white w3-momentary', 'Show Kiwi', 'hfdl_show_kiwi_cb', 1),

         w3_checkbox('w3-margin-T-10//'+ cbox, 'Show day/night', 'hfdl.day_night_visible', true, 'hfdl_day_night_visible_cb'),
         w3_checkbox(cbox, 'Show graticule', 'hfdl.graticule_visible', true, 'hfdl_graticule_visible_cb'),

         w3_inline('w3-margin-T-10 w3-valign/', 
            w3_checkbox('//'+ cbox, 'Show flights', 'hfdl.flights_visible', true, 'hfdl_flights_visible_cb'),
            w3_button('id-hfdl-clear-old w3-margin-left w3-btn w3-small w3-grey w3-text-css-white w3-momentary', 'Clear old', 'hfdl_clear_old_cb', 1)
         ),
         w3_checkbox(cbox, 'Show ground stations', 'hfdl.gs_visible', true, 'hfdl_gs_visible_cb')
      );

	var controls_html =
		w3_div('id-hfdl-controls w3-text-white',
         w3_col_percent('w3-tspace-8 w3-valign/',
            w3_div('w3-medium w3-text-aqua', '<b>HFDL decoder</b>'), 25,
            w3_div('', 'From <b><a href="https://github.com/szpajder/dumphfdl" target="_blank">dumphfdl</a></b> by Tomasz Lemiech &copy;2021 GPL-3.0</b>')
         ),

         w3_inline('w3-tspace-4 w3-halign-space-between/',
            w3_div('id-hfdl-msg')
         ),
         
         w3_inline('w3-margin-T-16/w3-margin-between-16',
            w3_inline('w3-valign-end w3-round-large w3-padding-small w3-text-white w3-grey/',
               w3_select(hfdl.sfmt, 'Display', '', 'hfdl.dsp', hfdl.dsp, hfdl.dsp_s, 'hfdl_display_cb'),
               w3_div('w3-margin-L-16',
                  w3_checkbox('/w3-label-inline w3-label-not-bold', 'Uplink', 'hfdl.uplink', hfdl.uplink, 'w3_bool_cb'),
                  w3_checkbox('/w3-label-inline w3-label-not-bold', 'Downlink', 'hfdl.downlink', hfdl.downlink, 'w3_bool_cb')
               )
            ),
            w3_inline('id-hfdl-menus w3-tspace-6 w3-gap-16/')
         ),

         w3_inline('w3-tspace-8 w3-valign/w3-margin-between-12',
            w3_button('w3-padding-smaller', 'Next', 'w3_select_next_prev_cb', { dir:w3_MENU_NEXT, id:'hfdl.menu', isNumeric:true, func:'hfdl_np_pre_select_cb' }),
            w3_button('w3-padding-smaller', 'Prev', 'w3_select_next_prev_cb', { dir:w3_MENU_PREV, id:'hfdl.menu', isNumeric:true, func:'hfdl_np_pre_select_cb' }),
            w3_button('id-hfdl-clear-button w3-padding-smaller w3-css-yellow', 'Clear', 'hfdl_clear_button_cb'),
            w3_button('id-hfdl-log w3-padding-smaller w3-purple', 'Log', 'hfdl_log_cb'),
            w3_button('id-id-hfdl-test w3-padding-smaller w3-aqua', 'Test', 'hfdl_test_cb', 1),
            w3_div('id-hfdl-bar-container w3-progress-container w3-round-large w3-white w3-hide|width:70px; height:16px',
               w3_div('id-hfdl-bar w3-progressbar w3-round-large w3-light-green|width:0%', '&nbsp;')
            ),
            
            w3_input('id-hfdl-log-mins/w3-label-not-bold/|padding:0;width:auto|size=4',
               'log min', 'hfdl.log_mins', hfdl.log_mins, 'hfdl_log_mins_cb')
         )
      );

	ext_panel_show(controls_html, data_html, null);
   ext_set_data_height(hfdl.dataH);
	ext_set_controls_width_height(hfdl.ctrlW, hfdl.ctrlH);

	time_display_setup('hfdl');
	var el = w3_el('hfdl-time-display');
	el.style.top = px(10);

	hfdl_msg('w3-text-css-yellow', '&nbsp;');
	HFDL_environment_changed( {resize:1, freq:1} );
   hfdl.save_agc_delay = ext_agc_delay(100);
	ext_send('SET start');
	
	hfdl.saved_mode = ext_get_mode();
	ext_set_mode('iq');
   ext_set_passband(hfdl.pb.lo, hfdl.pb.hi);    // optimal passband for HFDL
	
	// our sample file is 12k only
	if (ext_nom_sample_rate() != 12000)
	   w3_add('id-hfdl-test', 'w3-disabled');
	
	hfdl.kmap = kiwi_map_init('hfdl', [28, 15], 2, 17);

   // age flights
   hfdl.locations_age_interval = setInterval(function() {
      var old = Date.now() - hfdl.too_old_min*60*1000;
      w3_obj_enum(hfdl.flights, function(key, i, o) {
         //console.log('FL-OLD '+ o.flight +' '+ ((o.upd - old)/1000).toFixed(3));
         if (o.upd < old) {
            console.log('FL-OLD '+ o.flight +' > '+ hfdl.too_old_min +'m');
            w3_color(o.el, 'white', 'grey');
            if (hfdl.prev_flight == o.flight) {
               //console.log('HFDL grey is prev: '+ o.flight);
               hfdl.prev_flight = null;
            }
         }
      });
   }, hfdl.test_flight? 10000 : 60000);
   
	w3_do_when_rendered('id-hfdl-menus',
	   function() {
         ext_send('SET reset');
         hfdl.ext_url = kiwi_SSL() +'files.kiwisdr.com/hfdl/systable.cjson';
         hfdl.int_url = kiwi_url_origin() +'/extensions/HFDL/systable.backup.cjson';
         hfdl.using_default = false;
         hfdl.double_fault = false;
         if (0 && dbgUs) {
            kiwi_ajax(hfdl.ext_url +'.xxx', 'hfdl_get_systable_done_cb', 0, -500);
         } else {
            kiwi_ajax(hfdl.ext_url, 'hfdl_get_systable_done_cb', 0, 10000);
         }
      }
   );
   // REMINDER: w3_do_when_rendered() returns immediately
}


////////////////////////////////
// map
////////////////////////////////

function hfdl_show_kiwi_cb(path, idx, first)
{
   //console.log('hfdl_show_kiwi_cb idx='+ idx +' first='+ first);
   idx = +idx;
   var show = !idx;
   w3_color(path, show? 'lime':'white');
   
   //console.log('hfdl_show_kiwi_cb gs_visible='+ hfdl.gs_visible +' show='+ show);
   if (hfdl.flights_visible)
	   kiwi_map_markers_visible('id-hfdl-flight', !show);
   if (hfdl.gs_visible)
      hfdl_gs_visible(!show);
   w3_hide2('id-hfdl-kiwi-icon', !show);
}

function hfdl_clear_old_cb(path, idx, first)
{
   //console.log('hfdl_clear_old_cb idx='+ idx +' first='+ first);
   if (!(+idx)) return;
   var old = Date.now() - hfdl.too_old_min*60*1000;
   w3_obj_enum(hfdl.flights, function(key, i, o) {
      if (o.upd > old) return;
      console.log('FL-CLR '+ o.flight);
      o.mkr.remove();
      delete hfdl.flights[o.flight];
   });
}

function hfdl_flights_visible_cb(path, checked, first)
{
   //console.log('hfdl_flights_visible_cb checked='+ checked +' first='+ first);
   if (first) return;
   hfdl.flights_visible = checked;
	kiwi_map_markers_visible('id-hfdl-flight', checked);
}

function hfdl_gs_visible(vis)
{
	kiwi_map_markers_visible('id-hfdl-AFT-active', vis);
	kiwi_map_markers_visible('id-hfdl-gs', vis);
}

function hfdl_gs_visible_cb(path, checked, first)
{
   //console.log('hfdl_gs_visible_cb checked='+ checked +' first='+ first);
   if (first) return;
   hfdl.gs_visible = checked;
   w3_checkbox_set(path, checked);     // for benefit of direct callers
   hfdl_gs_visible(checked);
}

function hfdl_day_night_visible_cb(path, checked, first)
{
   if (first) return;
   if (!hfdl.kmap.day_night) return;
   var checked = w3_checkbox_get(path);
   kiwi_map_day_night_visible(hfdl.kmap, checked);
}

function hfdl_graticule_visible_cb(path, checked, first)
{
   //console.log('hfdl_graticule_visible_cb checked='+ checked +' first='+ first);
   if (first) return;
   if (!hfdl.kmap.graticule) return;
   checked = w3_checkbox_get(path);
   kiwi_map_graticule_visible(hfdl.kmap, checked);
}

function hfdl_map_stations()
{
   hfdl.refs = kiwi_deep_copy(hfdl.stations.stations);
   var markers = [];
   hfdl.refs.forEach(function(r, i) {
      //r.id_lcase = r.id.toLowerCase();
      var mkr = hfdl_place_gs_marker(i);
      markers.push(mkr);
      r.mkr = mkr;
   });
   //jksx not referenced? hfdl.gs_markers = markers;
   ext_send('SET send_lowres_latlon');
}

function hfdl_place_gs_marker(gs_n)
{
   var i;
   var r = hfdl.refs[gs_n];
   r.id = r.name.split(', ')[1];
   r.title = r.name;
   
   hfdl.bf_gs[r.id] = gs_n;
   var marker = kiwi_map_add_marker_div(hfdl.kmap, kmap.NO_ADD_TO_MAP, [r.lat, r.lon], '', [12, 12], [0, 0], 1.0);

   // when not using MarkerCluster add marker to map here
   if (hfdl.kmap.map) {
      //console.log(r);
      var left = (r.id == 'New Zealand');
      kiwi_style_marker(hfdl.kmap, kmap.ADD_TO_MAP, marker, r.id, 'id-hfdl-gs', left);
   
      // band icons
      var sign = left? 1:-1;
      for (i = 0; i < hfdl.bf.length; i++) {
         var xo = sign*19*i;
         kiwi_map_add_marker_div(hfdl.kmap, kmap.ADD_TO_MAP, [r.lat, r.lon],
            'id-hfdl-AFT id-hfdl-AFT-'+ gs_n +'-'+ i +' w3-hide',
            [sign* (left? 26:6) + xo, left? 33 : -13], [0, 0], 1.0);

         var el = w3_el('id-hfdl-AFT-'+ gs_n +'-'+ i);
         el.addEventListener('mouseenter', function(ev) {
            var mkr = ev.target;
            hfdl.saved_color = mkr.style.background;
            mkr.style.background = 'yellow';
         });
         el.addEventListener('mouseleave', function(ev) {
            var mkr = ev.target;
            mkr.style.background = hfdl.saved_color;
         });
         el.addEventListener('click', function(ev) {
         
            // match on freq and gs name both to distinguish multiple gs on same freq
            console.log('*click*');
            console.log(ev);
            var mkr = ev.target;
            //var cf = parseInt(mkr.textContent);
            var cf = w3_match_wildcard(mkr, 'id-hfdl-AFT-f-');
            if (cf == false) return;
            console.log('click cf1='+ dq(cf));
            cf = cf.split('f-')[1];
            cf = cf.toLowerCase().replace(/_/g, ' ');
            console.log('click cf2='+ dq(cf));
            var rv = hfdl_menu_match(null, cf);
            if (rv.found_menu_match) {
               var b = hfdl_freq_2_band(parseInt(cf));
               var rv2 = hfdl_menu_match(b, b.toString());
               console.log('click BAND cf='+ dq(cf) +' b='+ b +' '+ dq(rv2.match_menu) +' '+ dq(rv2.match_val));
               hfdl_pre_select_cb(rv2.match_menu, rv2.match_val, false);
               hfdl_pre_select_cb(rv.match_menu, rv.match_val, false);
            }
         });

         el.innerHTML = ((hfdl.bf[i].toString().length < 2)? '&nbsp;' : '') + hfdl.bf[i];
         el.style.background = hfdl.bf_color[i];
      }

   }
   
   r.type_host = false;
   return marker;
}

function hfdl_flight_update(flight_name, lat, lon)
{
   var flight_o;
   
   if (!hfdl.flights[flight_name]) {
      console.log('FL-NEW '+ flight_name +' '+ lat.toFixed(4) +' '+ lon.toFixed(4));

      var marker = kiwi_map_add_marker_div(hfdl.kmap, kmap.NO_ADD_TO_MAP,
         [lat, lon], '', [12, 12], [0, 0], 1.0);
      flight_o = { flight: flight_name, mkr: marker, upd: Date.now(), pos: [] };
      if (hfdl.test_flight && flight_name.startsWith('ABC')) {
         console.log('HFDL accelerate age: '+ flight_name);
         flight_o.upd -= (hfdl.too_old_min-0.5)*60*1000;
         hfdl.test_flight = false;
      }
      flight_o.pos.push([lat, lon]);
      hfdl.flights[flight_name] = flight_o;
      
      kiwi_style_marker(hfdl.kmap, kmap.ADD_TO_MAP, marker, flight_name,
         'id-hfdl-flight id-hfdl-flight-'+ flight_name + (hfdl.flights_visible? '' : ' w3-hide'),
         kmap.DIR_RIGHT,
         function(ev) {
            // sometimes multiple instances exist, so iterate to catch them all
            w3_iterate_classname('id-hfdl-flight-'+ flight_name,
               function(el) {
                  if (el) {
                     //console.log(el);
                     w3_color(el, 'black', 'yellow');
                     flight_o.el = el;
                  }
               }
            );
         }
      );
   } else {
      flight_o = hfdl.flights[flight_name];
      var marker = flight_o.mkr;
      marker.setLatLng([lat, lon]);
      var n = flight_o.pos.push([lat, lon]);
      w3_color(flight_o.el, 'black', 'yellow');    // might have re-appeared after previously going grey

      var now = Date.now();
      var dt = Math.floor(((now - flight_o.upd) / 60000) % 60);
      flight_o.upd = now;
      console.log('FL-UPD '+ flight_name +' '+ lat.toFixed(4) +' '+ lon.toFixed(4) +' #'+ n +' '+ dt +'m');
   }
   
   if (hfdl.prev_flight) {
      flight_o = hfdl.flights[hfdl.prev_flight];
      w3_color(flight_o.el, 'white', 'blue');
   }
   hfdl.prev_flight = flight_name;
}

function hfdl_update_AFT(gs, freqs)
{
   var i;
   //if (dbgUs) console.log('AFT: '+ gs);
   var left = (gs == 'New Zealand');
   if (!left) freqs.sort(kiwi_sort_numeric);
   //if (dbgUs) console.log(freqs);
   var gs_n = hfdl.bf_gs[gs];
   var hi = 0;
   freqs.forEach(function(s, i) {
      var el = w3_el('id-hfdl-AFT-'+ gs_n +'-'+ i);
      var b = hfdl_freq_2_band(s);
      el.innerHTML = ((b.toString().length < 2)? '&nbsp;' : '') + b;
      w3_remove_wildcard(el, 'id-hfdl-AFT-f-');
      //console.log(gs +'-'+ i +' <'+ s +'> '+ typeof(s));
      w3_add(el, 'id-hfdl-AFT-f-'+ (+s).toFixed(0) +'_'+ gs.replace(/ /g, '_'));

      w3_obj_enum(hfdl.bf,
         function(key, i, o){
            //console.log('AFT: key='+ key +' i='+ i);
            //console.log(o);
            el.style.background = hfdl.bf_color[i];
         },
         { value_match: b }
      );
      if (hfdl.gs_visible) w3_remove(el, 'w3-hide');
      w3_add(el, 'id-hfdl-AFT-active');
      hi = i+1;
   });
   for (i = hi; i < hfdl.bf_cf.length; i++) {
      var el = w3_el('id-hfdl-AFT-'+ gs_n +'-'+ i);
      w3_add(el, 'w3-hide');
      w3_remove(el, 'id-hfdl-AFT-active');
   }
}

function hfdl_map_click_cb(kmap, e)
{
   //console.log('hfdl_click_info_cb');
}

function hfdl_map_move_end_cb(kmap, e)
{
   //var m = kmap.map;
   //var c = m.getCenter();
   //w3_innerHTML('id-hfdl-info', 'map center: '+ c.lat.toFixed(2) +', '+ c.lng.toFixed(2) +' z'+ m.getZoom());
}

////////////////////////////////
// end map
////////////////////////////////


function hfdl_freq_2_band(f)
{
   var b = Math.floor((+f)/1000);
   if (b == 2) b = 3;
   return b;
}

function hfdl_menu_match(m_freq, m_str)
{
   var rv = { found_menu_match: false, match_menu: 0, match_val: 0 };
   var menu = 'hfdl.menu1';   // limit matching to "Bands" menu
   var match = false;

   if (dbgUs) console.log('CONSIDER '+ menu +' '+ dq(m_str) +' -----------------------------------------');
   w3_select_enum(menu, function(option, j) {
      if (option.disabled) return;
      var val = +option.value;
      if (rv.found_menu_match || val == -1) return;
      var menu_f = parseFloat(option.innerHTML);
      var menu_s = option.innerHTML.toLowerCase();
      if (dbgUs) console.log('CONSIDER '+ val +' '+ dq(option.innerHTML));

      if (isNumber(m_freq) && isNumber(menu_f)) {
         if (menu_f == m_freq) {
            if (dbgUs) console.log('MATCH num: '+ dq(menu_s) +'['+ j +']');
            match = true;
         }
      } else {
         if (dbgUs) console.log('menu_s '+ dq(menu_s) +' m_str '+ dq(m_str) +' '+ TF(menu_s.includes(m_str)));
         if (menu_s.includes(m_str)) {
            if (dbgUs) console.log('MATCH str: '+ dq(menu_s) +'['+ j +']');
            match = true;
         }
      }

      if (match) {
         if (dbgUs) console.log('MATCH rv menu='+ menu +' val='+ val +' '+ dq(option.innerHTML));
         // delay call to hfdl_pre_select_cb() until other params processed below
         rv.match_menu = menu;
         rv.match_val = val;
         rv.match_entry = option.innerHTML;
         rv.found_menu_match = true;
      }
   });
   
   return rv;
}

function hfdl_get_systable_done_cb(stations)
{
   hfdl.stations = stations;
   
   ext_get_menus_cb(hfdl, stations,
      'hfdl_get_systable_done_cb',  // retry_cb

      function(cb_param) {    // done_cb
         hfdl_map_stations();
         hfdl_render_menus();
   
         hfdl.url_params = ext_param();
         if (dbgUs) console.log('url_params='+ hfdl.url_params);
   
         if (hfdl.url_params) {
            var p = hfdl.url_params.split(',');

            // first URL param can be a match in the preset menus
            var m_freq = p[0].parseFloatWithUnits('kM', 1e-3);
            var m_str = kiwi_decodeURIComponent('hfdl', p[0]).toLowerCase();
            if (dbgUs) console.log('URL freq='+ m_freq);
            var do_test = 0;

            // select matching menu item frequency
            var rv = hfdl_menu_match(m_freq, m_str);

            if (rv.found_menu_match)
               p.shift();

            p.forEach(function(a, i) {
               if (dbgUs) console.log('HFDL param2 <'+ a +'>');
               var r;
               if (w3_ext_param('help', a).match) {
                  ext_help_click();
               } else
               if (w3_ext_param('map', a).match) {
                  hfdl.show = hfdl.SHOW_MAP;
               } else
               if (w3_ext_param('split', a).match) {
                  hfdl.show = hfdl.SHOW_SPLIT;
               } else
               if ((r = w3_ext_param('display', a)).match) {
                  if (isNumber(r.num)) {
                     var idx = w3_clamp(r.num, 0, hfdl.dsp_s.length-1, 0);
                     //console.log('display '+ r.num +' '+ idx);
                     hfdl_display_cb('id-hfdl.dsp', idx);
                  }
               } else
               if ((r = w3_ext_param('log_time', a)).match) {
                  if (isNumber(r.num)) {
                     hfdl_log_mins_cb('id-hfdl.log_mins', r.num);
                  }
               } else
               if ((r = w3_ext_param('gs', a)).match) {
                  if (isNumber(r.num) && r.num == 0) {
                     hfdl_gs_visible_cb('id-hfdl.gs_visible', false);
                  }
               } else
               if (w3_ext_param('test', a).match) {
                  do_test = 1;
               } else
                  console.log('HFDL: unknown URL param "'+ a +'"');
            });
      
            if (rv.found_menu_match) {
               // select band first if freq is not already a band specifier
               var f = rv.match_entry.split(' ')[0];

               if (f.length > 2) {
                  var b = hfdl_freq_2_band(f);
                  var rv2 = hfdl_menu_match(b, b.toString());
                  console.log('BAND b='+ b +' '+ dq(rv2.match_menu) +' '+ dq(rv2.match_val));
                  hfdl_pre_select_cb(rv2.match_menu, rv2.match_val, false);
               }
         
               hfdl_pre_select_cb(rv.match_menu, rv.match_val, false);
            }
         }

         setTimeout(function() { hfdl_show_cb('id-hfdl.show', hfdl.show); }, 1000);

         if (do_test) hfdl_test_cb('', 1);
      }, null
   );
}

function hfdl_render_menus()
{
   var i;
   var band = [];
   for (i = 0; i < hfdl.bf.length; i++) band[i] = [];
   
   var add_bands_menu = function(f) {
      //console.log(f);
      var f_n = parseInt(f);
      for (i = 0; i < hfdl.bf.length; i++) {
         if (f_n < (hfdl.bf[i] + 1) * 1000) {
            band[i].push(f);
            break;
         }
      }
   };

   var new_menu = function(el, i, oo, menu_s) {
      var o = kiwi_deep_copy(oo);
      hfdl.menu_n++;
      hfdl['menu'+i] = -1;
      hfdl.menus[i] = o;
      hfdl.menu_s[i] = menu_s;
      var m = [];

      w3_obj_enum(o, function(key_s, j, o2) {
         //console.log(menu_s +'.'+ i +'.'+ key_s +'.'+ j +':');
         //console.log(o2);
         var name_s = o2.name.split(', ')[1];
         o2.frequencies.sort(kiwi_sort_numeric);
         var freq_a = [];
         o2.frequencies.forEach(function(f) {
            if (f < hfdl.bf.length) {
               var a = [ name_s, 'w3-bold w3-text-blue' ];
               freq_a.push(a);
            } else {
               freq_a.push(f);
            }
            if (i == 0 && f != 0) add_bands_menu(f +' '+ name_s);
         });
         if (i == 0)
            m[name_s] = freq_a;
         else
            m.push(freq_a);
      });

      //console.log('m=...');
      //console.log(m);
      var s = w3_select_hier(hfdl.sfmt, menu_s, 'select', 'hfdl.menu'+ i, -1, m, 'hfdl_pre_select_cb');
      var el2 = w3_create_appendElement(el, 'div', s);
      w3_add(el2, 'id-'+ menu_s);
   };
   
   var s;
   var el = w3_el('id-hfdl-menus');
   el.innerHTML = '';
   hfdl.menu_n = 0;

   // "Stations" menu
   w3_obj_enum(hfdl.stations, function(key_s, i, o2) {
      //console.log(key_s +'.'+ i +':');
      //console.log(o2);
      if (key_s != 'stations') return;
      new_menu(el, 0, o2, 'Stations');
   });

   // "Bands" menu
   var bands = [];
   for (i = 0; i < hfdl.bf.length; i++) {
      band[i].sort(kiwi_sort_numeric_reverse);
      var o = {};
      o.name = 'x, '+ hfdl.bf[i] + ' MHz';
      o.frequencies = [ i ];
      band[i].forEach(function(f) { o.frequencies.push(f); });
      bands[i.toString()] = o;
   }
   //console.log(bands);
   new_menu(el, 1, bands, 'Bands');
   //console.log('hfdl_render_menus hfdl.menu_n='+ hfdl.menu_n);
}

function hfdl_format_cb(path, idx, first)
{
   if (first) return;
   hfdl.format_m = +idx;
   w3_set_value(path, +idx);     // for benefit of direct callers
   hfdl_render_menus();
}

function hfdl_msg(color, s)
{
   var el = w3_el('id-hfdl-msg');
   w3_remove_then_add(el, hfdl.msg_last_color, color);
   hfdl.msg_last_color = color;
   el.innerHTML = '<b>'+ s +'</b>';
   //console.log('MSG '+ s);
}

function hfdl_clear_menus(except)
{
   // reset frequency menus
   for (var i = 0; i < hfdl.menu_n; i++) {
      if (isNoArg(except) || i != except)
         w3_select_value('hfdl.menu'+ i, -1);
   }
}

function hfdl_np_pre_select_cb(path, val, first)
{
   hfdl_pre_select_cb(path, val, first);
}

function hfdl_pre_select_cb(path, val, first)
{
   if (first) return;
   val = +val;
   if (dbgUs) console.log('hfdl_pre_select_cb path='+ path +' val='+ val);

	var menu_n = parseInt(path.split('hfdl.menu')[1]);
   if (dbgUs) console.log('hfdl_pre_select_cb path='+ path +' val='+ val +' menu_n='+ menu_n);

   // find matching object entry in hfdl.menus[] hierarchy and set hfdl.* parameters from it
   var header = '';
   var cont = 0;
   var found = false;

	w3_select_enum(path, function(option) {
	   if (found) return;
	   if (dbgUs) console.log('hfdl_pre_select_cb menu_n='+ menu_n +' opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML +' opt.id='+ option.id);
	   
	   var menu0_hdr = (menu_n == 0 && option.disabled && option.value != -1);
	   var menu1_hdr = (menu_n == 1 && option.id && option.id.split('-')[2] == 0);
	   if (dbgUs) console.log('menu0_hdr='+ menu0_hdr +' menu1_hdr='+ menu1_hdr);
	   if (menu0_hdr || menu1_hdr) {
	      if (cont)
	         header = header +' '+ option.innerHTML;
	      else
	         header = option.innerHTML;
	      cont = 1;
	   } else {
	      cont = 0;
	   }
	   
	   if (option.value != val) return;
	   found = true;
      hfdl.cur_menu = menu_n;
      hfdl.cur_header = header;
	   
      hfdl.menu_sel = option.innerHTML +' ';
      if (dbgUs) console.log('hfdl_pre_select_cb opt.val='+ option.value +' menu_sel='+ hfdl.menu_sel +' opt.id='+ option.id);

      var id = option.id.split('id-')[1];
      if (isUndefined(id)) {
	      console.log('hfdl_pre_select_cb: option.id isUndefined');
	      console.log('hfdl_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	      console.log(option);
	      return;
      }
      
      id = id.split('-');
      var i = +id[0];
      var j = +id[1];
      if (dbgUs) console.log('hfdl_pre_select_cb i='+ i +' j='+ j);
      var o1 = w3_obj_seq_el(hfdl.menus[menu_n], i);
      //console.log(hfdl.menus[menu_n]);
      //console.log('o1=...');
      //console.log(o1);
      if (dbgUs) w3_console.log(o1, 'o1');
      o2 = w3_obj_seq_el(o1.frequencies, j);
      //console.log('o2=...');
      //console.log(o2);
      if (dbgUs) w3_console.log(o2, 'o2');
   
      var s = null, show_msg = 0;
      o2 = parseInt(o2);
      if (isNumber(o2)) {
         if (dbgUs) console.log(o2);
         
         if (o2 < hfdl.bf.length) {
            var znew = hfdl.bf_z[o2];
            var cf = hfdl.bf_cf[o2];
            //console.log('$show band '+ o2 +' cf='+ cf +' z='+ znew);
            if (zoom_level == znew)
               zoom_step(ext_zoom.OUT);   // force ext_tune() to re-center waterfall on cf
            ext_tune(cf, 'iq', ext_zoom.ABS, znew);
            
            // switch to EiBi database displaying only HFDL labels
            console.log('$hfdl_pre_select_cb db='+ dx.db +' single_type='+ !dx_is_single_type(dx.DX_HFDL));
            if (dx.db != dx.DB_EiBi || !dx_is_single_type(dx.DX_HFDL)) {
               dx_set_type(dx.DX_HFDL);
               dx_database_cb('', dx.DB_EiBi);
            }
            show_msg = 1;
         } else {
            hfdl_tune(o2);
            s = (+o2).toFixedNZ(1) +' kHz';
            show_msg = 1;
         }
      }

      if (show_msg) {
         hfdl_msg('w3-text-css-yellow', hfdl.menu_s[menu_n] +', '+ hfdl.cur_header + (s? (': '+ s) : ''));
      }

      // if called directly instead of from menu callback, select menu item
      w3_select_value(path, val);
	});

   // reset other frequency menus
   hfdl_clear_menus(menu_n);
}

function hfdl_tune(f_kHz)
{
   if (dbgUs) console.log('hfdl_tune tx f='+ f_kHz.toFixed(2));
   hfdl.freq = f_kHz;
   ext_tune(f_kHz, 'iq', ext_zoom.CUR);
   ext_set_passband(hfdl.pb.lo, hfdl.pb.hi);    // optimal passband for HFDL
   ext_send('SET display_freq='+ f_kHz.toFixed(2));
}

function hfdl_clear_button_cb(path, val, first)
{
   if (first) return;
   //console.log('hfdl_clear_button_cb'); 
   hfdl.console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-hfdl-console-msgs', 'id-hfdl-console-msg', hfdl.console_status_msg_p);
   hfdl.log_txt = '';
   hfdl_test_cb('', 0);
}

function hfdl_show_cb(path, idx, first)
{
	//console.log('hfdl_show_cb: idx='+ idx +' first='+ first);
	if (first) return;
   idx = +idx;
   hfdl.show = idx;
	w3_set_value(path, idx);
	w3_hide2('id-hfdl-msgs', idx == hfdl.SHOW_MAP);
	w3_hide2('id-hfdl-map', idx == hfdl.SHOW_MSGS);
	var splitH = 150;
	w3_el('id-hfdl-msgs').style.top = px((idx == hfdl.SHOW_SPLIT)? (hfdl.dataH - splitH) : 0);
	w3_el('id-hfdl-msgs').style.height = px((idx == hfdl.SHOW_SPLIT)? splitH : hfdl.dataH);
	if (idx == hfdl.SHOW_SPLIT)
	   w3_scrollDown('id-hfdl-console-msg');
}

function hfdl_display_cb(path, idx, first)
{
	//console.log('hfdl_display_cb: idx='+ idx +' first='+ first);
	if (first) return;
   idx = +idx;
   hfdl.dsp = idx;
	w3_set_value(path, idx);
}

function hfdl_test_cb(path, val, first)
{
   if (first) return;
   val = +val;
   if (dbgUs) console.log('hfdl_test_cb: val='+ val);
   hfdl.testing = val;
	w3_el('id-hfdl-bar').style.width = '0%';
   w3_show_hide('id-hfdl-bar-container', hfdl.testing);
   
   // positive value gates test audio to a specific freq
   // negative value applies test audio at all times
   //var f = '24000.60';
   var f = -(+ext_get_freq_kHz()).toFixed(2);
   if (dbgUs) console.log('hfdl_test_cb='+ (val? f : 0));
	ext_send('SET test='+ (val? f : 0));
}

function hfdl_log_mins_cb(path, val)
{
   hfdl.log_mins = w3_clamp(+val, 0, 24*60, 0);
   console.log('hfdl_log_mins_cb path='+ path +' val='+ val +' log_mins='+ hfdl.log_mins);
	w3_set_value(path, hfdl.log_mins);

   kiwi_clearInterval(hfdl.log_interval);
   if (hfdl.log_mins != 0) {
      console.log('HFDL logging..');
      hfdl.log_interval = setInterval(function() { hfdl_log_cb(); }, hfdl.log_mins * 60000);
   }
}

function hfdl_log_cb()
{
   var txt = new Blob([hfdl.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = kiwi_timestamp_filename('HFDL.', '.log.txt');
   document.body.appendChild(a);
   console.log('hfdl_log: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);
}

// automatically called on changes in the environment
function HFDL_environment_changed(changed)
{
   //w3_console.log(changed, 'HFDL_environment_changed');

   if (changed.freq || changed.mode) {
      var f_kHz = (+ext_get_freq_kHz()).toFixed(0);
      var hfdl_f_kHz = (+hfdl.freq).toFixed(0);
      var mode = ext_get_mode();
      //console.log('HFDL_environment_changed: freq='+ hfdl_f_kHz +' f_kHz='+ f_kHz +' mode='+ mode);
	   if (mode != 'iq') {
	      //console.log('hfdl_clear_menus()');
	      hfdl_clear_menus();
	   } else
	   if (hfdl_f_kHz != f_kHz && mode == 'iq') {
	      // try and match new freq to one of the menu entries
	      //console.log('HFDL_environment_changed: f='+ f_kHz);
	      var rv = hfdl_menu_match(+f_kHz, f_kHz);
         if (rv.found_menu_match)
            hfdl_pre_select_cb(rv.match_menu, rv.match_val, false);
      }
   }

   if (changed.resize) {
      var el = w3_el('id-hfdl-data');
      if (!el) return;
      var left = Math.max(0, (window.innerWidth - hfdl.dataW - time_display_width()) / 2);
      //console.log('hfdl_resize wiw='+ window.innerWidth +' hfdl.dataW='+ hfdl.dataW +' time_display_width='+ time_display_width() +' left='+ left);
      el.style.left = px(left);
      return;
   }

   /*
      if (changed.resize) {
         var el = w3_el('id-hfdl-data');
         var left = (window.innerWidth - 1024 - time_display_width()) / 2;
         el.style.left = px(left);
      }
   */
}

function HFDL_focus()
{
   hfdl.dx_db_save = dx.db;
   hfdl.dx_type_save = dx.eibi_types_mask;
   console.log('HFDL save dx_db_save='+ hfdl.dx_db_save +' dx_type_save='+ hfdl.dx_type_save.toHex());
}

function HFDL_blur()
{
   // anything that needs to be done when extension blurred (closed)
	ext_set_mode(hfdl.saved_mode);
   ext_agc_delay(hfdl.save_agc_delay);
   kiwi_clearInterval(hfdl.log_interval);
   kiwi_clearInterval(hfdl.locations_age_interval);
   kiwi_map_blur(hfdl.kmap);
   
   console.log('HFDL restore dx_db_save='+ hfdl.dx_db_save +' dx_type_save='+ hfdl.dx_type_save.toHex());
   dx.eibi_types_mask = hfdl.dx_type_save;
   dx_database_cb('', hfdl.dx_db_save);
}

function HFDL_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'HFDL decoder help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               'Periodic downloading of the HFDL message log to a file can be specified via the <i>log min</i> value. ' +
               'Adjust your browser settings so these files are downloaded ' +
               'and saved automatically without causing a browser popup window for each download.' +

               '<br><br>URL parameters: <br>' +
               w3_text('|color:orange', '(menu match) &nbsp; map|split &nbsp; display:[<i>012</i>] &nbsp; ' +
               'log_time:<i>mins</i> &nbsp; gs:0 &nbsp; test') +
               '<br><br>' +
               'The first URL parameter can be a frequency entry from the "Bands" menu (e.g. "8977") or the ' +
               'numeric part of the blue "full band" entry (e.g. "5" part of "5 MHz" entry). <br>' +
               '<i>map</i> will initially show the map instead of the message panel. ' +
               '<i>split</i> will show both. <br>' +
               '[012] refers to the order of selections in the corresponding menu. <br>' +
               '<i>gs:0</i> initially disables display of the ground stations.' +
               '<br><br>' +
               'Keywords are case-insensitive and can be abbreviated. So for example these are valid: <br>' +
               '<i>ext=hfdl,8977</i> &nbsp;&nbsp; ' +
               '<i>ext=hfdl,8977,d:1</i> &nbsp;&nbsp; <i>ext=hfdl,8977,l:10</i> &nbsp;&nbsp; <i>ext=hfdl,5,map</i><br>' +
               ''
            )
         );

      confirmation_show_content(s, 610, 300);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}

// called to display HTML for configuration parameters in admin interface
function HFDL_config_html()
{
   ext_config_html(hfdl, 'hfdl', 'HFDL', 'HFDL configuration');
}
