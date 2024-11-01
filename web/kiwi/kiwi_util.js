// KiwiSDR utilities
//
// Copyright (c) 2014-2024 John Seamons, ZL4VO/KF6VO


// isUndeclared(v) => use inline "if (window.v) ..." (i.e. can't pass undeclared v as func arg)
function isUndefined(v) { return (typeof(v) === 'undefined'); }
function isDefined(v) { return (typeof(v) !== 'undefined'); }
function isNull(v) { return (v === null); }
function isNumber(v) { return (typeof(v) === 'number' && !isNaN(v)); }
function isBoolean(v) { return (typeof(v) === 'boolean'); }
function isString(v) { return (typeof(v) === 'string'); }
function isNonEmptyString(v) { return (isString(v) && v !== ''); }
function isEmptyString(v) { return (!isString(v) || v == ''); }         // NB: !isString() ||
function isArray(v) { return (Array.isArray(v)); }
function isNonEmptyArray(v) { return (isArray(v) && v.length > 0); }
function isEmptyArray(v) { return (!isArray(v) || v.length == 0); }     // NB: !isString() ||
function isFunction(v) { return (typeof(v) === 'function'); }
function isObject(v) { return (typeof(v) === 'object'); }
function isArg(v) { return (isUndefined(v) || isNull(v))? false:true; }
function isNoArg(v) { return (isUndefined(v) || isNull(v))? true:false; }
function kiwi_typeof(v) { return isNull(v)? 'null' : (isArray(v)? 'array' : typeof(v)); }

function ifString(s, alt) { return (isString(s)? s : alt); }

// browsers have added includes() only relatively recently
try {
	if (!String.prototype.includes) {
		String.prototype.includes = function(str) {
			return (this.indexOf(str) >= 0);
		};
	}
} catch(ex) {
	console.log("kiwi_util: String.prototype.includes");
}

// stackoverflow.com/questions/646628/how-to-check-if-a-string-startswith-another-string
try {
	if (!String.prototype.startsWith) {
		String.prototype.startsWith = function(str) {
			return this.indexOf(str) == 0;
		};
	}
} catch(ex) {
	console.log("kiwi_util: String.prototype.startsWith");
}

// developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/endsWith
try {
   if (!String.prototype.endsWith) {
      String.prototype.endsWith = function(search, this_len) {
         if (this_len === undefined || this_len > this.length) {
            this_len = this.length;
         }
           return this.substring(this_len - search.length, this_len) === search;
      };
   }
} catch(ex) {
	console.log("kiwi_util: String.prototype.endsWith");
}

var kiwi_util = {
   last_time: '',
   
   _last_: null
};

var kiwi_iOS, kiwi_iPhone, kiwi_MacOS, kiwi_linux, kiwi_Windows, kiwi_android;
var kiwi_safari, kiwi_firefox, kiwi_chrome, kiwi_opera, kiwi_smartTV;
var kiwi_touch;

// wait until DOM has loaded before proceeding (browser has loaded HTML, but not necessarily images)
document.onreadystatechange = function() {
	//console.log("onreadystatechange="+document.readyState);
	//if (document.readyState == "interactive") {
	if (document.readyState == "complete") {
	   var website = false;
	   var el = w3_el('id-kiwi-container');
	   if (el && el.getAttribute('data-type') == 'website') website = true;
		var s = navigator.userAgent;
		if (!website) console.log(s);
		//alert(s);
		kiwi_iOS = (s.includes('iPhone') || s.includes('iPad'));
		kiwi_iPhone = s.includes('iPhone');
		kiwi_MacOS = s.includes('OS X');
		kiwi_linux = s.includes('Linux');
		kiwi_Windows = s.includes('Win');
		kiwi_android = s.includes('Android');

		kiwi_safari = /safari\/([0-9]+)/i.exec(s);
		kiwi_browserVersion = /version\/([0-9]+)/i.exec(s);
		kiwi_firefox = /firefox\/([0-9]+)/i.exec(s);
		kiwi_chrome = /chrome\/([0-9]+)/i.exec(s);

		kiwi_opera = /opera\/([0-9]+)/i.exec(s);
		if (!kiwi_opera) kiwi_opera = /OPR\/([0-9]+)/i.exec(s);

		kiwi_smartTV = s.includes('SmartTV');
		if (!kiwi_smartTV) kiwi_smartTV = s.includes('SMART-TV');
		if (kiwi_smartTV) {
		   if (s.includes('Samsung')) kiwi_smartTV = 'Samsung';
		   else
		   if (s.includes('LG')) kiwi_smartTV = 'LG';
		}
		
		// It seems iPads no longer return "iPad" in the user agent (iPhones still return "iPhone").
		// So use more generic (and recommended) way of determining touch/mobile devices.
		// see: developer.mozilla.org/en-US/docs/Web/HTTP/Browser_detection_using_the_user_agent#mobile_device_detection
      // NB: don't use "'maxTouchPoints' in navigator" because very old iPads don't support "in" keyword.
      if (navigator.hasOwnProperty('maxTouchPoints')) {
         kiwi_touch = (navigator.maxTouchPoints > 0);
      } else
      if (navigator.hasOwnProperty('msMaxTouchPoints')) {
         kiwi_touch = (navigator.msMaxTouchPoints > 0);
      } else
      if (window.hasOwnProperty('matchMedia')) {
         var mQ = matchMedia('(pointer: coarse)');
         if (mQ.media === '(pointer: coarse)') {
            kiwi_touch = !!mQ.matches;
         } else {
            kiwi_touch = false;
         }
      } else {
         if (window.hasOwnProperty('orientation')) {
            kiwi_touch = true;   // deprecated, but good fallback
         } else {
            kiwi_touch = false;
         }
      }
		
		if (!website) console.log(
		   'MacOS='+ kiwi_MacOS +
		   ' Linux='+ kiwi_linux +
		   ' Windows='+ kiwi_Windows +
		   ' iOS='+ kiwi_iOS +
		   ' iPhone='+ kiwi_iPhone +
		   ' Android='+ kiwi_android +
		   ' SmartTV='+ kiwi_smartTV);
		
		// madness..
		if (kiwi_opera) {
		   kiwi_chrome = kiwi_safari = null;
		} else
		if (kiwi_chrome) {
		   kiwi_safari = null;
		} else
		if (kiwi_safari) {
		   if (kiwi_browserVersion) kiwi_safari[1] = kiwi_browserVersion[1];
		}

		if (!website) console.log('Safari='+ kiwi_isSafari() + ' Firefox='+ kiwi_isFirefox() + ' Chrome='+ kiwi_isChrome() + ' Opera='+ kiwi_isOpera());

      // packages to load early on
      kiwi.noLocalStorage = (isUndefined(localStorage) || localStorage == null)? true:false;
      if (!website) {
         migrateCookies();
         
         kiwi_load_js_dir('pkgs/js/', ['sprintf/sprintf.js', 'SHA256.js', 'coloris/coloris.js'],
            function() {
               //console.log(sprintf('%s', 'sprintf loaded'));
               kiwi_load2();
            }
         );
      } else {
         kiwi_load2();
      }
	}
};

function kiwi_load2()
{
   if (typeof(kiwi_check_js_version) !== 'undefined') {
      // done as an AJAX because needed long before any websocket available
      kiwi_ajax("/VER", 'kiwi_version_cb');
   } else {
		kiwi.conn_tstamp = (new Date()).getTime();   // fallback
      if (typeof(kiwi_bodyonload) !== 'undefined')
         kiwi_bodyonload('');
   }
}

function kiwi_is_iOS() { return kiwi_iOS; }

function kiwi_is_iPhone() { return kiwi_iPhone; }

function kiwi_isMacOS() { return kiwi_MacOS; }

function kiwi_isWindows() { return kiwi_Windows; }

function kiwi_isLinux() { return kiwi_linux; }

function kiwi_isAndroid() { return kiwi_android; }

function kiwi_isSmartTV() { return kiwi_smartTV; }

function kiwi_isTouch() { return kiwi_touch; }

function kiwi_isMobile() { return (force_mobile || kiwi_isTouch() || kiwi_is_iOS() || kiwi_isAndroid() || (kiwi_isSmartTV() == 'Samsung') || mobile_laptop_test); }

// returns the version number or NaN (later of which will evaluate false in numeric comparisons)
function kiwi_isSafari() { return (kiwi_safari? kiwi_safari[1] : NaN); }

function kiwi_isFirefox() { return (kiwi_firefox? kiwi_firefox[1] : NaN); }

function kiwi_isChrome() { return (kiwi_chrome? kiwi_chrome[1] : NaN); }

function kiwi_isOpera() { return (kiwi_opera? kiwi_opera[1] : NaN); }

var kiwi_version_fail = false;

function kiwi_version_cb(response_obj)
{
	version_maj = response_obj.maj; version_min = response_obj.min;
	kiwi.admin_save_pwd = response_obj.sp;
	//console.log('v'+ version_maj +'.'+ version_min +': KiwiSDR server asp='+ kiwi.admin_save_pwd);
	var s='';
	
	kiwi_check_js_version.forEach(function(el) {
		if (el.VERSION_MAJ != version_maj || el.VERSION_MIN != version_min) {
			if (kiwi_version_fail == false) {
				s = 'Your browser is trying to use incorrect versions of the KiwiSDR Javascript files.<br>' +
					'Please clear your browser cache and try again.<br>' +
					'Or click the button below to continue anyway.<br><br>' +
					'v'+ version_maj +'.'+ version_min +': KiwiSDR server<br>';
				kiwi_version_fail = true;
			}
			s += 'v'+ el.VERSION_MAJ +'.'+ el.VERSION_MIN +': '+ el.file +'<br>';
		}
		//console.log('v'+ el.VERSION_MAJ +'.'+ el.VERSION_MIN +': '+ el.file);
	});
	
	if (kiwi_version_fail)
	   s += '<br>'+ w3_button('w3-css-yellow', 'Continue anyway', 'kiwi_version_continue_cb');
	
	kiwi.conn_tstamp = isDefined(response_obj.ts)? response_obj.ts : (new Date()).getTime();
	//console.log(response_obj);
	//console.log('conn_tstamp='+ kiwi.conn_tstamp);
	
	kiwi_bodyonload(s);
}

