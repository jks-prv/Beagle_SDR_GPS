// Copyright (c) 2017 John Seamons, ZL/KF6VO

var tdoa = {
   ext_name: 'TDoA',    // NB: must match tdoa.cpp:tdoa_ext.name
   first_time: true,
   w_data: 1024,
   h_data: 465,
   
   // set from json on callback
   hosts: [],
   // has:
   // .i .id .h .p .lat .lon .fm .u .um .mac .a .n
   // we add:
   // .idx fixme needed?
   // .id_lcase
   // .hp         = h.h : h.p
   // .call       .id with '-' => '/'
   // .mkr        marker on gmap_kiwi
   
   tfields: 6,
   ngood: 0,

   // indexed by field number
   field: [],
   // .host
   // .inuse
   // .good
   // .mkr        only when field set by clicking on host marker
   
   list: [],      // indexed by list size (ngood)
   // .h
   // .id
   // .call
   // .p
   // .field_idx

   results: [],   // indexed by menu entry

   overlay: [],   // indexed by map number
   
   cur_map: null,
   gmap_kiwi: null,
   gmap_result: null,
   mapZoom_nom: 17,
   heatmap_visible: true,
   
   FIRST_REF: 2,
   // to tdoa.refs we add: .idx .id_lcase .mkr
   known_location: '',
   
   quick_zoom: [
      { n:'Europe', lat:52, lon:10, z:4 },
      { n:'N.Amer', lat:40, lon:-90, z:3 },
      { n:'S.Amer', lat:-20, lon:-50, z:3 },
      { n:'Asia', lat:40, lon:120, z:3 },
      { n:'Japan', lat:40, lon:140, z:4 },
      { n:'Aus/NZ', lat:-30, lon:150, z:3 },
   ],
   
   sample_status: [
      { s:'sampling complete', retry:0 },
      { s:'connection failed', retry:0 },
      { s:'all channels in use', retry:1 },
      { s:'no recent GPS timestamps', retry:0 },
      // 4 max (2 bits * 6 rx = 12 bits total currently)
      { s:'', retry:0 }
   ],
   SAMPLE_STATUS_BLANK: 4,
   
   submit_status: [
      { n:0,   f:0,  m:'TDoA complete' },
      { n:1,   f:1,  m:'Octave' },
      { n:2,   f:0,  m:'some Kiwis had a sampling error' },
      { n:3,   f:0,  m:'some Kiwis had no GPS timestamps at all' },
      { n:4,   f:0,  m:'some Kiwis had no recent GPS timestamps' },
      { n:5,   f:1,  m:'pair map' },
      { n:6,   f:1,  m:'combined map' },
      { n:7,   f:1,  m:'dt plot' },
      { n:8,   f:1,  m:'pair contour' },
      { n:9,   f:1,  m:'combined contour' },
      { n:10,  f:1,  m:'map plot' },
      { n:11,  f:0,  m:'reposition map not to span 180 deg meridian' },
   ],
   
   //result_menu
   KIWI_MAP: 0, TDOA_MAP: 1, COMBINED_MAP: 2, SINGLE_MAPS: 3,
   
   state: 0, READY_SAMPLE: 0, READY_COMPUTE: 1, RUNNING: 2, RETRY: 3, RESULT: 4, ERROR: 5,
   
   key: '',
   select: undefined,
};

function TDoA_main()
{
	ext_switch_to_client(tdoa.ext_name, tdoa.first_time, tdoa_recv);		// tell server to use us (again)
	if (!tdoa.first_time)
		tdoa_controls_setup();
	tdoa.first_time = false;
}

function tdoa_load_cb() {
   //console.log('kiwi_load_js CALLBACK');
   tdoa_controls_setup();
}

