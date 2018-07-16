// Copyright (c) 2017 John Seamons, ZL/KF6VO

var tdoa = {
   ext_name: 'TDoA',    // NB: must match tdoa.cpp:tdoa_ext.name
   first_time: true,
   w_data: 1024,
   h_data: 465,
   
   tfields: 6,
   inuse: [],
   ngood: 0,
   good: [],
   list: [],
   mkr: [],
   overlay: [],
   
   gmap_kiwi: null,
   gmap_result: null,
   mapZoom_nom: 17,
   nfixed_maps: 3,
   overlay_visible: true,
   
   refs: [
      { },  // first holds generated marker
      { id:'NWC', t:'MSK', f:19.8, p:400, z:11, lat:-21.8163, lon:114.1656, mz:14 },
      { id:'ICV', t:'MSK', f:20.27, p:400, z:11, lat:40.9229, lon:9.7316, mz:19 },
      { id:'SainteAssise', t:'MSK', f:20.9, p:400, z:11, lat:48.5450, lon:2.5763, mz:15 },
      { id:'NPM', t:'MSK', f:21.4, p:400, z:11, lat:21.4201, lon:-158.1509 },
      { id:'DHO38', t:'MSK', f:23.4, p:400, z:11, lat:53.0816, lon:7.6163, mz:16 },
      { id:'NAA', t:'MSK', f:24.0, p:400, z:11, lat:44.6464, lon:-67.2811, mz:15 },
      { id:'NLK', t:'MSK', f:24.8, p:400, z:11, lat:48.2036, lon:-121.9171, mz:15 },
      { id:'NLM4', t:'MSK', f:25.2, p:400, z:11, lat:46.3660, lon:-98.3355, mz:16 },
      { id:'TBB', t:'FSK', f:26.7, p:100, z:12, lat:37.4092, lon:27.3242, mz:16 },
      { id:'Negev', t:'FSK', f:29.7, p:100, z:12, lat:30.9720, lon:35.0970, mz:15 },
      { id:'TFK/NRK', t:'MSK', f:37.5, p:400, z:12, lat:63.8504, lon:-22.4667, mz:16 },
      { id:'NAU', t:'MSK', f:40.75, p:400, z:11, lat:18.3987, lon:-67.1774 },
      { id:'NSY', t:'MSK', f:45.9, p:400, z:11, lat:37.1256, lon:14.4363 },
      { id:'Kerlouan', t:'MSK', f:62.6, p:400, z:11, lat:48.6377, lon:-4.3507, mz:16 },
      { id:'BPC', t:'time station', f:68.5, p:100, z:11, lat:34.4565, lon:115.8369, mz:18 },
      { id:'DCF77', t:'time station', f:77.5, p:1000, z:10, lat:50.0152, lon:9.0112, mz:16 },
      { id:'Anthorn', t:'Loran-C nav', f:100, p:10000, z:8, lat:54.9117, lon:-3.2785, mz:16 },  // also MSF 60
      { id:'TDF', t:'time station', f:162, p:200, z:10, lat:47.1695, lon:2.2046 },
      
      { id:'Inskip', t:'DHFCS\nGYN2 FSK', f:81, p:100, z:12, lat:53.8276, lon:-2.8364, mz:16 }, // also HF STANAG
      //{ id:'ForestMoor', t:'DHFCS', lat:54.0060, lon:-1.7249, mz:16 },   // rx-only per Martin (paired with Inskip)
      { id:'StEval', t:'DHFCS', lat:50.4786, lon:-5.0004, mz:15 },
      { id:'Croughton', t:'RAF/USAF', lat:51.987457, lon:-1.179636, mz:16 },
      { id:'Crimond', t:'Royal Navy', lat:57.617474, lon:-1.886923 },
      
      { id:'Cypress', t:'DHFCS Akrotiri', lat:34.6176, lon:32.9423, mz:16 },
      { id:'Ascension', t:'DHFCS', lat:-7.9173, lon:-14.3895 },
      { id:'Falklands', t:'DHFCS', lat:-51.8465, lon:-58.4510 },

      { id:'Shanwick', t:'HF ATC', lat:52.782104, lon:-8.930790 },

      { id:'Frederikshavn', t:'Danish Army', lat:57.407019, lon:10.515327, mz:18 },

      { id:'Dutch Navy', t:'Goeree-Overflakkee NL\nSTANAG 4285', lat:51.8073, lon:3.8931, mz:18 },

      { id:'Spain', t:'Navy radio', lat:40.477133, lon:-3.196901, mz:16 },

      { id:'Rosnay', t:'MSK', lat:46.7130, lon:1.2454, mz:14 },
      { id:'FUE', t:'Brest\nSTANAG 4285', lat:48.4260, lon:-4.2407 },
      { id:'FUG', t:'La Regine\nSTANAG 4285', lat:43.3868, lon:2.0975, mz:15 },
      { id:'FUO', t:'Toulon', lat:43.1370, lon:6.0605, mz:18 },
      { id:'Vernon', t:'', lat:49.094616, lon:1.507884, mz:17 },

      { id:'6WW', t:'Dakar', lat:14.7604, lon:-17.2740 },
      { id:'FUM', t:'Papeete\nSTANAG 4285', lat:-17.5054, lon:-149.4828, mz:19 },
      { id:'FUX', t:'La Reunion\nSTANAG 4285', lat:-20.9101, lon:55.5844 },
      { id:'FUJ', t:'Noumea\nSTANAG 4285', lat:-22.3054, lon:166.4548 },
      { id:'FUF', t:'Martinique\nSTANAG 4285', lat:14.5322, lon:-60.9790 },
      { id:'FUV', t:'Djibouti\nSTANAG 4285', lat:11.535952, lon:43.155575 },

      { id:'CKN', t:'Vancouver MSK', lat:49.108321, lon:-122.242931 },
      { id:'CFH', t:'Halifax MSK', lat:44.967743, lon:-63.983839 },

      { id:'RNZ', t:'Radio NZ', lat:-38.843134, lon:176.429749, mz:18 },
      { id:'ZLM', t:'Marine Radio\nMS-188, STANAG 4285', f:4250.30, p:3300, z:10, lat:-38.869305, lon:176.439008 },
      { id:'Irirangi', t:'RNZ Navy', lat:-39.4592, lon:175.6682, mz:18 },
   ],
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
      'sampling complete',
      'connection failed',
      'all channels in use',
      'no recent GPS timestamps'
      // 4 max (2 bits * 6 rx = 12 bits total currently)
   ],
   
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
   
   state: 0, READY_SAMPLE: 0, READY_COMPUTE: 1, RUNNING: 2, RESULT: 3, ERROR: 4,
   
   key: '',
   results: [],
   results_file: [],
   select: undefined,
};