function kiwi_version_continue_cb()
{
   seriousError = false;
	kiwi_bodyonload('');
}

function ord(c, base)
{
   if (isString(base))
      return c.charCodeAt(0) - base.charCodeAt(0);
   else
      return c.charCodeAt(0);
}

function chr(i)
{
   return String.fromCharCode(i);
}

function sq(s)
{
	return '\''+ s +'\'';
}

function dq(s)
{
	return '\"'+ s +'\"';
}

// "xyz":  -- found frequently in JSON
function dqc(s)
{
	return '"'+ s +'":';
}

function paren(s)
{
   return '('+ s +')';
}

function TF(cond)
{
   return (cond? 'T':'F');
}

function plural(num, word)
{
   if (num == 1) return word; else return word +'s';
}

function arrayBufferToString(buf) {
	// stackoverflow.com/questions/6965107/converting-between-strings-and-arraybuffers
	var s;
	try {
	   // with Safari, the following gets a "RangeError: Maximum call stack size exceeded"
	   // for large transfers like "MSG dx_json=..."
	   if (0) {
	      s = String.fromCharCode.apply(null, new Uint8Array(buf));
	   } else {
         var u8buf = new Uint8Array(buf);
         s = '';
         for (var i = 0; i < u8buf.length; i++) s += String.fromCharCode(u8buf[i]);
      }
	} catch (ex) {
	   console.log(buf);
	   console.log(ex);
	   kiwi_trace('arrayBufferToString');
	   s = null;
	}
	return s;
}

function arrayBufferToStringLen(buf, len)
{
	var u8buf = new Uint8Array(buf);
	var output = String();
	len = Math.min(len, u8buf.length);
	for (var i = 0; i < len; i++) output += String.fromCharCode(u8buf[i]);
	return output;
}

function kiwi_dup_array(a)
{
   return a.slice();
}

function kiwi_dedup_array(a, func)
{
   var ra = [];
   a.forEach(function(v, i) {
      if (func) {
         var rv = func(v);
         if (!rv || rv.err) return;
         v = rv.v;
      }
      if (!ra.includes(v)) ra.push(v);
   });
   return ra;
}

// turn something iterable with forEach, like a nodeList, into a proper array (which a nodeList is not)
function iterable_to_array(iter)
{
   var a = [];
   iter.forEach(function(e) { a.push(e); });
   return a;
}

function kiwi_char_iter(s, func)
{
   var ra = [];
   s.split('').forEach(
      function(c,i) {
         ra[i] = func(c,i);
      }
   );
   return ra;
}

function kiwi_array_remove_undefined(ar)
{
   var ra = [];
   var j = 0;
   ar.forEach(
      function(ae,i) {
         if (isDefined(ae)) {
            ra[j] = ae;
            j++;
         }
      }
   );
   return ra;
}

function kiwi_string_to_hex(s, sep)
{
   var sa = kiwi_char_iter(s,
      function (c,i) {
         return c.charCodeAt(0).toHex(-2);
      }
   );
   return sa.join(sep || '');
}

// NB: don't use on an array (will return object, not array)
function kiwi_shallow_copy(obj)
{
   return Object.assign({}, obj);
}

function kiwi_deep_copy(obj)
{
   return JSON.parse(JSON.stringify(obj));
}

function kiwi_sort_numeric(a, b) {
   return parseFloat(a) - parseFloat(b);
}

function kiwi_sort_numeric_reverse(a, b) {
   return parseFloat(b) - parseFloat(a);
}

function kiwi_sort_ignore_case(a, b) {
   a = a.toLowerCase();
   b = b.toLowerCase();
   if (a === b) return 0;
   if (a < b) return -1;
   return 1;
}

function kiwi_bitReverse(v, len) {
   if (v == 0) return 0;
   var rv = 0;
   for (i = 0; i < len; i++) {
      rv |= ((v >> i) & 1) << (len - 1 - i);
   }
   return rv >>> 0;     // force unsigned result if b31 set
}

function kiwi_bitCount(n) {
   if (n == 0) return 0;
   return n.toString(2).match(/1/g).length;
}

// external API compatibility
function getFirstChars(buf, len)
{
   arrayBufferToStringLen(buf, len);
}

function removeEnding(str, ending)
{
   if (str.endsWith(ending))
      return str.slice(0, -ending.length);
   else
      return str;
}

function kiwi_inet4_d2h(inet4_str, opt)
{
	var s = inet4_str.split('.');
	//console.log(s);
	if (s.length != 4) return null;
	var check = function(v) {
	   var n = parseInt(v);    // includes "" => NaN
	   if (!isNumber(n) || n < 0 || n > 255) return null;
	   return n;
	};
	var a, b, c, d;
	if ((a = check(s[0])) == null) return null;
	if ((b = check(s[1])) == null) return null;
	if ((c = check(s[2])) == null) return null;
	if ((d = check(s[3])) == null) return null;
	var ip = (a<<24) | (b<<16) | (c<<8) | d;
	
	if (opt && opt['no_local_ip']) {
	   //console.log('CHECK '+ kiwi_ip_str(ip));
	   if (
         (ip >= kiwi_ip_10_lo && ip <= kiwi_ip_10_hi) ||
         (ip >= kiwi_ip_172_16_lo && ip <= kiwi_ip_172_16_hi) ||
         (ip >= kiwi_ip_192_168_lo && ip <= kiwi_ip_192_168_hi) ||
         (ip == kiwi_ip_loopback)) {
	         //console.log('EXCLUDE LOCAL '+ kiwi_ip_str(ip));
            return null;
      }
	}
	
	return ip;
}

var kiwi_ip_10_lo = kiwi_inet4_d2h('10.0.0.0');
var kiwi_ip_10_hi = kiwi_inet4_d2h('10.255.255.255');
var kiwi_ip_172_16_lo = kiwi_inet4_d2h('172.16.0.0');
var kiwi_ip_172_16_hi = kiwi_inet4_d2h('172.31.255.255');
var kiwi_ip_192_168_lo = kiwi_inet4_d2h('192.168.0.0');
var kiwi_ip_192_168_hi = kiwi_inet4_d2h('192.168.255.255');
var kiwi_ip_loopback = kiwi_inet4_d2h('127.0.0.1');

function kiwi_h2n_32(ip, o)
{
	var n;
	switch (o) {
		case 0: n = ip>>24; break;
		case 1: n = ip>>16; break;
		case 2: n = ip>>8; break;
		case 3: n = ip; break;
		default: n = 0; break;
	}
	return n & 0xff;
}

function kiwi_ip_str(ip)
{
	var s='';
	
	if (isNumber(ip))
		s = kiwi_h2n_32(ip,0)+'.'+kiwi_h2n_32(ip,1)+'.'+kiwi_h2n_32(ip,2)+'.'+kiwi_h2n_32(ip,3);
	else
	if (isArray(ip))
		s = ip[3] +'.'+ ip[2] +'.'+ ip[1] +'.'+ ip[0];
	else
	if (isObject(ip))
		s = ip.a +'.'+ ip.b +'.'+ ip.c +'.'+ ip.d;
	
	return s;
}

// http://stackoverflow.com/questions/2998784/how-to-output-integers-with-leading-zeros-in-javascript
Number.prototype.leadingZeros = function(size)
{
	var s = String(this);
	if (!isNumber(size)) size = 2;
	while (s.length < size) s = '0'+ s;
	return s;
};

// size is total number of digits, padded to the left with zeros
String.prototype.leadingZeros = function(size)
{
	var s = String(this);
	if (!isNumber(size)) size = 2;
	while (s.length < size) s = '0'+ s;
	return s;
};

// size is total number of characters, padded to the left with spaces
Number.prototype.fieldWidth = function(size)
{
	var s = String(this);
	if (!isNumber(size)) return s;
	while (s.length < size) s = ' '+ s;
	return s;
};

String.prototype.fieldWidth = function(size)
{
	var s = String(this);
	if (!isNumber(size)) return s;
	while (s.length < size) s = ' '+ s;
	return s;
};

// unlike parseInt() considers the entire string
String.prototype.filterInt = function() {
	var s = String(this);
	if (/^(\-|\+)?([0-9]+|Infinity)$/.test(s))
		return Number(s);
	return NaN;
};

String.prototype.parseIntEnd = function() {
	var s = String(this);
	var a = s.match(/[^-\d]+([-\d]+$)/);
	//console.log('parseIntEnd <'+ s +'>');
	//console.log(a);
	if (a.length == 2 && isNumber(+a[1]))
		return Number(a[1]);
	return NaN;
};

String.prototype.withSign = function()
{
	var s = this;
	var n = Number(s);
	return (n < 0)? s : ('+'+ s);
};

String.prototype.positiveWithSign = function()
{
	var s = this;
	var n = Number(s);
	return (n <= 0)? s : ('+'+ s);
};

function isHexDigit(c) { return ('0123456789ABCDEFabcdef'.indexOf(c) > -1); }

// pad with left zeros to 'digits' length
// +digits: add leading '0x'
// -digits: no leading '0x'
Number.prototype.toHex = function(digits)
{
	if (!isNumber(digits)) digits = 0;
   var add_0x = (digits < 0)? 0:1;
   digits = Math.abs(digits);
	var n = Number(this);
	n = n >>> 0;   // make unsigned
	var s = n.toString(16);
	while (s.length < digits) s = '0'+ s;
	if (add_0x) s = '0x'+ s;
	return s;
};

// minimum number of digits: remove trailing zeros and/or decimal point
Number.prototype.toFixedNZ = function(d)
{
	var n = Number(this);
	var s = n.toFixed(d);
	while (s.endsWith('0'))
	   s = s.slice(0, -1);
	if (s.endsWith('.')) s = s.slice(0, -1);   // nnn.0 => nnn. => nnn
	return s;
};

Number.prototype.toUnits = function()
{
	var n = Number(this);
	if (n < 1000) {
		return n.toString();             // nnn
	} else
	if (n < 1e6) {
		return (n/1e3).toFixed(1)+'k';   // nnn.fk
	} else
	if (n < 1e9) {
		return (n/1e6).toFixed(1)+'M';   // nnn.fM
	} else {
		return (n/1e9).toFixed(1)+'G';   // nnn.fG
	}
};

