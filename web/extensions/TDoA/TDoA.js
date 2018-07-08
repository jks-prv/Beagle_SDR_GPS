// Copyright (c) 2017 John Seamons, ZL/KF6VO

var tdoa = {
   ext_name: 'TDoA',    // NB: must match tdoa.cpp:tdoa_ext.name
   first_time: true,
   w_data: 1024,
   h_data: 465,
   
   tfields: 6,
   field: [],
   ngood: 0,
   good: [],
   list: [],
   
   gmap: null,
   
   hosts: [
      { h:'kiwisdr.jks.com', p:8073, id:'ZL/KF6VO', lat:-37.63, lon:176.17 },
      { h:'sielsdr.ddns.net', p:8073, id:'DF0KL', lat:53.66, lon:7.28 },
      { h:'f1jeksdr1.ddns.net', p:8073, id:'F1JEK', lat:45.78, lon:0.61 },
      { h:'hb9ryz.homeip.net', p:8073, id:'HB9RYZ', lat:47.17, lon:8.43 },
      { h:'hb9ibe.internet-box.ch', p:8073, id:'HB9IBE', lat:46.46, lon:6.23 },
      { h:'southwest.ddns.net', p:8073, id:'G8JNJ', lat:50.95, lon:-2.75 },      // slow inet?
      { h:'kiwi-sdr1-leiden.impactam.nl', p:8073, id:'JO22fd', lat:52.12, lon:4.57 },  // low GPS sats
      { h:'85.191.71.157', p:8073, id:'OZ1AEF', lat:56.08, lon:9.99 },
      { h:'83.162.220.82', p:8073, id:'PA0RDT', lat:51.5, lon:3.6 },    // low GPS sensitivity?
      { h:'emeraldsdr.ddns.net', p:8073, id:'IO62nu', lat:52.87, lon:-6.86 },
      { h:'on5kq.ddns.net', p:8073, id:'ON5KQ', lat:50.7862, lon:3.1944 },
   ],
   
   refs: [
      { id:'NWC', t:'MSK', f:19.8, p:400, z:11, lat:-21.8163, lon:114.1656 },
      { id:'ICV', t:'MSK', f:20.27, p:400, z:11, lat:40.9230, lon:9.7315 },
      { id:'SainteAssise', t:'MSK', f:20.9, p:400, z:11, lat:48.5450, lon:2.5763 },
      { id:'NPM', t:'MSK', f:21.4, p:400, z:11, lat:21.4201, lon:-158.1509 },
      { id:'DHO38', t:'MSK', f:23.4, p:400, z:11, lat:53.0816, lon:7.6163 },
      { id:'NAA', t:'MSK', f:24.0, p:400, z:11, lat:44.6464, lon:-67.2811 },
      { id:'NLK', t:'MSK', f:24.8, p:400, z:11, lat:48.2036, lon:-121.9171 },
      { id:'NLM4', t:'MSK', f:25.2, p:400, z:11, lat:46.3660, lon:-98.3355 },
      { id:'TBB', t:'FSK', f:26.7, p:100, z:12, lat:37.4092, lon:27.3242 },
      { id:'Negev', t:'FSK', f:29.7, p:100, z:12, lat:30.9686, lon:35.0970 },
      { id:'TFK/NRK', t:'MSK', f:37.5, p:400, z:12, lat:63.8504, lon:-22.4667 },
      { id:'NAU', t:'MSK', f:40.75, p:400, z:11, lat:18.3987, lon:-67.1774 },
      { id:'NSY', t:'MSK', f:45.9, p:400, z:11, lat:37.1256, lon:14.4363 },
      { id:'Kerlouan', t:'MSK', f:62.6, p:400, z:11, lat:48.6377, lon:-4.3507 },
      { id:'DCF77', t:'time station', f:77.5, p:1000, z:10, lat:50.0152, lon:9.0112 },
      { id:'Anthorn', t:'Loran-C nav', f:100, p:10000, z:8, lat:54.9117, lon:-3.2785 },
      { id:'TDF', t:'time station', f:162, p:200, z:10, lat:47.1695, lon:2.2046 },

      //{ id:'Rosnay', t:'MSK', lat:46.7130, lon:1.2454 },
      //{ id:'LaRegine', t:'MSK', lat:43.3868, lon:2.0975 },
   ],
   known_location_idx: null,
   
   quick_zoom: [
      { n:'Europe', lat:52, lon:10, z:4 },
      { n:'N.Amer', lat:40, lon:-90, z:3 },
      { n:'S.Amer', lat:-20, lon:-50, z:3 },
      { n:'Asia', lat:40, lon:120, z:3 },
      { n:'Japan', lat:40, lon:140, z:4 },
      { n:'Aus/NZ', lat:-30, lon:150, z:3 },
   ],
   
   sample_status: [
      'sampling complete', 'connection failed', 'all channels in use', 'no recent GPS timestamps'
   ],
   
   submit_status: [
      'TDoA complete', 'Octave error', 'sampling error', 'no GPS timestamps', 'no recent GPS timestamps'
   ],
   
   READY: 0, RUNNING: 1, ERROR: 2, RESULT: 3,
   state: 0,
   key: '',
   results: [],
   results_file: [],
   select: -1,
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
				tdoa.key = param[1];
				//console.log('tdoa_recv: key='+ tdoa.key);
				break;

         // called on submit after sampling completes
			case "sample_status":
				var status = parseInt(param[1]);
				console.log('tdoa_recv: sample_status='+ status.toHex());
				var error = false;
				
            for (var i = 0; i < tdoa.ngood; i++) {
               var field_idx = tdoa.list[i].field_idx;
               var stat = (status >> (i*2)) & 3;
               console.log('field #'+ i +' stat='+ stat +' field_idx='+ field_idx);
               console.log('field #'+ i +' h='+ tdoa.list[i].h);
               if (stat) {
                  tdoa_set_icon('sample', field_idx, 'fa-times-circle', 20, 'red');
                  error = true;
               } else {
                  tdoa_set_icon('sample', field_idx, 'fa-check-circle', 20, 'lime');
               }
               w3_innerHTML('id-tdoa-sample-status-'+ field_idx, tdoa.sample_status[stat]);
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
				   tdoa_submit_status(tdoa.ERROR, tdoa.submit_status[status]);
				   break;
				} else {
               tdoa_set_icon('submit', -1, 'fa-check-circle', 20, 'lime');
               var i, j;
               tdoa.results = [];
               tdoa.results[0] = 'Kiwi map';
               tdoa.results[1] = 'TDoA map';
               tdoa.results_file = [];
               tdoa.results_file[0] = 'Kiwi map';
               tdoa.results_file[1] = 'TDoA map';
               var ri = 2;
               for (i = 0; i < tdoa.ngood; i++) {
                  for (j = i+1; j < tdoa.ngood; j++) {
                     //console.log('i:'+ i +'='+ tdoa.list[i].id +' j:'+ j +'='+ tdoa.list[j].id);
                     tdoa.results[ri] = tdoa.list[i].call +'-'+ tdoa.list[j].call +' map';
                     tdoa.results_file[ri] = tdoa.list[i].id +'-'+ tdoa.list[j].id +' map';
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
				   tdoa_submit_status(tdoa.RESULT, tdoa.submit_status[0]);
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
   
   var hosts = tdoa.hosts;
   for (i = 0; i < hosts.length; i++) {
      hosts[i].hp = hosts[i].h +':'+ hosts[i].p;
   }

	var data_html =
      time_display_html('tdoa') +
      
      w3_div('id-tdoa-data w3-display-container',
         w3_div('id-tdoa-gmap|left:0px; width:'+ px(tdoa.w_data) +'; height:'+ px(tdoa.h_data), ''),
         w3_div('id-tdoa-png w3-display-topleft w3-scroll-y w3-hide|left:0px; width:'+ px(tdoa.w_data) +'; height:'+ px(tdoa.h_data), '')
      );
   
	var control_html =
	   w3_divs('w3-tspace-8',
		   w3_div('id-tdoa-controls w3-text-aqua', '<b>Time difference of arrival (TDoA) direction finding service</b>'),
		   //w3_div('w3-text-white', 'It is essential to read the instructions (help button) before using this tool'),
		   w3_div('id-tdoa-hosts-container'),
		   w3_inline('',
            w3_button('id-tdoa-submit-button w3-padding-smaller w3-css-yellow', 'Submit', 'tdoa_submit_cb'),
		      w3_div('id-tdoa-submit-icon-c w3-margin-left'),
            w3_div('id-tdoa-results-select w3-margin-left w3-hide'),
            w3_div('id-tdoa-submit-status w3-margin-left')
         ),
		   w3_inline('',
            w3_select('w3-text-red', '', 'quick zoom', 'tdoa.quick_zoom', -1, tdoa.quick_zoom, 'tdoa_quick_zoom_cb'),
            w3_div('id-tdoa-known-location w3-margin-left w3-text-white')
         )
      );

	ext_panel_show(control_html, data_html, null);
	time_display_setup('tdoa');

	ext_set_controls_width_height(600, 265);
   ext_set_data_height(tdoa.h_data);

   var s = '';
	for (i = 0; i < tdoa.tfields; i++) {
	   s +=
	      w3_inline(i? 'w3-margin-T-3':'',
            w3_input('w3-padding-smaller||size=8', '', 'tdoa.id_field-'+ i, ''),
            w3_input('w3-margin-L-3 w3-padding-smaller||size=32', '', 'tdoa.host_field-'+ i, '', 'tdoa_host_field_cb'),
            w3_icon('w3-margin-L-8 id-tdoa-clear-icon-'+ i, 'fa-cut', 20, 'red', 'tdoa_clear_cb', i),
		      w3_div('w3-margin-L-8 id-tdoa-sample-icon-c'+ i),
		      w3_div('w3-margin-L-8 id-tdoa-sample-status-'+ i)
         );
   }
   w3_innerHTML('id-tdoa-hosts-container', s);
   
   var latlon = new google.maps.LatLng(20, 0);
   var map_div = w3_el('id-tdoa-gmap');

   tdoa.gmap = new google.maps.Map(map_div,
      {
         zoom: 2,
         center: latlon,
         navigationControl: false,
         mapTypeControl: false,
         streetViewControl: false,
         draggable: true,
         gestureHandling: 'greedy',
         mapTypeId: google.maps.MapTypeId.SATELLITE
      });
   
   var day_night = new DayNightOverlay( { map:tdoa.gmap, fillColor:'rgba(0,0,0,0.4)' } );
   var grid = new Graticule(tdoa.gmap, false);

   //tdoa_place_map_marker(tdoa.hosts);
   
   for (i = 0; i < tdoa.refs.length; i++) {
      var r = tdoa.refs[i];
      var latlon = new google.maps.LatLng(r.lat, r.lon);
      var marker = new MarkerWithLabel({
         position: latlon,
         draggable: false,
         raiseOnDrag: false,
         map: tdoa.gmap,
         labelContent: r.id,
         labelAnchor: new google.maps.Point(r.id.length*3.6, 36),
         labelClass: "cl-tdoa-gmap-ref-label",
         labelStyle: {opacity: 1.0},
         title: 'Known location: '+ r.t + (r.f? (', '+ r.f +' kHz'):''),
         icon: kiwi_mapPinSymbol('lime', 'black')
      });
      marker.refs_idx = i;
      google.maps.event.addListener(marker, "click", function() {
         //console.log('z='+ tdoa.gmap.getZoom());
         var r = tdoa.refs[this.refs_idx];
         w3_innerHTML('id-tdoa-known-location',
            'Display known location: '+ r.id +', '+ r.t + (r.f? (', '+ r.f +' kHz'):'')
         );
         tdoa.known_location_idx = this.refs_idx;
         if (r.f)
            ext_tune(r.f, 'iq', ext_zoom.ABS, r.z, -r.p/2, r.p/2);
      });
   }
   
   //ext_set_mode('iq');
   kiwi_ajax('http://kiwisdr.com/tdoa/files/kiwi.gps.json', 'tdoa_get_hosts_cb');

   tdoa_ui_reset();
	TDoA_environment_changed( {resize:1} );
   tdoa.state = tdoa.READY;
}

function tdoa_place_map_marker(hosts)
{
   //console.log('tdoa_place_map_marker');
   //console.log(hosts);

   for (i = 0; i < hosts.length; i++) {
      var h = hosts[i];
      //console.log(h);
      hosts[i].hp = h.h +':'+ h.p;
      hosts[i].call = h.id.replace(/\-/g, '/');    // decode slashes

      var latlon = new google.maps.LatLng(h.lat, h.lon);
      var marker = new MarkerWithLabel({
         position: latlon,
         draggable: false,
         raiseOnDrag: false,
         map: tdoa.gmap,
         labelContent: h.call,
         labelAnchor: new google.maps.Point(h.call.length*3.6, 36),
         labelClass: "cl-tdoa-gmap-host-label",
         labelStyle: {opacity: 1.0},
         title: h.n +'\n'+ h.fm +' GPS fixes/min',
         icon: kiwi_mapPinSymbol('red', 'white')
      });
      marker.hosts_idx = i;

      google.maps.event.addListener(marker, "click", function() {
         //console.log(this);
         
         if (tdoa.click_pending === true) return;
         tdoa.click_pending = true;
         tdoa.click_hosts_idx = this.hosts_idx;

         tdoa.click_timeout = setTimeout(function() {
            tdoa.click_pending = false;

            for (var j = 0; j < tdoa.tfields; j++) {
               if (w3_get_value('tdoa.id_field-'+ j) == hosts[tdoa.click_hosts_idx].call) return;  // already in list
            }
            
            var icon = 'id-tdoa-sample-icon-';
            for (var j = 0; j < tdoa.tfields; j++) {
               if (tdoa.field[j] == undefined) {      // put in first empty field
                  var i = tdoa.click_hosts_idx;
                  var h = hosts[i];
                  var host = h.hp;
                  tdoa.field[j] = true;
                  tdoa.good[j] = true;
                  w3_set_value('tdoa.id_field-'+ j, h.call);
                  w3_set_value('tdoa.host_field-'+ j, host);
                  tdoa_set_icon('sample', j, 'fa-refresh', 20, 'lightGrey');
   
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

function tdoa_get_hosts_cb(obj, param)
{
   var err;
   
   if (!obj) {
      console.log('tdoa_get_hosts_cb obj:');
      console.log(obj);
      return;
   }
   
   //console.log(obj);
   tdoa_place_map_marker(obj);
}

function TDoA_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-tdoa-data');
	var left = (window.innerWidth - integ_w - time_display_width()) / 2;
	//console.log('tdoa_resize wiw='+ window.innerWidth +' integ_w='+ integ_w +' time_display_width='+ time_display_width() +' left='+ left);
	el.style.left = px(left);
}

function tdoa_ui_reset()
{
	tdoa_set_icon('submit', -1, 'fa-refresh', 20, 'lightGrey');
   tdoa_submit_status(tdoa.READY, '');

   w3_hide('id-tdoa-results-select');
   w3_show('id-tdoa-gmap');
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
   tdoa_set_icon('sample', field_idx, 'fa-refresh', 20, 'lightGrey');
   tdoa.field[field_idx] = true;
   tdoa.good[field_idx] = true;
   var s = (dbgUs && val == 'kiwisdr.jks.com:8073')? 'www:8073' : val;
      kiwi_ajax('http://'+ s +'/status', 'tdoa_sample_status_cb', field_idx, 5000);
}

function tdoa_clear_cb(path, val, first)
{
   if (tdoa.state == tdoa.RUNNING) return;
   var field_idx = +val;
   tdoa.field[field_idx] = undefined;
   tdoa.good[field_idx] = false;
   w3_set_value('tdoa.id_field-'+ field_idx, '');
   w3_set_value('tdoa.host_field-'+ field_idx, '');
   w3_innerHTML('id-tdoa-sample-icon-c'+ field_idx, '');
   w3_innerHTML('id-tdoa-sample-status-'+ field_idx, '');
}

function tdoa_sample_status_cb(obj, field_idx)
{
   var err;
   
   if (obj.response) {
      var arr = obj.response.split('\n');
      console.log('tdoa_sample_status_cb field_idx='+ field_idx +' arr.len='+ arr.length);
      console.log(arr);
      var users, users_max = fixes_min = null;
      for (var i = 0; i < arr.length; i++) {
         var a = arr[i];
         if (a.startsWith('users=')) users = a.split('=')[1];
         if (a.startsWith('users_max=')) users_max = a.split('=')[1];
         if (a.startsWith('fixes_min=')) fixes_min = a.split('=')[1];
      }
      console.log('u='+ users +' u_max='+ users_max);
      if (users_max == null) return;      // unexpected response
      if (users < users_max) {
         w3_color('id-tdoa-sample-icon-'+ field_idx, 'lime');
         var s = fixes_min? (fixes_min +' GPS fixes/min') : 'channel available';
         w3_innerHTML('id-tdoa-sample-status-'+ field_idx, s);
         return;
      }
      err = 'all channels in use';
   } else {
      console.log(obj);
      err = 'no response';
   }

   w3_color('id-tdoa-sample-icon-'+ field_idx, 'red');
   w3_innerHTML('id-tdoa-sample-status-'+ field_idx, err);
   tdoa.good[field_idx] = false;
}

function tdoa_submit_cb()
{
   tdoa_ui_reset();
   setTimeout(tdoa_submit_cb2, 500);
}

function tdoa_submit_cb2()
{
   //console.log('tdoa_submit_cb tdoa.state='+ tdoa.state);
   var i, j;

   //console.log('tdoa_submit_cb pbc='+ freq_passband_center() +' f='+ ext_get_freq() +' cf='+ ext_get_carrier_freq());
   
   tdoa.ngood = 0;
   for (i = 0; i < tdoa.tfields; i++) if (tdoa.good[i]) tdoa.ngood++;
   
   if (tdoa.ngood < 2) {
      tdoa_submit_status(tdoa.ERROR, 'at least 2 available hosts needed');
      return;
   }
   
   if (tdoa.gmap.getZoom() < 4) {
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

      var a = w3_get_value('tdoa.host_field-'+ i).split(':');
      if (list_i) h_v += ',', p_v += ',', id_v += ',';
      h =  a[0];
      //h = 'kiwisdr.jks.com';      // fixme
      tdoa.list[list_i].h = h;
      h_v += h;
      p =  a[1];
      //p = 8073;
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
      console.log('list_i='+ list_i);
      console.log(tdoa.list[list_i]);
      list_i++;
   }

   var s = h_v + p_v + id_v;
   s += '&f='+ (ext_get_passband_center_freq()/1e3).toFixed(2);
   var pb = ext_get_passband();
   s += '&w='+ pb.high.toFixed(0);

   var b = tdoa.gmap.getBounds();
   var ne = b.getNorthEast();
   var sw = b.getSouthWest();
   var lat_n = Math.round(ne.lat()+1);
   var lat_s = Math.round(sw.lat()-1);
   var lon_e = Math.round(ne.lng()+1);
   var lon_w = Math.round(sw.lng()-1);
   //fixme var shrink = (lon_e - lon_w) / 6;

   s += "&pi=struct('lat',\\["+ lat_s +":0.05:"+ lat_n +"\\],'lon',\\["+ lon_w +":0.05:"+ lon_e +"\\]";
   
   if (tdoa.known_location_idx != null) {
      var r = tdoa.refs[tdoa.known_location_idx];
      var name = r.id.replace(/[^0-9A-Za-z]/g, '');   // e.g. can't have spaces in names at the moment
      s += ",'known_location',struct('coord',\\["+ r.lat +","+ r.lon +"\\],'name',"+ sq(name) +")";
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
   tdoa_submit_status(tdoa.RUNNING, 'sampling started');
}

function tdoa_result_select_cb(path, idx, first)
{
   idx = +idx;
   //console.log('tdoa_result_select_cb idx='+ idx);
   if (idx == 0) {
      w3_show('id-tdoa-gmap');
      w3_hide('id-tdoa-png');
   } else {
      w3_hide('id-tdoa-gmap');
      w3_innerHTML('id-tdoa-png',
         // FIXME: need to specify width, height?
         '<img src="http://kiwisdr.com/tdoa/files/'+ tdoa.key +'/'+ tdoa.results_file[idx] +'.png" />'
      );
      w3_show('id-tdoa-png');
   }
}

function tdoa_quick_zoom_cb(path, idx, first)
{
   if (first) return;
   var q = tdoa.quick_zoom[+idx];
   var latlon = new google.maps.LatLng(q.lat, q.lon);
   tdoa.gmap.panTo(latlon);
   tdoa.gmap.setZoom(q.z);
}

function TDoA_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-text-aqua', '<b>Beta test version. Improvements are under development.</b>') +
         '<br>More complete instruction on the kiwisdr.com website will follow.<br><br>' +
         
         'Once you have configured this extension, and click the "Submit" button, ' +
         'information is sent to the kiwisdr.com server. The server then records ' +
         '30 seconds of IQ data from the two to six sampling Kiwis specified. ' +
         'The frequency and passband of <i>this</i> Kiwi will be used for all recording. ' +
         'So make sure it is set correctly before proceeding (always use IQ mode). ' +
         'After sampling, the TDoA process will be run on the server. After it finishes a menu ' +
         'will appear with selections for the various TDoA result maps produced. ' +
         'They will be downloaded and appear in place of the Google map. ' +
         'You may then make changes and click submit again.<br><br>' +
         
         'Zoom into the area of interest on the Google map (note "quick zoom" menu). ' +
         'Click on the desired blue Kiwi sampling stations. If they are not responding or have ' +
         'had no recent GPS solutions an error message will appear. ' +
         '<b>Important:</b> the position and zooming of the Google map determines the same for the resulting TDoA maps. ' +
         'Double click on the blue markers to open that Kiwi in a new tab to check if it is receiving the target signal well. ' +
         'You can manually edit the sampling station list (white fields). ' +
         'All public Kiwis with GPS receive capability will soon automatically be listed. ' +
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
      confirmation_show_content(s, 600, 455);
   }
   return true;
}

function TDoA_blur()
{
	ext_set_data_height();     // restore default height
}
