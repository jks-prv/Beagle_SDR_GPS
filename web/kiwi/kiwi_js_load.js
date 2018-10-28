// KiwiSDR
//
// Copyright (c) 2018 John Seamons, ZL/KF6VO

console.log('kiwi_js_load.js RUNNING');

// see: feather.elektrum.org/book/src.html

// all current script elements
js_load_scripts = document.getElementsByTagName('script');

// search forwards for all instances of ourselves, remove script element, and call callback
for (var i = 0; i < js_load_scripts.length; i++) {
   var se = js_load_scripts[i];
   if (se.src.includes('kiwi/kiwi_js_load.js')) {
      //console.log(se);
      console.log('kiwi_js_load.js '+ se.src);
      if (se.kiwi_cb) {
         console.log(se.kiwi_cb);
         w3_call(se.kiwi_cb);
      } else {
         var queryString = se.src.replace(/^[^\?]+\??/,'');
         //console.log(queryString);
         var params = kiwi_parseQuery(queryString);
         //console.log(params);
         console.log('kiwi_js_load.js OLD');
         if (params.cb) w3_call(params.cb);
      }
      se.remove();
   }
}