// allow 'k' (1e3) and 'M' (1e6) suffix
// use "adj" param to convert returned result in kHz (=1e-3), MHz (=1e-6) etc.
String.prototype.parseFloatWithUnits = function(allowed_suffixes, adj, frac_digits) {
	var s = String(this);
	var v = parseFloat(s);
	if (isNaN(v)) return NaN;
	if (!allowed_suffixes || allowed_suffixes.includes('k'))
      if (new RegExp('([-0-9.]*k)').test(s)) { v *= 1e3; if (isNumber(adj)) v *= adj; }
	if (!allowed_suffixes || allowed_suffixes.includes('M'))
      if (new RegExp('([-0-9.]*M)').test(s)) { v *= 1e6; if (isNumber(adj)) v *= adj; }
   return kiwi_round(v, frac_digits);
};

function kiwi_round(v, frac_digits)
{
   if (!isNumber(frac_digits) || frac_digits < 0) return v;
   var factor = Math.pow(10, frac_digits);
   return Math.round(v * factor) / factor;
}

Number.prototype.withSign = function()
{
	var n = Number(this);
	var s = n.toString();
	return (n < 0)? s : ('+'+ s);
};

var kHz = function(f) { return (f / 1e3).toFixed(3) +'k'; };

// need symmetry for negative f in passband calcs
function _10Hz(f) {
   var step = 10;
   if (f >= 0) {
      return Math.trunc(f/step) * step;
   } else {
      return -(Math.ceil(-f/step) * step);
   }
}

function _10Hz_round(f) {
   var step = 10;
   return Math.round(f/step) * step;
}

// like setTimeout() except also calls once immediately
function kiwi_setTimeout(func, msec, param)
{
   func(param);
   return setTimeout(func, msec, param);
}

function kiwi_clearTimeout(timeout)
{
   try { clearTimeout(timeout); } catch(e) {}
}

// like setInterval() except also calls once immediately
function kiwi_setInterval(func, msec, param)
{
   func(param);
   return setInterval(func, msec, param);
}

function kiwi_clearInterval(interval)
{
   try { clearInterval(interval); } catch(e) {}
}

var littleEndian = (function() {
	var buffer = new ArrayBuffer(2);
	new DataView(buffer).setInt16(0, 256, true /* littleEndian */);
	// Int16Array uses the platform's endianness.
	return new Int16Array(buffer)[0] === 256;
})();

//console.log('littleEndian='+ littleEndian);

function _nochange(v)
{
   if (arguments.length == 0) return undefined;
   return (v === undefined);
}

function _default(v)
{
   if (arguments.length == 0) return NaN;
   return isNaN(v);
}

function _change(v)
{
   return (!_nochange(v) && !_default(v));
}

// usage: console_log('id', arg0, arg1 ...)
// prints: "<id>: 'arg0'=<arg0_value> 'arg1'=<arg1_value> ..."
// i.e. the string 'argN', not the name of the argN parameter (unfortunately)
// see console_nv() below for partial solution to this problem
function console_log()
{
   //console.log('console_log: '+ typeof(arguments));
   //console.log(arguments);
   var s;
   for (var i = 0; i < arguments.length; i++) {
      var arg = arguments[i];
      if (i == 0) {
         s = arg +': ';
      } else {
         s += 'arg'+ (i-1) +'='+ arg +' ';
      }
   }
   console.log('CONSOLE_LOG '+ s);
}

// usage: console_nv('id', {arg0}, {arg1}, 'a.b.c' (i.e. FQN) ...)
// prints: "<id>: 'actual_arg0_name'=<arg0_value> 'actual_arg1_name'=<arg1_value> ..."
//
// So this works for:
// local/global simple vars (incl obj): YES
// local obj deref: NO
// global deref (but FQN only): YES (specify FQN as a string)
//
// e.g. console_nv('screen_char', {r}, {c}, 'kiwi.d.p.nrows')
function console_nv()
{
   var s;
   for (var i = 0; i < arguments.length; i++) {
      var arg = arguments[i];
      if (i == 0) {
         s = arg +': ';
      } else {
         var name, val;
         if (isObject(arg)) {
            name = Object.keys(arg)[0];
            val = arg[name];
            s += name +'='+ JSON.stringify(val) +' ';
         } else
         if (isString(arg)) {
            try {
               val = getVarFromString(arg);
            } catch(ex) {
               val = '[not defined]';
            }
            //var lio = arg.lastIndexOf('.');
            //name = (lio == -1)? arg : arg.substr(lio+1);
            name = arg;
            s += name +'='+ val +' ';
         } else {
            s += '[arg'+ (i-1) +' unknown] ';
         }
      }
   }
   console.log('CONSOLE_NV '+ s);
}

function console_log_dbgUs()
{
   if (!dbgUs) return;
   s = '';
   for (var i = 0; i < arguments.length; i++)
      s += arguments[i].toString();
   console.log(s);
}

// console log via a timeout for routines that are realtime critical (e.g. audio on_process() routines)
function kiwi_log(s)
{
   setTimeout(function(s) {
      console.log(s);
   }, 1, s);
}

function kiwi_trace_log(s)
{
   setTimeout(function(s) {
      kiwi_trace(s);
   }, 1, s);
}


////////////////////////////////
// canvas log
//    use mute button / space bar to clear log
////////////////////////////////

function canvas_log_int(s)
{
   var el;
   if (!kiwi.canvas_log_init) {
      //kiwi_trace();
      el = w3_el('id-news');
      el.style.top = '';
      el.style.bottom = '';
      el.style.left = '';
      el.style.right = '';
      if (kiwi_isMobile()) {
      
         // see ext.js for table of device w/h values
         // these assume portrait orientation
         var w = screen.width;
         var h = screen.height;
         if (w >= 600 && h >= 950) {   // iPad & tablets
            //el.style.top = '36px';     // top
            el.style.top = '200px';    // bottom
            //el.style.bottom = '0';
            //el.style.right = '0';
            el.style.left = '0';
            el.style.width = '350px';
            //el.style.height = '300px';
            el.style.height = '400px';
         } else {                      // phones
            el.style.top = '36px';     // top
            //el.style.top = '300px';    // bottom
            //el.style.right = '0';        // right
            el.style.left = '0';         // left
            //el.style.width = '350px';
            el.style.width = '150px';
            el.style.height = '150px';
         }
      } else {                         // non-mobile
         el.style.bottom = '0';
         el.style.left = '0';
         el.style.width = '350px';
         el.style.height = '300px';
      }
      el.style.visibility = 'visible';
      el.style.zIndex = 9999;
      kiwi.canvas_log_init = true;
   }
   el = w3_el('id-news-inner');
   w3_innerHTML(el, s);
   w3_scrollDown(el);
}

function canvas_log_clear()
{
   if (kiwi.canvas_log_init == true)
      canvas_log('\f');
}

function canvas_log(s)
{
   if (!isString(s)) s = s.toString();
   if (isUndefined(owrx.news_acc_s)) owrx.news_acc_s = '<br>';
   var d = new Date(), ts = '';
   var time = d.toTimeString().slice(4,8);
   if (time != kiwi_util.last_time) {
      ts = time +' ';
      kiwi_util.last_time = time;
   }

   if (s == '\f') {
      owrx.news_acc_s = '<br>';
   } else
   if (s.charAt(0) == '$') {
      owrx.news_acc_s += '<br>'+ ts + s;
   } else {
      owrx.news_acc_s += ((owrx.news_acc_s != '<br>')? ' | ' : '') + ts + s;
   }

   canvas_log_int(owrx.news_acc_s);
}

function canvas_log2(s)
{
   if (kiwi_isMobile())
      toggle_or_set_hide_bars(owrx.HIDE_BANDBAR);
   w3_innerHTML('id-rx-title', kiwi.log2_seq +': '+ s);
   kiwi.log2_seq++;
}

function canvas_catch_log(ex)
{
   canvas_log(ex.toString());
   canvas_log(ex.stack);
}


// from: stackoverflow.com/questions/11547672/how-to-stringify-event-object
// Calculate a string representation of a node's DOM path.
function kiwi_pathToSelector(node)
{
  if (!node || !node.outerHTML) {
    return null;
  }

  var path;
  while (node.parentElement) {
    var name = node.localName;
    if (!name) break;
    name = name.toLowerCase();
    var parent = node.parentElement;

    var domSiblings = [];

    if (parent.children && parent.children.length > 0) {
      for (var i = 0; i < parent.children.length; i++) {
        var sibling = parent.children[i];
        if (sibling.localName && sibling.localName.toLowerCase) {
          if (sibling.localName.toLowerCase() === name) {
            domSiblings.push(sibling);
          }
        }
      }
    }

    if (domSiblings.length > 1) {
      name += ':eq(' + domSiblings.indexOf(node) + ')';
    }
    path = name + (path ? '>' + path : '');
    node = parent;
  }

  return path;
}

// Generate a JSON version of the event.
function kiwi_serializeEvent(e) {
  if (e) {
    var o = {
      eventName: e.toString(),
      altKey: e.altKey,
      bubbles: e.bubbles,
      button: e.button,
      buttons: e.buttons,
      cancelBubble: e.cancelBubble,
      cancelable: e.cancelable,
      clientX: e.clientX,
      clientY: e.clientY,
      composed: e.composed,
      ctrlKey: e.ctrlKey,
      currentTarget: e.currentTarget ? e.currentTarget.outerHTML : null,
      defaultPrevented: e.defaultPrevented,
      detail: e.detail,
      eventPhase: e.eventPhase,
      fromElement: e.fromElement ? e.fromElement.outerHTML : null,
      isTrusted: e.isTrusted,
      layerX: e.layerX,
      layerY: e.layerY,
      metaKey: e.metaKey,
      movementX: e.movementX,
      movementY: e.movementY,
      offsetX: e.offsetX,
      offsetY: e.offsetY,
      pageX: e.pageX,
      pageY: e.pageY,
      path: kiwi_pathToSelector(e.path && e.path.length ? e.path[0] : null),
      relatedTarget: e.relatedTarget ? e.relatedTarget.outerHTML : null,
      returnValue: e.returnValue,
      screenX: e.screenX,
      screenY: e.screenY,
      shiftKey: e.shiftKey,
      sourceCapabilities: e.sourceCapabilities ? e.sourceCapabilities.toString() : null,
      target: e.target ? e.target.outerHTML : null,
      timeStamp: e.timeStamp,
      toElement: e.toElement ? e.toElement.outerHTML : null,
      type: e.type,
      view: e.view ? e.view.toString() : null,
      which: e.which,
      x: e.x,
      y: e.y
    };

    var s = JSON.stringify(o, null, 2);
    //console.log(s);
    return s;
  }
  
  return '(null)';
}