function TDoA_main()
{
	ext_switch_to_client(tdoa.ext_name, tdoa.first_time, tdoa_recv);		// tell server to use us (again)
	if (!tdoa.first_time)
		tdoa_controls_setup();
	tdoa.first_time = false;
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
			console.log('tdoa_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
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
				tdoa_controls_setup();
				break;

			case "key":
				console.log('TDoA: key='+ param[1]);
			   if (0 && tdoa.state != tdoa.RUNNING) {
				   console.log('TDoA: key ignored because state='+ tdoa.state);
			      return;
			   }
				tdoa.key = param[1];
				break;

         // called on submit after sampling completes
			case "sample_status":
				var status = parseInt(param[1]);
				console.log('tdoa_recv: sample_status='+ status.toHex());
				var error = false;
				
            for (var i = 0; i < tdoa.ngood; i++) {
               var field_idx = tdoa.list[i].field_idx;
               var stat = (status >> (i*2)) & 3;
               //console.log('field #'+ i +' stat='+ stat +' field_idx='+ field_idx);
               //console.log('field #'+ i +' h='+ tdoa.list[i].h);
               if (stat) {
                  tdoa_set_icon('sample', field_idx, 'fa-times-circle', 20, 'red');
                  error = true;
               } else {
                  tdoa_set_icon('sample', field_idx, 'fa-check-circle', 20, 'lime');
               }
               var s = (stat >= tdoa.sample_status.length)? 'recording error' : tdoa.sample_status[stat];
               w3_innerHTML('id-tdoa-sample-status-'+ field_idx, s);
            }
            
            if (!error) {
               var icon = 'id-tdoa-submit-icon';
               w3_color(icon, 'lime');
               w3_add(icon, 'fa-spin');
               tdoa_submit_status(tdoa.RUNNING, 'TDoA running');
            } else {
               tdoa_submit_status(tdoa.ERROR, 'sampling error');
            }
            
				break;

         // called on submit after TDoA completes
			case "submit_status":
				var status = parseInt(param[1]);
				console.log('tdoa_recv: submit_status='+ status);
				if (status != 0) {
               var s = (status >= tdoa.submit_status.length)? 'unknown' : tdoa.submit_status[status].m;
               if (tdoa.submit_status[status].f == 1) s += ' error: zoom map in or check reception';
				   tdoa_submit_status(tdoa.ERROR, s);
				   break;
				} else {
               tdoa_set_icon('submit', -1, 'fa-check-circle', 20, 'lime');

               var i, j;
               tdoa.results = [];
               tdoa.results[0] = 'Kiwi map';
               tdoa.results[1] = 'TDoA map';
               tdoa.results[2] = 'TDoA composite';
               tdoa.results_file = [];
               tdoa.results_file[0] = 'Kiwi map';
               tdoa.results_file[1] = 'TDoA map';
               tdoa.results_file[2] = 'TDoA composite';
               tdoa.results_file2 = [];
               tdoa.results_file2[1] = 'TDoA map';
               tdoa.results_file2[2] = 'TDoA composite';
               var ri = tdoa.nfixed_maps;
               for (i = 0; i < tdoa.ngood; i++) {
                  for (j = i+1; j < tdoa.ngood; j++) {
                     //console.log('i:'+ i +'='+ tdoa.list[i].id +' j:'+ j +'='+ tdoa.list[j].id);
                     tdoa.results[ri] = tdoa.list[i].call +'-'+ tdoa.list[j].call +' map';
                     tdoa.results_file[ri] = tdoa.list[i].id +'-'+ tdoa.list[j].id +' map';
                     tdoa.results_file2[ri] = tdoa.list[i].id +'-'+ tdoa.list[j].id;
                     ri++;
                  }
               }
               for (i = 0; i < tdoa.ngood; i++) {
                  for (j = i+1; j < tdoa.ngood; j++) {
                     tdoa.results[ri] = tdoa.list[i].call +'-'+ tdoa.list[j].call +' dt';
                     tdoa.results_file[ri] = tdoa.list[i].id +'-'+ tdoa.list[j].id +' dt';
                     ri++;
                  }
               }

               w3_innerHTML('id-tdoa-results-select',
                  w3_select('w3-text-red', '', 'TDoA results', 'tdoa.result_select', -1, tdoa.results, 'tdoa_result_select_cb')
               );
               w3_show('id-tdoa-results-select');

               w3_innerHTML('id-tdoa-results-options',
                  w3_inline('/w3-margin-between-16',
                     w3_checkbox('id-tdoa-results-newmaps w3-label-inline w3-label-not-bold', 'New map type', 'tdoa.new_maps', true, 'w3_checkbox_value'),
                     w3_checkbox('id-tdoa-results-overlay w3-label-inline w3-label-not-bold', 'Overlay visible', 'tdoa.overlay_visible', true, 'tdoa_overlay_visable_cb')
                  )
               );
               w3_show('id-tdoa-results-options');

				   tdoa_submit_status(tdoa.RESULT, '');
				}
				
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
   
	var data_html =
      time_display_html('tdoa') +
      
      w3_div('id-tdoa-data w3-display-container',
         w3_div('id-tdoa-gmap-kiwi|left:0px; width:'+ px(tdoa.w_data) +'; height:'+ px(tdoa.h_data), ''),
         w3_div('id-tdoa-gmap-result w3-hide|left:0px; width:'+ px(tdoa.w_data) +'; height:'+ px(tdoa.h_data), ''),
         w3_div('id-tdoa-png w3-display-topleft w3-scroll-y w3-hide|left:0px; width:'+ px(tdoa.w_data) +'; height:'+ px(tdoa.h_data), '')
      );
   
	var control_html =
	   w3_divs('w3-tspace-8',
		   w3_div('id-tdoa-controls w3-text-aqua', '<b>Time difference of arrival (TDoA) direction finding service</b>'),
		   //w3_div('w3-text-white', 'It is essential to read the instructions (help button) before using this tool'),
		   w3_div('id-tdoa-hosts-container'),
		   w3_inline('',
            w3_button('id-tdoa-submit-button w3-padding-smaller w3-css-yellow', 'Submit', 'tdoa_submit_button_cb'),
		      w3_div('id-tdoa-submit-icon-c w3-margin-left'),
            w3_div('id-tdoa-results-select w3-margin-left w3-hide'),
            w3_div('id-tdoa-results-options w3-margin-left w3-hide'),
            w3_div('id-tdoa-submit-status w3-margin-left')
         ),
		   w3_inline_percent('',
            w3_select('w3-text-red', '', 'quick zoom', 'tdoa.quick_zoom', -1, tdoa.quick_zoom, 'tdoa_quick_zoom_cb'), 20,
            w3_input('id-tdoa-known-location w3-padding-tiny', '', 'tdoa.known_location', '', 'tdoa_known_location_cb',
               'Map ref location; type lat, lon and name or click white map markers'
            )
         )
      );

	ext_panel_show(control_html, data_html, null);
	time_display_setup('tdoa');

	ext_set_controls_width_height(600, 255);
   ext_set_data_height(tdoa.h_data);

   var s = '';
	for (i = 0; i < tdoa.tfields; i++) {
	   s +=
	      w3_inline(i? 'w3-margin-T-3':'',
            w3_input('w3-padding-tiny||size=10', '', 'tdoa.id_field-'+ i, ''),
            w3_input('w3-margin-L-3 w3-padding-tiny||size=28', '', 'tdoa.host_field-'+ i, '', 'tdoa_host_field_cb'),
            w3_icon('w3-margin-L-8 id-tdoa-clear-icon-'+ i, 'fa-cut', 20, 'red', 'tdoa_clear_cb', i),
		      w3_div('w3-margin-L-8 id-tdoa-sample-icon-c'+ i),
		      w3_div('w3-margin-L-8 id-tdoa-sample-status-'+ i)
         );
   }
   w3_innerHTML('id-tdoa-hosts-container', s);
   
   var latlon = new google.maps.LatLng(20, 0);

   tdoa.gmap_kiwi = new google.maps.Map(w3_el('id-tdoa-gmap-kiwi'),
      {
         zoom: 2,
         center: latlon,
         navigationControl: false,
         mapTypeControl: true,
         streetViewControl: false,
         draggable: true,
         scaleControl: true,
         gestureHandling: 'greedy',
         mapTypeId: google.maps.MapTypeId.SATELLITE
      });
   
   var day_night = new DayNightOverlay( { map:tdoa.gmap_kiwi, fillColor:'rgba(0,0,0,0.4)' } );
   var grid = new Graticule(tdoa.gmap_kiwi, false);

   for (i = 1; i < tdoa.refs.length; i++) tdoa_place_ref_marker(i, tdoa.gmap_kiwi);
   
   //ext_set_mode('iq');   // FIXME: currently undoes pb set by &pbw=nnn in URL
   
   // request json list of GPS-active Kiwis
   kiwi_ajax('http://kiwisdr.com/tdoa/files/kiwi.gps.json', 'tdoa_get_hosts_cb');

   tdoa_ui_reset();
	TDoA_environment_changed( {resize:1} );
   tdoa.state = tdoa.READY_SAMPLE;
}

function tdoa_place_host_markers(hosts)
{
   console.log('TDoA hosts='+ hosts.length);
   //console.log(hosts);

   for (i = 0; i < hosts.length; i++) {
      var h = hosts[i];
      //console.log(h);
      
      if (h.h == '') {
         console.log('TDoA warning h empty?');
         console.log(h);
         continue;
      }
      
      hosts[i].hp = h.h +':'+ h.p;
      hosts[i].call = h.id.replace(/\-/g, '/');    // decode slashes
      
      var title = h.n +'\nUsers: '+ h.u +'/'+ h.um +'\nAntenna: ' + h.a +'\n'+ h.fm +' GPS fixes/min';

      var latlon = new google.maps.LatLng(h.lat, h.lon);
      var marker = new MarkerWithLabel({
         position: latlon,
         draggable: false,
         raiseOnDrag: false,
         map: tdoa.gmap_kiwi,
         labelContent: h.call,
         labelAnchor: new google.maps.Point(h.call.length*3.6, 36),
         labelClass: 'cl-tdoa-gmap-host-label',
         labelStyle: {opacity: 1.0},
         title: title,
         icon: kiwi_mapPinSymbol('red', 'white')
      });
      marker.hosts_idx = i;

      google.maps.event.addListener(marker, "click", function() {
         if (tdoa.click_pending === true) return;
         tdoa.click_pending = true;
         tdoa.click_hosts_idx = this.hosts_idx;
         tdoa.click_hosts_mkr = this;

         tdoa.click_timeout = setTimeout(function() {
            tdoa.click_pending = false;

            for (var j = 0; j < tdoa.tfields; j++) {
               if (w3_get_value('tdoa.id_field-'+ j) == hosts[tdoa.click_hosts_idx].call) return;  // already in list
            }
            
            for (var j = 0; j < tdoa.tfields; j++) {
               if (!tdoa.inuse[j]) {       // put in first empty field
                  var i = tdoa.click_hosts_idx;
                  var h = hosts[i];
                  var host = h.hp;
                  tdoa.inuse[j] = true;
                  tdoa.good[j] = true;
                  tdoa.mkr[j] = tdoa.click_hosts_mkr;
                  w3_set_value('tdoa.id_field-'+ j, h.call);
                  w3_set_value('tdoa.host_field-'+ j, host);
                  tdoa_set_icon('sample', j, 'fa-refresh fa-spin', 20, 'lightGrey');
                  //tdoa_change_marker_style(tdoa.click_hosts_mkr, 'red', 'white', 'cl-tdoa-gmap-selected-label');

                  // launch status check
                  var s = (dbgUs && host == 'kiwisdr.jks.com:8073')? 'www:8073' : host;
                  kiwi_ajax('http://'+ s +'/status', 'tdoa_sample_status_cb', j, 5000);
                  return;
               }
            }
         }, 300);
      });

      google.maps.event.addListener(marker, "dblclick", function(event) {
         kiwi_clearTimeout(tdoa.click_timeout);    // keep single click event from hapenning
         tdoa.click_pending = false;
         
         var url = 'http://'+ hosts[this.hosts_idx].hp +'/?f='+ ext_get_freq_kHz() + ext_get_mode() +'z'+ ext_get_zoom();
         var pb = ext_get_passband();
         url += '&pbw='+ (pb.high * 2).toFixed(0);
         //console.log('double-click '+ url);
         var win = window.open(url, '_blank');
         if (win) win.focus();
      });
   }
}

function tdoa_place_ref_marker(idx, map)
{
   var r = tdoa.refs[idx];
   var latlon = new google.maps.LatLng(r.lat, r.lon);
   var marker = new MarkerWithLabel({
      position: latlon,
      draggable: false,
      raiseOnDrag: false,
      map: map,
      labelContent: r.id,
      labelAnchor: new google.maps.Point(r.id.length*3.6, 36),
      labelClass: 'cl-tdoa-gmap-ref-label',
      labelStyle: {opacity: 1.0},
      title: 'Known location: '+ r.t + (r.f? (', '+ r.f +' kHz'):''),
      icon: kiwi_mapPinSymbol('lime', 'black')
   });
   marker.refs_idx = idx;
   google.maps.event.addListener(marker, "click", function() {
      if (tdoa.click_pending === true) return;
      tdoa.click_pending = true;
      tdoa.click_refs_idx = this.refs_idx;

      tdoa.click_timeout = setTimeout(function() {
         tdoa.click_pending = false;

         console.log('Google map z='+ map.getZoom());
         if (map == tdoa.gmap_kiwi) {
            var r = tdoa.refs[tdoa.click_refs_idx];
            var t = r.t.replace('\n', ' ');
            var loc = r.lat.toFixed(2) +' '+ r.lon.toFixed(2) +' '+ r.id +' '+ t + (r.f? (' '+ r.f +' kHz'):'');
            w3_set_value('id-tdoa-known-location', loc);
            tdoa.known_location_idx = tdoa.click_refs_idx;
            if (r.f)
               ext_tune(r.f, 'iq', ext_zoom.ABS, r.z, -r.p/2, r.p/2);
         }
      }, 300);
   });

   google.maps.event.addListener(marker, "dblclick", function(event) {
      kiwi_clearTimeout(tdoa.click_timeout);    // keep single click event from hapenning
      tdoa.click_pending = false;
      
      var r = tdoa.refs[this.refs_idx];
      if (map.getZoom() >= (r.mz || tdoa.mapZoom_nom)) {
         map.panTo(new google.maps.LatLng(20, 0));
         map.setZoom(2);
      } else {
         map.panTo(new google.maps.LatLng(r.lat, r.lon));
         map.setZoom(r.mz || tdoa.mapZoom_nom);
      }
   });
}

function tdoa_change_marker_style(mkr, body_color, outline_color, label_class)
{
   mkr.setMap(null);
   mkr.labelClass = label_class;
   mkr.icon = kiwi_mapPinSymbol(body_color, outline_color);
   mkr.setMap(tdoa.gmap_kiwi);
}

function tdoa_get_hosts_cb(obj, param)
{
   var err;
   
   if (!obj) {
      console.log('tdoa_get_hosts_cb obj:');
      console.log(obj);
      return;
   }
   
   //console.log(obj);
   tdoa_place_host_markers(obj);
}

function TDoA_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-tdoa-data');
	if (!el) return;
	var left = (window.innerWidth - integ_w - time_display_width()) / 2;
	//console.log('tdoa_resize wiw='+ window.innerWidth +' integ_w='+ integ_w +' time_display_width='+ time_display_width() +' left='+ left);
	el.style.left = px(left);
}

