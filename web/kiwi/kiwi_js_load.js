// KiwiSDR
//
// Copyright (c) 2018 John Seamons, ZL/KF6VO

console.log('kiwi_js_load.js RUNNING');

// see: feather.elektrum.org/book/src.html

// all current script elements
js_load_scripts = document.getElementsByTagName('script');

// search backwards for the most recently added
//for (var i = js_load_scripts.length - 1; i >= 0; i--) {

// search forwards for the first one -- it will be removed afterwards
for (var i = 0; i < js_load_scripts.length; i++) {
   var se = js_load_scripts[i];
   if (se.src.includes('kiwi/kiwi_js_load.js')) {
      console.log(se);
      console.log(se.src);
      var queryString = se.src.replace(/^[^\?]+\??/,'');
      se.remove();
      //console.log(queryString);
      var params = kiwi_parseQuery(queryString);
      //console.log(params);
      if (params.cb) w3_call(params.cb);
   }
}