function key_stringify(evt)
{
   if (!evt) return '(no evt?)';
   var s = '';
   if (evt.shiftKey) s += 'shift-';
   if (evt.ctrlKey)  s += 'ctrl-';
   if (evt.altKey)   s += 'alt-';
   if (evt.metaKey)  s += 'meta-';
   s += evt.key? evt.key : '(no key?)';
   return s;
}

function event_dump(evt, id, oneline)
{
   var k = evt.key || '(no key)';
   var tgt_id, ct_id;

   var button = "LMR"[evt.button];
   var buttons = '';
   if (evt.buttons & 1) buttons += 'L';
   if (evt.buttons & 4) buttons += 'M';   // yes, 4 is middle
   if (evt.buttons & 2) buttons += 'R';

   if (oneline) {
      var trel = (isDefined(evt.relatedTarget) && evt.relatedTarget)? (' Trel='+ evt.relatedTarget.id) : '';
      var key = key_stringify(evt);
      tgt_id = evt.target? evt.target.id : '(null)';
      ct_id = evt.currentTarget? evt.currentTarget.id : '(null)';
      console.log('event_dump '+ id +' |'+ evt.type +'| k='+ key +' b='+ button +' bs='+ buttons +' T='+ tgt_id +' Tcur='+ ct_id + trel);
   } else {
      console.log('================================');
      if (isNoArg(evt)) {
         console.log('EVENT_DUMP: '+ id +' bad evt');
         kiwi_trace();
         return;
      }
      console.log('EVENT_DUMP: '+ id +' type='+ evt.type);
      console.log('key: '+ key_stringify(evt));
      ct_id = evt.currentTarget? evt.currentTarget.id : '(null)';
      console.log('this.id='+ this.id +' tgt.name='+ evt.target.nodeName +' tgt.id='+ evt.target.id +' ctgt.id='+ ct_id);
      console.log('button='+ button +' buttons='+ buttons +' detail='+ evt.detail);
      
      if (evt.type.startsWith('touch')) {
         console.log('pageX='+ evt.pageX +' clientX='+ evt.clientX +' screenX='+ evt.screenX);
         console.log('pageY='+ evt.pageY +' clientY='+ evt.clientY +' screenY='+ evt.screenY);
      } else {
         console.log('pageX='+ evt.pageX +' clientX='+ evt.clientX +' screenX='+ evt.screenX +' offX(EXP)='+ evt.offsetX +' layerX(DEPR)='+ evt.layerX);
         console.log('pageY='+ evt.pageY +' clientY='+ evt.clientY +' screenY='+ evt.screenY +' offY(EXP)='+ evt.offsetY +' layerY(DEPR)='+ evt.layerY);
      }
      
      console.log('evt, evt.target, evt.currentTarget, evt.relatedTarget, elementFromPoint:');
      console.log(evt);
      console.log(evt.target);
      console.log(evt.currentTarget);
      console.log(evt.relatedTarget);
      var el_s = w3_elementAtPointer(evt);
      if (el_s)
         console.log(el_s);
      else
         console.log('(no x,y)');
      console.log('----');
   }
}

// NB: returns a function reference
function kiwi_rateLimit(cb, msec)
{
   var waiting = false;
   var rtn = function() {
      if (waiting) return;
      waiting = true;
      var args = arguments;
      setTimeout(function() {
         waiting = false;
         if (isString(cb))
            w3_call(cb);
         else
            cb.apply(this, args);
      }, msec);
   };
   return rtn;
}

function kiwi_UTC_minutes()
{
   var d = new Date();
   return (d.getUTCHours()*60 + d.getUTCMinutes());
}

function kiwi_dhms(secs)
{
	var t = +secs;
	var sec = Math.trunc(t % 60); t = Math.trunc(t/60);
	var min = Math.trunc(t % 60); t = Math.trunc(t/60);
	var hr  = Math.trunc(t % 24); t = Math.trunc(t/24);
	var days = t;
	var hms = hr +':'+ min.leadingZeros(2) +':'+ sec.leadingZeros(2);
	var d = '';
	if (days) d = days +'d:';
	var dhms = d + hms;
	return { dhms:dhms, hms:hms, days:days, hr:hr, min:min, sec:sec, secs:secs };
}

function kiwi_hh_mm(hh_mm)
{
   if (isNumber(hh_mm)) return hh_mm;

   if (isString(hh_mm)) {
      var t = hh_mm.split(':');
      var hr = +t[0];
      if (t.length > 1) {
         var min = (+t[1]) / 60;
         hr = (hr < 0)? (hr - min) : (hr + min);
      }
      if (t[0] == '-0' || t[0] == '-00')
         hr = -hr;
      return hr;
   }

   return null;
}
//console.log('# '+ kiwi_hh_mm(-11) +' '+ + kiwi_hh_mm('-10') +' '+ kiwi_hh_mm('10:55') +' '+ kiwi_hh_mm('-10:55'));

// stackoverflow.com/questions/8619879/javascript-calculate-the-day-of-the-year-1-366
function kiwi_UTCdoyToDate(doy, year, hour, min, sec)
{
   return new Date(Date.UTC(year, 0, doy, hour, min, sec));    // yes, doy = 1..366 really works!
}

function kiwi_decodeURIComponent(id, uri)
{
   var i, j, k;
   var obj = null, double_fail = false;
   
   if (dbgUs && arguments.length != 2) {
      alert('kiwi_decodeURIComponent: requires exactly two args');
      kiwi_trace();
      return null;
   }
   
   while (obj == null) {
      try {
         obj = decodeURIComponent(uri);
      } catch(ex) {
         console.log('kiwi_decodeURIComponent('+ id +'): decode fail');
         console.log(uri);
         console.log(ex);
      
         if (double_fail) {
            console.log('kiwi_decodeURIComponent('+ id +'): DOUBLE DECODE FAIL');
            console.log(uri);
            console.log(ex);
            return null;
         }
         double_fail = true;

         // v1.464
         // Recover from broken UTF-8 sequences stored in cfg.
         // Remove all "%xx" sequences, for xx >= 0x80, whenever decodeURIComponent() fails.
         // User will have to manually repair UTF-8 sequences since information has been lost.
         //
         // NB: If a cfg field contains only valid UTF-8 sequences then the decodeURIComponent() will not fail
         // and this code will not be triggered. That way corrections made to broken fields will persist in the cfg.
         // This is why bulk removal of >= %80 sequences cannot be done in _cfg_load_json() on the server side.
         // Doing that would always eliminate *any* UTF-8 sequence. Even valid ones.

         for (i = 0; i < uri.length - 2; i++) {
            if (uri.charAt(i) == '%') {
               var c1 = uri.charAt(i+1);
               var c2 = uri.charAt(i+2);
               if (isHexDigit(c1) && isHexDigit(c2)) {
                  //console.log(c1 +' '+ TF(c1 >= '8'));
                  if (c1 >= '8') {
                     //var x0 = uri.charAt(i-1);
                     //x0 = x0.charCodeAt(0);
                     j = Math.max(i-5, 0); k = Math.min(i+6, uri.length);
                     console.log('BAD @'+ i +' <'+ uri.slice(j,k) +'>');
                     uri = uri.substr(0,i) + uri.substr(i+3);
                     i = 0;   // NB: this makes the for loop start over!
                     //console.log('FIX <'+ uri +'>');
                  }
               } else {
                  // % => %25
                  uri = uri.substr(0,i) +'%25'+ uri.substr(i+1);
                  // NB: Okay to just let the loop proceed here since index i++ now
                  // points to the '2' of the '%25' we just inserted.
               }
            }
         }
         
         // Must check for two cases of '%' in the last two locations in the string (not caught by above code).
         i = uri.length - 2;
         if (uri.charAt(i) == '%') uri = uri.substr(0,i) +'%25'+ uri.substr(i+1);
         i = uri.length - 1;
         if (uri.charAt(i) == '%') uri = uri.substr(0,i) +'%25';

         console.log('kiwi_decodeURIComponent FINAL FIX <'+ uri +'>');
         obj = null;
      }
   }
   
   return obj;
}

kiwi_util.str_recode_lookup = [];
var e1 = function(c) { kiwi_util.str_recode_lookup[ord(c)] = 1; };
var e2 = function(c) { kiwi_util.str_recode_lookup[ord(c)] = 2; };
var er = function(r1, r2, v) {
   for (var i = r1; i <= r2; i++)
      kiwi_util.str_recode_lookup[i] = v;
};
er(0, 127, 0);
e1('&'); e1("'"); e1('+'); e1(';'); e1('<'); e1('>'); e1('`');
e2('"'); e2('%'); e2('\\');
er(0x00, 0x1f, 2); er(0x7f, 0x7f, 2);
//console.log(kiwi_util.str_recode_lookup);

/*

XXX This doesn't work because of UTF-8!
      Like server-side, needs to be done on already %-encoded UTF-8 bytes

// Equivalent of server side kiwi_url_decode_selective() except
// that it works in the opposite direction, i.e. encoding un-encoded strings.
// Encodes a more limited set of characters than encodeURIComponent()
// to help with readability of the various configuration files.

function kiwi_str_encode_selective(s, fewer_encoded)
{
   var d = '';
   
   for (var i = 0; i < s.length; i++) {
      var c = s[i];
      var n = ord(c);
      var lu = kiwi_util.str_recode_lookup[n];
      if ((fewer_encoded && lu == 2) || (!fewer_encoded && lu != 0))
         d += '%'+ n.toHex(-2); else d += c;
   }
   
   return d;
}
console.log(kiwi_str_encode_selective('ABC!@#$^*()_-={}[]|:,./?~   "%&\'+;<>\\`XYZ'+ String.fromCharCode(0, 1, 0x1f, 0x7f, 0x80)));
*/