function tdoa_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

		if (cmd == 0) {
			// do something ...
		} else {
			//console.log('tdoa_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		}
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('tdoa_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('tdoa_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
			   tdoa.auth = param[1];
			   kiwi_load_js(
               [
                  'http://maps.googleapis.com/maps/api/js?key=AIzaSyCtWThmj37c62a1qYzYUjlA0XUVC_lG8B8',
                  //'http://kiwisdr.com/php/tdoa.php?slow=1',
                  'pkgs/js/daynightoverlay.js',
                  'pkgs/js/markerwithlabel.js',
                  'pkgs/js/v3_ll_grat.js',
                  'pkgs/js/markerclusterer.js',
               ],
               'tdoa_load_cb'
            );
				break;

			default:
				console.log('tdoa_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function tdoa_controls_setup()
{
   var i;
   
   for (i = 0; i < tdoa.tfields; i++) tdoa.field[i] = {};
   
   var wh = '; width:'+ px(tdoa.w_data) +'; height:'+ px(tdoa.h_data);
   var cbox = 'w3-label-inline w3-label-right w3-label-not-bold';
   var cbox2 = 'w3-margin-left//'+ cbox;
   
	var data_html =
      time_display_html('tdoa') +
      
      w3_div('id-tdoa-data w3-display-container|left:0px'+ wh,
         w3_div('id-tdoa-gmap-kiwi|left:0px'+ wh, ''),
         w3_div('id-tdoa-gmap-result w3-hide|left:0px'+ wh, ''),
         w3_div('id-tdoa-png w3-display-topleft w3-scroll-y w3-hide|left:0px'+ wh, '')
      ) +
      
      w3_div('id-tdoa-options w3-display-right w3-text-white w3-light-greyx|top:230px; right:0px; width:200px; height:200px',
         w3_text('w3-text-aqua w3-bold', 'TDoA options'),
         w3_checkbox('id-tdoa-results-newmaps '+ cbox, 'New map format', 'tdoa.new_maps', true, 'w3_checkbox_set'),
         w3_checkbox(cbox, 'Show heatmap', 'tdoa.heatmap_visible', true, 'tdoa_heatmap_visible_cb'),
         w3_checkbox(cbox, 'Show day/night', 'tdoa.day_night_visible', true, 'tdoa_day_night_visable_cb'),
         w3_text('', ''),

         w3_checkbox(cbox, 'Reference locations', 'tdoa.refs_visible', true, 'tdoa_refs_visable_cb'),
         w3_checkbox(cbox2, 'VLF/LF', 'tdoa.refs_v', true, 'tdoa_refs_cb'),
         w3_checkbox(cbox2, 'Milcom', 'tdoa.refs_m', true, 'tdoa_refs_cb'),
         w3_checkbox(cbox2, 'Radar', 'tdoa.refs_r', true, 'tdoa_refs_cb'),
         w3_checkbox(cbox2, 'Aero', 'tdoa.refs_a', true, 'tdoa_refs_cb'),
         w3_checkbox(cbox2, 'Marine', 'tdoa.refs_w', true, 'tdoa_refs_cb'),
         w3_checkbox(cbox2, 'Broadcast', 'tdoa.refs_b', true, 'tdoa_refs_cb'),
         w3_checkbox(cbox2, 'Utility', 'tdoa.refs_u', true, 'tdoa_refs_cb'),
         w3_checkbox(cbox2, 'Time/Freq', 'tdoa.refs_t', true, 'tdoa_refs_cb')
      );
   tdoa.ref_ids = ['v','m','r','a','w','b','u','t'];
   
	var control_html =
	   w3_divs('w3-tspace-8',
		   w3_inline('w3-halign-space-between|width:86%/',
		      w3_div('id-tdoa-controls w3-medium w3-text-aqua', 
		         '<b><a href="https://www.rtl-sdr.com/kiwisdr-tdoa-direction-finding-now-freely-available-for-public-use" target="_blank">TDoA</a> direction finding service</b>'
		      ),
		      w3_div('',
               w3_div('id-tdoa-info w3-small|color: hsl(0, 0%, 70%)'),
               w3_inline('',
                  w3_div('id-tdoa-result w3-small|color: hsl(0, 0%, 70%)', 'most likely:'),
                  w3_div('id-tdoa-gmap-link w3-margin-L-4')
               )
            ),
		      w3_div('id-tdoa-bookmark')
		   ),
		   w3_div('id-tdoa-hosts-container'),
		   w3_inline('',
            w3_button('id-tdoa-submit-button w3-padding-smaller w3-css-yellow', 'Submit', 'tdoa_submit_button_cb'),
		      w3_div('id-tdoa-submit-icon-c w3-margin-left'),
            w3_div('id-tdoa-results-select w3-margin-left w3-hide'),
            w3_div('id-tdoa-results-options w3-margin-left w3-hide'),
            w3_div('id-tdoa-submit-status w3-margin-left'),
            w3_button('id-tdoa-rerun-button w3-margin-left w3-padding-smaller w3-css-yellow w3-hide||title="rerun TDoA using same samples"',
               'Rerun', 'tdoa_rerun_button_cb'
            )
         ),
		   w3_inline_percent('',
            w3_select('w3-text-red', '', 'quick zoom', 'tdoa.quick_zoom', -1, tdoa.quick_zoom, 'tdoa_quick_zoom_cb'), 20,
            w3_input('id-tdoa-known-location w3-padding-tiny', '', 'tdoa.known_location', '', 'tdoa_edit_known_location_cb',
               'Map ref location: type lat, lon and name or click white map markers'
            )
         )
      );

	ext_panel_show(control_html, data_html, null);
	time_display_setup('tdoa');

	ext_set_controls_width_height(600, 270);
   ext_set_data_height(tdoa.h_data);

   var s = '';
	for (i = 0; i < tdoa.tfields; i++) {
	   s +=
	      w3_inline(i? 'w3-margin-T-3':'',
            w3_input('w3-padding-tiny||size=10', '', 'tdoa.id_field-'+ i, ''),
            w3_input('w3-margin-L-3 w3-padding-tiny||size=28', '', 'tdoa.url_field-'+ i, '', 'tdoa_edit_url_field_cb'),
            w3_icon('w3-margin-L-8 id-tdoa-clear-icon-'+ i, 'fa-cut', 20, 'red', 'tdoa_clear_cb', i),
		      w3_div('w3-margin-L-8 id-tdoa-sample-icon-c'+ i),
		      w3_div('w3-margin-L-8 id-tdoa-download-icon-c'+ i),
		      w3_div('w3-margin-L-8 id-tdoa-sample-status-'+ i)
         );
   }
   w3_innerHTML('id-tdoa-hosts-container', s);
   
   tdoa.gmap_kiwi = new google.maps.Map(w3_el('id-tdoa-gmap-kiwi'),
      {
         zoom: 2,
         center: new google.maps.LatLng(20, 0),
         navigationControl: false,
         mapTypeControl: true,
         streetViewControl: false,
         draggable: true,
         scaleControl: true,
         gestureHandling: 'greedy',
         mapTypeId: google.maps.MapTypeId.SATELLITE
      });
   
   tdoa.cur_map = tdoa.gmap_kiwi;
   tdoa.day_night = new DayNightOverlay( { map:tdoa.gmap_kiwi, fillColor:'rgba(0,0,0,0.35)' } );
   var grid = new Graticule(tdoa.gmap_kiwi, false);

   tdoa.gmap_kiwi.addListener('zoom_changed', tdoa_info_cb);
   tdoa.gmap_kiwi.addListener('center_changed', tdoa_info_cb);
   tdoa.gmap_kiwi.addListener('maptypeid_changed', tdoa_info_cb);
   
   // Request json list of reference markers.
   // Can't use file w/ .json extension since our file contains comments and
   // Firefox improperly caches json files with errors!
   kiwi_ajax('http://kiwisdr.com/tdoa/refs.cjson', 'tdoa_get_refs_cb');

   //ext_set_mode('iq');   // FIXME: currently undoes pb set by &pbw=nnn in URL
   
   tdoa_ui_reset(true);
	TDoA_environment_changed( {resize:1} );
   tdoa.state = tdoa.WAIT_HOSTS;
}

function tdoa_info_cb()
{
   var m = tdoa.cur_map;
   var c = m.getCenter();
   w3_innerHTML('id-tdoa-info', 'map center: '+ c.lat().toFixed(2) +', '+ c.lng().toFixed(2) +' z'+ m.getZoom());
   tdoa_update_link();
}

function tdoa_update_link()
{
   var url = kiwi_url_origin() +'/?f='+ ext_get_freq_kHz() +'iqz'+ ext_get_zoom();
   var pb = ext_get_passband();
   url += '&pbw='+ (pb.high * 2).toFixed(0) +'&ext=tdoa';

   var m = tdoa.cur_map;
   var c = m.getCenter();
   url += ',lat:'+ c.lat().toFixed(2) +',lon:'+ c.lng().toFixed(2) +',z:'+ m.getZoom();
   if (m.getMapTypeId() != 'satellite') url += ',map:1';

   tdoa.field.forEach(function(f, i) {
      if (f.good) {
         url += ','+ w3_get_value('tdoa.id_field-'+ i);
      }
   });

   var r = w3_get_value('id-tdoa-known-location');
   if (r && r != '') {
      r = r.replace(/\s+/g, ' ').split(/[ ,]/g);
      if (r.length >= 2) {
         url += ','+ r[2].replace(/[^0-9A-Za-z]/g, '');  // e.g. can't have spaces in names at the moment
      }
   }

	w3_innerHTML('id-tdoa-bookmark', w3_link('', url, w3_icon('w3-text-css-lime', 'fa-external-link-square', 16)));
}


////////////////////////////////
/// markers
////////////////////////////////

function tdoa_place_host_marker(h, map)
{
   h.hp = h.h +':'+ h.p;
   h.call = h.id.replace(/\-/g, '/');    // decode slashes
   
   var title = h.n +'\nUsers: '+ h.u +'/'+ h.um;
   if (h.tc) {
      if (h.tc <= 0) {
         console.log('TDoA OPT-OUT: '+ h.tc +' '+ h.id +' '+ h.h);
         return null;
      }
      title += '\nTDoA channels: '+ h.tc;
   }
   title += '\nAntenna: '+ h.a +'\n'+ h.fm +' GPS fixes/min';

   var latlon = new google.maps.LatLng(h.lat, h.lon);
   var marker = new MarkerWithLabel({
      position: latlon,
      draggable: false,
      raiseOnDrag: false,
      map: map,
      labelContent: h.call,
      labelAnchor: new google.maps.Point(h.call.length*3.6, 36),
      labelClass: (map == tdoa.gmap_kiwi)? 'cl-tdoa-gmap-host-label' : 'cl-tdoa-gmap-selected-label',
      labelStyle: {opacity: 1.0},
      title: title,
      icon: kiwi_mapPinSymbol('red', 'white')
   });
   marker.kiwi_mkr_2_hosts = h;

   google.maps.event.addListener(marker, "click", function() {
      if (tdoa.click_pending === true) return;
      tdoa.click_pending = true;

      tdoa.click_timeout = setTimeout(function(marker) {
         tdoa.click_pending = false;
         tdoa_host_marker_click(marker);
      }, 300, this);
   });

   google.maps.event.addListener(marker, "dblclick", function() {
      kiwi_clearTimeout(tdoa.click_timeout);    // keep single click event from hapenning
      tdoa.click_pending = false;
      
      var url = 'http://'+ this.kiwi_mkr_2_hosts.hp +'/?f='+ ext_get_freq_kHz() + ext_get_mode() +'z'+ ext_get_zoom();
      var pb = ext_get_passband();
      url += '&pbw='+ (pb.high * 2).toFixed(0);
      //console.log('double-click '+ url);
      var win = window.open(url, '_blank');
      if (win) win.focus();
   });
   
   return marker;
}

function tdoa_host_marker_click(mkr)
{
   //console.log('tdoa_host_marker_click:');
   var h = mkr.kiwi_mkr_2_hosts;
   //console.log(h);
   for (var j = 0; j < tdoa.tfields; j++) {
      if (w3_get_value('tdoa.id_field-'+ j) == h.call) return;  // already in list
   }
   
   for (var j = 0; j < tdoa.tfields; j++) {
      var f = tdoa.field[j];
      if (f.inuse) continue;     // put in first empty field
      
      //console.log('tdoa_host_marker_click: insert @'+ j);
      var url = h.hp;
      f.inuse = true;
      f.good = true;
      
      // will be cleared if tdoa_host_click_status_cb() fails
      f.mkr = mkr;
      f.host = h;

      w3_set_value('tdoa.id_field-'+ j, h.call);
      w3_set_value('tdoa.url_field-'+ j, url);
      tdoa_set_icon('sample', j, 'fa-refresh fa-spin', 20, 'lightGrey');
      tdoa_rerun_clear();

      // launch status check
      var s = (dbgUs && url == 'kiwisdr.jks.com:8073')? 'www:8073' : url;
      kiwi_ajax('http://'+ s +'/status', 'tdoa_host_click_status_cb', j, 5000);
      break;
   }
}

function tdoa_place_ref_marker(idx, map)
{
   var r = tdoa.refs[idx];
   r.idx = idx;
   var latlon = new google.maps.LatLng(r.lat, r.lon);
   var title;
   if (r.t != '')
      title = r.t + (r.f? (', '+ r.f +' kHz'):'');
   else
      title = '';
   var marker = new MarkerWithLabel({
      position: latlon,
      draggable: false,
      raiseOnDrag: false,
      map: map,
      labelContent: r.id,
      labelAnchor: new google.maps.Point(r.id.length*3.6, 36),
      labelClass: 'cl-tdoa-gmap-ref-label',
      labelStyle: {opacity: 1.0},
      title: title,
      icon: kiwi_mapPinSymbol('lime', 'black')
   });
   marker.kiwi_mkr_2_refs = r;

   // DO NOT REMOVE: fix for terrible Firefox runaway problem
   // no click events for result map markers
   if (map != tdoa.gmap_kiwi && map != null) return marker;

   google.maps.event.addListener(marker, "click", function() {
      if (tdoa.click_pending === true) return;
      tdoa.click_pending = true;

      tdoa.click_timeout = setTimeout(function(marker) {
         tdoa.click_pending = false;

         var map = marker.map;
         // single-click on kiwi map ref marker tunes the kiwi -- don't do this on result map
         if (map == tdoa.gmap_kiwi) tdoa_ref_marker_click(marker);
      }, 300, this);
   });

   // double-click ref marker on any map to zoom in/out
   google.maps.event.addListener(marker, "dblclick", function() {
      kiwi_clearTimeout(tdoa.click_timeout);    // keep single click event from hapenning
      tdoa.click_pending = false;
      
      var map = this.map;
      var r = this.kiwi_mkr_2_refs;
      if (map.getZoom() >= (r.mz || tdoa.mapZoom_nom)) {
         if (tdoa.last_center && tdoa.last_zoom) {
            map.panTo(tdoa.last_center);
            map.setZoom(tdoa.last_zoom);
         } else {
            map.panTo(new google.maps.LatLng(20, 0));
            map.setZoom(2);
         }
      } else {
         tdoa.last_center = map.getCenter();
         tdoa.last_zoom = map.getZoom();
         map.panTo(new google.maps.LatLng(r.lat, r.lon));
         map.setZoom(r.mz || tdoa.mapZoom_nom);
      }
   });
   
   return marker;
}

function tdoa_ref_marker_click(mkr)
{
   var r = mkr.kiwi_mkr_2_refs;
   if (r.idx == 0 || r.idx == 1) alert('foo!');    // fixme remove
   var t = r.t.replace('\n', ' ');
   var loc = r.lat.toFixed(2) +' '+ r.lon.toFixed(2) +' '+ r.id +' '+ t + (r.f? (' '+ r.f +' kHz'):'');
   w3_set_value('id-tdoa-known-location', loc);
   if (tdoa.known_location_idx) {
      var rp = tdoa.refs[tdoa.known_location_idx];
      rp.selected = false;
      console.log('ref_click: deselect ref '+ rp.id);
   }
   tdoa.known_location_idx = r.idx;
   r.selected = true;
   console.log('ref_click: select ref '+ r.id);
   tdoa_rebuild_refs();
   if (r.f)
      ext_tune(r.f, 'iq', ext_zoom.ABS, r.z, -r.p/2, r.p/2);
   tdoa_update_link();
}

function tdoa_change_marker_style(mkr, body_color, outline_color, label_class)
{
   mkr.setMap(null);
   mkr.labelClass = label_class;
   mkr.icon = kiwi_mapPinSymbol(body_color, outline_color);
   mkr.setMap(tdoa.gmap_kiwi);
}


////////////////////////////////
/// callbacks
////////////////////////////////

function tdoa_get_refs_cb(refs)
{
   var err;
   
   if (!refs) {
      console.log('tdoa_get_refs_cb refs='+ refs);
      return;
   }
   
   //console.log(refs);
   tdoa.refs = refs;
   
   var markers = [];
   for (i = tdoa.FIRST_REF; i < tdoa.refs.length-1; i++) {
      var r = tdoa.refs[i];
      r.id_lcase = r.id.toLowerCase();
      // NB: .push() instead of [i] since i doesn't start at zero
      var mkr = tdoa_place_ref_marker(i, null);
      markers.push(mkr);
      r.mkr = mkr;
   }
   tdoa.refs_markers = markers;
   tdoa_rebuild_refs();
   
   // request json list of GPS-active Kiwis
   kiwi_ajax('http://kiwisdr.com/tdoa/files/kiwi.gps.json', 'tdoa_get_hosts_cb');
}

function tdoa_get_hosts_cb(hosts)
{
   var err;
   
   if (!hosts) {
      console.log('tdoa_get_hosts_cb hosts='+ hosts);
      return;
   }
   
   //console.log(hosts);
   tdoa.hosts = hosts;
   var markers = [];
   for (var i = 0; i < hosts.length; i++) {
      var h = hosts[i];
      //console.log(h);
      h.idx = i;
      
      if (h.h == '') {
         console.log('TDoA WARNING: hosts.h empty?');
         console.log(h);
         continue;
      }
      
      for (var j = 0; j < i; j++) {
         var h0 = hosts[j];

         // dither multiple Kiwis at same location
         if (h0.lat == h.lat && h0.lon == h.lon)
            h.lon += 0.001;

         // prevent duplicate IDs
         if (h0.id == h.id) {
            console.log('TDoA duplicate ID: #'+ h.i +' '+ h.id +' #'+ h0.i +' '+ h0.id);
            if (h0.dup == undefined) h0.dup = 0;
            h0.dup++;
            h.id += '/'+ h0.dup;
         }
      }
      h.id_lcase = h.id.toLowerCase();

      // NB: .push() instead of [i] because error checking on hosts[] might leave gaps
      var mkr = tdoa_place_host_marker(h, tdoa.gmap_kiwi);
      if (mkr) markers.push(mkr);
      h.mkr = mkr;
   }

   // now that we have all Kiwi and ref markers we can process extension parameters
	var lat, lon, zoom, maptype, init_submit;
   var p = ext_param();
   if (p) {
      p = p.split(',');
      for (var i=0, len = p.length; i < len; i++) {
         var a = p[i];
         console.log('TDoA: param <'+ a +'>');
         if (a.includes(':')) {
            if (a.startsWith('lat:')) {
               lat = parseFloat(a.substring(4));
            }
            if (a.startsWith('lon:') || a.startsWith('lng:')) {
               lon = parseFloat(a.substring(4));
            }
            if (a.startsWith('z:')) {
               zoom = parseInt(a.substring(2));
            }
            if (a.startsWith('map:')) {
               maptype = parseInt(a.substring(4));
            }
            if (a.startsWith('submit:')) {
               init_submit = true;
            }
         } else {
            a = a.toLowerCase();

            // match on refs
            var match = false;
            tdoa.refs.forEach(function(r) {
               if (!match && r.id_lcase == a) {
                  console.log('TDoA refs match: '+ a);
                  r.selected = true;
                  tdoa_ref_marker_click(r.mkr);
                  match = true;
               }
            });

            // match on hosts
            match = false;
            tdoa.hosts.forEach(function(h) {
               if (!match && h.id_lcase == a) {
                  console.log('TDoA hosts match: '+ a);
                  h.selected = true;
                  tdoa_host_marker_click(h.mkr);
                  match = true;
               }
            });
         }
      }
   }
   
   tdoa_rebuild_hosts();
   tdoa_rebuild_refs();
   
   if (maptype == 1) tdoa.gmap_kiwi.setMapTypeId('roadmap');
   if (lat && lon) tdoa.gmap_kiwi.panTo(new google.maps.LatLng(lat, lon));
   if (zoom) tdoa.gmap_kiwi.setZoom(zoom);
   tdoa.state = tdoa.READY_SAMPLE;
   
   if (init_submit == true)
      tdoa.init_submit_timeout = setTimeout(tdoa_submit_button_cb2, 6000);     // give time for host status to be acquired
}

function TDoA_environment_changed(changed)
{
   if (changed.resize) {
      var el = w3_el('id-tdoa-data');
      if (!el) return;
      var left = Math.max(0, (window.innerWidth - tdoa.w_data - time_display_width()) / 2);
      //console.log('tdoa_resize wiw='+ window.innerWidth +' tdoa.w_data='+ tdoa.w_data +' time_display_width='+ time_display_width() +' left='+ left);
      el.style.left = px(left);
      return;
   }
   
   tdoa_update_link();
}


////////////////////////////////
// UI
////////////////////////////////

function tdoa_ui_reset(reset_map)
{
	tdoa_set_icon('submit', -1, 'fa-refresh', 20, 'lightGrey');
   tdoa_submit_state(tdoa.READY_SAMPLE, '');
   tdoa_info_cb();
   w3_innerHTML('id-tdoa-result', 'most likely:');
   w3_innerHTML('id-tdoa-gmap-link', '');

   w3_hide('id-tdoa-results-select');
   w3_hide('id-tdoa-results-options');
   
   if (reset_map) {
      w3_show('id-tdoa-gmap-kiwi');
      w3_hide('id-tdoa-gmap-result');
      w3_hide('id-tdoa-png');
   }
}

// field_idx = -1 if the icon isn't indexed
function tdoa_set_icon(name, field_idx, icon, size, color)
{
   field_idx = (field_idx >= 0)? field_idx : '';
   w3_innerHTML('id-tdoa-'+ name +'-icon-c'+ field_idx,
      (icon == '')? '' :
      w3_icon('id-tdoa-'+ name +'-icon'+ field_idx, icon, size, color)
   );
}

function tdoa_submit_state(state, msg)
{
   if (state == tdoa.ERROR) {
      tdoa_set_icon('submit', -1, 'fa-times-circle', 20, 'red');
   }
   w3_innerHTML('id-tdoa-submit-status', msg);
   w3_show_hide('id-tdoa-rerun-button', state == tdoa.RESULT);
   tdoa.state = state;
}

function tdoa_rerun_clear()
{
   w3_hide('id-tdoa-rerun-button');
   tdoa.rerun = 0;
}

function tdoa_edit_url_field_cb(path, val, first)
{
   // launch status check
   var field_idx = +path.match(/\d+/).shift();
   var f = tdoa.field[field_idx];
   //console.log('FIXME tdoa_edit_url_field_cb path='+ path +' val='+ val +' field_idx='+ field_idx);
   w3_innerHTML('id-tdoa-sample-status-'+ field_idx, '');

   // un-highlight if field was previously set from marker
   if (f.mkr) tdoa_change_marker_style(f.mkr, 'red', 'white', 'cl-tdoa-gmap-host-label');
   f.mkr = undefined;
   f.host = undefined;
   f.inuse = true;
   f.good = true;
   tdoa_set_icon('download', field_idx, '');

   if (val == '') {
      tdoa_set_icon('sample', field_idx, '');
      return;
   }

   tdoa_set_icon('sample', field_idx, 'fa-refresh fa-spin', 20, 'lightGrey');
   tdoa_rerun_clear();
   var s = (dbgUs && val == 'kiwisdr.jks.com:8073')? 'www:8073' : val;
      kiwi_ajax('http://'+ s +'/status', 'tdoa_host_click_status_cb', field_idx, 5000);
}

function tdoa_clear_cb(path, val, first)
{
   if (tdoa.state == tdoa.RUNNING) return;

   var field_idx = +val;
   var f = tdoa.field[field_idx];
   f.inuse = false;
   f.good = false;
   w3_set_value('tdoa.id_field-'+ field_idx, '');
   w3_set_value('tdoa.url_field-'+ field_idx, '');
   tdoa_set_icon('sample', field_idx, '');
   tdoa_set_icon('download', field_idx, '');
   w3_innerHTML('id-tdoa-sample-status-'+ field_idx, '');

   // un-highlight if field was previously set from marker
   if (f.mkr) {
      tdoa_change_marker_style(f.mkr, 'red', 'white', 'cl-tdoa-gmap-host-label');
      if (f.host) {
         //console.log('clear selected: '+ f.host.call);
         f.host.selected = false;
      }
   }
   f.mkr = undefined;
   f.host = undefined;
   tdoa_rebuild_hosts();
   tdoa_update_link();
}

function tdoa_edit_known_location_cb(path, val, first)
{
   // manual entry invalidates the marker selection
   if (tdoa.known_location_idx) {
      var rp = tdoa.refs[tdoa.known_location_idx];
      rp.selected = false;
      console.log('edit_known_location: deselect ref '+ rp.id);
   }
   tdoa.known_location_idx = undefined;
   tdoa_update_link();
}

function tdoa_host_click_status_cb(obj, field_idx)
{
   var err;
   
   var f = tdoa.field[field_idx];
   
   if (obj.response) {
      var arr = obj.response.split('\n');
      //console.log('tdoa_host_click_status_cb field_idx='+ field_idx +' arr.len='+ arr.length);
      //console.log(arr);
      var offline = auth = users = users_max = fixes_min = null;
      for (var i = 0; i < arr.length; i++) {
         var a = arr[i];
         if (a.startsWith('offline=')) offline = a.split('=')[1];
         if (a.startsWith('auth=')) auth = a.split('=')[1];
         if (a.startsWith('users=')) users = a.split('=')[1];
         if (a.startsWith('users_max=')) users_max = a.split('=')[1];
         if (a.startsWith('fixes_min=')) fixes_min = a.split('=')[1];
      }

      if (fixes_min == null) return;      // unexpected response

      if (offline == 'yes') {
         err = 'Kiwi is offline';
      } else
      if (auth == 'password') {
         err = 'Kiwi requires password';
      } else
      if (fixes_min == 0) {
         err = 'no recent GPS timestamps';
      } else {
         //console.log('u='+ users +' u_max='+ users_max);
         if (users_max == null) return;      // unexpected response
         if (users < users_max) {
            tdoa_set_icon('sample', field_idx, 'fa-refresh', 20, 'lime');
            var s = fixes_min? (fixes_min +' GPS fixes/min') : 'channel available';
            w3_innerHTML('id-tdoa-sample-status-'+ field_idx, s);
            if (f.mkr) tdoa_change_marker_style(f.mkr, 'red', 'white', 'cl-tdoa-gmap-selected-label');
            f.host.selected = true;
            tdoa_rebuild_hosts();
            tdoa_update_link();
            return;
         }
         err = 'all channels in use';
      }
   } else {
      console.log(obj);
      err = 'no response';
   }

   tdoa_set_icon('sample', field_idx, 'fa-times-circle', 20, 'red');
   w3_innerHTML('id-tdoa-sample-status-'+ field_idx, err);
   f.good = false;
   f.mkr = undefined;
   f.host = undefined;
}


////////////////////////////////
/// submit processing
////////////////////////////////

function tdoa_submit_button_cb()
{
   kiwi_clearTimeout(tdoa.init_submit_timeout);
   var state_save = tdoa.state;
   //console.log('tdoa_submit_button_cb tdoa.state='+ state_save);
   tdoa_ui_reset(false);   // NB: resets tdoa.state

   if (state_save == tdoa.RUNNING || state_save == tdoa.RETRY) {
      tdoa.response.key = 0;     // ignore subsequent responses
      //
      for (var i = 0; i < tdoa.tfields; i++) {
         var f = tdoa.field[i];
         if (!f.good) continue;
         tdoa_set_icon('sample', i, 'fa-refresh', 20, 'lightGrey');
         tdoa_set_icon('download', i, '');
         w3_innerHTML('id-tdoa-sample-status-'+ i, 'stopped');
      }
      w3_button_text('id-tdoa-submit-button', 'Submit', 'w3-css-yellow', 'w3-red');
      return;
   }
   
   setTimeout(tdoa_submit_button_cb2, 1000);
}

function tdoa_submit_button_cb2()
{
   var i, j;
   
   //console.log('tdoa_submit_button_cb pbc='+ freq_passband_center() +' f='+ ext_get_freq() +' cf='+ ext_get_carrier_freq());
   
   tdoa.ngood = 0;
   tdoa.field.forEach(function(f, i) { if (f.good) tdoa.ngood++; });

   
   if (tdoa.ngood < 2) {
      tdoa_submit_state(tdoa.ERROR, 'at least 2 available hosts needed');
      return;
   }
   
   if (tdoa.cur_map.getZoom() < 3) {
      tdoa_submit_state(tdoa.ERROR, 'must be zoomed in further');
      return;
   }

   // construct hosts list
   // extract from fields in case fields manually edited
   var h, p, id;
   var h_v = '&h=', p_v = '&p=', id_v = '&id=';
   var list_i = 0;
   tdoa.list = [];

   for (i = 0; i < tdoa.tfields; i++) {
      var f = tdoa.field[i];
      if (!f.good) continue;
      tdoa.list[list_i] = {};
      var li = tdoa.list[list_i];

      var a = w3_get_value('tdoa.url_field-'+ i);
      if (a == '') {
         f.good = false;
         continue;
      }
      a = a.split(':');
      if (a.length != 2 || a[0] == '' || a[1] == '') {
         f.good = false;
         continue;
      }

      if (list_i) h_v += ',', p_v += ',', id_v += ',';
      h =  a[0];
      li.h = h;
      h_v += h;
      p =  a[1];
      li.p = p;
      p_v += p;
      id = w3_get_value('tdoa.id_field-'+ i);
      if (id == '') {
         id = 'id_'+ i;
         w3_set_value('tdoa.id_field-'+ i, id);
      }
      id = id.replace(/[^0-9A-Za-z\-\/]/g, '');    // limit charset because id used in a filename
      li.call = id;
      id = id.replace(/\//g, '-');     // encode slashes
      li.id = id;
      id_v += id;

      li.field_idx = i;
      //console.log('list_i='+ list_i);
      //console.log(li);
      list_i++;
   }

   if (list_i < 2) {
      tdoa_submit_state(tdoa.ERROR, 'at least 2 available hosts needed');
      return;
   }
   
   var s = h_v + p_v + id_v;
   s += '&f='+ (ext_get_passband_center_freq()/1e3).toFixed(2);
   var pb = ext_get_passband();
   s += '&w='+ pb.high.toFixed(0);

   if (tdoa.cur_map == null) tdoa.cur_map = tdoa.gmap_kiwi;
   var b = tdoa.cur_map.getBounds();
   var ne = b.getNorthEast();
   var sw = b.getSouthWest();
   var lat_n = Math.round(ne.lat()+1);
   var lat_s = Math.round(sw.lat()-1);
   var lon_e = Math.round(ne.lng()+1);
   var lon_w = Math.round(sw.lng()-1);

   s += "&pi=struct('lat_range',\\["+ lat_s +","+ lat_n +"\\],'lon_range',\\["+ lon_w +","+ lon_e +"\\]";
   
   var r = w3_get_value('id-tdoa-known-location');
   if (r && r != '') {
      r = r.replace(/\s+/g, ' ').split(/[ ,]/g);
      if (r.length >= 3) {
         var lat = parseFloat(r[0]), lon = parseFloat(r[1]);
         if (!isNaN(lat) && !isNaN(lon)) {
            var name = r[2].replace(/[^0-9A-Za-z]/g, '');   // e.g. can't have spaces in names at the moment
            s += ",'known_location',struct('coord',\\["+ lat +","+ lon +"\\],'name',"+ sq(name) +")";
         }
      }
   }
   
   s += ")";
   
   if (tdoa.rerun) s += '&rerun='+ tdoa.response.key;    // key from previous run
   //console.log(s);
   
   tdoa.last_menu_select = undefined;
   tdoa.response = {};
   
   tdoa.response.seq = 0;
   kiwi_ajax_progress('http://kiwisdr.com/php/tdoa.php?auth='+ tdoa.auth + s,
      function(json) {     // done callback
         //console.log('DONE');
         tdoa_protocol_response_cb(json);
      }, 0,
      0,    // timeout
      function(json) {     // progress callback
         //console.log('PROGRESS');
         tdoa_protocol_response_cb(json);
      }, 0
   );

   if (!tdoa.rerun) {
      tdoa.field.forEach(function(f, i) {
         if (f.good) {
            tdoa_set_icon('sample', i, 'fa-refresh fa-spin', 20, 'lime');
            tdoa_set_icon('download', i, '');
            w3_innerHTML('id-tdoa-sample-status-'+ i, 'sampling started');
         }
      });
      tdoa_submit_state(tdoa.RUNNING, 'sampling started');
   } else {
      tdoa_submit_state(tdoa.RUNNING, 'rerun using same samples');
      tdoa.rerun = 0;
   }
   
   w3_button_text('id-tdoa-submit-button', 'Stop', 'w3-red', 'w3-css-yellow');
   w3_color('id-tdoa-submit-icon', 'lime');
   tdoa.gmap_map_type = tdoa.cur_map.getMapTypeId();
   tdoa.gmap_zoom = tdoa.cur_map.getZoom();
   tdoa.gmap_center = tdoa.cur_map.getCenter();
}

function tdoa_rerun_button_cb()
{
   tdoa.rerun = 1;
   tdoa_submit_button_cb();
}

function tdoa_sample_status_cb(status)
{
   //console.log('tdoa_sample_status_cb: sample_status='+ status.toHex());
   var error = false;
   var retry = 0;

   for (var i = 0; i < tdoa.ngood; i++) {
      var li = tdoa.list[i];
      var field_idx = li.field_idx;
      var stat = (status >> (i*2)) & 3;
      //console.log('field #'+ i +' stat='+ stat +' field_idx='+ field_idx);
      //console.log('field #'+ i +' h='+ li.h);

      if (stat) {
         tdoa_set_icon('sample', field_idx, 'fa-times-circle', 20, 'red');
         error = true;
      } else {
         // if there were any errors at all can't display download links because file list is not sent
         if (status != 0) {
            tdoa_set_icon('sample', field_idx, 'fa-refresh', 20, 'lime');
            stat = tdoa.SAMPLE_STATUS_BLANK;
         } else {
            tdoa_set_icon('sample', field_idx, 'fa-check-circle', 20, 'lime');
            var url = tdoa.response.files[i].replace(/^\.\.\/files/, 'kiwisdr.com/tdoa/files');
            w3_innerHTML('id-tdoa-download-icon-c'+ field_idx,
               w3_link('', url, w3_icon('w3-text-aqua', 'fa-download', 18))
            );
         }
      }
      var s = (stat >= tdoa.sample_status.length)? 'recording error' : tdoa.sample_status[stat].s;
      retry |= tdoa.sample_status[stat].retry;
      w3_innerHTML('id-tdoa-sample-status-'+ field_idx, s);
   }
   
   if (!error) {
      tdoa_set_icon('submit', -1, 'fa-refresh fa-spin', 20, 'lime');
      tdoa_submit_state(tdoa.RUNNING, 'TDoA running');
   } else {
      if (0 && retry) {    // fixme: doesn't work yet
         tdoa_set_icon('submit', -1, 'fa-cog fa-spin', 20, 'yellow');
         tdoa.retry_count = 15;
         tdoa.retry_interval = setInterval(function() {
            tdoa_submit_state(tdoa.RETRY, 'sampling error, will retry in '+ tdoa.retry_count +'s');
            if (tdoa.retry_count == 0) {
               kiwi_clearInterval(tdoa.retry_interval);
               console.log('RETRY');
               tdoa_submit_button_cb();
            }
            tdoa.retry_count--;
         }, 1000);
      } else {
         w3_button_text('id-tdoa-submit-button', 'Submit', 'w3-css-yellow', 'w3-red');
         tdoa_submit_state(tdoa.ERROR, 'sampling error');
      }
   }
}

function tdoa_submit_status_cb(status)
{
   //console.log('tdoa_submit_status_cb: submit_status='+ status +
   //   ' lat='+ tdoa.response.likely_lat +' lon='+ tdoa.response.likely_lon);
   if (status != 0) {
      var s = (status >= tdoa.submit_status.length)? 'unknown' : tdoa.submit_status[status].m;
      if (tdoa.submit_status[status].f == 1) s += ' error: zoom map in or check reception';
      tdoa_submit_state(tdoa.ERROR, s);
   } else {
      tdoa_set_icon('submit', -1, 'fa-check-circle', 20, 'lime');

      // prepare TDoA results menu
      var i, j;
      tdoa.results = [];
      // NB: [].menu must be assigned first so it works with w3_select()
      tdoa.results[0] = {};
      tdoa.results[1] = {};
      tdoa.results[2] = {};
      tdoa.results[0].menu = 'Kiwi map';
      tdoa.results[1].menu = 'TDoA map';
      tdoa.results[2].menu = 'TDoA combined';
      tdoa.results[0].file = 'Kiwi map';
      tdoa.results[1].file = 'TDoA map';
      tdoa.results[2].file = 'TDoA combined';
      tdoa.results[1].ids = 'TDoA map';
      tdoa.results[2].ids = 'TDoA combined';
      var ri = tdoa.SINGLE_MAPS;
      var npairs = tdoa.ngood * (tdoa.ngood-1) / 2;

      for (i = 0; i < tdoa.ngood; i++) {
         for (j = i+1; j < tdoa.ngood; j++) {
            var li1 = tdoa.list[i];
            var li2 = tdoa.list[j];
            //console.log('i:'+ i +'='+ li1.id +' j:'+ j +'='+ li2.id);
            tdoa.results[ri] = {};
            tdoa.results[ri].menu = li1.call +'-'+ li2.call +' map';
            tdoa.results[ri].file = li1.id +'-'+ li2.id +' map';
            tdoa.results[ri].ids = li1.id +'-'+ li2.id;
            tdoa.results[ri].list1 = i; tdoa.results[ri].list2 = j;
            tdoa.results[ri + npairs] = {};
            tdoa.results[ri + npairs].menu = li1.call +'-'+ li2.call +' dt';
            tdoa.results[ri + npairs].file = li1.id +'-'+ li2.id +' dt';
            ri++;
         }
      }

      w3_innerHTML('id-tdoa-results-select',
         w3_select('w3-text-red', '', 'TDoA results', 'tdoa.result_select', -1, tdoa.results, 'tdoa_result_menu_click_cb')
      );
      w3_show('id-tdoa-results-select');
      w3_show('id-tdoa-results-options');

      tdoa_result_menu_click_cb('', tdoa.TDOA_MAP);
      w3_select_value('tdoa.result_select', tdoa.TDOA_MAP);
      tdoa_submit_state(tdoa.RESULT, 'TDoA complete');
   }

   w3_button_text('id-tdoa-submit-button', 'Submit', 'w3-css-yellow', 'w3-red');
}

function tdoa_protocol_response_cb(json)
{
   if (typeof(json) == 'string') {
      json = json.trim();
      var sl = json.length;
      if (sl && json[0] == '{' && json[sl-1] != '}') {
         //console.log('added }');
         json += '}';
      }
      //console.log(json);
      var obj = JSON.parse(json);
   } else
      obj = json;
   //console.log(obj);
   
   if (tdoa.response.seq > 0 && obj.key && obj.key != tdoa.response.key) {
      //console.log('TDoA ignoring response with wrong key (expecting '+ tdoa.response.key +')');
      //console.log(obj);
      return;
   }

   // follow protocol order, processing each item only once
   // NB: no "else"s after these if statements because e.g.
   // key and status0 are delivered at the same time
   var v;
   if (tdoa.response.seq == 0 && obj.key != undefined) {
      tdoa.response.key = obj.key;
      tdoa.response.seq++;
      //console.log('GOT key='+ obj.key);
   }
   if (tdoa.response.seq == 1 && obj.files != undefined) {
      tdoa.response.files = obj.files;
      tdoa.response.seq++;
      //console.log('GOT files='+ tdoa.response.files.toString());
   }
   if (tdoa.response.seq == 2 && obj.status0 != undefined) {
      v = obj.status0;
      tdoa_sample_status_cb(v);
      tdoa.response.seq++;
      //console.log('GOT status0='+ v);
   }
   if (tdoa.response.seq == 3 && obj.likely != undefined) {
      var latlon = obj.likely.split(',');
      tdoa.response.likely_lat = parseFloat(latlon[0]).toFixed(2);
      tdoa.response.likely_lon = parseFloat(latlon[1]).toFixed(2);
      tdoa.response.seq++;
      //console.log('GOT likely='+ tdoa.response.likely_lat +','+ tdoa.response.likely_lon);
   }
   if (tdoa.response.seq == 4 && obj.status1 != undefined) {
      v = obj.status1;
      tdoa_submit_status_cb(v);
      tdoa.response.seq++;
      //console.log('GOT status1='+ v);
   }
}


////////////////////////////////
/// results UI
////////////////////////////////

function tdoa_result_menu_click_cb(path, idx, first)
{
   idx = +idx;
   tdoa.last_menu_select = idx;
   var new_maps = w3_el('id-tdoa-results-newmaps').checked;
   //console.log('tdoa_result_menu_click_cb idx='+ idx +' new_maps='+ new_maps);

   if (idx == tdoa.KIWI_MAP) {
      // Kiwi selection map
      w3_show('id-tdoa-gmap-kiwi');
      w3_hide('id-tdoa-gmap-result');
      w3_hide('id-tdoa-png');
      tdoa_info_cb();
      tdoa.cur_map = tdoa.gmap_kiwi;
   } else {
      var npairs = tdoa.ngood * (tdoa.ngood-1) / 2;
      
      // new maps (new maps mode but not dt maps)
      if (new_maps && idx < (tdoa.SINGLE_MAPS + npairs)) {
         w3_hide('id-tdoa-gmap-kiwi');
         w3_hide('id-tdoa-png');
   
         tdoa.gmap_result = new google.maps.Map(w3_el('id-tdoa-gmap-result'),
            {
               zoom: tdoa.gmap_zoom,
               center: tdoa.gmap_center,
               navigationControl: false,
               mapTypeControl: true,
               streetViewControl: false,
               draggable: true,
               scaleControl: true,
               gestureHandling: 'greedy',
               mapTypeId: tdoa.gmap_map_type
            });
         tdoa.cur_map = tdoa.gmap_result;

         tdoa.gmap_result.addListener('zoom_changed', tdoa_info_cb);
         tdoa.gmap_result.addListener('center_changed', tdoa_info_cb);
         tdoa.gmap_result.addListener('maptypeid_changed', tdoa_info_cb);
   
         var grid = new Graticule(tdoa.gmap_result, false);
         w3_show('id-tdoa-gmap-result');
         
         var ms, me;
         if (idx == tdoa.COMBINED_MAP) {
            // stack overlay and contours for combined mode
            ms = tdoa.SINGLE_MAPS; me = tdoa.SINGLE_MAPS + npairs;
         } else {
            ms = idx; me = ms+1;
         }
         //console.log('SET ms/me='+ ms +'/'+ me +' idx='+ idx);
         
         for (var mi = ms; mi < me; mi++) {
            var fn = 'http://kiwisdr.com/tdoa/files/'+ tdoa.response.key +'/'+ tdoa.results[mi].ids +'_contour_for_map.json';
            //console.log(fn);
            kiwi_ajax(fn, function(j, midx) {
               //console.log(j);

               tdoa.overlay[midx] = new google.maps.GroundOverlay(
                  'http://kiwisdr.com/tdoa/files/'+ j.filename,
                  j.imgBounds,
                  {opacity : 0.5});
               //console.log('SET midx='+ midx +'/'+ ms +'/'+ me +' ovl='+ tdoa.overlay[midx] +' heatmap_visible='+ tdoa.heatmap_visible);
               tdoa.overlay[midx].setMap(tdoa.heatmap_visible? tdoa.gmap_result : null);

               var pg = new Array(j.polygons.length);
               var i;
               for (i=0;i<j.polygons.length;++i) {
                  pg[i] = new google.maps.Polygon({
                      paths: j.polygons[i],
                      strokeColor:   j.polygon_colors[i],
                      strokeOpacity: 1,
                      strokeWeight:  1,
                      fillOpacity:   0
                  });
                  pg[i].setMap(tdoa.gmap_result);
               }

               var pl= new Array(j.polylines.length);
               for (i=0;i<j.polylines.length;++i) {
                  pl[i] = new google.maps.Polyline({
                      path: j.polylines[i],
                      strokeColor:   j.polyline_colors[i],
                      strokeOpacity: 1,
                      strokeWeight:  1,
                      fillOpacity:   0
                  });
                  pl[i].setMap(tdoa.gmap_result);
               }
            }, mi);
         }

         // figure out which markers to show on the result maps
         var show_mkrs = [];
         if (idx == tdoa.TDOA_MAP || idx == tdoa.COMBINED_MAP) {
            // TDoA and combined map get all host markers
            ms = tdoa.SINGLE_MAPS; me = tdoa.SINGLE_MAPS + npairs;
         } else {
            ms = idx; me = ms+1;
         }
         
         for (var mi = ms; mi < me; mi++) {
            var list1 = tdoa.results[mi].list1;
            var list2 = tdoa.results[mi].list2;
            //console.log('mi='+ mi +' L1='+ list1 +' L2='+ list2);
            show_mkrs[list1] = tdoa.list[list1].field_idx;
            show_mkrs[list2] = tdoa.list[list2].field_idx;
         }
         //console.log('show_mkrs=');
         //console.log(show_mkrs);
         for (var i = 0; i < show_mkrs.length; i++) {
            var field_idx = show_mkrs[i];
            //console.log('field_idx='+ field_idx);
            if (field_idx == undefined) continue;
            var f = tdoa.field[field_idx];
            //console.log(f);
            
            // if field was set via marker show marker on result map
            if (f.mkr && f.host) tdoa_place_host_marker(f.host, tdoa.gmap_result);
         }

         if (tdoa.known_location_idx != undefined) {
            // use most recently selected marker, if any
            tdoa_place_ref_marker(tdoa.known_location_idx, tdoa.gmap_result);
         } else {
            // otherwise generate marker from manually entered lat/lon
            var r = w3_get_value('id-tdoa-known-location');
            if (r && r != '') {
               r = r.replace(/\s+/g, ' ').split(/[ ,]/g);
               if (r.length >= 2) {
                  var lat = parseFloat(r[0]), lon = parseFloat(r[1]);
                  if (!isNaN(lat) && !isNaN(lon)) {
                     var ref = tdoa.refs[0];
                     ref.id = r[2].replace(/[^0-9A-Za-z]/g, '');   // old maps can't have spaces in names at the moment
                     ref.lat = lat; ref.lon = lon;
                     r.shift(); r.shift(); r.shift();
                     ref.t = r.join(' ');
                     ref.f = undefined;
                     tdoa_place_ref_marker(0, tdoa.gmap_result);
                  }
               }
            }
         }

         if (tdoa.response.likely_lat != undefined && tdoa.response.likely_lon != undefined) {
            var ref = tdoa.refs[1];
            ref.id = tdoa.response.likely_lat +', '+ tdoa.response.likely_lon;
            ref.lat = tdoa.response.likely_lat; ref.lon = tdoa.response.likely_lon;
            ref.t = 'Most likely position';
            tdoa_place_ref_marker(1, tdoa.gmap_result);
         }
      } else {
         // old maps or new dt maps
         w3_hide('id-tdoa-gmap-kiwi');
         w3_hide('id-tdoa-gmap-result');
         var s;
         if (idx == tdoa.COMBINED_MAP)
            s = w3_div('w3-text-yellow', 'Available only for new map option');
         else
            s = '<img src="http://kiwisdr.com/tdoa/files/'+ tdoa.response.key +'/'+ tdoa.results[idx].file +'.png" />';
         w3_innerHTML('id-tdoa-png', s);
         w3_show('id-tdoa-png');
         tdoa.cur_map = tdoa.gmap_kiwi;
      }

      if (tdoa.response.likely_lat != undefined && tdoa.response.likely_lon != undefined) {
         w3_innerHTML('id-tdoa-result',
            w3_text('|color: hsl(0, 0%, 70%)', 'most likely: ') +
            w3_text('|color: hsl(0, 0%, 70%)', tdoa.response.likely_lat +', '+ tdoa.response.likely_lon)
         );
         var url = 'https://google.com/maps/place/'+ tdoa.response.likely_lat +','+ tdoa.response.likely_lon;
         w3_innerHTML('id-tdoa-gmap-link', w3_link('', url, w3_icon('w3-text-aqua', 'fa-map-marker', 18)));
      }
   }
}

function tdoa_heatmap_visible_cb(path, checked)
{
   tdoa.heatmap_visible = checked;
   //console.log('tdoa_heatmap_visible_cb heatmap_visible='+ tdoa.heatmap_visible +' last_menu_select='+ tdoa.last_menu_select);
   if (!tdoa.last_menu_select) return;
   
   var new_maps = w3_el('id-tdoa-results-newmaps').checked;
   var ms, me;
   if (new_maps && tdoa.last_menu_select == tdoa.COMBINED_MAP) {
      var npairs = tdoa.ngood * (tdoa.ngood-1) / 2;
      ms = tdoa.SINGLE_MAPS; me = tdoa.SINGLE_MAPS + npairs;
   } else {
      ms = tdoa.last_menu_select; me = ms+1;
   }
   
   for (var mi = ms; mi < me; mi++) {
      var overlay = tdoa.overlay[mi];
      //console.log('tdoa_heatmap_visible_cb mi='+ mi +' ovl='+ overlay);
      if (overlay) overlay.setMap(tdoa.heatmap_visible? tdoa.gmap_result : null);
   }
}

function tdoa_day_night_visable_cb(path, checked)
{
   tdoa.day_night.setMap(checked? tdoa.gmap_kiwi : null);
}

function tdoa_refs_visable_cb(path, checked, first)
{
   //console.log('### tdoa_refs_visable_cb checked='+ checked +' first='+ first);
   if (first) return;

   tdoa.ref_ids.forEach(function(id) {
      w3_checkbox_set('tdoa.refs_'+ id, checked? true:false);
   });
   
   tdoa_rebuild_refs();
}

function tdoa_refs_cb(path, checked, first)
{
   if (first) return;
   tdoa_rebuild_refs();
}

function tdoa_rebuild_hosts()
{
   // remove previous
   if (tdoa.cur_host_markers) {
      tdoa.hosts.forEach(function(h) {
         h.mkr.setMap(null);
      });
      tdoa.hosts_clusterer.clearMarkers();
   }

   // make hosts clusters contain only un-param-selected host markers
   tdoa.cur_host_markers = [];
   for (var i = 0; i < tdoa.hosts.length; i++) {
      var h = tdoa.hosts[i];
      if (h.mkr) {
         if (h.selected) {
            //console.log('outside cluster: '+ h.call);
            h.mkr.setMap(tdoa.gmap_kiwi);
         } else {
            tdoa.cur_host_markers.push(h.mkr);
         }
      }
   }
   
   //console.log('tdoa_rebuild_hosts cur_host_markers='+ tdoa.cur_host_markers.length);

   tdoa.hosts_clusterer = new MarkerClusterer(tdoa.gmap_kiwi, tdoa.cur_host_markers, {
      averageCenter: true,
      gridSize: 30,     // default 60
      cssClass: 'cl-tdoa-cluster-base cl-tdoa-cluster-kiwi',
      onMouseoverCluster: function(this_, ev) {
         s = '';
         this_.cluster_.markers_.forEach(function(m) {
            s += ((s != '')? '\n':'') + m.labelContent;
         });
         this_.div_.title = s;
      }
   });
}

function tdoa_rebuild_refs()
{
   // remove previous
   if (tdoa.cur_ref_markers) {
      tdoa.refs_markers.forEach(function(m) {
         m.setMap(null);
      });
      tdoa.refs_clusterer.clearMarkers();
   }

   // build new list
   ids = [];
   tdoa.ref_ids.forEach(function(id) {
      if (w3_checkbox_get('tdoa.refs_'+ id))
         ids.push(id);
   });
   //console.log('tdoa_rebuild_refs <'+ ids +'>');

   tdoa.cur_ref_markers = [];
   for (var i = tdoa.FIRST_REF; i < tdoa.refs.length-1; i++) {
      var r = tdoa.refs[i];
      //console.log(r.r +' '+ r.id);
      ids.forEach(function(id) {
         if (r.r.includes(id)) {
            if (r.selected) {
               console.log('selected marker: '+ r.id);
               r.mkr.setMap(tdoa.gmap_kiwi);
            } else {
               tdoa.cur_ref_markers.push(r.mkr);
            }
         }
      });
   }
   //console.log('tdoa_rebuild_refs cur_ref_markers='+ tdoa.cur_ref_markers.length);

   tdoa.refs_clusterer = new MarkerClusterer(tdoa.gmap_kiwi, tdoa.cur_ref_markers, {
      averageCenter: true,
      gridSize: 30,     // default 60
      cssClass: 'cl-tdoa-cluster-base cl-tdoa-cluster-refs',
      onMouseoverCluster: function(this_, ev) {
         var s = '';
         this_.cluster_.markers_.forEach(function(m) {
            s += ((s != '')? '\n':'') + m.labelContent;
         });
         this_.div_.title = s;
      }
   });
}

function tdoa_quick_zoom_cb(path, idx, first)
{
   if (first) return;
   var q = tdoa.quick_zoom[+idx];
   var latlon = new google.maps.LatLng(q.lat, q.lon);
   tdoa.gmap_kiwi.panTo(latlon);
   tdoa.gmap_kiwi.setZoom(q.z);
}

function TDoA_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'TDoA Help. Improvements are being made almost daily.') +
         '<br>See the <a href="http://valentfx.com/vanilla/categories/kiwisdr-tdoa-topics" target="_blank">Kiwi forum</a> for more information. ' +
         'If you are getting errors check these <br> common problems:<ul>' +
         '<li>Not zoomed-in far enough. The TDoA process will run out of memory or have problems plotting the maps.</li>' +
         '<li>Not all Kiwis used for sampling have good reception of target signal. ' +
         'Open a connection to each Kiwi by double clicking on its marker to check the reception.</li>' +
         '<li>Don\'t use Kiwis that are spaced too far apart (i.e. a few thousand km).</li>' +
         '<li>Use minimum IQ-mode passband. Just enough to capture the signal. ' +
         'Use the "p" and "P" keys to narrow/widen the passband.</li>' +
         '</ul>' +

         'Once you configure this extension, and click the "Submit" button, ' +
         'information is sent to the kiwisdr.com server. The server then records ' +
         '30 seconds of IQ data from the two to six sampling Kiwis specified. ' +
         'The frequency and passband of <b><i>this</i></b> Kiwi will be used for all recording. ' +
         'So make sure it is set correctly before proceeding (always use IQ mode and minimum necessary passband). ' +
         'After sampling, the TDoA process will be run on the server. After it finishes a result map will appear. ' +
         'Additional maps may be viewed with the TDoA result menu. ' +
         'You can pan and zoom the resulting maps and click submit again after making any changes.<br><br>' +
         
         'To begin zoom into the general area of interest on the Kiwi map (note the "quick zoom" menu). ' +
         'Click on the desired blue/red Kiwi sampling stations. If they are not responding or have ' +
         'had no recent GPS solutions an error message will appear. ' +
         '<br><b>Important:</b> the position and zooming of the Kiwi map determines the same for the resulting TDoA maps. ' +
         'Double click on the blue markers to open that Kiwi in a new tab to check if it is receiving the target signal well. ' +
         'You can also manually edit the sampling station list (white fields). ' +
         '' +
         '' +
         '<br><br>' +
         
         'You can click on the white map markers to set the frequency/passband of some well-known reference stations. ' +
         'The known locations of these stations will be shown in the result maps ' +
         'so you can see how well it agrees with the TDoA solution. ' +
         'Practice with VLF/LF references stations as their ground-wave signals usually give good results. ' +
         'Start with only two sampling stations and check the quality of the solution before adding more. ' +
         'Of course you need three or more stations to generate a localized solution. ' +
         '' +
         '' +
         '' +
         '' +
         '';
      confirmation_show_content(s, 600, 600);
   }
   return true;
}

function TDoA_focus()
{
	tdoa.optbar = ext_get_optbar();

   // switch optbar off to not obscure map
   ext_set_optbar('optbar-off');
}

function TDoA_blur()
{
	ext_set_data_height();     // restore default height
	
	// restore optbar if it wasn't changed
	if (ext_get_optbar() == 'optbar-off')
	   ext_set_optbar(tdoa.optbar);
}

function TDoA_config_focus()
{
   //console.log('TDoA_config_focus');
	admin_set_decoded_value('tdoa_id');
}

function TDoA_config_blur()
{
   //console.log('TDoA_config_blur');
}

function TDoA_config_html()
{
   // Let cfg.tdoa_nchans retain values > rx_chans if it was set when another configuration
   // was used. Just clamp the menu value to the current rx_chans;
	var nchans = ext_get_cfg_param('tdoa_nchans', -1);
	if (nchans == -1) nchans = rx_chans;      // has never been set
	tdoa.nchans = Math.min(nchans, rx_chans);
   tdoa.chans_u = { 0:'none' };
   for (var i = 1; i <= rx_chans; i++)
      tdoa.chans_u[i] = i.toFixed(0);

	ext_admin_config(tdoa.ext_name, 'TDoA',
		w3_div('id-TDoA w3-text-teal w3-hide',
			'<b>TDoA configuration</b>',
			'<hr>',
			w3_inline_percent('w3-container',
            w3_div('w3-center',
               w3_select('', 'Number of simultaneous channels available<br>for connection by the TDoA service',
                  '', 'tdoa_nchans', tdoa.nchans, tdoa.chans_u, 'admin_select_cb'),
               w3_div('w3-margin-T-8 w3-text-black',
                  'If you want to limit incoming connections from the <br> kiwisdr.com TDoA service set this value.<br>' +
                  'These connections are identified in the user lists and logs as: <br>' +
                  '"TDoA_service" (Fremont, California, USA)'
               )
            ), 40,
            
			   w3_div('w3-text-black'), 15,

		      w3_div('',
		         w3_input('||size=12', 'TDoA ID (alphanumeric and \'/\', 12 char max)', 'cfg.tdoa_id', '', 'tdoa_id_cb',
		            'TDoA Google map marker id (e.g. callsign)'
		         ),
               w3_div('w3-margin-T-8 w3-text-black',
                  'When this Kiwi is used as a measurement station for the TDoA service it displays an ' +
                  'identifier on a Google map pin and in the first column of the TDoA extension control panel (when selected). ' +
                  '<br><br>You can customize that identifier by setting this field. By default the identifier will be ' +
                  'one of two items. Either what appears to be a ham callsign in the the "name" field on the ' +
                  'admin page sdr.hu tab or the grid square computed from your Kiwis lat/lon.'
               )
		      ), 35,

			   w3_div('w3-text-black'), 10
         )
		), 'TDoA_config'
	);
}

function tdoa_id_cb(path, val)
{
   // sanitize input
   val = val.replace(/[^a-zA-Z0-9\/]/g, '');
   val = val.substr(0, 12);   // 12 chars max
   w3_set_value(path, val);
   w3_string_set_cfg_cb(path, val);
}

function tdoa_chans_cb(path, idx, first)
{
   if (first) return;
   tdoa.nchans = +idx;
}