function tdoa_ui_reset()
{
	tdoa_set_icon('submit', -1, 'fa-refresh', 20, 'lightGrey');
   tdoa_submit_status(tdoa.READY_SAMPLE, '');

   w3_hide('id-tdoa-results-select');
   w3_hide('id-tdoa-results-options');
   w3_show('id-tdoa-gmap-kiwi');
   w3_hide('id-tdoa-gmap-result');
   w3_hide('id-tdoa-png');
}

function tdoa_set_icon(name, field_idx, icon, size, color)
{
   w3_innerHTML('id-tdoa-'+ name +'-icon-c'+ ((field_idx >= 0)? field_idx:''),
      w3_icon('id-tdoa-'+ name +'-icon'+ ((field_idx >= 0)? ('-'+ field_idx):''), icon, size, color)
   );
}

function tdoa_submit_status(state, msg)
{
   if (state == tdoa.ERROR) {
      tdoa_set_icon('submit', -1, 'fa-times-circle', 20, 'red');
   }
   w3_innerHTML('id-tdoa-submit-status', msg);
   tdoa.state = state;
}

function tdoa_host_field_cb(path, val, first)
{
   // launch status check
   var field_idx = +path.match(/\d+/).shift();
   //console.log('FIXME tdoa_host_field_cb path='+ path +' val='+ val +' field_idx='+ field_idx);
   w3_innerHTML('id-tdoa-sample-icon-c'+ field_idx, '');
   w3_innerHTML('id-tdoa-sample-status-'+ field_idx, '');
   if (tdoa.mkr[field_idx])
      tdoa_change_marker_style(tdoa.mkr[field_idx], 'red', 'white', 'cl-tdoa-gmap-host-label');
   tdoa.inuse[field_idx] = true;
   tdoa.good[field_idx] = true;

   if (val == '') return;
   tdoa_set_icon('sample', field_idx, 'fa-refresh fa-spin', 20, 'lightGrey');
   var s = (dbgUs && val == 'kiwisdr.jks.com:8073')? 'www:8073' : val;
      kiwi_ajax('http://'+ s +'/status', 'tdoa_sample_status_cb', field_idx, 5000);
}

