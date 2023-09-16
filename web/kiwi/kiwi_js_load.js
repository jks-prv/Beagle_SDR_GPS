// KiwiSDR
//
// Copyright (c) 2018 John Seamons, ZL4VO/KF6VO

console.log('kiwi_js_load.js RUNNING');

// see: feather.elektrum.org/book/src.html

// all current script elements
js_load_scripts = document.getElementsByTagName('script');

// Search forward for first instance of ourselves, remove script element, and call callback.
// Other instances of ourselves may be further on in the list if there are multiple loads pending.

for (var i = 0; i < js_load_scripts.length; i++) {
   var se = js_load_scripts[i];
   //console.log('se '+ i +'/'+ js_load_scripts.length);
   //console.log(se);
   if (se.src.includes('kiwi/kiwi_js_load.js')) {
      //console.log('kiwi_js_load.js USE '+ i +'/'+ js_load_scripts.length +' '+ se.src);
      if (se.kiwi_cb) {
         console.log('kiwi_js_load.js CALLBACK '+ ifString(se.kiwi_cb, '(func)') +' for '+ se.kiwi_js);

         // NB: if se.kiwi_cb is function pointer this will incorrectly show parent function
         // instead of anonymous (nameless) function, which is confusing
         //console.log(se.kiwi_cb);
         w3_call(se.kiwi_cb);
      } else {
         var queryString = se.src.replace(/^[^\?]+\??/,'');
         //console.log(queryString);
         var params = kiwi_parseQuery(queryString);
         //console.log(params);
         console.log('kiwi_js_load.js (OLD) CALLBACK '+ ifString(params.cb, '(func)'));
         if (params.cb) w3_call(params.cb);
      }
      se.remove();
      break;
   }
   //else console.log('kiwi_js_load.js SKIP '+ i +'/'+ js_load_scripts.length +' '+ se.src);
}

console.log('kiwi_js_load.js DONE');