// js version of service-side kiwi_url_decode_selective()
function kiwi_str_decode_selective_inplace(src, fewer_encoded)
{
   var i, dst = '';
   var src_len = src.length;
   var dst_len = src.length + 1;
   var hex_digit = /[0-9a-fA-F]/;
   
    for (i = 0; i < src_len; i++) {
        if (src[i] == '%' && i + 2 < src_len &&
            src[i + 1].match(hex_digit) &&
            src[i + 2].match(hex_digit)) {
            
            var c = (parseInt(src[i+1], 16) << 4) | parseInt(src[i+2], 16);
            
             // preserve UTF-8 encoding values >= 0x80!
            if (c < 0x80 && (kiwi_util.str_recode_lookup[c] == 0 || (fewer_encoded && kiwi_util.str_recode_lookup[c] == 1))) {
                dst += chr(c);
                i += 2;
            } else {
                dst += '%';
            }
        } else {
            dst += src[i];
        }
    }

   return dst;
}

function kiwi_stringify(o)
{
   return JSON.stringify(o);
}

function kiwi_JSON(json, pretty)
{
   return JSON.stringify(json, null, pretty? 3:undefined);
}

function kiwi_JSON_parse(tag, json, func)
{
   var obj;
   try {
      obj = JSON.parse(json);
   } catch(ex) {
      console.log('kiwi_JSON_parse('+ tag +'): JSON parse fail');
      console.log(json);
      console.log(ex);
      w3_call(func, ex);
      obj = null;
   }
   return obj;
}

// remove ANSI "ESC[ ... m" sequences and our HTML escape sequences
function kiwi_remove_escape_sequences(s)
{
   var a1 = s.split('');
   var a2 = [];
   var inANSI = false, inHTML = false;
   for (var i = 0; i < a1.length; i++) {
      var c = a1[i];
      if (inANSI) {
         if (c == 'm') inANSI = false;
      } else
      if (inHTML) {
         if (c == kiwi.esc_gt) inHTML = false;
      } else {
         if (c == '\u001b' && (i+1) < a1.length && a1[i+1] == '[') {
            i++;
            inANSI = true;
         } else
         if (c == kiwi.esc_lt) {
            inHTML = true;
         } else {
            if (c != '\f')    // ignore kiwi_output_msg() screen clear form feed
               a2.push(c);
         }
      }
   }
   
   return a2.join('');
}

function kiwi_timestamp()
{
   return new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z');
}

function kiwi_timestamp_filename(pre, post)
{
   var el = w3_el('id-freq-input');
   var freq_mode = el? ('_'+ el.value +'_'+ ext_get_mode()) : '';
   return pre + kiwi_host() +'_'+ kiwi_timestamp() + freq_mode + post;
}


////////////////////////////////
// HTML helpers
////////////////////////////////

function kiwi_clean_html(s)
{
   return s.replace(/\&/g, '&amp;').replace(/</g, '&lt;').replace(/\>/g, '&gt;');
}

function kiwi_clean_newline(s)
{
   return s.replace(/[\r\n\f\v]/g, '').replace(/\t/g, ' ');
}

// returns "http://" or "https://" depending if connection to Kiwi is using SSL or not
function kiwi_SSL()
{
   return window.location.protocol +'//';
}

// returns the entire url including port and query string
function kiwi_url()
{
   return window.location.href;
}

// returns http[s]://host[:port]
function kiwi_url_origin()
{
   return window.location.origin;
}

// returns host[:port]
function kiwi_host_port()
{
   return window.location.host;
}

// returns host (no :port)
function kiwi_host()
{
   return window.location.hostname;
}

function kiwi_remove_protocol(url)
{
   return url.replace(/^http:\/\//, '').replace(/^https:\/\//, '');
}

function kiwi_open_or_reload_page(obj)   // { url, hp, path, qs, tab, delay }
{
   var url = w3_opt(obj, 'url', null);
   if (url == 'reload')
      url = kiwi_url();
   else
   if (isNull(url)) {
      var host_port = w3_opt(obj, 'hp', kiwi_host_port());
      var pathname = w3_opt(obj, 'path', '', '/');
      var query = w3_opt(obj, 'qs', '', '/?');
      url = kiwi_SSL() + host_port + pathname + query;
   }
   var delay = w3_opt(obj, 'delay', 0);
   console.log('kiwi_open_or_reload_page: '+ url +' delay='+ delay);
   
   var reload = function() {
      var rv;
      if (w3_opt(obj, 'tab', false)) {
         rv = window.open(url, '_blank');
      } else {
         window.location.href = url;
         rv = true;
      }
      return rv;
   };
   
   var rval;
   if (delay) {
      setTimeout(function(url) { reload(); }, delay);
      rval = true;
   } else {
      rval = reload();
   }
   return rval;
}

function kiwi_add_end(s, end)
{
   if (!s.endsWith(end)) s = s + end;
   return s;
}

// pnames can be: 'string' or [ 'string1', 'string2', ... ]
function kiwi_url_param(pnames, default_val, not_found_val)
{
   var pn_isArray = isArray(pnames);
   var pn_isString = isString(pnames);
   var pn_isNumber = isNumber(pnames);
	if (isUndefined(default_val)) default_val = true;
	if (isUndefined(not_found_val)) not_found_val = null;

   // skip initial '?'
	var params = (window.location && window.location.search && window.location.search.substr(1));
	if (!params) return not_found_val;
	
   var rv = not_found_val;
   params.split("&").forEach(function(pv, i) {
      var pv_a = pv.split("=");
      if (pn_isArray) {
         pnames.forEach(function(pn) {
            if (pn != pv_a[0]) return;
            rv = (pv_a.length >= 2)? pv_a[1] : default_val;
            //console.log('kiwi_url_param '+ pn +'='+ rv);
         });
      } else
      if (pn_isString) {
         if (pnames == pv_a[0]) {
            rv = (pv_a.length >= 2)? pv_a[1] : default_val;
            //console.log('kiwi_url_param '+ pnames +'='+ rv);
         }
      } else
      if (pn_isNumber) {
         if (i == +pnames)
            rv = pv_a[0];
      }
   });
   
   return rv;
}

function kiwi_add_search_param(location, param)
{
   var p;
   var qs = location.search;
   console.log('qs=<'+ qs +'>');
   if (qs == '') p = '/?';
   else if (qs.includes('?')) p = '&';
   else p = '/?';
   return p + param;
}

function kiwi_remove_search_param(url, p)
{
   url = url
      .replace('?'+ p +'&', '?')
      .replace('&'+ p +'&', '&')
      .replace('?'+ p +'', '')
      .replace('&'+ p +'', '');
   return url;
}

function html(el_id)
{
   return w3_el(el_id);
}

function px(s)
{
   if (isNoArg(s) || s == '') return '0';
   var num = parseFloat(s);
   if (isNaN(num)) {
      console.log('px num='+ s);
      kiwi_trace();
      return '0';
   }
	return num.toFixed(0) +'px';
}

function unpx(s)
{
   return parseFloat(s);   // parseFloat() because s = "nnnpx"
}

function css_style(el, prop)
{
   el = w3_el(el);
   if (!el) return null;
	var style = getComputedStyle(el, null);
	//console.log('css_style id='+ el.id);
	//console.log(style);
	var val = style.getPropertyValue(prop);
	return val;
}

function css_style_num(el, prop)
{
   el = w3_el(el);
   if (!el) return null;
	var v = parseInt(css_style(el, prop));
	if (isNaN(v)) v = 0;
	//console.log('css_style_num '+ prop +'='+ v);
	return v;
}

function html_LR_border_pad(el)
{
	var bL = css_style_num(el, 'border-left-width');
	var bR = css_style_num(el, 'border-right-width');
	var pL = css_style_num(el, 'padding-left');
	var pR = css_style_num(el, 'padding-right');
	//console.log('html_LR_border_pad id='+el.id+' border: '+bL+','+bR);
	//console.log('html_LR_border_pad id='+el.id+' padding: '+pL+','+pR);
	return bL+bR + pL+pR;
}

// from http://www.switchonthecode.com/tutorials/javascript-tutorial-the-scroll-wheel
function cancelEvent(ev)
{
	ev = ev ? ev : window.event;
	if (ev.stopPropagation) ev.stopPropagation();
	if (ev.preventDefault) ev.preventDefault();
	if (ev.stopImmediatePropagation) ev.stopImmediatePropagation();
	ev.cancelBubble = true;
	ev.cancel = true;
	ev.returnValue = false;
	return false;
}

function ignore(ev)
{
	return cancelEvent(ev);
}

function kiwi_rgb(r, g, b)
{
   if (r == null) return '';
   
   if (isArray(r)) {
      g = r[1];
      b = r[2];
      r = r[0];
   }

	return 'rgb('+ Math.floor(r) +','+ Math.floor(g) +','+ Math.floor(b) +')';
}

function hsl(h, s, l)
{
   s = Math.floor(s);
   if (s < 0) s = 0;
   if (s > 100) s = 100;
   l = Math.floor(l);
   if (l < 0) l = 0;
   if (l > 100) l = 100;
	return 'hsl('+ Math.round(h) +','+ s +'%,'+ l +'%)';
}


////////////////////////////////
// storage (browser)
////////////////////////////////

// NB: appinventor.mit.edu used by KiwiSDR Android app doesn't have localStorage

function kiwi_storeRead(k, def)
{
   var rv;
   if (isNoArg(k)) return null;

   if (kiwi.noLocalStorage) {
      rv = readCookie(k, def);
      //alert('sRd k='+ k +' d='+ def +' r='+ rv);
      return rv;
   }

	try {
      // requires that rv == null means item not in storage
      // not that the value stored is null
	   rv = localStorage.getItem(k);
	   if (rv == null && isArg(def))
	      rv = def;
	} catch(ex) {
      console.log('$>kiwi_storeRead CATCH: '+ k);
      console.log('$>kiwi_storeRead CATCH: '+ ex);
	   rv = null;
	}

	return rv;
}

function kiwi_storeInit(k, init)
{
   var rv;
   if (isNoArg(k)) return null;

   if (kiwi.noLocalStorage) {
      rv = initCookie(k, init);
      //alert('sIn k='+ k +' i='+ init +' r='+ rv);
      return rv;
   }

	try {
	   rv = localStorage.getItem(k);
	   if (rv == null) {
	      if (!isArg(init)) init = '';  // don't allow stored values of null or undefined
	      kiwi_storeWrite(k, init);     // how it differs from kiwi_storeRead()
	      rv = init;
	   }
	} catch(ex) {
      console.log('$>kiwi_storeInit CATCH: '+ k);
      console.log('$>kiwi_storeInit CATCH: '+ ex);
	   rv = null;
	}
	
	return rv;
}

function kiwi_storeWrite(k, v)
{
   if (isNoArg(k)) return null;

   if (kiwi.noLocalStorage) {
      var rv = writeCookie(k, v);
      //alert('sWr k='+ k +' v='+ v +' r='+ rv);
      return rv;
   }
	   
	try {
      if (!isArg(v)) v = '';     // don't allow stored values of null or undefined
	   localStorage.setItem(k, v);
	} catch(ex) {
      console.log('$>kiwi_storeWrite CATCH: '+ k);
      console.log('$>kiwi_storeWrite CATCH: '+ ex);
	   return null;
	}
	
	return true;
}

function kiwi_storeDelete(k)
{
   if (isNoArg(k)) return null;
   
   if (kiwi.noLocalStorage) {
      var rv = deleteCookie(k);
      //alert('sDel k='+ k +' r='+ rv);
      return rv;
   }
	   
	try {
	   localStorage.removeItem(k);
	} catch(ex) {
	   return null;
	}
	
	return true;
}

function migrateCookies() {
   if (kiwi.noLocalStorage || !isArg(document.cookie)) return;
	var ca = document.cookie.split(';');
	var len = ca.length;
	if (len == 0 || (len == 1 && isEmptyString(ca))) return;
	for (var i = 0; i < len; i++) {
		var c = ca[i].trim();
		var a = c.split('=');
		var k = a[0];
		var v = a[1];
		if (kiwi.noLocalStorage) {
		   alert(k +'='+ v);
		} else {
         console.log('migrateCookies k='+ k +' v='+ sq(v) +' '+ typeof(v));
         var rv = kiwi_storeRead(k);
         if (0) {
            if (isArg(rv)) {
               console.log('migrateCookies WARNING key already exists in new storage? k='+ k +' v='+ sq(rv));
            } else {
               kiwi_storeWrite(k, v);
            }
         } else {
            // compatibility with HTML_HEAD <script> that does: document.cookie = "key = val";
            kiwi_storeWrite(k, v);
         }
         deleteCookie(k);
      }
	}
   console.log('migrateCookies #'+ len);
}

// from http://www.quirksmode.org/js/cookies.html
function _createCookie(name, value, days) {
	var expires = "";
	if (days) {
		var date = new Date();
		date.setTime(date.getTime() + (days*24*60*60*1000));
		expires = "; expires="+ date.toGMTString();
	}
	//console.log('_createCookie <'+ name +"="+ value + expires +"; path=/" +'>');
	document.cookie = name +"="+ value + expires +"; path=/; SameSite=Lax;";
}

// returns:
// cookie exists: cookie value
// cookie doesn't exist: default value else null (if no default specified)
function readCookie(name, defaultValue) {
	var nameEQ = name + "=";
	var ca = document.cookie.split(';');
	for (var i=0; i < ca.length; i++) {
		var c = ca[i];
		while (c.charAt(0) == ' ') c = c.substring(1, c.length);
		//console.log('readCookie '+ name +' consider <'+ c +'>');
		if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length, c.length);
	}
	if (isArg(defaultValue)) {
	   return defaultValue;
	} else {
	   return null;
	}
}