function tdoa_clear_cb(path, val, first)
{
   if (tdoa.state == tdoa.RUNNING) return;

   var field_idx = +val;
   tdoa.inuse[field_idx] = false;
   tdoa.good[field_idx] = false;
   w3_set_value('tdoa.id_field-'+ field_idx, '');
   w3_set_value('tdoa.host_field-'+ field_idx, '');
   w3_innerHTML('id-tdoa-sample-icon-c'+ field_idx, '');
   w3_innerHTML('id-tdoa-sample-status-'+ field_idx, '');
   if (tdoa.mkr[field_idx])
      tdoa_change_marker_style(tdoa.mkr[field_idx], 'red', 'white', 'cl-tdoa-gmap-host-label');
}

function tdoa_known_location_cb(path, val, first)
{
   // manual entry invalidates the marker selection
   tdoa.known_location_idx = undefined;
}

function tdoa_sample_status_cb(obj, field_idx)
{
   var err;
   
   if (obj.response) {
      var arr = obj.response.split('\n');
      //console.log('tdoa_sample_status_cb field_idx='+ field_idx +' arr.len='+ arr.length);
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
            tdoa_change_marker_style(tdoa.mkr[field_idx], 'red', 'white', 'cl-tdoa-gmap-selected-label');
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
   tdoa.good[field_idx] = false;
}

function tdoa_submit_button_cb()
{
   tdoa_ui_reset();
   setTimeout(tdoa_submit_button_cb2, 500);
}

function tdoa_submit_button_cb2()
{
   //console.log('tdoa_submit_button_cb tdoa.state='+ tdoa.state);
   var i, j;

   //console.log('tdoa_submit_button_cb pbc='+ freq_passband_center() +' f='+ ext_get_freq() +' cf='+ ext_get_carrier_freq());
   
   tdoa.ngood = 0;
   for (i = 0; i < tdoa.tfields; i++) if (tdoa.good[i]) tdoa.ngood++;
   
   if (tdoa.ngood < 2) {
      tdoa_submit_status(tdoa.ERROR, 'at least 2 available hosts needed');
      return;
   }
   
   if (tdoa.gmap_kiwi.getZoom() < 3) {
      tdoa_submit_status(tdoa.ERROR, 'must be zoomed in further');
      return;
   }

   // extract from fields in case fields manually edited
   var h, p, id;
   var h_v = '&h=', p_v = '&p=', id_v = '&id=';
   var list_i = 0;
   tdoa.list = [];
   for (i = 0; i < tdoa.tfields; i++) {
      if (!tdoa.good[i]) continue;
      tdoa.list[list_i] = {};

      var a = w3_get_value('tdoa.host_field-'+ i);
      if (a == '') {
         tdoa.good[i] = false;
         continue;
      }
      a = a.split(':');
      if (a.length != 2 || a[0] == '' || a[1] == '') {
         tdoa.good[i] = false;
         continue;
      }

      if (list_i) h_v += ',', p_v += ',', id_v += ',';
      h =  a[0];
      tdoa.list[list_i].h = h;
      h_v += h;
      p =  a[1];
      tdoa.list[list_i].p = p;
      p_v += p;
      id = w3_get_value('tdoa.id_field-'+ i);
      if (id == '') {
         id = 'id_'+ i;
         w3_set_value('tdoa.id_field-'+ i, id);
      }
      id = id.replace(/[^0-9A-Za-z\-\/]/g, '');    // limit charset because id used in a filename
      tdoa.list[list_i].call = id;
      id = id.replace(/\//g, '-');     // encode slashes
      tdoa.list[list_i].id = id;
      id_v += id;

      tdoa.list[list_i].field_idx = i;
      //console.log('list_i='+ list_i);
      //console.log(tdoa.list[list_i]);
      list_i++;
   }

   if (list_i < 2) {
      tdoa_submit_status(tdoa.ERROR, 'at least 2 available hosts needed');
      return;
   }
   
   var s = h_v + p_v + id_v;
   s += '&f='+ (ext_get_passband_center_freq()/1e3).toFixed(2);
   var pb = ext_get_passband();
   s += '&w='+ pb.high.toFixed(0);

   var b = tdoa.gmap_kiwi.getBounds();
   var ne = b.getNorthEast();
   var sw = b.getSouthWest();
   var lat_n = Math.round(ne.lat()+1);
   var lat_s = Math.round(sw.lat()-1);
   var lon_e = Math.round(ne.lng()+1);
   var lon_w = Math.round(sw.lng()-1);
   //fixme var shrink = (lon_e - lon_w) / 6;

   //s += "&pi=struct('lat',\\["+ lat_s +":0.05:"+ lat_n +"\\],'lon',\\["+ lon_w +":0.05:"+ lon_e +"\\]";
   s += "&pi=struct('lat_range',\\["+ lat_s +","+ lat_n +"\\],'lon_range',\\["+ lon_w +","+ lon_e +"\\],'plot_kiwi_json',true";
   
   var r = w3_get_value('id-tdoa-known-location');
   if (r && r != '') {
      r = r.replace(/\s+/g, ' ').split(/[ ,]/g);
      if (r.length >= 2) {
         var lat = parseFloat(r[0]), lon = parseFloat(r[1]);
         if (!isNaN(lat) && !isNaN(lon)) {
            var name = r[2].replace(/[^0-9A-Za-z]/g, '');   // e.g. can't have spaces in names at the moment
            s += ",'known_location',struct('coord',\\["+ lat +","+ lon +"\\],'name',"+ sq(name) +")";
         }
      }
   }
   
   s += ")";

   console.log(s);
   ext_send('SET sample '+ encodeURIComponent(s));

   for (i = 0; i < tdoa.tfields; i++) {
      if (!tdoa.good[i]) continue
      tdoa_set_icon('sample', i, 'fa-refresh fa-spin', 20, 'lime');
      w3_innerHTML('id-tdoa-sample-status-'+ i, 'sampling started');
   }
   
   w3_color('id-tdoa-submit-icon', 'lime');
   tdoa.gmap_zoom = tdoa.gmap_kiwi.getZoom();
   tdoa.gmap_center = tdoa.gmap_kiwi.getCenter();
   tdoa_submit_status(tdoa.RUNNING, 'sampling started');
}

function tdoa_result_select_cb(path, idx, first)
{
   idx = +idx;
   tdoa.select = idx;
   var new_maps = w3_el('id-tdoa-results-newmaps').checked;
   //console.log('tdoa_result_select_cb idx='+ idx +' new_maps='+ new_maps);

   if (idx == 0) {
      w3_show('id-tdoa-gmap-kiwi');
      w3_hide('id-tdoa-gmap-result');
      w3_hide('id-tdoa-png');
   } else {
      var npairs = tdoa.ngood * (tdoa.ngood-1) / 2;
      if (new_maps && idx < (tdoa.nfixed_maps + npairs)) {
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
               mapTypeId: google.maps.MapTypeId.SATELLITE
            });
         var grid = new Graticule(tdoa.gmap_result, false);
         w3_show('id-tdoa-gmap-result');
         
         var ms, me;
         if (new_maps && idx == 2) {
            var npairs = tdoa.ngood * (tdoa.ngood-1) / 2;
            ms = tdoa.nfixed_maps; me = tdoa.nfixed_maps + npairs;
         } else {
            ms = idx; me = ms+1;
         }
         //console.log('SET ms/me='+ ms +'/'+ me +' idx='+ idx);
         
         for (var mi = ms; mi < me; mi++) {
            var fn = 'http://kiwisdr.com/tdoa/files/'+ tdoa.key +'/'+ tdoa.results_file2[mi] +'_contour_for_map.json';
            //console.log(fn);
            kiwi_ajax(fn, function(j, midx) {
               //console.log(j);

               tdoa.overlay[midx] = new google.maps.GroundOverlay(
                  'http://kiwisdr.com/tdoa/files/'+ j.filename,
                  j.imgBounds,
                  {opacity : 0.5});
               //console.log('SET midx='+ midx +'/'+ ms +'/'+ me +' ovl='+ tdoa.overlay[midx] +' overlay_visible='+ tdoa.overlay_visible);
               tdoa.overlay[midx].setMap(tdoa.overlay_visible? tdoa.gmap_result : null);

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
         
         if (tdoa.known_location_idx != undefined) {
            // use most recently selected marker, if any
            tdoa_place_ref_marker(tdoa.known_location_idx, tdoa.gmap_result);
         } else {
            // cons up marker from manually entered lat/lon
            var r = w3_get_value('id-tdoa-known-location');
            if (r && r != '') {
               r = r.replace(/\s+/g, ' ').split(/[ ,]/g);
               if (r.length >= 2) {
                  var lat = parseFloat(r[0]), lon = parseFloat(r[1]);
                  if (!isNaN(lat) && !isNaN(lon)) {
                     var ref = tdoa.refs[0];
                     ref.id = r[2].replace(/[^0-9A-Za-z]/g, '');   // e.g. can't have spaces in names at the moment
                     ref.lat = lat; ref.lon = lon;
                     r.shift(); r.shift(); r.shift();
                     ref.t = r.join(' ');
                     tdoa_place_ref_marker(0, tdoa.gmap_result);
                  }
               }
            }
         }
      } else {
         w3_hide('id-tdoa-gmap-kiwi');
         w3_hide('id-tdoa-gmap-result');
         var s;
         if (idx == 2)
            s = w3_div('w3-text-yellow', 'Available only for new map option');
         else
            s = '<img src="http://kiwisdr.com/tdoa/files/'+ tdoa.key +'/'+ tdoa.results_file[idx] +'.png" />';
         w3_innerHTML('id-tdoa-png', s);
         w3_show('id-tdoa-png');
      }
   }
}

function tdoa_overlay_visable_cb(path, checked)
{
   tdoa.overlay_visible = checked;
   //console.log('tdoa_overlay_visable_cb overlay_visible='+ tdoa.overlay_visible +' menu_select='+ tdoa.select);
   if (!tdoa.select) return;
   
   var new_maps = w3_el('id-tdoa-results-newmaps').checked;
   var ms, me;
   if (new_maps && tdoa.select == 2) {
      var npairs = tdoa.ngood * (tdoa.ngood-1) / 2;
      ms = tdoa.nfixed_maps; me = tdoa.nfixed_maps + npairs;
   } else {
      ms = tdoa.select; me = ms+1;
   }
   
   for (var mi = ms; mi < me; mi++) {
      var overlay = tdoa.overlay[mi];
      //console.log('tdoa_overlay_visable_cb mi='+ mi +' ovl='+ overlay);
      if (overlay) overlay.setMap(tdoa.overlay_visible? tdoa.gmap_result : null);
   }
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
         w3_text('w3-text-aqua', '<b>Beta test version. Improvements are under development.</b>') +
         '<br>See the <a href="http://valentfx.com/vanilla/categories/kiwisdr-tdoa-topics" target="_blank">Kiwi forum</a> for more information. ' +
         'If you are getting errors check these <br> common problems:<ul>' +
         '<li>Not zoomed-in far enough. The TDoA process will run out of memory or have problems plotting the maps.</li>' +
         '<li>Not all Kiwis used for sampling have good reception of target signal. ' +
         'Open a connection to each Kiwi by double clicking on its marker to check the reception.</li>' +
         '<li>Don\'t use Kiwis that are spaced too far apart (i.e. a few thousand km).</li>' +
         '<li>Use minimum IQ-mode passband. Just enough to capture the signal. ' +
         'While holding the shift key click the "+" and "-" magnifying glass icons to adjust the passband.</li>' +
         '</ul>' +

         'Once you have configured this extension, and click the "Submit" button, ' +
         'information is sent to the kiwisdr.com server. The server then records ' +
         '30 seconds of IQ data from the two to six sampling Kiwis specified. ' +
         'The frequency and passband of <i>this</i> Kiwi will be used for all recording. ' +
         'So make sure it is set correctly before proceeding (always use IQ mode and minimum necessary passband). ' +
         'After sampling, the TDoA process will be run on the server. After it finishes a menu ' +
         'will appear with selections for the various TDoA result maps produced. ' +
         'You can pan and zoom the resulting maps and click submit again after making any changes.<br><br>' +
         
         'To begin zoom into the general area of interest on the Google map (note the "quick zoom" menu). ' +
         'Click on the desired blue Kiwi sampling stations. If they are not responding or have ' +
         'had no recent GPS solutions an error message will appear. ' +
         '<b>Important:</b> the position and zooming of the Google map determines the same for the resulting TDoA maps. ' +
         'Double click on the blue markers to open that Kiwi in a new tab to check if it is receiving the target signal well. ' +
         'You can also manually edit the sampling station list (white fields). ' +
         '' +
         '' +
         '<br><br>' +
         
         'You can click on the white map markers to set the frequency/passband of some well-known LF/VLF stations. ' +
         'The known locations of these stations will be shown in the result map ' +
         'so you can see how well it agrees with the TDoA solution. ' +
         'Practice with these as their ground-wave signals usually give good results. ' +
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

function TDoA_blur()
{
	ext_set_data_height();     // restore default height
}
