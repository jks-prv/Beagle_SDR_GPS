
// Copyright (c) 2021 John Seamons, ZL/KF6VO

var hfdl = {
   ext_name: 'HFDL',    // NB: must match HFDL.cpp:hfdl_ext.name
   first_time: true,
   dataH: 445,
   dataW: 1024,
   ctrlW: 600,
   ctrlH: 185,
   freq: 0,
   sfmt: 'w3-text-red w3-ext-retain-input-focus',
   pb: { lo: 300, hi: 2600 },
   
   stations: null,
   url: kiwi_SSL() +'files.kiwisdr.com/hfdl/systable.cjson',
   using_default: false,
   double_fault: false,

   bf_cf:  [ 3500,  4670,  5600,  6625,  8900, 10065,  11290,  13310, 15020, 17945, 21965 ],
   bf_z:   [    4,     9,     6,     7,     7,     8,      7,      8,     8,     8,     8 ],
   bf:     [],
   bf_gs:  {},
   bf_color: [
      'pink', 'lightCoral', 'lightSalmon', 'khaki', 'gold', 'yellowGreen', 'lime', 'cyan', 'deepSkyBlue', 'lavender', 'violet'
   ],
   
   MSGS: 0,
   MAP: 1,
   SPLIT: 2,
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
   too_old_min: 10,
   flights_visible: true,
   gs_visible: true,
   
   // map
   cur_map: null,
   map_layers: [],
   init_latlon: [28, 15],
   init_zoom: 2,
   mapZoom_nom: 17,
   hosts_clusterer: null,
   refs_clusterer: null,
   heatmap_visible: true,
   show_hosts: true,
   show_refs: true,

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, ncol: 135 }
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
				kiwi_load_js(['pkgs/js/sprintf/sprintf.js']);
				kiwi_load_js_dir('pkgs_maps/', ['pkgs_maps.js', 'pkgs_maps.css'], 'hfdl_controls_setup');
				break;

			case "lowres_latlon":
			   var latlon = kiwi_decodeURIComponent('', param[1]);
			   if (dbgUs) console.log('lowres_latlon='+ latlon);
			   latlon = JSON.parse(latlon);

            var kiwi_icon =
               L.icon({
                  iconUrl: 'kiwi/gfx/kiwi-with-headphones.51x67.png',
                  iconAnchor: [25, 33]
               });

            var kiwi_mkr = L.marker(latlon, { 'icon':kiwi_icon, 'opacity':1.0 });
            //console.log(kiwi_mkr);
            kiwi_mkr.addTo(hfdl.cur_map);
            w3_add(kiwi_mkr._icon, 'id-hfdl-kiwi-icon w3-hide');
            kiwi_mkr._icon.style.zIndex = 99999;
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
               w3_show('id-hfdl-record');
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
   hfdl.log_txt += kiwi_remove_ANSI_escape_sequences(kiwi_decodeURIComponent('HFDL', c));

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
         w3_div('id-hfdl-msgs w3-hide|width:1024px; height:'+ px(hfdl.dataH) +';  z-index:1; overflow:hidden; position:absolute;',
            w3_div(sprintf('id-hfdl-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|width:%dpx; position:absolute; overflow-x:hidden;', 1024),
               '<pre><code id="id-hfdl-console-msgs"></code></pre>'
            )
         ),
         w3_div('|'+ wh +' position:absolute; z-index:0|id="id-hfdl-map"')
      ) +
      
      w3_div('id-hfdl-options w3-display-right w3-text-white|top:230px; right:0px; width:250px; height:200px',
         w3_text('w3-text-aqua w3-bold', 'HFDL options'),
         w3_select('w3-margin-T-4 w3-width-auto '+ hfdl.sfmt, '', 'show', 'hfdl.show', hfdl.show, hfdl.show_s, 'hfdl_show_cb'),
         
         w3_button('id-hfdl-show-kiwi w3-margin-T-10 class-button w3-cyan w3-momentary', 'Show Kiwi', 'hfdl_show_kiwi_cb', 1),

         w3_checkbox('w3-margin-T-10//'+ cbox, 'Show day/night', 'hfdl.day_night_visible', true, 'hfdl_day_night_visible_cb'),
         w3_checkbox(cbox, 'Show graticule', 'hfdl.graticule_visible', true, 'hfdl_graticule_visible_cb'),

         w3_inline('w3-margin-T-10 w3-valign/', 
            w3_checkbox('//'+ cbox, 'Show flights', 'hfdl.flights_visible', true, 'hfdl_flights_visible_cb'),
            w3_button('id-hfdl-show-kiwi w3-margin-left class-button w3-small w3-grey w3-momentary', 'Clear old', 'hfdl_clear_old_cb', 1)
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
            w3_button('id-id-hfdl-test w3-padding-smaller w3-aqua', 'Test', 'hfdl_test_cb', 1),
            w3_div('id-hfdl-bar-container w3-progress-container w3-round-large w3-white w3-hide|width:70px; height:16px',
               w3_div('id-hfdl-bar w3-progressbar w3-round-large w3-light-green|width:0%', '&nbsp;')
            ),
            
            w3_input('id-hfdl-log-mins/w3-label-not-bold/w3-ext-retain-input-focus|padding:0;width:auto|size=4',
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
	
	hfdl_map_init();

	w3_do_when_rendered('id-hfdl-menus', function() {
      ext_send('SET reset');
	   hfdl.double_fault = false;
	   if (0 && dbgUs) {
         kiwi_ajax(hfdl.url +'.xxx', 'hfdl_get_systable_done_cb', 0, -500);
	   } else {
         kiwi_ajax(hfdl.url, 'hfdl_get_systable_done_cb', 0, 10000);
      }
   });
}


////////////////////////////////
// map
////////////////////////////////

function hfdl_map_init()
{
   var map_tiles;
   var maxZoom = 19;
	
   // OSM raster tiles
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

   var map = map_tiles('hybrid');
   var m = L.map('id-hfdl-map',
      {
         maxZoom: maxZoom,
         minZoom: 1,
         doubleClickZoom: false,    // don't interfere with double-click of host/ref markers
         zoomControl: false
      }
   ).setView(hfdl.init_latlon, hfdl.init_zoom);
   
   // NB: hack! jog map slightly to prevent grey, unfilled tiles after basemap change
   m.on('baselayerchange', function(e) {
      var map = e.target;
      var center = map.getCenter();
      hfdl_pan_zoom(map, [center.lat + 0.1, center.lng], -1);
      hfdl_pan_zoom(map, [center.lat, center.lng], -1);
   });

   m.on('click', hfdl_click_info_cb);
   m.on('moveend', function(e) { hfdl_pan_end(e); });
   m.on('zoomend', function(e) { hfdl_zoom_end(e); });
   
   L.control.zoom_Kiwi( { position: 'topright', zoomNomText: '2', zoomNomLatLon: hfdl.init_latlon } ).addTo(m);
   
   map.addTo(m);
   hfdl.cur_map = m;

   var scale = L.control.scale()
   scale.addTo(m);
   scale.setPosition('bottomleft');

   var terminator = new Terminator();
   terminator.setStyle({ fillOpacity: 0.35 });
   terminator.addTo(m);
   setInterval(function() {
      var t2 = new Terminator();
      terminator.setLatLngs(t2.getLatLngs());
      terminator.redraw();
   }, 60000);
   terminator._path.style.cursor = 'grab';
   hfdl.day_night = terminator;
   if (dbgUs) terminator.getPane()._jks = 'Terminator';

   hfdl.graticule = L.latlngGraticule();
   hfdl.graticule.addTo(m);
   hfdl.map_layers.push(hfdl.graticule);

   m.on('zoom', hfdl_info_cb);
   m.on('move', hfdl_info_cb);

   setInterval(function() {
      hfdl_flights_age();
   }, 60000);
}

function hfdl_flights_age()
{
   var old = Date.now() - hfdl.too_old_min*60*1000;
   w3_obj_enum(hfdl.flights, function(key, i, o) {
      if (o.upd < old) {
         console.log('FL-OLD '+ o.flight +' > '+ hfdl.too_old_min +'m');
         o.el.style.background = 'grey';
      }
   });
}

function hfdl_flight_update(flight, lat, lon)
{
   
   if (!hfdl.flights[flight]) {
      console.log('FL-NEW '+ flight +' '+ lat.toFixed(4) +' '+ lon.toFixed(4));
      
      var icon =
         L.divIcon({
            className: '',
            iconAnchor: [12, 12],
            tooltipAnchor: [0, 0],
            html: ''
         });
      var marker = L.marker([lat, lon], { 'icon':icon, 'opacity':1.0 });
      var o = { flight: flight, mkr: marker, upd: Date.now(), pos: [] };
      o.pos.push([lat, lon]);
      hfdl.flights[flight] = o;

      marker.bindTooltip(flight,
         {
            className:  'id-hfdl-flight id-hfdl-flight-'+ flight + (hfdl.flights_visible? '' : ' w3-hide'),
            permanent:  true, 
            direction:  'right'
         }
      );

      marker.on('add', function(ev) {
      
         // sometimes multiple instances exist, so iterate to catch them all
         w3_iterate_classname('id-hfdl-flight-'+ flight,
            function(el) {
               if (el) {
                  //console.log(el);
                  w3_color(el, 'white', 'blue');
                  o.el = el;
               }
            }
         );
      });

      marker.addTo(hfdl.cur_map);
   } else {
      var o = hfdl.flights[flight];
      var marker = o.mkr;
      marker.setLatLng([lat, lon]);
      var n = o.pos.push([lat, lon]);
      o.el.style.background = 'blue';     // might have re-appeared after previously going grey

      var now = Date.now();
      var dt = Math.floor(((now - o.upd) / 60000) % 60);
      o.upd = now;
      console.log('FL-UPD '+ flight +' '+ lat.toFixed(4) +' '+ lon.toFixed(4) +' #'+ n +' '+ dt +'m');
   }
}

function hfdl_place_gs_marker(gs_n, map)
{
   var i;
   var r = hfdl.refs[gs_n];
   r.idx = gs_n;
   r.id = r.name.split(', ')[1];
   r.title = r.name;
   
   hfdl.bf_gs[r.id] = gs_n;
   
   var icon =
      L.divIcon({
         className: '',
         iconAnchor: [12, 12],
         tooltipAnchor: [0, 0],
         html: ''
      });
   var marker = L.marker([r.lat, r.lon], { 'icon':icon, 'opacity':1.0 });

   // when not using MarkerCluster add marker to map here
   if (map) {
      marker.kiwi_mkr_2_ref_or_host = r;     // needed before call to hfdl_style_marker()
      //console.log(r);
      var left = (r.id == 'New Zealand');
      hfdl_style_marker(marker, r.idx, r.id, 'gs', map, left);
   
      // band icons
      var sign = left? 1:-1;
      for (i = 0; i < hfdl.bf.length; i++) {
         var xo = sign*19*i;
         var icon =
            L.divIcon({
               className: 'id-hfdl-AFT id-hfdl-AFT-'+ gs_n +'-'+ i +' w3-hide',
               iconAnchor: [sign* (left? 26:6) + xo, left? 33 : -13]
               //, html: '&nbsp;'
            });
         var AFT_mkr = L.marker([r.lat, r.lon], { 'icon':icon, 'opacity':1.0 });
         //console.log(AFT_mkr);
         //console.log(AFT_mkr);
         AFT_mkr.addTo(map);

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

         //jks test
         el.innerHTML = ((hfdl.bf[i].toString().length < 2)? '&nbsp;' : '') + hfdl.bf[i];
         el.style.background = hfdl.bf_color[i];

      }

   }
   
   r.type_host = false;
   marker.kiwi_mkr_2_ref_or_host = r;
   return marker;
}

function hfdl_style_marker(marker, idx, name, type, map, left)
{
   marker.bindTooltip(name,
      {
         className:  'id-hfdl-'+ type +' id-hfdl-'+ type +'-'+ idx,
         permanent:  true, 
         direction:  left? 'left' : 'right'
      }
   );
   
   // Can only access element to add title via an indexed id.
   // But element only exists as marker emerges from cluster.
   // Fortunately we can use 'add' event to trigger insertion of title.
   //console.log('style');
   //console.log(marker);
   marker.on('add', function(ev) {
      var rh = ev.sourceTarget.kiwi_mkr_2_ref_or_host;
      //console.log('.on '+ type +' '+ rh.idx);
      //console.log(ev);
      //console.log('ADD '+ (rh.selected? 1:0) +' #'+ rh.idx +' '+ rh.id +' host='+ rh.type_host);
      
      // sometimes multiple instances exist, so iterate to catch them all
      w3_iterate_classname('id-hfdl-'+ type +'-'+ rh.idx,
         function(el) {
            if (el) {
               //console.log(el);
               if (rh.type_host) {
                  if (rh.selected) w3_color(el, 'black', 'yellow'); else w3_color(el, 'white', 'blue');
               }
               el.title = rh.title;
               el.kiwi_mkr_2_ref_or_host = rh;
            }
         }
      );
   });
   
   // only add to map in cases where L.markerClusterGroup() is not used
   if (map) {
      //console.log(marker);
      marker.addTo(map);
      hfdl.map_layers.push(marker);
      //console.log('marker '+ name +' x,y='+ map.latLngToLayerPoint(marker.getLatLng()));
   }
}

function hfdl_freq_2_band(f)
{
   var b = Math.floor((+f)/1000);
   if (b == 2) b = 3;
   return b;
}

function hfdl_update_AFT(gs, freqs)
{
   var i;
   //if (dbgUs) console.log('AFT: '+ gs);
   var left = (gs == 'New Zealand');
   if (!left) freqs.sort(function(a,b) { return parseFloat(a) - parseFloat(b); });
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

function hfdl_day_night_visible_cb(path, checked, first)
{
   if (first) return;
   if (!hfdl.day_night) return;
   var checked = w3_checkbox_get(path);
   if (checked) {
      hfdl.day_night.addTo(hfdl.cur_map);
      hfdl.map_layers.push(hfdl.day_night);
      hfdl.day_night._path.style.cursor = 'grab';
   } else {
      hfdl.day_night.remove();
   }
}

function hfdl_graticule_visible_cb(path, checked, first)
{
   //console.log('hfdl_graticule_visible_cb checked='+ checked +' first='+ first);
   if (first) return;
   if (!hfdl.graticule) return;
   checked = w3_checkbox_get(path);
   if (checked) {
      hfdl.graticule.addTo(hfdl.cur_map);
      hfdl.map_layers.push(hfdl.graticule);
   } else {
      hfdl.graticule.remove();
   }
}

function hfdl_map_markers_visible(el, id, vis)
{
	w3_iterate_children(el,
	   function(el, i) {
	      if (el.className.includes(id)) {
	         w3_hide2(el, !vis);
	      }
	   }
	);
}

function hfdl_show_kiwi_cb(path, idx, first)
{
   //console.log('hfdl_show_kiwi_cb idx='+ idx +' first='+ first);
   idx = +idx;
   var show = !idx;
   w3_color(path, show? 'lime':'white');
   
   //console.log('hfdl_show_kiwi_cb gs_visible='+ hfdl.gs_visible +' show='+ show);
   if (hfdl.flights_visible)
	   hfdl_map_markers_visible('leaflet-tooltip-pane', 'id-hfdl-flight', !show);
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
	hfdl_map_markers_visible('leaflet-tooltip-pane', 'id-hfdl-flight', checked);
}

function hfdl_gs_visible(vis)
{
	hfdl_map_markers_visible('leaflet-marker-pane', 'id-hfdl-AFT-active', vis);
	hfdl_map_markers_visible('leaflet-tooltip-pane', 'id-hfdl-gs', vis);
}

function hfdl_gs_visible_cb(path, checked, first)
{
   //console.log('hfdl_gs_visible_cb checked='+ checked +' first='+ first);
   if (first) return;
   hfdl.gs_visible = checked;
   w3_checkbox_set(path, checked);     // for benefit of direct callers
   hfdl_gs_visible(checked);
}

function hfdl_click_info_cb(ev)
{
   //console.log('hfdl_click_info_cb');
}

function hfdl_zoom_end(e)
{
}

function hfdl_pan_end(e)
{
}

function hfdl_info_cb()
{
   //var m = hfdl.cur_map;
   //var c = m.getCenter();
   //w3_innerHTML('id-hfdl-info', 'map center: '+ c.lat.toFixed(2) +', '+ c.lng.toFixed(2) +' z'+ m.getZoom());
}

function hfdl_pan_zoom(map, latlon, zoom)
{
   //console.log('hfdl_pan_zoom z='+ zoom);
   //console.log(map);
   //console.log(latlon);
   //console.log(zoom);
   if (latlon == null) latlon = map.getCenter();
   if (zoom == -1) zoom = map.getZoom();
   map.setView(latlon, zoom, { duration: 0, animate: false });
}

function hfdl_quick_zoom_cb(path, idx, first)
{
   if (first) return;
   var q = hfdl.quick_zoom[+idx];

   // NB: hack! jog map slightly to prevent grey, unfilled tiles (doesn't always work?)
   hfdl_pan_zoom(hfdl.cur_map, [q.lat + 0.1, q.lon], q.z);
   hfdl_pan_zoom(hfdl.cur_map, [q.lat, q.lon], -1);
}

function hfdl_ref_marker_offset(doOffset)
{
   if (!hfdl.known_location_idx) return;
   var r = hfdl.refs[hfdl.known_location_idx];

   //if (doOffset) {
   if (false) {   // don't like the way this makes the selected ref marker inaccurate until zoom gets close-in
      // offset selected reference so it doesn't cover solo (unclustered) nearby reference
      var m = hfdl.cur_map;
      var pt = m.latLngToLayerPoint(L.latLng(r.lat, r.lon));
      pt.x += 20;    // offset in x & y to better avoid overlapping
      pt.y -= 20;
      r.mkr.setLatLng(m.layerPointToLatLng(pt));
   } else {
      // reset the offset
      r.mkr.setLatLng([r.lat, r.lon]);
   }
}

function hfdl_marker_click(mkr)
{
   //console.log('hfdl_marker_click');

   var r = mkr.kiwi_mkr_2_ref_or_host;
   var t = r.title.replace('\n', ' ');
   var loc = r.lat.toFixed(2) +' '+ r.lon.toFixed(2) +' '+ r.id +' '+ t + (r.f? (' '+ r.f +' kHz'):'');
   w3_set_value('id-hfdl-known-location', loc);
   if (hfdl.known_location_idx) {
      var rp = hfdl.refs[hfdl.known_location_idx];
      rp.selected = false;
      console.log('ref_click: deselect ref '+ rp.id);
      hfdl_ref_marker_offset(false);
   }
   hfdl.known_location_idx = r.idx;
   r.selected = true;
   console.log('ref_click: select ref '+ r.id);
   hfdl_rebuild_stations();
   if (r.f)
      ext_tune(r.f, 'iq', ext_zoom.ABS, r.z, -r.p/2, r.p/2);
   hfdl_update_link();
   hfdl_ref_marker_offset(true);
}

function hfdl_map_stations()
{
   hfdl.refs = kiwi_deep_copy(hfdl.stations.stations);    // FIXME refs => stations
   var markers = [];
   hfdl.refs.forEach(function(r, i) {
      //r.id_lcase = r.id.toLowerCase();
      var mkr = hfdl_place_gs_marker(i, hfdl.cur_map);
      markers.push(mkr);
      r.mkr = mkr;
   });
   hfdl.gs_markers = markers;
   ext_send('SET send_lowres_latlon');

   hfdl_rebuild_stations();
}

function hfdl_rebuild_stations(opts)
{
//jks
if (1) return;
   if (!hfdl.refs) return;
   
   // remove previous
   if (hfdl.cur_ref_markers) {
      hfdl.gs_markers.forEach(function(m) {
         m.unbindTooltip(); m.remove(); m.off();
      });
      if (hfdl.refs_clusterer) hfdl.refs_clusterer.clearLayers();
   }

   // build new list
//jks
/*
   var ids = [];
   hfdl.ref_ids.forEach(function(id) {
      if (w3_checkbox_get('hfdl.refs_'+ id))
         ids.push(id);
   });
*/
   //console.log('hfdl_rebuild_stations REFS <'+ ids +'> show_refs='+ hfdl.show_refs +' opts=...');
   //console.log(opts);

   // make ref clusters contain only un-selected ref markers
   hfdl.cur_ref_markers = [];
   var show_refs = (opts == undefined)? hfdl.show_refs : opts.show;
   if (show_refs) {
      for (var i = hfdl.FIRST_REF; i < hfdl.refs.length-1; i++) {
         var r = hfdl.refs[i];
         var done = false;
         //console.log(r.r +' '+ r.id);
      if (1) {
         hfdl_style_marker(r.mkr, r.idx, r.id, 'ref', hfdl.cur_map);
      } else {
         ids.forEach(function(id) {
            if (r.r.includes(id) && !done) {
               //jks
               //if (r.selected) {
               if (1 || r.selected) {
                  //console.log('selected marker: '+ r.id);
                  hfdl_style_marker(r.mkr, r.idx, r.id, 'ref', hfdl.cur_map);
               } else {
                  hfdl_style_marker(r.mkr, r.idx, r.id, 'ref');
                  hfdl.cur_ref_markers.push(r.mkr);
               }
               done = true;
            }
         });
      }
      }
   }
   //console.log('hfdl_rebuild_stations cur_ref_markers='+ hfdl.cur_ref_markers.length);
//jks
if (1) return;

   var mc = L.markerClusterGroup({
      maxClusterRadius: hfdl.prev_ui? 30:80,
      spiderLegPolylineOptions: { weight: 1.5, color: 'white', opacity: 1.0 },
      iconCreateFunction: function(mc) {
         return new L.DivIcon({
            html: '<div><span>' + mc.getChildCount() + '</span></div>',
            className: 'marker-cluster id-hfdl-marker-cluster-refs',
            iconSize: new L.Point(40, 40)
         });
      }
   });
   mc.addLayers(hfdl.cur_ref_markers);
   if (hfdl.prev_ui) {
      mc.on('clustermouseover', function(a) { hfdl_create_cluster_list(a.layer); });
   } else {
      mc.on('clustermouseover', function (a) {
         //console.log('REFS clustermouseover '+ (hfdl.ii++) +' spiderfied='+ hfdl.spiderfied);
         if (hfdl.spiderfied) {
            if (hfdl.spiderfied == a.layer) {
               //console.log('REFS clustermouseover '+ (hfdl.ii++) +' SAME');
               return;
            }
            hfdl.spiderfied.unspiderfy();
            hfdl.spiderfy_deferred = a.layer;
         } else {
            a.layer.spiderfy();
            hfdl.spiderfied = a.layer;
         }
      });
      mc.on('unspiderfied', function(a) {
         //console.log('REFS unspiderfied '+ (hfdl.ii++) +' deferred='+ hfdl.spiderfy_deferred);
         if (hfdl.spiderfy_deferred) {
            hfdl.spiderfy_deferred.spiderfy();
            hfdl.spiderfied = hfdl.spiderfy_deferred;
            hfdl.spiderfy_deferred = false;
         } else {
            hfdl.spiderfied = false;
         }
      });
      //mc.on('clustermouseout', function(a) { console.log('REFS mouse out '+ (hfdl.ii++)); });
   }
   hfdl.cur_map.addLayer(mc);
   hfdl.refs_clusterer = mc;
}

////////////////////////////////
// end map
////////////////////////////////


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
         if (dbgUs) console.log('menu_s '+ dq(menu_s) +' m_str '+ dq(m_str) +' '+ (menu_s.includes(m_str)? 'T':'F'));
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
   var url = hfdl.url;
   var fault = false;
   
   if (!stations) {
      console.log('hfdl_get_systable_done_cb: stations='+ stations);
      fault = true;
   } else
   
   if (stations.AJAX_error && stations.AJAX_error == 'timeout') {
      console.log('hfdl_get_systable_done_cb: TIMEOUT');
      hfdl.using_default = true;
      fault = true;
   } else
   if (stations.AJAX_error && stations.AJAX_error == 'status') {
      console.log('hfdl_get_systable_done_cb: status='+ stations.status);
      hfdl.using_default = true;
      fault = true;
   } else
   if (!isObject(stations)) {
      console.log('hfdl_get_systable_done_cb: not array');
      fault = true;
   }
   
   if (fault) {
      if (hfdl.double_fault) {
         console.log('hfdl_get_systable_done_cb: default station list fetch FAILED');
         console.log(stations);
         return;
      }
      console.log(stations);
      
      // load the default station list from a file embedded with the extension (should always succeed)
      var url = kiwi_url_origin() + '/extensions/HFDL/systable.backup.cjson';
      console.log('hfdl_get_systable_done_cb: using default station list '+ url);
      hfdl.using_default = true;
      hfdl.double_fault = true;
      kiwi_ajax(url, 'hfdl_get_systable_done_cb', 0, /* timeout */ 10000);
      return;
   }
   
   console.log('hfdl_get_systable_done_cb: from '+ url);
   if (isDefined(stations.AJAX_error)) {
      console.log(stations);
      return;
   }
   hfdl.stations = stations;
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
            extint_help_click();
         } else
         if (w3_ext_param('map', a).match) {
            hfdl.show = hfdl.MAP;
         } else
         if (w3_ext_param('split', a).match) {
            hfdl.show = hfdl.SPLIT;
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
   if (dbgUs) console.log(hfdl.stations);
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
         o2.frequencies.sort(function(a,b) { return parseInt(a) - parseInt(b); });
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
      band[i].sort(function(a,b) { return parseInt(b) - parseInt(a); });
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
      if (!isArg(except) || i != except)
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
      var i = id[0];
      var j = id[1];
      if (dbgUs) console.log('hfdl_pre_select_cb i='+ i +' j='+ j);
      var o1 = w3_obj_seq_el(hfdl.menus[menu_n], i);
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
	w3_hide2('id-hfdl-msgs', idx == hfdl.MAP);
	w3_hide2('id-hfdl-map', idx == hfdl.MSGS);
	var splitH = 150;
	w3_el('id-hfdl-msgs').style.top = px((idx == hfdl.SPLIT)? (hfdl.dataH - splitH) : 0);
	w3_el('id-hfdl-msgs').style.height = px((idx == hfdl.SPLIT)? splitH : hfdl.dataH);
	if (idx == hfdl.SPLIT)
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
   if (hfdl.log_mins) hfdl.log_interval = setInterval(function() { hfdl_log_cb(); }, hfdl.log_mins * 60000);
}

function hfdl_log_cb()
{
   var ts = kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') +'_'+ w3_el('id-freq-input').value +'_'+ cur_mode;
   var txt = new Blob([hfdl.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = 'HFDL.'+ ts +'.log.txt';
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

function HFDL_blur()
{
   // anything that needs to be done when extension blurred (closed)
	ext_set_mode(hfdl.saved_mode);
   ext_agc_delay(hfdl.save_agc_delay);
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
               '<i>(menu match)</i> &nbsp; map|split &nbsp; display:[012] &nbsp; ' +
               'log_time:<i>mins</i> &nbsp; gs:0 &nbsp; test' +
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