function writeCookie(cookie, value)
{
	_createCookie(cookie, value, 42*365);
}

// returns:
// 
// cookie doesn't exist: write initValue; return initValue
// else return cookie value
function initCookie(cookie, initValue)
{
	var v = readCookie(cookie);
	if (v == null) {
		writeCookie(cookie, initValue);
		return initValue;
	} else {
		return v;
	}
}

// returns:
// cookie doesn't exist OR != initValue: write cookie with initValue; return true
// else return false
/*
function updateCookie(cookie, initValue)
{
	var v = readCookie(cookie);
	if (v == null || v != initValue) {
		writeCookie(cookie, initValue);
		return true;
	} else {
		return false;
	}
}
*/

function deleteCookie(cookie)
{
	var v = readCookie(cookie);
	if (v == null) return;
	_createCookie(cookie, 0, -1);
}


////////////////////////////////
// var get/set from string
////////////////////////////////

// Get function from string, with or without scopes (by Nicolas Gauthier)
// stackoverflow.com/questions/912596/how-to-turn-a-string-into-a-javascript-function-call
// returns null if var doesn't exist in last scope, throws error if scope level missing
function getVarFromString(path)
{
	var scope = window;
	var scopeSplit = path.split('.');
	for (i = 0; i < scopeSplit.length - 1; i++) {
		var scope_name = scopeSplit[i];
		scope = scope[scope_name];
		if (isUndefined(scope)) {
			//console.log('getVarFromString: NO SCOPE '+ path +' scope_name='+ scope_name);
			throw 'no scope';
		}
	}
	
	var r = scope[scopeSplit[scopeSplit.length - 1]];
	//console.log('getVarFromString: s='+ path +' r='+ r);
	return r;	// can be null or undefined
}

// And our extension for setting a var (i.e. object, prim) value.
// Create object at each scope level if required before final variable set.
function setVarFromString(string, val)
{
	var scope = window;
	var scopeSplit = string.split('.');
	for (i = 0; i < scopeSplit.length - 1; i++) {
		//console.log('setVarFromString '+ i +' '+ scopeSplit[i] +'='+ scope[scopeSplit[i]]);
		if (scope[scopeSplit[i]] == undefined) {
			scope[scopeSplit[i]] = { };
		}
		scope = scope[scopeSplit[i]];
	}
	
	scope[scopeSplit[scopeSplit.length - 1]] = val;
}

// from: stackoverflow.com/questions/332422/how-do-i-get-the-name-of-an-objects-type-in-javascript
// NB: may not work in all cases
function getType(o) { return o && o.constructor && o.constructor.name; }

// see: feather.elektrum.org/book/src.html
function kiwi_parseQuery ( query ) {
   var Params = {};
   if ( ! query ) return Params; // return empty object
   var Pairs = query.split(/[;&]/);
   for ( var i = 0; i < Pairs.length; i++ ) {
      var KeyVal = Pairs[i].split('=');
      if ( ! KeyVal || KeyVal.length != 2 ) continue;
      var key = unescape( KeyVal[0] );
      var val = unescape( KeyVal[1] );
      val = val.replace(/\+/g, ' ');
      Params[key] = val;
   }
   return Params;
}

// see: stackoverflow.com/questions/118241/calculate-text-width-with-javascript
function getTextWidth(text, font) {
  // re-use canvas object for better performance
  var canvas = getTextWidth.canvas || (getTextWidth.canvas = document.createElement("canvas"));
  var context = canvas.getContext("2d");
  context.font = font;
  var metrics = context.measureText(text);
  return metrics.width;
}


////////////////////////////////
// cross-domain GET
//
// For the case where we don't control the remote server, and it isn't returning an
// "Access-Control-Allow-Origin: *" option (e.g. wsprnet.org). So the iframe technique must be used.
////////////////////////////////

// http://stackoverflow.com/questions/298745/how-do-i-send-a-cross-domain-post-request-via-javascript

function kiwi_GETrequest(id, url, opt)
{
   var debug = w3_opt(opt, 'debug', 0);

  // Add the iframe with a unique name
  var iframe = document.createElement("iframe");
  var uniqueString = id +'_'+ (new Date()).toUTCString().substr(17,8);
  document.body.appendChild(iframe);
  iframe.style.display = "none";
  iframe.contentWindow.name = uniqueString;  // link for form.target below
  w3_add(iframe, 'id-kiwi_GETrequest');   // for benefit of browser inspector

  // construct a form with hidden inputs, targeting the iframe via the uniqueString
  var form = document.createElement("form");
  form.target = uniqueString;    // link to iframe.contentWindow.name above
  form.action = url;
  form.method = "GET";
  w3_add(form, 'id-kiwi_GETrequest');  // for benefit of browser inspector
  
  if (debug) console.log('kiwi_GETrequest: '+ uniqueString);
  return { iframe:iframe, form:form, debug:debug };
}

function kiwi_GETrequest_submit(request, opt)
{
   var no_submit = w3_opt(opt, 'no_submit', 0);
   var gc = w3_opt(opt, 'gc', 0);
   
	if (no_submit) {
		console.log('kiwi_GETrequest_submit: NO SUBMIT');
	} else {
		document.body.appendChild(request.form);
		request.form.submit();
		if (request.debug) console.log('kiwi_GETrequest_submit: SUBMITTED');
	}

	if (gc) setTimeout(function() {
		//console.log('kiwi_GETrequest GC');
		document.body.removeChild(request.form);
		document.body.removeChild(request.iframe);
	}, 60000);
}

function kiwi_GETrequest_param(request, name, value)
{
  var input = document.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  request.form.appendChild(input);

  if (request.debug) console.log('kiwi_GETrequest_param: '+ name +'='+ value);
}


////////////////////////////////
// AJAX
////////////////////////////////

// only works on cross-domains if server sends a CORS access wildcard

var ajax_state = { DONE:4 };

function kiwi_ajax(url, callback, cb_param, timeout, debug)
{
	kiwi_ajax_prim('GET', null, url, callback, cb_param, timeout, undefined, undefined, debug);
}

function kiwi_ajax_progress(url, callback, cb_param, timeout, progress_cb, progress_cb_param, debug)
{
	kiwi_ajax_prim('GET', null, url, callback, cb_param, timeout, progress_cb, progress_cb_param, debug);
}

function kiwi_ajax_send(data, url, callback, cb_param, timeout, debug)
{
	kiwi_ajax_prim('PUT', data, url, callback, cb_param, timeout, undefined, undefined, debug);
}

