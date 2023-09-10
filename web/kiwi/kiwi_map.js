
// Copyright (c) 2022 John Seamons, ZL4VO/KF6VO

var kmap = {
   ADD_TO_MAP: true,
   NO_ADD_TO_MAP: false,
   DIR_LEFT: true,
   DIR_RIGHT: false,
   VISIBLE: true,
   NOT_VISIBLE: false,

   _last_last: 0
};

function kiwi_map_init(ext_name, init_latlon, init_zoom, mapZoom_nom, move_cb, zoom_cb)
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

   var tiles = map_tiles('hybrid');
   var map = L.map('id-'+ ext_name +'-map',
      {
         maxZoom: maxZoom,
         minZoom: 1,
         doubleClickZoom: false,    // don't interfere with double-click of host/ref markers
         zoomControl: false
      }
   ).setView(init_latlon, init_zoom);
   
   // NB: hack! jog map slightly to prevent grey, unfilled tiles after basemap change
   map.on('baselayerchange', function(e) {
      var tmap = e.target;
      var center = tmap.getCenter();
      kiwi_pan_zoom(tmap, [center.lat + 0.1, center.lng], -1);
      kiwi_pan_zoom(tmap, [center.lat, center.lng], -1);
   });
   
   var kmap = { ext_name: ext_name, map: map };

   map.on('click', function(e, kmap) { w3_call(ext_name +'_map_click_cb', kmap, e); });
   map.on('move', function(e, kmap) { w3_call(ext_name +'_map_move_cb', kmap, e); });
   map.on('zoom', function(e, kmap) { w3_call(ext_name +'_map_zoom_cb', kmap, e); });
   map.on('moveend', function(e, kmap) { w3_call(ext_name +'_map_move_end_cb', kmap, e); });
   map.on('zoomend', function(e, kmap) { w3_call(ext_name +'_map_zoom_end_cb', kmap, e); });
   
   L.control.zoom_Kiwi( { position: 'topright', zoomNomText: '2', zoomNomLatLon: init_latlon } ).addTo(map);
   
   tiles.addTo(map);

   var scale = L.control.scale()
   scale.addTo(map);
   scale.setPosition('bottomleft');

   var day_night = new Terminator();
   day_night.setStyle({ fillOpacity: 0.35 });
   day_night.addTo(map);
   kmap.day_night_interval = setInterval(function() {
      var t2 = new Terminator();
      day_night.setLatLngs(t2.getLatLngs());
      day_night.redraw();
   }, 60000);
   day_night._path.style.cursor = 'grab';

   var graticule = L.latlngGraticule();
   graticule.addTo(map);

   kmap.map_layers = [ graticule ];
   kmap.graticule = graticule;
   kmap.day_night = day_night;
   kmap.init_latlon = init_latlon;
   kmap.init_zoom = init_zoom;
   kmap.mapZoom_nom = mapZoom_nom;
   
   return kmap;
}

function kiwi_map_blur(kmap)
{
   kiwi_clearInterval(kmap.day_night_interval);
}

////////////////////////////////
// map
////////////////////////////////

function kiwi_map_add_marker_icon(kmap, addToMap, url, latlon, anchor, opacity, func)
{
   var kiwi_icon =
      L.icon({
         iconUrl: url,
         iconAnchor: anchor
      });

   var kiwi_mkr = L.marker(latlon, { 'icon':kiwi_icon, 'opacity':opacity });
   //console.log(kiwi_mkr);
   if (addToMap) {
      kiwi_mkr.addTo(kmap.map);
      if (func) func(kiwi_mkr._icon);
   }
}

function kiwi_map_add_marker_div(kmap, addToMap, latlon, className, iconAnchor, tooltipAnchor, opacity)
{
   var icon =
      L.divIcon({
         className: className,
         html: '',
         iconAnchor: iconAnchor,
         tooltipAnchor: tooltipAnchor
      });
   var marker = L.marker(latlon, { 'icon':icon, 'opacity':opacity });
   if (addToMap) marker.addTo(kmap.map);
   return marker;
}

function kiwi_style_marker(kmap, addToMap, marker, name, className, left, func)
{
   marker.bindTooltip(name,
      {
         className:  className,
         permanent:  true, 
         direction:  left? 'left' : 'right'
      }
   );
   
   // Can only access element to add title via an indexed id.
   // But element only exists as marker emerges from cluster.
   // Fortunately we can use 'add' event to trigger insertion of title.
   //console.log('style');
   //console.log(marker);
   if (func) marker.on('add', function(ev) {
      func(ev);
   });
   
   // only add to map in cases where L.markerClusterGroup() is not used
   if (addToMap) {
      //console.log(marker);
      marker.addTo(kmap.map);
      kmap.map_layers.push(marker);
      //console.log('marker '+ name +' x,y='+ map.latLngToLayerPoint(marker.getLatLng()));
   }
}

function kiwi_map_day_night_visible(kmap, vis)
{
   var day_night = kmap.day_night;
   if (vis) {
      day_night.addTo(kmap.map);
      kmap.map_layers.push(day_night);
      day_night._path.style.cursor = 'grab';
   } else {
      day_night.remove();
      kmap.map_layers = kmap.map_layers.filter(function(ae) { ae != day_night });
   }
}

function kiwi_map_graticule_visible(kmap, vis)
{
   var graticule = kmap.graticule;
   if (vis) {
      graticule.addTo(kmap.map);
      kmap.map_layers.push(graticule);
   } else {
      graticule.remove();
      kmap.map_layers = kmap.map_layers.filter(function(ae) { ae != graticule });
   }
}

function kiwi_map_markers_visible(id, vis)
{
	w3_iterate_children('leaflet-marker-pane',
	   function(el, i) {
	      if (el.className.includes(id)) {
	         w3_hide2(el, !vis);
	      }
	   }
	);

	w3_iterate_children('leaflet-tooltip-pane',
	   function(el, i) {
	      if (el.className.includes(id)) {
	         w3_hide2(el, !vis);
	      }
	   }
	);
}

function kiwi_pan_zoom(map, latlon, zoom)
{
   //console.log('kiwi_pan_zoom z='+ zoom);
   //console.log(map);
   //console.log(latlon);
   //console.log(zoom);
   if (latlon == null) latlon = map.getCenter();
   if (zoom == -1) zoom = map.getZoom();
   map.setView(latlon, zoom, { duration: 0, animate: false });
}

/* jksx not referenced?
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
   if (r.f)
      ext_tune(r.f, 'iq', ext_zoom.ABS, r.z, -r.p/2, r.p/2);
   hfdl_update_link();
   hfdl_ref_marker_offset(true);
}
*/

////////////////////////////////
// end map
////////////////////////////////