var ajax_id = 0;
var ajax_requests = {};

function kiwi_ajax_prim(method, data, url, callback, cb_param, timeout, progress_cb, progress_cb_param, debug)
{
   ajax_id++;
	var ajax;
	
	var dbug = function(msg) {
	   if (debug) {
	      //kiwi_debug(msg);
	      console.log(msg);
	   }
	};
	
	//try {
	//	ajax = new window.XDomainRequest();
	//} catch(ex) {
		try {
			ajax = new XMLHttpRequest();
		} catch(ex) {
			try {
				ajax = new ActiveXObject("Msxml2.XMLHTTP");
			} catch(ex) {
				try {
					ajax = new ActiveXObject("Microsoft.XMLHTTP");
				} catch(ex) {
					return false;
				}
			}
		}
	//}

	ajax.kiwi_id = ajax_id;       // ajax{} => ajax_requests[] link
	ajax_requests[ajax_id] = {};

	ajax_requests[ajax_id].didTimeout = false;
	ajax_requests[ajax_id].timer = -1;

	timeout = timeout? timeout : 0;
	if (timeout) {
		ajax_requests[ajax_id].timer = setTimeout(function() {
		   var id = ajax.kiwi_id;
			ajax_requests[id].didTimeout = true;
			ajax_requests[id].timer = -1;
	      console.log('$AJAX TIMEOUT url='+ url);
	      var obj = { AJAX_error:'timeout' };
         w3_call(callback, obj, cb_param);
			ajax.abort();
		   delete ajax_requests[id];
		}, Math.abs(timeout));
	}
	
	if (progress_cb) {
      dbug('AJAX with progress_cb='+ progress_cb);
	   ajax.onprogress = function() {
	      var response = ajax.responseText.toString() || '';
         dbug('XHR.onprogress='+ response);
         w3_call(progress_cb, response, progress_cb_param);
	   };
	}
	
	ajax.onerror = function(e) {
      dbug('XHR.onerror='+ e);
      if (debug) console.log(e);
	};

	ajax.onabort = function(e) {
      dbug('XHR.onabort='+ e);
      if (debug) console.log(e);
	};

	ajax.onload = function(e) {
      dbug('XHR.onload='+ e);
      if (debug) console.log(e);
	};

	ajax.onloadstart = function(e) {
      dbug('XHR.onloadstart='+ e);
      if (debug) console.log(e);
	};

	ajax.onreadystatechange = function() {
      dbug('XHR.onreadystatechange readyState='+ ajax.readyState);
		if (ajax.readyState != ajax_state.DONE) {
			return false;
		}

		var id = ajax.kiwi_id;
		var timer = ajax_requests[id].timer;
		dbug('AJAX ORSC ENTER recovered id='+ id +' timer='+ timer +' cb='+ callback);
		if (timer != -1) {
		   dbug('AJAX ORSC CLEAR_TIMER recovered id='+ id +' timer='+ timer);
			kiwi_clearTimeout(timer);
			ajax_requests[id].timer = -1;
		}
		
      if (!ajax_requests[id].didTimeout) {

         var obj;
         
         dbug('XHR.status='+ ajax.status);
         dbug('XHR.statusText='+ ajax.statusText);
         dbug('XHR.responseType='+ ajax.responseType);
         dbug('XHR.response='+ ajax.response);
         dbug('XHR.responseText='+ ajax.responseText);
         dbug('XHR.responseURL='+ ajax.responseURL);
         if (debug) console.log(ajax);

         if (ajax.status != 200) {
            console.log('$AJAX BAD STATUS='+ ajax.status +' url='+ url);
            obj = { AJAX_error:'status', status:ajax.status };
         } else {
            var response = ajax.responseText.toString();
            if (response == null) response = '';
            
            if (response.startsWith('<?xml')) {
               obj = { XML:true, text:response };
            } else {
               var firstChar = response.charAt(0);
         
               if (firstChar != '{' && firstChar != '[' && firstChar != '"' && !(firstChar >= '0' && firstChar <= '9')) {
                  dbug("AJAX: response didn't begin with JSON '{' '[' '\"' or digit? "+ response);
                  obj = { AJAX_error:'JSON prefix', response:response };
               } else {
                  try {
                     response = kiwi_remove_cjson_comments(response);
                     obj = JSON.parse(response);		// response can be empty
                     dbug('AJAX JSON response:');
                     dbug(response);
                     dbug(obj);
                  } catch(ex) {
                     dbug("AJAX response JSON.parse failed: <"+ response +'>');
                     dbug(ex);
                     var ex_s = ex.toString();
                     var line = ex_s.match(/line ([0-9]*)/);
                     line = (isArray(line) && line.length >= 1)? +line[1] : null;
                     obj = { AJAX_error:'JSON parse', JSON_line:line, JSON_ex:ex_s,
                        lines:response.split('\n'), response:response };
                  }
               }
            }
         }
   
		   dbug('AJAX ORSC CALLBACK recovered id='+ id +' callback='+ typeof(callback));
         w3_call(callback, obj, cb_param);
      } else {
		   dbug('AJAX ORSC TIMED_OUT recovered id='+ id);
      }
		
		dbug('AJAX ORSC ABORT/DELETE');
		ajax.abort();
		delete ajax_requests[id];
	};

   if (timeout >= 0) {     // timeout < 0 is test mode
      // DANGER: some URLs are relative e.g. /VER
      //if (!url.startsWith('http://') && !url.startsWith('https://'))
      //   url = 'http://'+ url;
      ajax.open(method, url, /* async */ true);
      dbug('AJAX SEND id='+ ajax_id +' url='+ url);
      ajax.send(data);
   }
	return true;
}

// remove comments from JSON consisting of line beginning with '//' in column 1
function kiwi_remove_cjson_comments(s)
{
   var removed = false;
   var c_begin, c_end;
   while ((c_begin = s.indexOf('\n//')) != -1) {
      c_end = s.indexOf('\n', c_begin+3);
      s = s.slice(0, c_begin) + s.slice(c_end);
      removed = true;
   }
   //if (removed) console.log(s);
   return s;
}

function kiwi_scrollbar_width()
{
	if (kiwi_isMacOS()) return 10;		// MacOS/iOS browser scrollbars are all narrower
	return 15;
}

// https://github.com/kvz/phpjs/blob/master/functions/strings/strip_tags.js
function kiwi_strip_tags(input, allowed) {
  //  discuss at: http://phpjs.org/functions/strip_tags/
  // original by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // improved by: Luke Godfrey
  // improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  //    input by: Pul
  //    input by: Alex
  //    input by: Marc Palau
  //    input by: Brett Zamir (http://brett-zamir.me)
  //    input by: Bobby Drake
  //    input by: Evertjan Garretsen
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Onno Marsman
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Eric Nagel
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Tomasz Wesolowski
  //  revised by: Rafał Kukawski (http://blog.kukawski.pl/)
  //   example 1: strip_tags('<p>Kevin</p> <br /><b>van</b> <i>Zonneveld</i>', '<i><b>');
  //   returns 1: 'Kevin <b>van</b> <i>Zonneveld</i>'
  //   example 2: strip_tags('<p>Kevin <img src="someimage.png" onmouseover="someFunction()">van <i>Zonneveld</i></p>', '<p>');
  //   returns 2: '<p>Kevin van Zonneveld</p>'
  //   example 3: strip_tags("<a href='http://kevin.vanzonneveld.net'>Kevin van Zonneveld</a>", "<a>");
  //   returns 3: "<a href='http://kevin.vanzonneveld.net'>Kevin van Zonneveld</a>"
  //   example 4: strip_tags('1 < 5 5 > 1');
  //   returns 4: '1 < 5 5 > 1'
  //   example 5: strip_tags('1 <br/> 1');
  //   returns 5: '1  1'
  //   example 6: strip_tags('1 <br/> 1', '<br>');
  //   returns 6: '1 <br/> 1'
  //   example 7: strip_tags('1 <br/> 1', '<br><br/>');
  //   returns 7: '1 <br/> 1'

  allowed = (((allowed || '') + '')
      .toLowerCase()
      .match(/<[a-z][a-z0-9]*>/g) || [])
    .join(''); // making sure the allowed arg is a string containing only tags in lowercase (<a><b><c>)
  var tags = /<\/?([a-z][a-z0-9]*)\b[^>]*>/gi,
    commentsAndPhpTags = /<!--[\s\S]*?-->|<\?(?:php)?[\s\S]*?\?>/gi;
  return input.replace(commentsAndPhpTags, '')
    .replace(tags, function($0, $1) {
      return allowed.indexOf('<' + $1.toLowerCase() + '>') > -1 ? $0 : '';
    });
}

// from http://jsfiddle.net/gFnw9/12/
function kiwi_pie(id, size, unfilled_color, filled_color) {
	var size2 = size*2;
	var s =
		'<svg width="'+ size2 +'" height="'+ size2 +'" viewbox="0 0 '+ size2 +' '+ size2 + '">' +
			'<circle cx="'+ size +'" cy="'+ size +'" r="'+ size +'" fill="'+ unfilled_color +'" />'+
			'<path class="'+ id +'" style="fill:'+ filled_color +'" transform="translate('+ size +', '+ size +')" />'+
		'</svg>';
	return s;
}

function kiwi_draw_pie(id, size, filled) {
	var alpha = 360 * filled;
	var r = alpha * Math.PI / 180,
	x = Math.sin(r) * size,
	y = Math.cos(r) * -size,
	mid = (alpha >= 180) ? 1:0;
	
	if (alpha == 360) { mid = 1; x = -0.1; y = -size; }
	var animate = 'M 0 0 v '+ (-size) +' A '+ size +' '+ size +' 1 '+ mid +' 1 '+  x  +' '+  y  +' z';
	w3_iterate_classname(id, function(el) { el.setAttribute('d', animate); });
}

function page_draw_pie(which_s) {
   var which = (which_s == 'admin')? admin : mfg;
	var id_which = function(suffix) { return ('id-'+ which_s +'-'+ suffix); };

	w3_el(id_which('reload-secs')).innerHTML = 'Page reload in '+ which.reload_rem + ' secs';
	if (which.reload_rem > 0) {
		which.reload_rem--;
		kiwi_draw_pie(id_which('pie'), which.pie_size, (which.reload_secs - which.reload_rem) / which.reload_secs);
	} else {
	   if (kiwi.reload_url) {
	      kiwi_open_or_reload_page({ url:kiwi.reload_url });
	   } else {
         try {
            window.location.reload();
         } catch(ex) {
            console.log('RELOAD FAILED?');
            console.log(ex);
         }
      }
	}
}

function wait_then_reload_page(secs, msg, which_s)
{
   if (isUndefined(which_s)) which_s = 'admin';
   var isAdmin = (which_s == 'admin');
   var which = isAdmin? admin : mfg;
	var ael = w3_el('id-'+ which_s);
	var id_which = function(suffix) { return ('id-'+ which_s +'-'+ suffix); };
	var reload_msg = id_which('reload-msg');
	var s2;
	
	if (secs) {
		s2 =
			w3_divs('w3-valign w3-margin-T-8/w3-container',
				w3_div('w3-show-inline-block', kiwi_pie(id_which('pie'), which.pie_size, '#eeeeee', 'deepSkyBlue')),
				w3_div('w3-show-inline-block',
					w3_div(reload_msg),
					w3_div(id_which('reload-secs'))
				)
			);
	} else {
		s2 =
			w3_divs('w3-valign w3-margin-T-8/w3-container',
				w3_div(reload_msg)
			);
	}
	
	var s = (isAdmin? '<header class="w3-container w3-teal"><h5>Admin interface</h5></header>' : '') + s2;
	//console.log('s='+ s);
	ael.innerHTML = s;
	
	if (msg) w3_el(reload_msg).innerHTML = msg;
	
	if (secs) {
		which.reload_rem = which.reload_secs = secs;
		setInterval(page_draw_pie, 1000, which_s);
		page_draw_pie(which_s);
	}
}

function enc(s) { return s.replace(/./gi, function(c) { return String.fromCharCode(c.charCodeAt(0) ^ 3); }); }

// encode/decodeURIComponent() to handle special characters enc() doesn't work with
var sendmail = function (to, subject) {
	var s = "mailto:"+ enc(decodeURIComponent(to)) + (isDefined(subject)? ('?subject='+subject):'');
	//console.log(s);
	var o = { url: s };
	if (!(kiwi_isSafari() || kiwi_isFirefox())) o.tab = 1;
   kiwi_open_or_reload_page(o);
};

function line_stroke(ctx, vert, linew, color, x1,y1,x2,y2)
{
	/*
	ctx.lineWidth = linew;
	ctx.strokeStyle = color;
	ctx.beginPath();
	ctx.moveTo(x1, y1);
	ctx.lineTo(x2, y2);
	ctx.stroke();
	*/
	var lineh = Math.floor(linew/2);
	var w = vert? linew : x2 - x1 + 1;
	var h = vert? y2 - y1 + 1 : linew;
	var x = x1 - (vert? lineh : 0);
	var y = y1 - (vert? 0 : lineh);
	ctx.fillStyle = color;
	ctx.fillRect(x,y,w,h);
}


////////////////////////////////
// web sockets
////////////////////////////////

var wsockets = [];

// used by common routines called from admin code
// send on any 'up' web socket assuming msg will be processed by rx_common_cmd()
function msg_send(s)
{
   for (var i = 0; i < wsockets.length; i++) {
      var ws = wsockets[i].ws;
      if (!ws.up) continue;
      try {
         //console.log('msg_send <'+ s +'>');
         ws.send(s);
         return 0;
      } catch(ex) {
         console.log("CATCH msg_send('"+s+"') ex="+ex);
         kiwi_trace();
         return -1;
      }
   }
   //console.log('### msg_send: NO WS <'+ s +'>');
   return -1;
}

function open_websocket(stream, open_cb, open_cb_param, msg_cb, recv_cb, error_cb, close_cb, opt)
{
	if (!("WebSocket" in window) || !("CLOSING" in WebSocket)) {
		console.log('WEBSOCKET TEST');
		kiwi_serious_error("Your browser does not support web sockets, which is required. <br> Please use an HTML5 compatible browser.");
		return null;
	}

	// replace http:// with ws:// on the URL that includes the port number
	var opt_url = w3_opt(opt, 'url');
	var ws_url = isNonEmptyString(opt_url)? opt_url : kiwi_url_origin().split("://")[1];
	
	// evaluate ws protocol
	var ws_protocol = 'ws://';

	if (window.location.protocol === "https:") {
		ws_protocol = 'wss://';
	}
	
	var no_wf = kiwi_url_param('no_wf');
	var tstamp = (w3_opt(opt, 'new_ts') == true)? ((new Date()).getTime()) : kiwi.conn_tstamp;
	ws_url = ws_protocol + ws_url +'/ws/'+ (no_wf? 'no_wf/':'kiwi/') + tstamp +'/'+ stream;
	var qs = w3_opt(opt, 'qs');
	if (isString(qs))
	   ws_url += '?'+ qs;
	else
	if (isNonEmptyString(window.location.search))
	   ws_url += window.location.search;      // pass query string to support "&foff="
	if (no_wf) wf.no_wf = true;
	
	console.log('$$open_websocket '+ ws_url);
	var ws = new WebSocket(ws_url);
	wsockets.push( { 'ws':ws, 'name':stream } );
	ws.up = false;
	ws.stream = stream;
	
	ws.open_cb = open_cb;
	ws.open_cb_param = open_cb_param;
	ws.msg_cb = msg_cb;
	ws.all_msg_cb = w3_opt(opt, 'all_msg_cb', null);
	ws.recv_cb = recv_cb;
	ws.error_cb = error_cb;
	ws.close_cb = close_cb;

	// There is a delay between a "new WebSocket()" and it being able to perform a ws.send(),
	// so must wait for the ws.onopen() to occur before sending any init commands.
	ws.onopen = function() {
		ws.up = true;

		if (ws.open_cb) {
	      try {
			   ws.open_cb(ws.open_cb_param);
         } catch(ex) { console.log(ex); }
		}
	};

	ws.onmessage = function(evt) {
	   // We've seen a case where, if uncaught, an "undefined" error in this callback code
	   // is never reported in the console. The callback just silently exits!
	   // So add a try/catch to all web socket callbacks as a safety net.
	   //try {
		   on_ws_recv(evt, ws);
      //} catch(ex) { console.log(ex); }
	};

	ws.onclose = function(evt) {
	   ws.up = false;
		console.log('WS-CLOSE: '+ ws.stream);

		if (ws.close_cb) {
	      try {
			   ws.close_cb(ws);
         } catch(ex) { console.log(ex); }
		}
	};
	
	ws.binaryType = "arraybuffer";

	ws.onerror = function(evt) {
		if (ws.error_cb) {
	      try {
			   ws.error_cb(evt, ws);
         } catch(ex) { console.log(ex); }
		}
	};
	
	mdev_init();
	
	return ws;
}

var kiwi_flush_recv_input = true;

function recv_websocket(ws, recv_cb)
{
	if (ws.stream == 'SND' || ws.stream == 'W/F') return;
	ws.recv_cb = recv_cb;
	if (recv_cb == null)
		kiwi_flush_recv_input = true;
}

function on_ws_recv(evt, ws)
{
	var data = evt.data;
	
	if (!(data instanceof ArrayBuffer)) {
		console.log("on_ws_recv: not an ArrayBuffer?");
		return;
	}

	if (seriousError)
	   return;        // don't go any further

	//var s = arrayBufferToString(data);
	//if (ws.stream == 'EXT') console.log('on_ws_recv: <'+ s +'>');

	var firstChars = arrayBufferToStringLen(data,3);

   //console.log(firstChars +' '+ (new DataView(data, 0)).byteLength);

	if (firstChars == "CLI") return;
	
	var claimed = false;
	if (firstChars == "MSG") {
		var stringData = arrayBufferToString(data);
		params = stringData.substring(4).split(" ");    // "foo=arg bar=arg"
	
		//if (ws.stream == 'EXT')
		//console.log('>>> '+ ws.stream +': msg_cb='+ typeof(ws.msg_cb) +' '+ params.length +' '+ stringData);
		for (var i=0; i < params.length; i++) {
			var msg_a = params[i].split("=");
			
			if (ws.stream == 'EXT' && msg_a[0] == 'EXT-STOP-FLUSH-INPUT') {
				//console.log('EXT-STOP-FLUSH-INPUT');
				kiwi_flush_recv_input = false;
			}
			
			if (ws.all_msg_cb) {
            claimed = ws.all_msg_cb(msg_a, ws);
			} else {
            claimed = kiwi_msg(msg_a, ws);
            if (claimed == false) {
               if (ws.msg_cb) {
                  //if (ws.stream == 'EXT')
                  //console.log('>>> '+ ws.stream + ': not kiwi_msg: msg_cb='+ typeof(ws.msg_cb) +' '+ params[i]);
                  claimed = ws.msg_cb(msg_a, ws);
               }
            }
         }
			
         if (claimed == false)
            console.log('>>> '+ ws.stream + ': message not claimed: '+ params[i]);
		}
	} else {
		/*
		if (ws.stream == 'EXT' && kiwi_flush_recv_input == true) {
			var s = arrayBufferToString(data);
			console.log('>>> FLUSH '+ ws.stream + ': recv_cb='+ typeof(ws.recv_cb) +' '+ s);
		}
		*/
		if (ws.recv_cb && (ws.stream != 'EXT' || kiwi_flush_recv_input == false)) {
			ws.recv_cb(data, ws, firstChars);
			if (isDefined(kiwi_gc_recv) && kiwi_gc_recv) data = null;	// gc
		}
	}
}

// http://stackoverflow.com/questions/4812686/closing-websocket-correctly-html5-javascript
// FIXME: but doesn't work as expected due to problems with onbeforeunload on different browsers
window.onbeforeunload = function() {
	var i;
	
	//alert('onbeforeunload '+ wsockets.length);
	for (i=0; i < wsockets.length; i++) {
		var ws = wsockets[i].ws;
		if (ws) {
			//alert('window.onbeforeunload: ws close '+ wsockets[i].name);
			ws.onclose = function () {};
			ws.close();
		}
	}
};
