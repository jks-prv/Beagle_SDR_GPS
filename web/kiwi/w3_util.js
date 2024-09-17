// Copyright (c) 2016-2024 John Seamons, ZL4VO/KF6VO

/*

	////////////////////////////////
	// API summary (w3_*)
	////////////////////////////////
	
	                           integrated: L=label T=text 3=psa3()
	                           |  callback
	                              |  first: F=always false, not called on instan
	                                 |
	nav navdef

	label set_label

	link
   
	radio_button               T     no label option needed
	radio_button_get_param     T     w3-defer
	radio_unhighlight
	
	switch                     T
	switch_label               TL    rename w3_switch_label() to w3_switch()? Any external API users?
	switch_label_get_param           w3-defer
	switch_set_value
	switch_get_param                 w3-defer
	
	button                     T     w3-noactive w3-momentary w3-custom-events w3-disabled
    button_path
    button_text
	icon                       T     w3-momentary
	 spin
	
	input                      L3 cb F
	input_change
	input_get                        w3-defer
	
	textarea                   L
	textarea_get_param         L     w3-defer
	
	checkbox                   L3
	checkbox_get_param         L     w3-defer
	checkbox_get
	checkbox_set
	
	select                     L3
	select_hier                L
	select_get_param           L     w3-defer
	select_enum
	select_value
	select_set_if_includes
	
	slider                     L3
	slider_set
	slider_get_param                 w3-defer
	
	menu
	menu_items
	menu_popup
	menu_onclick
	
	inline                     3
	inline_percent             3
	divs                       3
	col_percent                3
	
	
	////////////////////////////////
	// Useful stuff
	////////////////////////////////
	
	<element attribute="attribute-values ..." inline-style-attribute="properties ...">
	"styles" refers to any style-attribute="properties ..." combination

	style properties set one of three ways:
		1) in an element as above
		
		2) in a .css:
		selector
		{
			property: property-value;
		}
		
		3) assigned by DOM:
		el.style.property = property-value;
	
	priority (specificity)
	
		[inline-style attribute], [id], [class, pseudo-class, attribute], [elements]
		
		The "count" at each level is determined by the number of specifiers, hence the "specific-ness"
		e.g. ul#nav li.active a => 0,1,1,3 with 3 due to the ul, li and a
		This would have priority over 0,1,1,2
	
	select (menu) behavior:
	   If you set the background-color inline then border reverts to the undesirable beveled appearance!

	in w3.css:
		w3-show-block
		w3-show-inline-block
		
		w3-section: margin TB 16px
		w3-container: padding TB 0.01em LR 16px
		w3-row:after: content: ""; display: table; clear: both
		w3-col: float left, width 100%
		w3-padding: T 8px, B 16px
		w3-row-padding: LR 8px
		w3-margin: TBLR 16px

	in w3_ext.css:
		w3-show-inline
		w3-valign
		w3-override-(colors)

	id="foo" on...(e.g. onclick)=func(this.id)


	Be sure to document our special side-effects:
	   w3-label-inline


	////////////////////////////////
	// Notes about HTML/DOM
	////////////////////////////////
	
	"Typically, the styles are merged, but when conflicts arise, the later declared style will generally win
	(unless the !important attribute is specified on one of the styles, in which case that wins).
	Also, styles applied directly to an HTML element take precedence over CSS class styles."
	
	window.
		inner{Width,Height}		does not include tool/scroll bars
		outer{Width,Height}		includes tool/scroll bars
	
	element.
		client{Width,Height}		viewable only; no: border, scrollbar, margin; yes: padding
		offset{Width,Height}		viewable only; includes padding, border, scrollbars

	
	////////////////////////////////
	// FIXME cleanups
	////////////////////////////////
	
	some routines that return nothing now could return el from internal w3_el() so
	   caller could do chained references (see w3_innerHTML(), w3_show() ...)
	
	needs uniform instantiation callbacks
	   - w3_nav
	   x w3_navdef
	   >>> w3_radio_button
	   >>> w3_switch
	   - w3_button (only 1 initial state)
	   - w3_icon
	   ??? w3_input(_psa): integrate, 
	   - w3_textarea
	   x w3_checkbox
	   x w3_select
	   x w3_slider
	   - w3_menu (no initial selection)
	
	general conversion to w3-flex layout
	w3_col_percent => w3_inline_percent
	kiwi/{admin,admin_sdr}.js: w3_select('' => w3_select(w3-text-red
	unify color setting: style.color vs w3-color, w3-text-color
	uniform default/init control values
	preface internal routines/vars with w3int_...
	move some routines (esp HTML) out of kiwi_util.js into here?
	make all 'id-', 'cl-' use uniform
	collapse into one func the setting of cfg value and el/control current displayed value
	w3-valign vs w3-show-inline vs w3-show-inline-block

	x use DOM el.classList.f() instead of ops on el.className
	x normalize use of embedded labels
	
	
	////////////////////////////////
	// API users
	////////////////////////////////
	
	1) kiwisdr.com website content
	
	2) antenna switch extension is a user of API:
	   visible_block()         DEP
	   w3_div()
	   w3_divs()
	   w3_inline()                   only in OLDER versions of the ant_switch ext
	   w3_btn()
	   w3_radio_btn(yes/no)    DEP   w3_radio_button
	   w3_string_set_cfg_cb()
	   w3_highlight()
	   w3_unhighlight()
	   w3_radio_unhighlight()
	   var w3_highlight_time
	   
	   ext:
	      ext_{get,set}_cfg_param
	         EXT_SAVE, EXT_NO_SAVE
	      ext_set_controls_width_height
	      ext_send
	      ext_admin_config
	      ext_param
	      ext_panel_show
	      ext_switch_to_client

*/


var w3 = {
   _last_: 0
};

var w3int = {
   btn_grp_uniq: 0,
   
   menu_cur_id: null,
   menu_active: false,
   menu_debug: false,
   menu_close_func: null,
   prev_menu_hover_el: null,
   
   reload_timeout: null,
   reload_seq: 0,
   
   rate_limit: {},
   rate_limit_need_shift: {},
   rate_limit_evt: {},
   
   eventListener: [],
   
   _last_: 0
};


////////////////////////////////
// deprecated
////////////////////////////////

function visible_block() {}      // FIXME: used by OLDER versions of the antenna switch ext


////////////////////////////////
// util
////////////////////////////////

var w3_console = {};

// XXX: Made another attempt to get this to work correctly.
// But is still has lots of corner-case failures compared to "console.log('foo='+ foo +' bar='+ bar)"
// e.g. wrt undefined, null and object/function values
w3_console.log = function(obj, prefix)
{
   var s = prefix? (prefix + ': ') : '';
   if (isString(obj)) {
      s += '"'+ obj.toString() +'"';
   } else
   if (isObject(obj)) {
      console.log(obj);
      var json = 'JSON '+ JSON.stringify(obj, w3int_console_replacer);
      if (json.includes('\n'))
         json = JSON.stringify(obj, w3int_console_replacer, 1);   // if json is multi-line, redo using pretty-print
      s += json;
   } else {
      s += obj.toString();
   }
   console.log(s);
   //if (isObject(obj)) console.log(obj);
};

function w3int_console_replacer(k, v)
{
   if (isUndefined(v)) return '<undefined>';
   if (isNull(v)) return '<null>';
   if (isFunction(v)) return '<function>';
   return '<'+ v.toString() +'>';
}

function w3_json_to_string_no_decode(id, s)
{
   s = s.replace(/\\"/g, '"');
   return s;
}

function w3_json_to_string(id, s)
{
   s = kiwi_decodeURIComponent(id, s);
   return w3_json_to_string_no_decode(id, s);
}

function w3_json_to_html_no_decode(id, s)
{
   //console.log('1> '+ kiwi_string_to_hex(s, '-'));
   //console.log('1> '+ JSON.stringify(s));
   s = s.replace(/\\n/g, '<br>').replace(/\n/g, '<br>').replace(/\\"/g, '"');
   //console.log('2> '+ JSON.stringify(s));
   return s;
}

function w3_json_to_html(id, s)
{
   s = kiwi_decodeURIComponent(id, s);
   return w3_json_to_html_no_decode(id, s);
}

function w3_html_to_nl(s)
{
   return s.replace(/<br>/g, '\n').replace(/\\n/g, '\n');
}

function w3_strip_quotes(s)
{
	if (isString(s) && (s.indexOf('\'') != -1 || s.indexOf('\"') != -1))
		return s.replace(/'/g, '').replace(/"/g, '') + ' [quotes stripped]';
	return s;
}

function w3_esc_dq(s)
{
	if (isString(s) && s.indexOf('\"') != -1)
		return s.replace(/"/g, '&quot;');
	return s;
}

// a multi-argument call that silently continues if func not found
function w3_call(func, arg0, arg1, arg2, arg3, arg4)
{
   var rv;

   if (isNoArg(func)) return rv;
   
	try {
	   if (isString(func)) {
         var f = getVarFromString(func);
         //console.log('w3_call: '+ func +'() = '+ typeof(f));
         if (isFunction(f)) {
            //var args = Array.prototype.slice.call(arguments);
            rv = f(arg0, arg1, arg2, arg3, arg4);
         } else {
            //console.log('w3_call: getVarFromString(func) not a function: '+ func +' ('+ typeof(f) +')');
         }
      } else
	   if (isFunction(func)) {
         rv = func(arg0, arg1, arg2, arg3, arg4);
	   } else {
	      console.log('w3_call: func not a string or function');
	      console.log(JSON.stringify(func));
	      //kiwi_trace();
	   }
	} catch(ex) {
		console.log('w3_call: while in func this exception occured:');
		console.log(ex);
		//console.log('w3_call: '+ ex.toString());
		//console.log(ex.stack);
	}
	
	return rv;
}

function w3_first_value(v)
{
   //console.log('w3_first_value');
   //console.log(v);
   if (v == null)
      return 'null';

   var rv = null;
   var to_v = typeof(v);
   
   if (to_v === 'number' || to_v === 'boolean' || to_v === 'string') {
      //console.log('w3_first_value prim');
      rv = v;
   } else
   if (isArray(v)) {
      //console.log('w3_first_value array');
      rv = v[0];
   } else
   if (to_v === 'object') {   // first value in object no matter what the key
      //console.log('w3_first_value obj');
      rv = w3_obj_seq_el(v, 0);
   } else {
      //console.log('w3_first_value other');
      rv = to_v;
   }
   //console.log('w3_first_value rv='+ rv);
   return rv;
}

function w3_second_value(v)
{
   //console.log('w3_second_value');
   //console.log(v);
   var rv = null;
   
   if (isArray(v) && v.length > 1) {
      //console.log('w3_second_value array[1]');
      if (v.length > 1)
         rv = v[1];
   }

   //console.log('w3_second_value rv='+ rv);
   return rv;
}

function w3_opt(opt, elem, default_val, pre, post)
{
   //console.log('w3_opt opt:');
   //console.log(opt);
   var s;
   if (isArg(opt) && isDefined(opt[elem])) {
      //console.log('w3_opt elem='+ elem +' DEFINED rv='+ opt[elem]);
      s = opt[elem];
   } else {
      //console.log('w3_opt elem='+ elem +' NOT DEFINED rv='+ default_val);
      s = default_val;
   }
   if (isNonEmptyString(s)) {
      if (isNonEmptyString(pre)) s = pre + s;
      if (isNonEmptyString(post)) s = s + post;
   }
   return s;
}

function w3_obj_num(o)
{
   if (isUndefined(o) || isNull(o) || isObject(o)) return o;
   if (isNumber(o)) return { num: o };
   if (isBoolean(o)) return { num: (o? 1:0) };
   if (isString(o)) return { num: parseFloat(o) };
   return o;
}

// given a sequential position in an object's keys, return the corresponding object element for that key
function w3_obj_seq_el(obj, idx)
{
   var keys_a = Object.keys(obj);
   if (!isNumber(idx) || idx < 0 || idx >= keys_a.length) return null;
   return obj[keys_a[idx]];
}

// given an object key, return its sequential position in that object
function w3_obj_key_seq(obj, key)
{
   var seq = null;
   Object.keys(obj).forEach(function(_key, i) {
      if (_key == key) seq = i;
   });
   return seq;
}

function w3_array_el_seq(arr, el)
{
   var seq = null;
   arr.forEach(function(a_el, i) {
      if (a_el == el) seq = i;
   });
   return seq;
}

function w3_obj_enum(obj, func, opt)
{
   var key_match = w3_opt(opt, 'key_match', undefined);
   var value_match = w3_opt(opt, 'value_match', undefined);

   Object.keys(obj).forEach(function(key, i) {
      var o = obj[key];
      if (isDefined(key_match)) {
         if (key == key_match) func(key, i, o);
      } else
      if (isDefined(value_match)) {
         if (o == value_match) func(key, i, o);
      } else {
         func(key, i, o);
      }
   });
}

function w3_enum_obj_or_array_of_objs(obj_arr, func)
{
   if (isArray(obj_arr)) {
      obj_arr.forEach(function(obj) {
         func(obj);
      });
   } else
   
   if (isObject(obj_arr)) {
      func(obj_arr);
   }
}

// arr:     [] string vals to iterate over
// s:       string val to match with startsWith() (case-insensitive)
// func:    (optional) called as func(arr-idx, val) only on first match
// returns: found = true|false
function w3_ext_param_array_match_str(arr, s, func)
{
   var found = false;
   arr.forEach(function(a_el, i) {
      var el = a_el.toString().toLowerCase();
      //console.log('w3_ext_param_array_match_str CONSIDER '+ i +' '+ el +' '+ s);
      if (!found && el.startsWith(s)) {
         //console.log('w3_ext_param_array_match_str MATCH '+ i);
         if (func) func(i, a_el.toString());
         found = true;
      }
   });
   return found;
}

// arr:     [] vals to iterate over
// n:       num val to match
// func:    (optional) called as func(arr-idx, val) only on first match
// returns: found = true|false
function w3_ext_param_array_match_num(arr, n, func)
{
   var found = false;
   arr.forEach(function(a_el, i) {
      var a_num = parseFloat(a_el);
      //console.log('w3_ext_param_array_match_num CONSIDER '+ i +' '+ a_num +' '+ n);
      if (!found && a_num == n) {
         //console.log('w3_ext_param_array_match_num MATCH '+ i);
         if (func) func(i, n);
         found = true;
      }
   });
   return found;
}

// s:       name.startsWith(s)   case-insensitive
//          null                 wildcard
// param:   name[:val]
// returns: { match:true|false, full_match:true|false, has_value:true|false, num:parseFloat(val), string:val, string_case:val, items:[] }
function w3_ext_param(s, param)
{
   var rv = { match:false, full_match:false, has_value:false };
   var pu = param.split(':');
   var pl = param.toLowerCase().split(':');
   //console.log('w3_ext_param: s='+ s +' pu='+ pu[0] +'|'+ pu[1]);
   if (isNull(s) || s.startsWith(pl[0])) {
      rv.match = true;
      if (isNull(s)) {
         rv.has_value = true;
         rv.num = parseFloat(param);
         rv.string = param;
         rv.string_case = param;
         rv.items = [];
      } else
      if (pl.length > 1) {
         rv.has_value = true;
         rv.num = parseFloat(pl[1]);
         rv.string = pl[1];
         rv.string_case = pu[1];
         rv.items = pu;
      } else {
         rv.num = 0;
         rv.string = rv.string_case = '';
         rv.items = [];
      }
   }
   if (s && s == pl[0]) rv.full_match = true;
   //console.log(rv);
   return rv;
}

function w3_clamp(v, min, max, clamp_val)
{
   if (isNaN(v)) return (isDefined(clamp_val)? clamp_val : NaN);
   
   if (isObject(min)) {
      var o = min;
      try {
         if (typeof(o.clamp) === 'undefined') {
            v = Math.max(o.min, Math.min(o.max, v));
         } else {
            if (v < o.min || v > o.max) v = o.clamp;
         }
      } catch(ex) {
         console.log('w3_clamp: bad obj:');
         console.log(o);
         return undefined;
      }
   } else {
      if (clamp_val == undefined)
         v = Math.max(min, Math.min(max, v));
      else
         if (v < min || v > max) v = clamp_val;
   }
   
   return v;
}

// to conditionally test for out-of-range use as:
// if (w3_clamp3(v, min, max)) { in-range } else { out-of-range }
function w3_clamp3(v, min, max, clamp_min, clamp_max, clamp_NaN)
{
   if (isUndefined(clamp_min)) clamp_min = NaN;
   if (isUndefined(clamp_max)) clamp_max = NaN;
   if (isUndefined(clamp_NaN)) clamp_NaN = NaN;
   if (isNaN(v)) return clamp_NaN;
   if (v < min) return clamp_min;
   if (v > max) return clamp_max;
   return v;
}


////////////////////////////////
// HTML
////////////////////////////////

function w3_add_id(path, suffix)
{
   if (!isString(path)) return path;
   path = path || '';
   if (path == '' || path == 'body') return path;
   path = path.startsWith('id-')? path : ('id-'+ path);
   suffix = suffix || '';
   return path + suffix;
}

// return document element reference either by id or name
function w3int_w3_el(id_name_class)
{
   if (!id_name_class || id_name_class == '') return null;
	var el = document.getElementById(id_name_class);
	if (el == null) {
		el = document.getElementsByName(id_name_class);		// 'name=' is deprecated
		if (el != null) el = el[0];	// use first from array
	}
	if (el == null) {
		el = document.getElementsByClassName(id_name_class);
		if (el != null) el = el[0];	// use first from array
	}
	return el;
}

function w3int_w3_els(id_name_class)
{
   if (!id_name_class || id_name_class == '') return null;
	var els = document.getElementById(id_name_class);
	var rv;
	if (els != null) {
	   rv = [els];
	} else {
      els = document.getElementsByName(id_name_class);		// 'name=' is deprecated
      if (els == null || els.length == 0) {
         els = document.getElementsByClassName(id_name_class);
      }
      rv = els;
   }
   if (rv.length == 0) rv = null;
	return rv;  // returns single-element array or iterable HTMLCollection
}

// allow an element-obj or element-id to be used
// try id without, then with, leading 'id-'; then including cfg prefix as a last resort
function w3_el(el_id)
{
	if (isString(el_id)) {
	   if (el_id == '') return null;
	   if (el_id == 'body') return document.body;
		var el = w3int_w3_el(el_id);
		if (el == null) {
			el = w3int_w3_el(w3_add_id(el_id));
			if (el == null) {
				el_id = w3_add_toplevel(el_id);
				el = w3int_w3_el(el_id);
				if (el == null) {
					el = w3int_w3_el(w3_add_id(el_id));
				}
			}
		}
		return el;
	}
	return (el_id);
}

// for when there are multiple elements with the same "id-*"
// returns element list and optionally calls iterates calling func()
function w3_els(el_id, func)
{
	if (isString(el_id)) {
	   if (el_id == '') return null;
	   if (el_id == 'body') return document.body;
		var els = w3int_w3_els(el_id);
		if (els == null) {
			els = w3int_w3_els(w3_add_id(el_id));
			if (els == null) {
				el_id = w3_add_toplevel(el_id);
				els = w3int_w3_els(el_id);
				if (els == null) {
					els = w3int_w3_els(w3_add_id(el_id));
				}
			}
		}
      if (els && func) for (var i = 0; i < els.length; i++) {
         func(els[i], i);
      }
		return els;
	}
	
	var el = w3_el(el_id);
	if (func) {
	   func(el, 0);
	}
	return [el];
}

// return id of element (i.e. 'id-*') if found
function w3_id(el_id)
{
	var el = w3_el(el_id);
	if (!el) return 'id-NULL';
	if (el.id && el.id.startsWith('id-')) return el.id;
	var done = false;
	var id = null;
	w3_iterate_classList(el, function(className, idx) {
      //console.log('w3_id CONSIDER '+ className);
	   if (className.startsWith('id-') && !done) {
	      id = className;
	      done = true;
	   }
	});
	return (id? id : 'id-NULL');
}

// assign innerHTML, silently failing if element doesn't exist
function w3_innerHTML(id)
{
	//var el = w3_el_softfail(id);
	var el = w3_el(id);
	if (!el) return null;
	
	// get
	if (arguments.length == 1)
	   return el.innerHTML;
	
	// set
	var s = '';
	var narg = arguments.length;
   for (var i=1; i < narg; i++) {
      s += arguments[i];
   }
	el.innerHTML = s;
	return el;
}

function w3_clearInnerHTML(id)
{
   w3_innerHTML(id, '');
}

function w3_append_innerHTML(id)
{
	var el = w3_el(id);
	if (!el) return null;
	var s = '';
	var narg = arguments.length;
   for (var i=1; i < narg; i++) {
      s += arguments[i];
   }
	el.innerHTML += s;
	return el;
}

function w3_get_innerHTML(id)
{
	var el = w3_el(id);
	if (!el) return null;
	return el.innerHTML;
}

function w3_iterate_classname(cname, func)
{
	var els = document.getElementsByClassName(w3_add_id(cname));
	if (els == null) return;
	for (var i=0; i < els.length; i++) {      // els is a collection, can't use forEach()
		func(els[i], i);
	}
}

function w3_iterate_classList(el_id, func)
{
	var el = w3_el(el_id);
	if (el && el.classList) for (var i = 0; i < el.classList.length; i++) {     // el.classList is a collection, can't use forEach()
		func(el.classList.item(i), i);
	}
	return el;
}

function w3_create_appendElement(el_parent, el_type, html)
{
   var el_child = document.createElement(el_type);
   w3_innerHTML(el_child, html);
	w3_el(el_parent).appendChild(el_child);
	return el_child;
}

function w3_iterate_parent(el_id, func)
{
	var el = w3_el(el_id);
	var i = 0;
	
	do {
		if (func(el, i) != null)
		   break;
		el = el.parentNode;
		i++;
	} while (el);
}

// excludes text and comment nodes
function w3_iterate_children(el_id, func)
{
	var el = w3_el(w3_add_id(el_id));
	
	for (var i=0; i < el.children.length; i++) {    // el.children is a collection, can't use forEach()
		var child_el = el.children[i];
		func(child_el, i);
	}
}

// excludes text and comment nodes
function w3_iterateDeep_children(el_id, func, level)
{
	var el = w3_el(el_id);
	if (isUndefined(level)) level = 0;
	
	for (var i=0; i < el.children.length; i++) {    // el.children is a collection, can't use forEach()
		var child_el = el.children[i];
		func(child_el, level, i);
		if (child_el.hasChildNodes)
			w3_iterateDeep_children(child_el, func, level+1);
	}
}

// includes text and comment nodes
function w3_iterate_childNodes(el_id, func)
{
	var el = w3_el(el_id);
	
	for (var i=0; i < el.childNodes.length; i++) {    // el.childNodes is a collection, can't use forEach()
		var child_el = el.childNodes[i];
		func(child_el, i);
	}
}

function w3_width_height(el_id, w, h)
{
	var el = w3_el(el_id);
	if (!el) return null;
	
	if (isArg(w)) {
	   if (isNumber(w)) w = px(w);
	   el.style.width = w;
	}

	if (isArg(h)) {
	   if (isNumber(h)) h = px(h);
	   el.style.height = h;
	}
}

// bounding box measured from the origin of parent
function w3_boundingBox_children(el_id, debug)
{
   var el = w3_el(el_id);
	var bbox = { id:el_id, x1:1e99, x2:0, y1:1e99, y2:0, w:0, h:0 };
	var found = false;
	w3_iterateDeep_children(el_id, function(el, level, i) {
		if (el.nodeName != 'DIV' && el.nodeName != 'SPAN' && el.nodeName != 'IMG')
			return;
		var position = css_style(el, 'position');
		if (position == 'static')
		   return;
		if (el.offsetHeight == 0)
		   return;
		if (debug) console.log('level='+ level +' i='+ i);
		if (debug) console.log(el);
		if (!found) { bbox.el = el; found = true; }
		//console.log(el.offsetParent);
		if (debug) console.log(el.nodeName +' el.oL='+ el.offsetLeft +' el.oW='+ el.offsetWidth +' el.oT='+ el.offsetTop +' el.oH='+ el.offsetHeight +' '+ position);

		bbox.x1 = Math.min(bbox.x1, el.offsetLeft);
		var x2 = el.offsetLeft + el.offsetWidth;
		bbox.x2 = Math.max(bbox.x2, x2);

		bbox.y1 = Math.min(bbox.y1, el.offsetTop);
		var y2 = el.offsetTop + el.offsetHeight;
		bbox.y2 = Math.max(bbox.y2, y2);
	});

	bbox.w = bbox.x2 - bbox.x1;
	bbox.h = bbox.y2 - bbox.y1;
	if (bbox.w == -1e+99) { bbox.w = bbox.h = 0; }
	//if (debug) w3_console.log(bbox, 'w3_boundingBox_children');
	if (debug) console.log(bbox);
	return bbox;
}

function w3_center_in_window(el_id, id)
{
	var el = w3_el(el_id);
	var rv = (window.innerHeight - el.clientHeight) / 2;
	//console.log('w3_center_in_window wh='+ window.innerHeight +' ch='+ el.clientHeight +' rv='+ rv + (id? (' '+ id) : ''));
	return rv;
}

// conditionally select field element
function w3_field_select(el_id, opts)
{
	var el = w3_el(el_id);
	el = (el && isFunction(el.select))? el : null;

   var trace = opts['trace'];
   if (trace) {
      var id = isObject(el_id)? el_id.id : el_id;
      console.log('w3_field_select id='+ id +' el='+ el +' v='+ (el? el.value:null));
      console.log(el);
      console.log(opts);
   }
   if (!el) return;
   
   var focus=0, select=0, blur=0;
   if (opts['mobile'] && kiwi_isMobile()) blur = 1; else focus = select = 1;
   if (opts['blur']) blur = 1;
   if (opts['focus_select']) focus = select = 1;
   if (opts['focus_only']) { focus = 1; select = 0; }
   if (opts['select_only']) select = 1;
   
   if (trace) console.log('w3_field_select focus='+ focus +' select='+ select +' blur='+ blur);
   //if (opts['log']) canvas_log('$'+ opts['log'] +': F='+ focus +' S='+ select +' B='+ blur);
   if (focus) el.focus();
   if (select) el.select();
   if (blur) el.blur();
   if (trace) kiwi_trace();
}

// add, remove or check presence of class properties
function w3_add(el_id, props)
{
   if (!el_id) return null;
   
   var first_el = null;
   // FIXME: why doesn't this work?!?
   //w3_els(el_id, function(el) {
   var el = w3_el(el_id);
      if (!el) return null;
      if (first_el) return;
      //console.log('$w3_add '+ el_id +' <'+ props +'>');
      if (!el || !props) return null;
      props = props.replace(/\s+/g, ' ').split(' ');
      props.forEach(function(p) {
         el.classList.add(p);
      });
      if (!first_el) first_el = el;
   //});

	return first_el;
	//return el;
}

function w3_remove(el_id, props)
{
   if (!el_id) return null;
   
   var first_el = null;
   //w3_els(el_id, function(el) {
   var el = w3_el(el_id);
      if (!el) return null;
      if (!first_el) first_el = el;
      //console.log('$w3_remove <'+ props +'>');
      if (!el || !props) return null;
      props = props.replace(/\s+/g, ' ').split(' ');
      props.forEach(function(p) {
         el.classList.remove(p);
      });
   //});

	return first_el;
}

function w3_match_wildcard(el_id, prefix)
{
	var el = w3_el(el_id);
	//console.log('w3_match_wildcard <'+ prefix +'>');
	if (!el) return null;
	for (var i = 0; i < el.classList.length; i++) {    // el.classList is a collection, can't use forEach()
	   var cl = el.classList.item(i);
	   //console.log('w3_match_wildcard CONSIDER <'+ cl +'>');
	   if (cl.startsWith(prefix))
	      return cl;
	}
	return false;
}

function w3_remove_wildcard(el_id, prefix)
{
	var el = w3_el(el_id);
	//console.log('w3_remove_wildcard <'+ prefix +'>');
	if (!el) return null;
	for (var i = 0; i < el.classList.length; i++) {    // el.classList is a collection, can't use forEach()
	   var cl = el.classList.item(i);
	   if (cl.startsWith(prefix)) el.classList.remove(cl);
	}
	return el;
}

function w3_set_props(el_id, props, cond)
{
	var el = w3_el(el_id);
	if (!el) return el;
	cond = isArg(cond)? cond : true;

	if (cond)
	   w3_add(el, props);
	else
	   w3_remove(el, props);
	return el;
}

function w3_remove_then_add(el_id, r_props, a_props)
{
	w3_remove(el_id, r_props);
	w3_add(el_id, a_props);
}

function w3_remove_then_add_cond(el_id, cond, t_props, f_props)
{
	w3_remove(el_id, t_props +' '+ f_props);
	w3_add(el_id, cond? t_props : f_props);
}

function w3_contains(el_id, prop)
{
	var el = w3_el(el_id);
	if (!el) return 0;
	var clist = el.classList;
	return (!clist || !clist.contains(prop))? 0:1;
}

function w3_parent_with(el_id, prop)
{
	var el = w3_el(el_id);
   if (!el) return;
	
	var found = null;
	w3_iterate_parent(el, function(parent) {
	   if (!found && w3_contains(parent, prop)) {
	      found = parent;
	   }
	});
	return found;
}

function w3_toggle(el_id, prop)
{
	var el = w3_el(el_id);
	if (!el) return;
	if (w3_contains(el, prop)) {
		w3_remove(el, prop);
		//console.log('w3_toggle: hiding '+ el_id);
	} else {
		w3_add(el, prop);
		//console.log('w3_toggle: showing '+ el_id);
	}
	return el;
}

function w3_appendAllClass(cname, prop)
{
	w3_iterate_classname(cname, function(el) { el.classList.add(prop); });
}
	
function w3_setAllHref(cname, href)
{
	w3_iterate_classname(cname, function(el) { el.href = href; });
}

// Can't simply do "el.style.display =" for these since things like
// w3-hide are declared "!important" by w3.css
// Must actually remove/insert them from the class property.
function w3_show(el_id, display)
{
	var el = w3_el(el_id);
	if (!el) return el;
	
	w3_remove(el, 'w3-hide');
	w3_add(el, display? display : 'w3-show-inline-block');
	return el;
}

function w3_create_attribute(el_id, name, val)
{
	var el = w3_el(el_id);
	if (el == null) return;
	var attr = document.createAttribute(name);
	attr.value = val;
	el.setAttributeNode(attr);
}

function w3_attribute(el_id, name, val, cond)
{
	var el = w3_el(el_id);
	if (el == null) return null;
	if (isUndefined(cond) || cond == true)
	   el.setAttribute(name, val);   // repeated sets only update (i.e. don't create duplicate attrs)
	else
	   el.removeAttribute(name);
}

function w3_show_block(el_id)
{
   return w3_show(el_id, 'w3-show-block');
}

function w3_show_inline_block(el_id)
{
   return w3_show(el_id, 'w3-show-inline-block');
}

function w3_show_inline(el_id)
{
   return w3_show(el_id, 'w3-show-inline-new');
}

function w3_show_table_cell(el_id)
{
   return w3_show(el_id, 'w3-show-table-cell');
}

function w3_hide(el_id)
{
	var el = w3_el(el_id);
	if (!el) return el;
	
   //w3_console.log(el, 'w3_hide BEGIN');
	el = w3_iterate_classList(el, function(className, idx) {
      //console.log('w3_hide CONSIDER '+ className);
	   if (className.startsWith('w3-show-')) {
         //console.log('w3_hide REMOVE '+ className);
	      w3_remove(el, className);
	   }
	});
	w3_add(el, 'w3-hide');
   //w3_console.log(el, 'w3_hide END');
	return el;
}

// if overrides are done correctly shouldn't need to remove any w3-show-*
// only toggle presence of w3-hide
function w3_hide2(el_id, cond)
{
	var el = w3_el(el_id);
	if (!el) return el;
	cond = isArg(cond)? cond : true;
	
	w3_set_props(el, 'w3-hide', cond);
	return el;
}

function w3_show_hide(el_id, show, display, n_parents_up)
{
   var rv;
   var el = w3_el(el_id);
   //w3_console.log(el, 'w3_show_hide BEGIN show='+ show);
   
   if (isNumber(n_parents_up))
      for (var i = 0; i < n_parents_up; i++) el = el.parentElement;
   
   if (show) {
      rv = w3_show(el, display? display : 'w3-show-block');
   } else {
      rv = w3_hide(el);
   }
   //w3_console.log(el, 'w3_show_hide END');
   return rv;
}

function w3_show_hide_inline(el, show)
{
   w3_show_hide(el, show, 'w3-show-inline-new');
}

function w3_disable(el_id, disable)
{
   el = w3_el(el_id);
   //console.log('w3_disable disable='+ disable +' t/o(el)='+ typeof(el) +' nodeName='+ el.nodeName);
   //console.log(el);
	w3_set_props(el, 'w3-disabled', disable);
	
	// for disabling menu popup and sliders
	if (isDefined(el.nodeName) && (el.nodeName == 'SELECT' || el.nodeName == 'INPUT')) {
	   try {
         if (disable)
            el.setAttribute('disabled', '');
         else
            el.removeAttribute('disabled');
      } catch(ex) {
         console.log('w3_disable:Attribute');
         console.log(ex);
      }
   }
	
	return el;
}

function w3_disable_multi(el_id, disable)
{
   w3_els(el_id,
      function(el, i) {
         //console.log('w3_disable_multi disable='+ disable +' t/o(el)='+ typeof(el) +' nodeName='+ el.nodeName);
         //console.log(el);
         w3_set_props(el, 'w3-disabled', disable);
   
         // for disabling menu popup and sliders
         if (isDefined(el.nodeName) && (el.nodeName == 'SELECT' || el.nodeName == 'INPUT')) {
            try {
               if (disable)
                  el.setAttribute('disabled', '');
               else
                  el.removeAttribute('disabled');
            } catch(ex) {
               console.log('w3_disable:Attribute');
               console.log(ex);
            }
         }
      }
   );
}

function w3_visible(el_id, visible)
{
	var el = w3_el(el_id);
	el.style.visibility = visible? 'visible' : 'hidden';
	return el;
}

function w3_isVisible(el_id)
{
	var el = w3_el(el_id);
	if (!el) return null;
	return (el.style.visibility == 'visible');
}

// our standard for confirming (highlighting) a control action (e.g.button push)
var w3_highlight_time = 250;
var w3_highlight_color = 'w3-selection-green';
var w3_selection_green = '#4CAF50';

function w3_highlight(el_id)
{
	var el = w3_el(el_id);
	//console.log('w3_highlight '+ el.id);
	w3_add(el, el.w3int_highlight_color || w3_highlight_color);
}

function w3_unhighlight(el_id)
{
	var el = w3_el(el_id);
	//console.log('w3_unhighlight '+ el.id);
	w3_remove(el, el.w3int_highlight_color || w3_highlight_color);
}

function w3_isHighlighted(el_id)
{
	var el = w3_el(el_id);
	return w3_contains(el, el.w3int_highlight_color || w3_highlight_color);
}

function w3_schedule_highlight(el_id)
{
   var el = w3_el(el_id);
   w3_highlight(el);
   setTimeout(function() { w3_unhighlight(el); }, w3_highlight_time);
}

function w3_set_highlight_color(el_id, color)
{
	var el = w3_el(el_id);
	if (!el) return null;

	if (w3_isHighlighted(el)) {
	   w3_unhighlight(el);
	   el.w3int_highlight_color = color;
	   w3_highlight(el);
	} else {
	   el.w3int_highlight_color = color;
	}

	return el;
}

var w3_flag_color = 'w3-override-yellow';

function w3_flag(path)
{
	w3_add(w3_el(path), w3_flag_color);
}

function w3_unflag(path)
{
	w3_remove(w3_el(path), w3_flag_color);
}

// for when you don't want to w3_add(el_id, "[w3-text-color]")
// returns previous color
function w3_color(el_id, color, bkgColor, cond)
{
	var el = w3_el(el_id);
	if (!el) return null;
	var prev_fg = el.style.color;
	var prev_bg = el.style.backgroundColor;
	
	// remember that setting colors to '' restores default
	cond = (isUndefined(cond) || cond);
   if (isArg(color)) el.style.color = cond? color:'';
   if (isArg(bkgColor)) el.style.backgroundColor = cond? bkgColor:'';
	return { color: prev_fg, backgroundColor: prev_bg };
}

// returns previous color
function w3_background_color(el_id, color)
{
	var el = w3_el(el_id);
	var prev = el.style.backgroundColor;
	if (color != undefined && color != null) el.style.backgroundColor = color;
	return prev;
}

// c1 = color if cond false
// c2 = color (optional) if cond true/undefined
// c[12] =
//    'css-color(fg)'
//    ['css-color(fg)', 'css-color(bg)']
//    'w3-text-color'(fg) or 'w3-color'(bg)
// CSS color '' to revert to default color
function w3_colors(el_id, c1, c2, cond)
{
   var el = w3_el(el_id);
   if (!el) return null;
   cond = isArg(cond)? cond : true;

   var fg1 = null, bg1 = null;
   if (isArray(c1)) {
      if (c1.length > 0) fg1 = c1[0];
      if (c1.length > 1) bg1 = c1[1];
   } else
   if (isString(c1)) {
      fg1 = c1;
   } else {
      return null;
   }
   
   //console.log('w3_colors fg1='+ fg1 +' '+ cond);
   if (fg1.startsWith('w3-')) {
      w3_remove(el, fg1);
      if (!cond) w3_add(el, fg1);
   } else {
         if (!cond) el.style.color = fg1;
   }
   if (bg1) {
      el.style.backgroundColor = bg1;
   }

   var fg2 = null, bg2 = null;
   if (isArray(c2)) {
      if (c2.length > 0) fg2 = c2[0];
      if (c2.length > 1) bg2 = c2[1];
   } else
   if (isString(c2)) {
      fg2 = c2;
   } else {
      return null;
   }
   
   //console.log('w3_colors fg2='+ fg2 +' '+ cond);
   if (fg2.startsWith('w3-')) {
      w3_remove(el, fg2);
      if (cond) w3_add(el, fg2);
   } else {
         if (cond) el.style.color = fg2;
   }
   if (bg2) {
      el.style.backgroundColor = bg2;
   }
}

function w3_flip_colors(el_id, colors, cond)
{
   var el = w3_el(el_id);
   if (!el) return null;
   var ar = colors? colors.split(' ') : null;
   if (!ar || ar.length != 2) return null;
   w3_remove(el, ar[0]);
   w3_remove(el, ar[1]);
   w3_add(el, ar[cond? 1:0]);
}

function w3_fg_color_with_opacity_against_bk_color(fg, opacity, bg)
{
   var fg_color = w3color(fg);
   if (!fg_color.valid) return null;
   var bg_color = w3color(bg);
   if (!bg_color.valid) return null;
   var _opacity = 1 - opacity;
   var red    = Math.round(opacity * fg_color.red   + _opacity * bg_color.red);
   var green  = Math.round(opacity * fg_color.green + _opacity * bg_color.green);
   var blue   = Math.round(opacity * fg_color.blue  + _opacity * bg_color.blue);
   return w3color(kiwi_rgb(red, green, blue));
}

function w3_opacity(el_id, opacity)
{
   var el = w3_el(el_id);
   if (!el) return null;
   el.style.opacity = opacity;
}

function w3_flash_fade(el_id, color, dwell_ms, fade_ms, color2)
{
   var el = w3_el(el_id);
   if (!el) return el;
   el.style.transition = 'background 0s';    // has to be "0s" and not just "0"
   el.style.background = color;
   setTimeout(function() {
      el.style.transition = 'background '+ (fade_ms? (fade_ms/1000) : 0.3) +'s linear';
      el.style.background = isString(color2)? color2 : 'black';
   }, dwell_ms? dwell_ms : 50);
   return el;
}

function w3_fillText_shadow(canvas, text, x, y, font, fontSize, color, opts) {
   var strokeWidth = w3_opt(opts, 'stroke', Math.floor(fontSize / 3));  // rule of thumb for good looking shadow

   var ctx = canvas.getContext("2d");
   ctx.font = px(fontSize) +' '+ font;
   ctx.miterLimit = 2;
   ctx.lineJoin = "circle";
   var tw = ctx.measureText(text).width;
   if (!w3_opt(opts, 'left')) x -= tw/2;     // i.e. centered

   ctx.strokeStyle = "black";
   ctx.lineWidth = strokeWidth;
   ctx.strokeText(text, x, y);

   ctx.fillStyle = color;
   ctx.lineWidth = 1;
   ctx.fillText(text, x, y);
}

function w3_fillText(ctx, x, y, text, color, opts)
{
   color = color || 'black';                    ctx.fillStyle = color;
   var font = w3_opt(opts, 'font');             if (isDefined(font)) ctx.font = font;
   var lineWidth = w3_opt(opts, 'lineWidth');   if (isDefined(lineWidth)) ctx.lineWidth = lineWidth;
   var align = w3_opt(opts, 'align');           if (isDefined(align)) ctx.textAlign = align;
   var baseline = w3_opt(opts, 'baseline');     if (isDefined(baseline)) ctx.textBaseline = baseline;

   var tw = ctx.measureText(text).width;
   if (!w3_opt(opts, 'left')) x -= tw/2;     // i.e. centered
   ctx.fillText(text, x, y);
}

function w3_check_restart_reboot(el_id)
{
	var el = w3_el(el_id);
   if (!el) return;
	
	w3_iterate_parent(el, function(el) {
		if (w3_contains(el, 'w3-restart')) {
			w3_restart_cb();
			return el;
		}
		if (w3_contains(el, 'w3-reboot')) {
			w3_reboot_cb();
			return el;
		}
		return null;
	});
}

// XXX just an idea -- not working yet
function w3_check_cfg_reload(el_id)
{
   if (!isAdmin()) return;
	var el = w3_el(el_id);
   if (!el || !w3_contains(el, 'w3-cfg-restart')) return;

   kiwi_clearTimeout(w3int.reload_timeout);
   w3int.reload_timeout = setTimeout(
      function() {
         w3int.update_seq++;
         ext_send('ADM reload_cfg');
         console.log('w3 reload_cfg');
         
         /* XXX this needs to be done in the context of each user connection
         setTimeout(
            function() {
               if (ext_name()) w3_call(ext_name() +'_cfg_reload');
               w3_call('ant_switch_cfg_reload');
         }, 5000);
         */
      }, 5000);
}

function w3_set_value(path, val)
{
   path = w3_add_id(path);
   if (1) {
      // set all similarly named elements
      w3_els(path, function(el) {
         if (el) el.value = val;
      });
   } else {
      var el = w3_el(path);
   /*
      if (val == '(encrypted)' && w3_contains(el, 'w3-encrypted'))
         w3_add(el, 'kiwi-pw');
   */
      if (el) el.value = val;
   }
}

function w3_set_decoded_value(path, val)
{
	//console.log('w3_set_decoded_value: path='+ path +' val='+ val);
	w3_set_value(path, kiwi_decodeURIComponent('w3_set_decoded_value:'+ path, val));
}

function w3_get_value(path)
{
	var el = w3_el(path);
	if (!el) return null;
	return el.value;
}

function w3_add_toplevel(path)
{
	if (!path.startsWith('cfg.') && !path.startsWith('adm.'))
		return 'cfg.'+ path;
	return path;
}

function w3_not_toplevel(path)
{
	if (path.startsWith('cfg.') || path.startsWith('adm.'))
		return path.substr(path.indexOf('.') + 1);
	return path;
}

function w3_basename(path)
{
	var i = path.lastIndexOf('.');
	if (i >= 0) {
		return path.substr(i+1);
	}
	return path;
}

function w3_psa3(psa3)
{
   //if (psa3.includes('w3-dump')) console.log('w3_psa3 in=['+ psa3 +']');
   psa3 = psa3 || '';
   a = psa3.split('/');
   var a0 = (a[0] || '').replace(/&slash;/g, '/');
   var a1 = (a[1] || '').replace(/&slash;/g, '/');
   var a2 = (a[2] || '').replace(/&slash;/g, '/');
   //if (psa3.includes('w3-dump')) console.log(a);

   if (a.length == 1)
      return { left:'', middle:'', right:a0 };
   else
   if (a.length == 2)
      return { left:'', middle:a0, right:a1};
   else
   if (a.length == 3)
      return { left:a0, middle:a1, right:a2 };
   else
      return { left:'', middle:'', right:'' };
}

// add space between string arguments
// empty strings ignored (join() won't do this)
function w3_sb()
{
   var a = [];
   for (var i = 0; i < arguments.length; i++) {
      var s = arguments[i];
      s = isString(s)? s.trim() : (isNumber(s)? s.toString() : '');
      if (s != '') a.push(s);
   }
	return a.length? a.join(' ') : '';
}

// add 'c' between string arguments
function w3_sbc(c)
{
   var a = [];
   for (var i = 1; i < arguments.length; i++) {
      var s = arguments[i];
      s = isString(s)? s.trim() : (isNumber(s)? s.toString() : '');
      if (s != '') a.push(s);
   }
	return a.length? a.join(c) : '';
}

// "space between ending with"
function w3_sbew(ew)
{
   var a = [];
   for (var i = 1; i < arguments.length; i++) {
      var s = arguments[i];
      s = isString(s)? s.trim() : (isNumber(s)? s.toString() : '');
      if (s != '') {
         if (s[s.length-1] != ew) s = s + ew;
         a.push(s);
      }
   }
	return a.length? a.join(' ') : '';
}

// append \n if s is not empty
function w3_nl(s)
{
   if (isArg(s) && s != '') s += '\n';
   return s;
}

// append <br> if s is not empty
function w3_br(s)
{
   if (isArg(s) && s != '') s += '<br>';
   return s;
}

// psa = prop|style|attr
// => <div [class="[prop] [extra_prop]"] [style="[style] [extra_style]"] [attr] [extra_attr]>
function w3_psa(psa, extra_prop, extra_style, extra_attr)
{
   if (psa && psa.includes('w3-dump')) {
	   console.log('psa_in=['+ psa +']');
	   console.log('extra_prop=['+ extra_prop +']');
	   console.log('extra_style=['+ extra_style +']');
	   console.log('extra_attr=['+ extra_attr +']');
	}

   if (psa && psa.startsWith('class=')) {
      //console.log('#### w3_psa RECURSION ####');
      return psa;    // already processed
   }
   
	var a = psa? psa.split('|') : [];
	psa = '';

	var prop = a[0] || '';
	if (extra_prop) prop = w3_sb(prop, extra_prop);
	if (prop != '') psa = 'class='+ dq(prop);

	var style = a[1] || '';
	if (extra_style) style = w3_sbew(';', style, extra_style);
	if (style != '') psa = w3_sb(psa, 'style='+ dq(style));

	var attr = (a[2] || '').replace(/&vbar;/g, '|');   // e.g. title="foo&vbar;bar" => title="foo|bar"
	if (extra_attr) attr = w3_sb(attr, extra_attr);
	if (attr != '') psa = w3_sb(psa, attr);

	//console.log('psa_out=['+ psa +']');
	return psa;
}

// like w3_psa() except returns in original psa format (i.e. not expanded to "class=...")
function w3_psa_mix(psa, extra_prop, extra_style, extra_attr)
{
   var dump = psa.includes('w3-dump');
   if (dump) console.log('--- w3_psa_mix psa='+ psa);
	var a = psa? psa.split('|') : [];

	var prop = a[0] || '';
   if (dump) console.log('prop=['+ prop +']');
   if (dump) console.log('extra_prop=['+ extra_prop +']');
	if (extra_prop) prop = w3_sb(prop, extra_prop);
   if (dump) console.log('mixed prop=['+ prop +']');

	var style = a[1] || '';
   if (dump) console.log('style=['+ style +']');
   if (dump) console.log('extra_style=['+ extra_style +']');
	if (extra_style) style = w3_sbew(';', style, extra_style);
   if (dump) console.log('mixed style=['+ style +']');

	var attr = a[2] || '';     // NB: don't do &vbar; => | expansion here since output is in psa format
   if (dump) console.log('attr=['+ attr +']');
   if (dump) console.log('extra_attr=['+ extra_attr +']');
	if (extra_attr) attr = w3_sb(attr, extra_attr);
   if (dump) console.log('mixed attr=['+ attr +']');

   psa = prop +'|'+ style +'|'+ attr;
	if (dump) console.log('mix_out=['+ psa +']');
	return psa;
}

function w3_set_psa(el, psa)
{
   el = w3_el(el);
   if (!el || isEmptyString(psa)) return null;
   psa = psa.split('/')[0];
   psa = psa.split('|');
   var _class = psa[0];
   var _style = psa[1];
   var _attr = psa[2];
   if (isArg(_class)) w3_add(el, _class);
   if (isArg(_style)) el.style = _style;
   // FIXME: handle attr
}

function w3int_init()
{
}

function w3int_post_action()
{
   // if it exists, re-select the main page frequency field
   w3_call('freqset_select');
}

function w3_copy_to_clipboard(val)
{
	var el = document.createElement("input");
	el.setAttribute('type', 'text');
	el.setAttribute('value', val);
   document.body.appendChild(el);
	el.select();
	document.execCommand("copy");
   document.body.removeChild(el);
}

function w3_isScrollingY(id)
{
   var el = w3_el(id);
   if (!el) return null;
   return (el.scrollHeight > el.clientHeight);
}

function w3_isScrollingX(id)
{
   var el = w3_el(id);
   if (!el) return null;
   return (el.scrollWidth > el.clientWidth);
}

// see: developer.mozilla.org/en-US/docs/Web/API/Element/scrollHeight#determine_if_an_element_has_been_totally_scrolled
function w3_isScrolledDown(id)
{
   var el = w3_el(id);
   if (!el) return null;
   return (Math.abs(el.scrollHeight - el.clientHeight - el.scrollTop) < 1);
}

// returns 0 to 1.0 (-1 on error)
function w3_scrolledPosition(id)
{
   var el = w3_el(id);
   if (!el) return null;
   //console.log('w3_scrolledPosition scrollTop='+ el.scrollTop +' scrollHeight='+ el.scrollHeight +' clientHeight='+ el.clientHeight);
   if (el.clientHeight == 0) return -1;      // element hidden most likely
   var scrollH = el.scrollHeight - el.clientHeight;
   if (scrollH <= 0) return 0;      // no scrollbar, so always at top
   return el.scrollTop / scrollH;
}

function w3_scrollTo(id, pos, jog)
{
   var el = w3_el(id);
   if (!el) return null;
   var scrollH = el.scrollHeight - el.clientHeight;
   if (scrollH < 0) scrollH = 0;    // no scrollbar, so always at top
   var top = scrollH * pos;
   var unchanged = (top == el.scrollTop);
   //console.log('w3_scrollTo: unchanged='+ unchanged +' pos='+ pos +' sH='+ el.scrollHeight +' cH='+ el.clientHeight +' top='+ top +' scrollTop='+ el.scrollTop);
   el.scrollTop = top;
   
   // jog to force scroll event
   if (jog == true) {
      el.scrollTop = top+1;
      el.scrollTop = top;
   }
   return unchanged;
}

// id element must have scroll property
function w3_scrollDown(id, cond)
{
   var el = w3_el(id);
   if (isArg(cond)? cond : true) el.scrollTop = el.scrollHeight;
   return el;
}

// id element must have scroll property
function w3_scrollTop(id, cond)
{
   var el = w3_el(id);
   if (isArg(cond)? cond : true) el.scrollTop = 0;
   return el;
}

// calls func(arg) when w3_el(id) exists in DOM
function w3_do_when_rendered(id, func, arg, poll_ms)
{
   var el = w3_el(id);
   poll_ms = isNumber(poll_ms)? poll_ms : 500;
   rend = isArg(el);
   //console.log('id='+ id +' ms='+ poll_ms +' rend='+ rend +' arg='+ arg);
   if (!rend) {
      setTimeout(function () {
         w3_do_when_rendered(id, func, arg, poll_ms);
      }, poll_ms);
   } else {
      if (isFunction(func))
         func(el, arg);
   }
   
   // REMINDER: Other than the FIRST call, returns from here don't go anywhere.
   // In particular the original caller is returned to:
   //    1) If the FIRST check of cond_func() is true and after func() runs.
   //    2) If the FIRST check of cond_func() is false and after the first setTimeout() runs.
   // But the caller is NOT suspended or somehow waiting for a delayed run of func() when
   // conf_func() is false for some number of setTimeout() periods.
   // That's not how javascript works (Web Workers aside). It is event driven and all threads
   // must complete without conditional delay.
}

// calls func(arg) when cond_func() becomes true
function w3_do_when_cond(cond_func, func, arg, poll_ms)
{
   poll_ms = isNumber(poll_ms)? poll_ms : 500;
   if (!cond_func(arg)) {
      setTimeout(function () {
         w3_do_when_cond(cond_func, func, arg, poll_ms);
      }, poll_ms);
   } else {
      if (isFunction(func))
         func(arg);
   }
   
   // REMINDER: Other than the FIRST call, returns from here don't go anywhere.
   // In particular the original caller is returned to:
   //    1) If the FIRST check of cond_func() is true and after func() runs.
   //    2) If the FIRST check of cond_func() is false and after the first setTimeout() runs.
   // But the caller is NOT suspended or somehow waiting for a delayed run of func() when
   // conf_func() is false for some number of setTimeout() periods.
   // That's not how javascript works (Web Workers aside). It is event driven and all threads
   // must complete without conditional delay.
}

// w3_elementAtPointer(evt)
// w3_elementAtPointer(x, y)
function w3_elementAtPointer(x, y)
{
   if (isObject(x)) {
      var evt = x;
      x = evt.pageX; y = evt.pageY;
   }
   if (isNumber(x) && isNumber(y)) {
      return document.elementFromPoint(x,y);
   }
   return null;
}

function w3_event_log(el, id, event_name)
{
   el = w3_el(el);
   if (el == null) return;
   el.addEventListener(event_name,
      function() {
         console.log('w3_event_log '+ id +': '+ event_name);
         //canvas_log(event_name);
      }
   );
}

// observe element event behavior
function w3_event_listener(id, el)
{
   el = w3_el(el);
   if (!el) return null;
   if (!w3int.eventListener[id]) {
      w3_event_log(el, id, 'mouseup');
      w3_event_log(el, id, 'mousedown');
      w3_event_log(el, id, 'click');
      w3_event_log(el, id, 'change');
      w3_event_log(el, id, 'submit');
      w3_event_log(el, id, 'focus');
      w3_event_log(el, id, 'blur');
      w3int.eventListener[id] = true;
   }
}


////////////////////////////////
// hr
////////////////////////////////

function w3_hr(psa)
{
   var p = w3_psa(psa);
	var s = '<hr '+ p +'>';
	var narg = arguments.length;
		for (var i=1; i < narg; i++) {
			s += arguments[i];
		}
	//console.log(s);
	return s;
}


////////////////////////////////
// nav
////////////////////////////////

function w3_click_nav(nav_id, next_id, cb_next, cb_param, from)
{
   //console.log('--------');
   //console.log('w3_click_nav START nav_id='+ nav_id +' next_id='+ next_id +' cb_next='+ cb_next +' cb_param='+ cb_param +' from='+ from);
	var next_id_nav = ('id-nav-'+ next_id).toLowerCase();    // to differentiate the nav anchor from the nav container
	var cur_id = null;
	var next_el = null;
   var cb_prev = null;
   var found = 0;

   var el = w3_el(nav_id);
   //console.log('w3_click_nav nav_id='+ nav_id +' el='+ el);
   if (!el) return;
   
	w3_iterate_children(el, function(el, i) {
	   //console.log('w3_click_nav consider: nodename='+ el.nodeName);
	   if (el.nodeName != 'A' || found >= 2) return;
	   //if (el.nodeName != 'DIV') return;

		//console.log('w3_click_nav consider: id=('+ el.id +') ==? next_id=('+ next_id_nav +') el.className="'+ el.className +'"');
		if (w3_contains(el, 'w3int-cur-sel')) {
			cur_id = el.id.substring(7);		// remove 'id-nav-' added by w3int_anchor()

		   //console.log('w3_click_nav FOUND-CUR cur_id='+ cur_id);
			w3_remove(el, 'w3int-cur-sel');
			w3_iterate_classList(el, function(s, i) {
			   if (s.startsWith('id-nav-cb-'))
			      cb_prev = s.substring(10);
			});
			found++;
		}

		if (el.id.toLowerCase().startsWith(next_id_nav)) {
		   //console.log('w3_click_nav FOUND-NEXT id='+ next_id);
		   next_id = el.id.split('id-nav-')[1];
			next_el = el;
			found++;
		}
	});

   // toggle visibility of current content
	if (cur_id && cb_prev) {
		//console.log('w3_click_nav BLUR cb_prev='+ cb_prev +' cur_id='+ cur_id);
		w3_call(cb_prev +'_blur', cur_id);
	}
	if (cur_id)
		w3_toggle(cur_id, 'w3-show-block');

   // make new nav item current and visible / focused
	if (next_el) {
		w3_add(next_el, 'w3int-cur-sel');
	   w3_check_restart_reboot(next_el);
	}

	w3_toggle(next_id, 'w3-show-block');
	if (cb_next != 'null') {
	   //console.log('w3_click_nav FOCUS cb_next='+ cb_next +' next_id='+ next_id);
      w3_call(cb_next +'_focus', next_id, cb_param);
   }
	//console.log('w3_click_nav DONE cb_prev='+ cb_prev +' cur_id='+ cur_id +' cb_next='+ cb_next +' next_id='+ next_id);
}

// id = unique, cb = undefined => cb = id
// id = unique, cb = func
// id = unique, cb = null => don't want focus/blur callbacks
function w3int_anchor(psa, text, navbar, id, cb, isSelected)
{
   if (cb === undefined) cb = id;
   var nav_cb = cb? ('id-nav-cb-'+ cb) : '';
   //console.log('w3int_anchor id='+ id +' cb='+ cb +' nav_cb='+ nav_cb);

	// store id prefixed with 'id-nav-' so as not to collide with content container id prefixed with 'id-'
	var attr = 'id="id-nav-'+ id +'" onclick="w3_click_nav('+ sq(navbar) +', '+ sq(id) +', '+ sq(cb) +')"';
	//console.log('w3int_anchor psa: '+ psa);
	//console.log('w3int_anchor attr: '+ attr);
   var p = w3_psa(psa, nav_cb + (isSelected? ' w3int-cur-sel':''), '', attr);
   //var p = w3_psa(psa, 'w3-show-inline '+ nav_cb + (isSelected? ' w3int-cur-sel':''), '', attr);
	//console.log('w3int_anchor p: '+ p);
	
	// store with id= instead of a class property so it's easy to find with el.id in w3_iterate_classname()
	// href of "javascript:void(0)" instead of "#" so page doesn't scroll to top on click
	var s = '<a '+ p +' href="javascript:void(0)">'+ text +'</a>';
   //var s = w3_div(p, text);
	//console.log('w3int_anchor: '+ s);
	return s;
}

function w3_navbar(psa)
{
   var p = w3_psa(psa, 'w3-navbar');
	var s = '<nav '+ p +'>';
	var narg = arguments.length;
		for (var i=1; i < narg; i++) {
			s += arguments[i];
		}
	s += '</nav>';
	//console.log(s);
	return s;
}

function w3_sidenav(psa)
{
   var p = w3_psa(psa, 'w3-sidenav w3-static w3-left w3-sidenav-full-height w3-light-grey');
	var s = '<nav '+ p +'>';
	var narg = arguments.length;
		for (var i=1; i < narg; i++) {
			s += arguments[i];
		}
	s += '</nav>';
	//console.log(s);
	return s;
}

function w3_nav(psa, text, navbar, id, cb, isSelected)
{
	return w3int_anchor(psa, text, navbar, id, cb, isSelected);
}

function w3_navdef(psa, text, navbar, id, cb)
{
	// must wait until instantiated before manipulating 
	setTimeout(function() {
		//console.log('w3_navdef instantiate focus');
		w3_toggle(id, 'w3-show-block');
	}, w3_highlight_time);
	
	return w3int_anchor(psa, text, navbar, id, cb, true);
}


////////////////////////////////
// labels
////////////////////////////////

function w3int_label(psa, text, path, extension)
{
   psa = psa || '';
   text = text || '';
   extension = extension || '';
   var dump = psa.includes('w3-dump');

   // don't emit empty label div
   //if (psa == '' && text == '' && extension == '') return '';
   if (text == '' && ((psa != '' && psa != '||') || extension != ''))
      console_nv('$w3int_label', {text}, {psa}, {extension});
   if (text == '') {
      if (dump) console.log('$w3int_label <empty string>');
      return '';
   }
   
   // most likely already an embedded w3_label()
   if (text.startsWith('<label ')) {
      if (dump) console.log('$w3int_label EMBEDDED '+ text);
      return text;
   }
   
   var id = w3_add_id(path, '-label');    // so w3_set_label() can find label
	//var inline = psa.includes('w3-label-inline');
	var p = w3_psa(psa, id);
	var s = '<label '+ p +'>'+ text + extension +'</label>';
	//var s = '<label '+ p +'>'+ text + extension + (inline? '':'<br>') +'</label>';
	if (dump) console.log('$w3int_label psa='+ psa +' text=<'+ text +'> s=<'+ s +'>');
	return s;
}

// if called directly always emit label div even if text is empty
function w3_label(psa, text, path, extension)
{
   if (arguments.length >= 4) {
      console.log('### w3_label ext='+ extension);
      //kiwi_trace();
   }
   if (isEmptyString(text)) text = '&nbsp;';
   return w3int_label(psa, text, path, extension);
}

function w3_get_label(label, path)
{
	return w3_get_innerHTML(w3_add_id(path, '-label'));
}

function w3_set_label(label, path)
{
	w3_innerHTML(w3_add_id(path, '-label'), label);
}


////////////////////////////////
// title
////////////////////////////////

function w3_title(id, text)
{
   //console.log('w3_title id='+ id +' text='+ text);
   var el = w3_el(id);
   if (!el) return null;
   w3_attribute(el, 'title', text);
   return el;
}


////////////////////////////////
// link
////////////////////////////////

function w3int_link_click(ev, cb, cb_param)
{
   //console.log('w3int_link_click cb='+ cb +' cb_param='+ cb_param);
   //console.log(ev);
   var el = ev.currentTarget;
   //console.log(el);
   w3_check_restart_reboot(el);

   // cb is a string because can't pass an object to onclick
   if (cb) {
      w3_call(cb, el, cb_param, /* first */ false);   // links don't have first callback
   }

   w3int_post_action();
}

function w3_link(psa, url, inner, title, cb, cb_param)
{
   var qual_url = url;
   var href = '';
   var target = '';
   if (url != '') {
      if (!url.startsWith('javascript:')) {
         if (!url.startsWith('http://') && !url.startsWith('https://'))
            qual_url = 'http://'+ url;
         target = ' target="_blank"';
      }
      href = 'href='+ dq(qual_url) +' ';
   }
   inner = inner || '';
   title = title || '';
   if (title != '') title = ' title='+ dq(title);

   // by default use pointer cursor if there is a callback
	var pointer = (cb && cb != '')? 'w3-pointer':'';
	cb_param = cb_param || 0;
	var onclick = cb? (' onclick="w3int_link_click(event, '+ sq(cb) +', '+ sq(cb_param) +')"') : '';

	var p = w3_psa(psa, pointer, '', href + target + title + onclick);

   // escape HTML '<' and '>' so they work with kiwi_output_msg()
	var esc_html = psa.includes('w3-esc-html');
	var lt = esc_html? kiwi.esc_lt : '<';
	var gt = esc_html? kiwi.esc_gt : '>';
	var s = lt +'a '+ p + gt + inner + lt +'/a'+ gt;
	//console.log(s);
	return s;
}

function w3_link_set(path, url)
{
   var el = w3_el(path);
   //console.log('w3_link_set: path='+ path +' url='+ url);
   //console.log(el);
   if (el && url != '') {
      if (!url.startsWith('http://') && !url.startsWith('https://'))
         url = 'http://'+ url;
      w3_attribute(el, 'href', url);
      w3_attribute(el, 'target', '_blank');
   }
}


////////////////////////////////
// image
////////////////////////////////

function w3_img_wh(src, func)
{
   // see: stackoverflow.com/questions/318630/get-the-real-width-and-height-of-an-image-with-javascript-in-safari-chrome
   // There are huge issues obtaining the true height/width of an img given variable state during loading (caching etc).
   var img = new Image();
   img.onload = function() {
      //console.log('w3_img_wh '+ this.width +'x'+ this.height +' src='+ src);
      func(this.width, this.height);
   };
   img.src = src;
}

function w3_img(psa, file, width, height)
{
   var w = isArg(width)? ('width='+ width) : '';
   var h = isArg(height)? ('height='+ height) : '';
   var src = 'src='+ dq(file);
	var p = w3_psa(psa, null, null, w3_sb(src, w, h));
	var s = '<img '+ p +' />';
	if (psa && psa.includes('w3-dump')) console.log('w3-dump: '+ s);
	return s;
}


////////////////////////////////
// buttons: radio
////////////////////////////////

var w3_SELECTED = true;
var w3_NOT_SELECTED = false;

function w3_radio_unhighlight(path)
{
	w3_iterate_classname(w3_add_id(path), function(el) { w3_unhighlight(el); });
}

function w3int_radio_click(ev, path, cb, cb_param)
{
	w3_radio_unhighlight(path);
	w3_highlight(ev.currentTarget);

	var idx = -1;
	w3_iterate_classname(w3_add_id(path), function(el, i) {
		if (w3_isHighlighted(el))
			idx = i;
		//console.log('w3int_radio_click CONSIDER path='+ path +' el='+ el +' idx='+ idx);
	});

	w3_check_restart_reboot(ev.currentTarget);

	// cb is a string because can't pass an object to onclick
	if (cb) {
		w3_call(cb, path, idx, /* first */ false, cb_param);   // radio buttons don't have first callback
	}

   w3int_post_action();
}

// deprecated (still used by antenna switch ext)
function w3_radio_btn(text, path, isSelected, save_cb, prop)
{
	prop = (arguments.length > 4)? arguments[4] : null;
	var _class = ' id-'+ path + (isSelected? (' '+ w3_highlight_color) : '') + (prop? (' '+prop) : '');
	var oc = 'onclick="w3int_radio_click(event, '+ sq(path) +', '+ sq(save_cb) +')"';
	var s = '<button class="w3-btn w3-ext-btn'+ _class +'" '+ oc +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

function w3_radio_button(psa, text, path, isSelected, cb, cb_param)
{
	cb_param = cb_param || 0;
	var onclick = cb? ('onclick="w3int_radio_click(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"') : '';
	var p = w3_psa(psa, w3_add_id(path) + (isSelected? (' '+ w3_highlight_color) : '') +' w3-btn w3-ext-btn', '', onclick);
	var s = '<button '+ p +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

// used when current value should come from config param
function w3_radio_button_get_param(psa, text, path, selected_if_val, init_val, cb, cb_param)
{
   // FIXME
   //var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	//console.log('w3_radio_button_get_param: '+ path);
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val, save);
	
	// set default selection of button based on current value
	var isSelected = (cur_val == selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
	return w3_radio_button(psa, text, path, isSelected, cb, cb_param);
}


////////////////////////////////
// switch: two button switch
////////////////////////////////

var w3_SWITCH_YES_IDX = 0, w3_SWITCH_NO_IDX = 1;

function w3_switch_s(idx) { return ((idx == w3_SWITCH_YES_IDX)? 'YES' : 'NO'); }

function w3_switch_val2idx(val) { return (val? w3_SWITCH_YES_IDX : w3_SWITCH_NO_IDX); }

function w3_switch_idx2val(idx) { return ((idx == w3_SWITCH_YES_IDX)? 1:0); }

function w3_switch(psa, text_0, text_1, path, text_0_selected, cb, cb_param)
{
   //console.log('w3_switch psa='+ psa);
	var s =
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-0'), text_0, path, text_0_selected? 1:0, cb, cb_param) +
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-1'), text_1, path, text_0_selected? 0:1, cb, cb_param);
	return s;
}

function w3_switch_label(psa, label, text_0, text_1, path, text_0_selected, cb, cb_param)
{
   //console.log('w3_switch_label psa='+ psa);
   var hasLabel = (label != '');
	var inline = psa.includes('w3-label-inline');
	var centered = psa.includes('w3-center');
	var left = psa.includes('w3-label-left');
	var bold = !psa.includes('w3-label-not-bold');
	
	var spacing = (hasLabel && !inline)? 'w3-margin-T-8' : '';
	if (hasLabel && inline) spacing += left? ' w3-margin-L-8' : ' w3-margin-R-8';
	if (centered) spacing += ' w3-halign-center';

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa_mix(psa3.left, (inline? 'w3-show-inline-new':'') + (centered? ' w3-halign-center w3-center':''));
   var psa_label = w3_psa_mix(psa3.middle, (hasLabel && bold)? 'w3-bold':'');
	var psa_inner = w3_psa();

   var ls = w3int_label(psa_label, label, path);
   var cs =
      w3_inline(spacing +'/'+ psa3.right,
         w3_radio_button('w3int-switch-0', text_0, path, text_0_selected? 1:0, cb, cb_param) +
         w3_radio_button('w3int-switch-1', text_1, path, text_0_selected? 0:1, cb, cb_param)
      );
	var s = w3_div(psa_outer, ((left || !inline)? (ls + cs) : (cs + ls)));
	return s;
}

// used when current value should come from config param
function w3_switch_get_param(psa, text_0, text_1, path, text_0_selected_if_val, init_val, cb, cb_param)
{
   // FIXME
   //var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val, save);

	// set default selection of button based on current value
	//if (w3_contains(psa, 'id-net-ssl'))
	//   console.log('w3_switch_get_param cur_val='+ cur_val +' text_0_selected_if_val='+ text_0_selected_if_val);
	var text_0_selected = (cur_val == text_0_selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
   return w3_switch(psa, text_0, text_1, path, text_0_selected, cb, cb_param);
}

// used when current value should come from config param
function w3_switch_label_get_param(psa, label, text_0, text_1, path, text_0_selected_if_val, init_val, cb, cb_param)
{
   // FIXME
   //var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val, save);

	// set default selection of button based on current value
	//if (w3_contains(psa, 'id-net-ssl'))
	//   console.log('w3_switch_get_param cur_val='+ cur_val +' text_0_selected_if_val='+ text_0_selected_if_val);
	var text_0_selected = (cur_val == text_0_selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
   return w3_switch_label(psa, label, text_0, text_1, path, text_0_selected, cb, cb_param);
}

function w3_switch_set_value(path, switch_idx)
{
   var sw = 'w3int-switch-'+ switch_idx;
   //console.log('w3_switch_set_value: switch='+ sw +' path='+ path);
	w3_iterate_classname(path, function(el, i) {
      //console.log('w3_switch_set_value: CONSIDER i='+ i);
      //console.log(el);
		if (w3_contains(el, sw)) {
		   //console.log('w3_switch_set_value: click()...');
		   //console.log(el);
			el.click();
		}
	});
}


////////////////////////////////
// buttons: single, clickable icon
////////////////////////////////

function w3int_btn_evt(ev, path, cb, cb_param)
{
   if (!w3_contains(path, 'w3-disabled')) {
      //console.log('w3int_btn_evt ev.type='+ ev.type +' path='+ path +' cb='+ cb +' cb_param='+ cb_param);

      if (w3int.autorepeat_canceller_path != null) {
         var diff = (w3int.autorepeat_canceller_path != path);
         var same = (w3int.autorepeat_canceller_path == path && ev.type == 'mousedown');
         if (diff || same) {
            //console.log(path +' CANCELLED '+ w3int.autorepeat_canceller_path);
            //if (kiwi_isMobile()) alert(path +' CANCELLED '+ w3int.autorepeat_canceller_path +' diff='+ TF(diff) +' same='+ TF(same));
            kiwi_clearInterval(w3int.autorepeat_canceller_interval);
            w3int.autorepeat_canceller_path = null;
         }
      }
      
      w3_check_restart_reboot(ev.currentTarget);
      
      var el = w3_el(path);
      var hold = w3_contains(el, 'w3-hold');
      //canvas_log(ev.type +' H='+ hold);

      if (hold) {
         //console.log('w3int_btn_evt HOLD '+ ev.type);
         if (ev.type == 'mousedown' || ev.type == 'touchstart') {
            el.hold_triggered = false;
            el.hold_timeout = setTimeout(function() {
               el.hold_triggered = true;
               //console.log('w3int_btn_evt HOLD_TRIGGERED, RUN ACTION');
               if (cb) {
                  //canvas_log('HOLD3');
                  w3_call(cb, path, cb_param, /* first */ false, { type: 'hold' });
               }
            }, 500);
            //canvas_log('HOLD1');
            return ignore(ev);   // don't run callback below
         } else {
            //canvas_log('HT='+ el.hold_timeout);
            if ((ev.type == 'click' || ev.type == 'mouseout' || ev.type == 'touchend') && isArg(el.hold_timeout)) {
               //if (ev.type == 'mouseout') console.log('MOUSEOUT-HOLD');
               var timeout = el.hold_timeout;
               var triggered = el.hold_triggered;
               el.hold_timeout = null;
               el.hold_triggered = false;
               kiwi_clearTimeout(timeout);
               if (triggered) {
                  //console.log('w3int_btn_evt HOLD_PREV_TRIGGERED, IGNORE CLICK');
                  if (w3_contains(el, 'w3-hold-done')) {
                     w3_call(cb, path, cb_param, /* first */ false, { type: 'hold-done' });
                  }
                  w3int_post_action();
                  //canvas_log('HOLD2');
                  return ignore(ev);
               }
               //canvas_log('HOLD4');
            
               // fall through -- normal mouseup/touchend click before hold timer expired
            }
         }
      }
   
      // ignore spurious mouseout events when w3-hold is enabled
      if (ev.type == 'mouseout') {
         //console.log('MOUSEOUT-REG');
         return;
      }
      
      // cb is a string because can't pass an object to onclick
      if (cb) {
         w3_call(cb, path, cb_param, /* first */ false, ev);   // buttons don't have first callback
      }
   }

   w3int_post_action();
}

function w3_autorepeat_canceller(path, interval)
{
   w3int.autorepeat_canceller_path = isUndefined(path)? null : path;
   w3int.autorepeat_canceller_interval = isUndefined(interval)? null : interval;
}

// deprecated (still used by older versions of antenna switch ext)
function w3_btn(text, cb)
{
   console.log('### DEPRECATED: w3_btn');
   return w3_button('', text, cb);
}

function w3_cb_param_encode(cbp)
{
   if (isNoArg(cbp)) {
      return 0;
   }
   
   if (isObject(cbp)) {
      return encodeURIComponent(JSON.stringify(cbp));
   }
   
   return cbp;
}

function w3_cb_param_decode(cbp)
{
   if (isNoArg(cbp)) {
      return 0;
   }
   
   if (isString(cbp)) {
      cbp = decodeURIComponent(cbp);
      var obj;
      if (cbp[0] == '{') {
         try {
            obj = JSON.parse(cbp);
         } catch(ex) {
            return cbp;
         }
         return obj;
      }
   }
   
   return cbp;
}

function w3int_button(psa, path, text, cb, cb_param)
{
   var momentary = psa.includes('w3-momentary');
   var hold = psa.includes('w3-hold');
   var custom_events = psa.includes('w3-custom-events');

	cb_param = w3_cb_param_encode(cb_param);
	var ev_cb = function(cbp) { return sprintf('"w3int_btn_evt(event,%s,%s,%s);"', sq(path), sq(cb), sq(cbp)); };
	var onclick = cb? ('onclick='+ ev_cb(cb_param)) : '';
	if (cb && momentary && !hold && !custom_events) {
	   onclick += ' onmousedown='+ ev_cb(0);
	   onclick += ' ontouchstart='+ ev_cb(0);
	} else
	if (cb && hold && !momentary && !custom_events) {
	   onclick += ' onmousedown='+ ev_cb(cb_param);
	   onclick += ' onmouseout='+ ev_cb(cb_param);        // needed to cancel w3-hold
	   onclick += ' ontouchstart='+ ev_cb(cb_param);
	   onclick += ' ontouchend='+ ev_cb(cb_param);        // mobile doesn't generate a click event
	} else
	if (cb && (custom_events || hold) && !momentary) {
	   onclick += ' onmouseenter='+ ev_cb(cb_param);
	   onclick += ' onmouseleave='+ ev_cb(cb_param);
	   onclick += ' onmousedown='+ ev_cb(cb_param);
	   onclick += ' onmousemove="ignore(event);"';
	   onclick += ' onmouseup="ignore(event);"';

	   onclick += ' ontouchstart='+ ev_cb(cb_param);
	   onclick += ' ontouchmove="ignore(event);"';
	   onclick += ' ontouchend='+ ev_cb(cb_param);        // mobile doesn't generate a click event
	}
	
	// w3-round-large listed first so its '!important' can be overriden by subsequent '!important's
	var default_style = (psa.includes('w3-round') || psa.includes('w3-circle'))? '' : ' w3-round-6px';
	var noactive = psa.includes('w3-noactive')? ' class-button-noactive w3-ext-btn-noactive' : '';
	var right = psa.includes('w3-btn-right')? 'w3-ialign-right' : '';
   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, right);
	var psa_inner = w3_psa(psa3.right, path +' w3-btn w3-ext-btn'+ default_style + noactive, '', onclick);
   if (psa.includes('w3-dump')) {
      console.log('w3_button');
      console.log(psa3);
      console.log('psa_outer='+ psa_outer);
      console.log('psa_inner='+ psa_inner);
   }
	var s = '<button '+ psa_inner +'>'+ text +'</button>';
	if (psa_outer != '') s = '<div '+ psa_outer +'>'+ s +'</div>';
	if (psa_outer.includes('w3-dump')) console.log(s);
	return s;
}

function w3_button(psa, text, cb, cb_param)
{
	var path = 'id-btn-grp-'+ w3int.btn_grp_uniq.toString();
	w3int.btn_grp_uniq++;
   return w3int_button(psa, path, text, cb, cb_param);
}

function w3_button_path(psa, path, text, cb, cb_param)
{
   return w3int_button(psa, path, text, cb, cb_param);
}

function w3_button_text(path, text, color_or_add_color, remove_color)
{
   var el = w3_el(path);
   if (!el) return null;
   if (isArg(text))
      el.innerHTML = text;
   if (color_or_add_color) {
      if (color_or_add_color.startsWith('w3-')) {
         w3_remove(el, remove_color);
         w3_add(el, color_or_add_color);
      } else
         el.style.color = color_or_add_color;
   }
   return el;
}


////////////////////////////////
// icon
////////////////////////////////

function w3_icon(psa, fa_icon, size, color, cb, cb_param)
{
   var momentary = psa.includes('w3-momentary');
   var hold = psa.includes('w3-hold');
   var custom_events = psa.includes('w3-custom-events');

   // by default use pointer cursor if there is a callback
	var pointer = (cb && cb != '')? ' w3-pointer':'';
	var path = 'id-btn-grp-'+ w3int.btn_grp_uniq.toString();
	w3int.btn_grp_uniq++;

	var font_size = null;
	if (isNumber(size) && size >= 0) font_size = px(size);
	else
	if (isString(size)) font_size = size;
	font_size = font_size? (' font-size:'+ font_size +';') : '';

   color = color || '';
   var c = color.split('|');
   color = '';
   if (c[0] != '') color = ' color:'+ c[0] +';';
   if (c.length >= 2 && c[1] != '') color += ' background-color:'+ c[1] +';';

	cb_param = w3_cb_param_encode(cb_param);
	var ev_cb = function(cbp) { return sprintf('"w3int_btn_evt(event, %s, %s, %s)"', sq(path), sq(cb), sq(cbp)); };
	var onclick = cb? ('onclick='+ ev_cb(cb_param)) : '';
	if (cb && (momentary || hold) && !custom_events) {
	   if (momentary) cb_param = 0;
	   onclick += ' onmousedown='+ ev_cb(cb_param);
      onclick += ' onmouseout='+ ev_cb(cb_param);    // needed to cancel w3-hold
	   onclick += ' ontouchstart='+ ev_cb(cb_param);
	} else
	if (cb && (custom_events || hold) && !momentary) {
	   onclick += ' onmouseenter='+ ev_cb(cb_param);
	   onclick += ' onmouseleave='+ ev_cb(cb_param);
	   onclick += ' onmousedown="ignore(event)"';
	   onclick += ' onmousemove="ignore(event)"';
	   onclick += ' onmouseup="ignore(event)"';

	   onclick += ' ontouchstart='+ ev_cb(cb_param);
	   onclick += ' ontouchmove="ignore(event)"';
	   onclick += ' ontouchend='+ ev_cb(cb_param);
	}

	var p = w3_psa(psa, path +' w3-ext-icon'+ pointer +' fa '+ fa_icon, font_size + color, onclick);
	var s = '<i '+ p +'></i>';
	//console.log(s);
	return s;
}

function w3_spin(el, cond)
{
   el = w3_el(el);
   if (!el) return null;
   cond = isArg(cond)? cond : true;
   w3_remove_then_add(el, 'fa-spin', cond? 'fa-spin':'');
}

/*

// prototype of callbacks using ev.currentTarget instead of path
// switch to using this someday

function w3int_icon_click(ev, cb, cb_param)
{
   console.log('w3int_icon_click cb='+ cb +' cb_param='+ cb_param);
   var el = ev.currentTarget;
   console.log(el);
   w3_check_restart_reboot(el);

   // cb is a string because can't pass an object to onclick
   if (cb) {
      w3_call(cb, el, cb_param, false);
   }

   w3int_post_action();
}

function w3_icon_cb2(psa, fa_icon, size, color, cb, cb_param)
{
   // by default use pointer cursor if there is a callback
	var pointer = (cb && cb != '')? ' w3-pointer':'';
	var path = 'id-btn-grp-'+ w3int.btn_grp_uniq.toString();
	w3int.btn_grp_uniq++;
	cb_param = cb_param || 0;

	var font_size = null;
	if (isNumber(size) && size >= 0) font_size = px(size);
	else
	if (isString(size)) font_size = size;
	font_size = font_size? (' font-size:'+ font_size +';') : '';

	color = (color && color != '')? (' color:'+ color) : '';
	var onclick = cb? ('onclick="w3int_icon_click(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"') : '';
	var p = w3_psa(psa, path + pointer +' fa '+ fa_icon, font_size + color, onclick);
	var s = '<i '+ p +'></i>';
	//console.log(s);
	return s;
}

*/


////////////////////////////////
// input
////////////////////////////////

// Detect empty lines (only \n) because onchange event not fired in that case.
// Also detect and process control character sequences.
function w3int_input_keydown(ev, path, cb)
{
   var i, s;
   var k = ev.key.toUpperCase();
   var ctrl = ev.ctrlKey;
	var el = w3_el(path);
   if (!el) return;
   var input_any_key = w3_contains(el, 'w3-input-any-key');
   var input_any_change = w3_contains(el, 'w3-input-any-change');
   var disabled = w3_parent_with(el, 'w3-disabled');
   //w3_remove(el, 'kiwi-pw');
   var trace = w3_contains(el, 'w3-trace');
   if (trace) console.log('w3int_input_keydown k='+ k + (ctrl? ' CTRL ':'') +' val=<'+ el.value +'> cb='+ cb +' disabled='+ disabled);
   var cb_a = cb.split('|');
   
   if (disabled) {
      if (trace) console.log('w3int_input_keydown w3-disabled');
      return;
   }
   
   if (input_any_key && cb_a[1]) {
      //console.log('w3int_input_keydown: input_any_key '+ k +' cb_a[1]='+ cb_a[1]);
      //event_dump(ev, "input_any_change", true);
      w3_call(cb_a[1], ev, true);
   }

	if (ev.key == 'Enter') {
      if (input_any_change) {
         // consider unchanged input value followed by Enter, at the end of input text, to be an input change
         //console.log('w3int_input_keydown: w3-input-any-change + Enter');

         // A newline is going to be added to el.value after w3int_input_keydown() returns.
         // If we are currently positioned somewhere in the potential run of newlines at the end of the text
         // then remove all the trailing newlines so only a single newline will remain.
         s = el.value;
         //console.log('newline strip: ss='+ el.selectionStart +' tl='+ el.textLength);
         for (i = el.selectionStart; i < el.textLength; i++) {
            if (s.charAt(i) != '\n')
               break;
         }
         //console.log('newline strip: i='+ i +' tl='+ el.textLength);
         if (i == el.textLength) {
            while (s.endsWith('\n'))
               s = s.slice(0, -1);
            //console.log('s='+ JSON.stringify(s));
            w3_set_value(el, s);    // update field so el.selectionStart etc below are also updated
         }

         if (trace) console.log('ss='+ el.selectionStart +' se='+ el.selectionEnd +' tl='+ el.textLength);
         if (
            // cursor at end of text
            (el.selectionStart == el.selectionEnd && el.selectionStart == el.textLength) ||
            
            // all text is selected
            (el.selectionStart == 0 && el.selectionEnd == el.textLength)) {
            if (trace) console.log('w3_input_change...');
            //console.log('el.value='+ JSON.stringify(w3_el('id-ale_2g-user-textarea').value));
            //console.log(el);
            w3_input_change(path, cb, 'kd');
         }
      } else
	   if (el.value == '') {
         // cause empty input lines followed by Enter to send empty command to shell
         //console.log('w3int_input_keydown: empty line + Enter');
         w3_input_change(path, cb, 'kd');
      }
	}

   // if Delete key (Backspace) when entire value is selected then consider it an input change
	if (ev.key == 'Backspace' && input_any_change && el.selectionStart == 0 && el.selectionEnd == el.value.length) {
      //console.log('w3int_input_keydown Delete: len='+ el.value.length +' ss='+ el.selectionStart +' se='+ el.selectionEnd);
      el.value = '';
      w3_input_change(path, cb, 'kd');
	}
}

function w3int_input_keyup(ev, path, cb)
{
/*
	var el = w3_el(path);
	if (ev.key == 'Backspace' && el.value == '')
      w3_input_change(path, cb, 'ku');
*/
}

// event generated from w3_input_force()
function w3_input_process(ev)
{
   var a = ev.detail.split('|');
   var path = a[0];
   var cb = a[1];
   //console_nv('w3_input_process', {ev}, {a}, {path}, {cb});
	var el = w3_el(path);
	if (el) {
      var trace = w3_contains(el, 'w3-trace');
      if (trace) console.log('w3_input_process path='+ path +' val='+ el.value +' cb='+ cb);
      w3_input_change(path, cb, 'w3_input_process');
   }
}

// 'from' arg (that is appended to w3_call() cb_a[] arg) is:
//    'ev' if called from normal onchange event
//    'kd' or 'ku' if 'w3-input-any-change' is specified and the "any change" criteria is met
//    'in' if 'w3-input-evt' is specified and an input event has been generated
function w3_input_change(path, cb, from)
{
	var el = w3_el(path);
	if (el) {
      var trace = w3_contains(el, 'w3-trace');
      if (trace) console.log('w3_input_change path='+ path +' from='+ from);
      
      // cb is a string because can't pass an object to onclick
      if (cb) {
         var cb_a = cb.split('|');
         if (isArg(from)) cb_a.push(from);
         //el.select();
         w3_call(cb_a[0], path, el.value, /* first */ false, cb_a);
         if (from == 'in') return;
      }

      w3_check_restart_reboot(el);
      //w3_check_cfg_reload(el);
      w3_schedule_highlight(el);

/*
      if (el.value != '' && w3_contains(el, 'w3-encrypted')) {
         w3_add(el, 'kiwi-pw');
         el.value = '(encrypted)';
      }
*/
   }
	
   w3int_post_action();
}

function w3int_input_up_down_cb(path, cb_param, first, ev)
{
   //event_dump(ev, path, 1);
   if (ev.type != 'click' && ev.type != 'touch') return;
   var cb_a = cb_param.split('|');
   var delta = +cb_a[0];
   var id_s = cb_a[1];
   var cb_s = cb_a[2];
   var val = +w3_el(id_s).value + delta;
   //console_nv('w3int_input_up_down_cb', {cb_param}, {ev});
   w3_input_force(id_s, cb_s, val.toFixed(0));
}

// No cb_param here because w3_input_change() passes the input value as the callback parameter.
// But can specify a composite callback name (i.e. "cb|param0|param1") that is passed to callback routine.
//
// NB: using w3_esc_dq(val) below eliminates the need to call admin_set_decoded_value() via
// admin tab *_focus() routines solely to work around the escaping of double quotes in the
// val issue.

function w3_input(psa, label, path, val, cb, placeholder)
{
   var dump = psa.includes('w3-dump');
   if (dump) console.log('w3_input()...');
	var id = w3_add_id(path);
	cb = cb || '';
	var phold = placeholder? ('placeholder="'+ placeholder +'"') : '';
	var custom = psa.includes('w3-custom-events');
	var onchange = ' onchange="w3_input_change('+ sq(path) +', '+ sq(cb) +', '+ sq('ev') +')"';
	var oninput = psa.includes('w3-input-evt')? (' oninput="w3_input_change('+ sq(path) +', '+ sq(cb) +', '+ sq('in') +')"') : '';
	var onkeydown = ' onkeydown="w3int_input_keydown(event, '+ sq(path) +', '+ sq(cb) +')"';
	var onkeyup = ' onkeyup="w3int_input_keyup(event, '+ sq(path) +', '+ sq(cb) +')"';
	var events = (path && !custom)? (onchange + oninput + onkeydown + onkeyup) : '';
	val = ' value='+ dq(w3_esc_dq(val) || '');
	var inline = psa.includes('w3-label-inline');
	var bold = !psa.includes('w3-label-not-bold');
	label = label || '';
	var label_spacing = (label != '' && inline)? 'w3int-margin-input' : '';

	// type="password" is no good because it forces the submit to be https which we don't support
	//var type = 'type='+ (psa.includes('w3-password')? '"password"' : '"text"');

   var psa3 = w3_psa3(psa);
   if (dump) console.log(psa3);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
   var style = psa.includes('w3-no-styling')? '' : 'w3-input w3-border w3-hover-shadow';
	var type = psa3.right.includes('type=')? '' : 'type="text"';
	var psa_inner = w3_psa(psa3.right, w3_sb(id, style, label_spacing), '', w3_sb(type, phold));
	if (dump) console.log('O:['+ psa_outer +'] L:['+ psa_label +'] I:['+ psa_inner +']');
	
   // NB: include id in an id= for benefit of keyboard shortcut field detection
	var in_s = '<input id='+ dq(id) +' '+ psa_inner + val + events +'>';
	var cb_s = '|'+ id +'|'+ cb;
	
	if (psa.includes('w3-up-down')) {
	   in_s = w3_inline('',
	      w3_div('w3-flex-col w3-margin-R-4',
	         w3_icon('w3-momentary w3-pointer w3-margin-B-1||title="up"', 'fa-arrow-up', 15, 'orange', 'w3int_input_up_down_cb', '1'+ cb_s),
	         w3_icon('w3-momentary w3-pointer w3-margin-T-1||title="down"', 'fa-arrow-down', 15, 'cyan', 'w3int_input_up_down_cb', '-1'+ cb_s)
	      ),
	      in_s
	   );
   }

	var s =
	   '<div '+ psa_outer +'>' +
         w3int_label(psa_label, label, path) +
         in_s +
      '</div>';
	if (dump) console.log(s);
	//w3int_input_set_id(id);
	return s;
}

function w3_input_force(path, cb, input)
{
   //console_nv('w3_input_force', {path}, {cb}, {input});
   var el = w3_el(path);
   if (!el) return;
   el.addEventListener('input', w3_input_process, true);
   el.value = input.replace('&vbar;', '|');
   el.dispatchEvent(new CustomEvent('input', { detail: path +'|'+ cb }));
   el.removeEventListener('input', w3_input_process, true);
}

/*
function w3int_input_set_id(id)
{
   console.log('### w3int_input_set_id el='+ id +' DO NOT USE');
   return;
	//if (id == '') return;
	//setTimeout(function() { w3int_input_set_id_timeout(id); }, 3000);
}

function w3int_input_set_id_timeout(id)
{
   var el = w3_el(id);
   //console.log('### w3int_input_set_id_timeout el='+ id +' CONSIDER');
   if (!el) return;
   if (el.id != '') {
      console.log('### w3int_input_set_id_timeout el='+ id +' id='+ id +' ALREADY SET?');
      return;
   }
   w3_iterate_classList(el, function(className, idx) {
	   if (className.startsWith('id-')) {
         console.log('### w3int_input_set_id_timeout el='+ id +' id='+ id +' SET --------');
	      el.id = className;
	   }
   });
}
*/

// used when current value should come from config param
function w3_input_get(psa, label, path, cb, init_val, placeholder)
{
   // FIXME
   //var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val, save);
	cur_val = kiwi_decodeURIComponent('w3_input_get:'+ path, cur_val);
	//console.log('w3_input_get: path='+ path +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input(psa, label, path, cur_val, cb, placeholder);
}


////////////////////////////////
// textarea
////////////////////////////////

// The test for 'w3-input-any-change' that suppresses inclusion of the 'onchange=' event handler
// fixes a strange problem we noticed: With the 'onchange=' the actual onchange event is long-delayed
// until the first unrelated click completely outside the textarea! This ends up causing double calls
// to w3_input_change(). The first from w3int_input_keydown() due to w3-input-any-change and the second
// from the delayed change event.
//
// The problem doesn't seem to occur when w3-input-any-change is used with w3_input()

function w3_textarea(psa, label, path, val, rows, cols, cb)
{
	var id = w3_add_id(path);
	var spacing = (label != '')? ' w3-margin-T-8' : '';
	var custom = psa.includes('w3-custom-events');
	var onchange = psa.includes('w3-input-any-change')? '' : (' onchange="w3_input_change('+ sq(path) +', '+ sq(cb) +')"');
	var onkeydown = ' onkeydown="w3int_input_keydown(event, '+ sq(path) +', '+ sq(cb) +')"';
	var onkeyup = ' onkeyup="w3int_input_keyup(event, '+ sq(path) +', '+ sq(cb) +')"';
	var events = (path && !custom)? (onchange + onkeydown + onkeyup) : '';
	var bold = !psa.includes('w3-label-not-bold');
   var psa3 = w3_psa3(psa);
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, 'w3-input w3-border w3-hover-shadow '+ id + spacing, '', 'rows='+ dq(rows) +' cols='+ dq(cols));
	val = val || '';

	var s =
	   w3_div(psa3.left,
         w3int_label(psa_label, label, path) +
		   '<textarea '+ psa_inner + events +'>'+ val +'</textarea>'
		);
	//if (psa.includes('w3-dump')) console.log('$');
	//if (psa.includes('w3-dump')) console.log(psa3);
	//if (psa.includes('w3-dump')) console.log(psa_inner);
	//if (psa.includes('w3-dump')) console.log('label='+ label);
	//if (psa.includes('w3-dump')) console.log('val='+ val);
	if (psa.includes('w3-dump')) console.log('s='+ s);
	return s;
}

// used when current value should come from config param
function w3_textarea_get_param(psa, label, path, rows, cols, cb, init_val)
{
   // FIXME
   //var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val, save);
	cur_val = kiwi_decodeURIComponent('w3_textarea_get_param:'+ path, cur_val);
	//if (psa.includes('w3-dump')) console.log('w3_textarea_get_param: path='+ path +' cur_val="'+ cur_val +'"');
	return w3_textarea(psa, label, path, cur_val, rows, cols, cb);
}


////////////////////////////////
// checkbox
////////////////////////////////

function w3int_checkbox_change(path, cb, cb_param)
{
	var el = w3_el(path);
	w3_check_restart_reboot(el);
	
	// cb is a string because can't pass an object to onclick
	if (cb) {
	   //console.log('w3int_checkbox_change el='+ el +' checked='+ el.checked);
		//el.select();
		w3_schedule_highlight(el);
		w3_call(cb, path, el.checked, /* first */ false, cb_param);
	}

   w3int_post_action();
}

function w3_checkbox(psa, label, path, checked, cb, cb_param)
{
	var id = w3_add_id(path);
	var onchange = ' onchange="w3int_checkbox_change('+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';
	var checked_s = checked? ' checked' : '';
	var inline = psa.includes('w3-label-inline');
	var left = psa.includes('w3-label-left');
	var bold = !psa.includes('w3-label-not-bold');
	var spacing = (label != '' && inline)? (left? ' w3-margin-L-8' : ' w3-margin-R-8') : '';

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, 'w3-input w3-width-auto w3-border w3-pointer w3-hover-shadow '+ id + spacing, '', 'type="checkbox"');

   var ls = w3int_label(psa_label, label, path);
   var cs = '<input '+ psa_inner + checked_s + onchange +'>';
	var s =
	   '<div '+ psa_outer +'>' +
	      (left? (ls + cs) : (cs + ls)) +
      '</div>';

	// run the callback after instantiation with the initial control value
	if (cb)
		setTimeout(function() {
			//console.log('w3_checkbox: initial callback: '+ cb +'('+ sq(path) +', '+ checked +')');
			w3_call(cb, path, checked, /* first */ true, cb_param);
		}, 500);

	//if (label == 'Title') console.log(s);
	return s;
}

// used when current value should come from config param
function w3_checkbox_get_param(psa, label, path, cb, init_val, cb_param)
{
   var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val, save, update_path_var);
	//console.log('w3_checkbox_get_param: path='+ path +' update_path_var='+ update_path_var +' cur_val='+ cur_val +' init_val='+ init_val);
	return w3_checkbox(psa, label, path, cur_val, cb, cb_param);
}

function w3_checkbox_get(path)
{
   var el = w3_el(path);
   if (!el) return;
	return (el.checked? true:false);
}

function w3_checkbox_set(path, checked)
{
   var el = w3_el(path);
   //console.log('w3_checkbox_set path='+ path +' el=...');
   //console.log(el);
   if (!el) return;
	el.checked = checked? true:false;
}


////////////////////////////////
// select (HTML menu)
////////////////////////////////

var W3_SELECT_SHOW_TITLE = -1;

// Event behavior of w3_select() i.e. HTML <select>|<option> elements
//    menu open: MD focus
//    change menu item: MU blur change click
//    same menu item: MD MU w3_click-tgt=OPTION-BLUR blur FSEL click
//    when menu popup doesn't cover cursor: MU w3_click-tgt=SELECT blur FSEL click
//
// Since change+blur events are not fired if the same menu item is clicked
// must do blur here on click event. But only if same selected item is detected
// by ev.target being an <option> element.

function w3int_select_click(ev, path, cb, cb_param)
{
   // ev.target = SELECT|OPTION, ev.currentTarget = SELECT
   //console.log('w3int_select_click: '+ w3_id(ev.currentTarget));
	var el = ev.target;
	if (el && el.nodeName && el.nodeName == 'OPTION') {
      //console.log('w3int_select_click: same menu item clicked: BLUR and FSEL');
	   el.blur();
      w3int_post_action();
   }
}

function w3int_select_change(ev, path, cb, cb_param)
{
   // ev.target = SELECT|OPTION, ev.currentTarget = SELECT
   //console.log('w3int_select_change: CHANGE '+ w3_id(ev.currentTarget));
	var el = ev.currentTarget;
	w3_check_restart_reboot(el);

	// cb is a string because can't pass an object to onclick
	if (cb) {
		w3_call(cb, path, el.value, /* first */ false, cb_param);
	}
	
   w3int_post_action();
}

function w3int_select(psa, label, title, path, sel, opts_s, cb, cb_param)
{
	var id = w3_add_id(path);
	var first = '';

	if (title != '') {
      var as = title.split('<br>');
      as.forEach(function(s,i) {
         first += '<option value="-1" '+ ((sel == W3_SELECT_SHOW_TITLE && i == 0)? 'selected':'') +' disabled>' + s +'</option>';
      });
	} else {
		if (sel == W3_SELECT_SHOW_TITLE) sel = 0;
	}
	
	var inline = psa.includes('w3-label-inline');
	var bold = !psa.includes('w3-label-not-bold');
	var spacing = (label != '' && !inline && !psa.includes('w3-margin-T-'))? 'w3-margin-T-8' : '';
	if (inline) spacing += 'w3-margin-left';
	if (cb == undefined) cb = '';
	var onchange = 'onchange="w3int_select_change(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';
	var onclick = 'onclick="w3int_select_click(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, w3_sb(id, 'w3-select-menu', spacing), '', onchange +' '+ onclick);

	var s =
	   '<div '+ psa_outer +'>' +
         w3int_label(psa_label, label, path) +
         // need <br> because both <label> and <select> are inline elements
         ((!inline && label != '')? '<br>':'') +
         '<select '+ psa_inner +'>'+ first + opts_s +'</select>' +
      '</div>';

	// run the callback after instantiation with the initial control value
	if (cb && sel != W3_SELECT_SHOW_TITLE)
		setTimeout(function() {
			//console.log('w3_select: initial callback: '+ cb +'('+ sq(path) +', '+ sel +')');
			w3_call(cb, path, sel, /* first */ true, cb_param);
		}, 500);

	if (psa.includes('w3-dump')) console.log(s);
	return s;
}

function w3int_select_options(sel, opts, show_empty)
{
   var s = '';
   
   // 'min:max'
   // range of integers (increment one assumed)
   if (isString(opts)) {
      var rng = opts.split(':');
      if (rng.length == 2) {
         var min = +rng[0];
         var max = +rng[1];
         var idx = 0;
         for (var i = min; i <= max; i++) {
            s += '<option value='+ dq(idx) +' '+ ((idx == sel)? 'selected':'') +'>'+ i +'</option>';
            idx++;
         }
      }
   } else

   // [ n, n ... ]
   // [ 's', 's', n, 's' ... ]
   // [ 's', n, { first_key:text, [name:text,] key:x ... }, n ... ]
   // mixed array of strings, numbers or objects
   // if object, take first object element's value as menu option text
   //    *unless* value obj.name exists, then obj.name is menu option text
   if (isArray(opts)) {
      opts.forEach(function(obj, i) {
         if (obj == null) return;
         if (isObject(obj)) {
            //var keys = Object.keys(obj);
            //obj = obj[keys[0]];
            var hasName = isDefined(obj.name);
            if (hasName && !show_empty && obj.name == '') return;
            obj = hasName? obj.name : w3_obj_seq_el(obj, 0);
         }
         s += '<option value='+ dq(i) +' '+ ((i == sel)? 'selected':'') +'>'+ obj +'</option>';
      });
   } else

   // { key0:text0, key1:text1, obj_text:{ [name:text] [value:i] } ... }
   // object: enumerate sequentially like an array.
   // object elements
   //    non-object: (number, string, etc.)
   //       use element value as menu option text
   //       option value is element's sequential position
   //    object:
   //       key itself is menu option text
   //          *unless* value obj.name exists, then obj.name is menu option text
   //       option value is elements's sequential position
   //          *unless* value obj.value exists, then obj.value is option value
   if (isObject(opts)) {
      if (opts == null) return '';
      w3_obj_enum(opts, function(key, i, obj) {
         var value, text, disabled = false;
         if (isObject(obj)) {
            value = isDefined(obj.value)? obj.value : i;
            var hasName = isDefined(obj.name);
            if (hasName && !show_empty && obj.name == '') return;
            text = hasName? obj.name : key;
            if (isDefined(obj.disabled) && obj.disabled) disabled = true;
         } else {
            value = i;
            text = obj;
         }
         s += '<option value='+ dq(value) + ((i == sel)? ' selected':'') + (disabled? ' disabled':'') +'>'+ text +'</option>\n';
      });
   }
   
   // If sel is null, e.g. as a result of a failing match from w3_obj_key_seq(),
   // set menu to a disabled entry named "UNKNOWN".
   if (sel == null) {
      s += '<option value=-1 selected disabled>(unknown)</option>\n';
   }
   
   return s;
}

function w3_select(psa, label, title, path, sel, opts, cb, cb_param)
{
   var s = w3int_select_options(sel, opts, psa.includes('w3-show-empty'));
	if (psa.includes('w3-dump')) console.log(s);
   return w3int_select(psa, label, title, path, sel, s, cb, cb_param);
}

// hierarchical -- menu entries interspersed with disabled (non-selectable) headers
// { "key0":[fv0, fv1 ...], "key1":[fv0, fv1 ...] ... }
// object: enumerate sequentially like an array using:
//    object keys as the disabled menu entries (string only, numeric keys ignored)
//    arrays as the menu options
//       array elements are w3_first_value()'s e.g. int, string, first array value etc.
function w3int_select_hier(psa, label, title, path, sel, collapse, opts, cb, cb_param)
{
   var s = '';
   var idx = 0;
   if (!isObject(opts)) return;

   w3_obj_enum(opts, function(key, i, a) {
      if (!isNumber(+key)) {
         as = key.split('_');       // '_' => menu header line break
         as.forEach(function(e) {
            if (!e || e != '')
               s += '<option value='+ dq(idx++) +' disabled>'+ e +'</option> ';
         });
      }
      if (!isArray(a)) return;

      a.forEach(function(el, j) {
         if (collapse && j >= collapse) return;     // only show first collapse number of entries
         var v = w3_first_value(el);
         if (v == 'null') return;
         var v2 = w3_second_value(el);
         var _class = v2? (' class='+ dq(v2)) : '';
         s += '<option value='+ dq(idx++) +' id="id-'+ i +'-'+ j +'"' + _class +'>'+ v.toString() +'</option> ';
      });
   });
   
   /*
   var keys = Object.keys(opts);
   for (var i=0; i < keys.length; i++) {
      var key = keys[i];
      s += '<option value='+ dq(idx++) +' disabled>'+ key +'</option> ';
      var a = opts[key];
      if (!isArray(a)) continue;

      for (var j=0; j < a.length; j++) {
         var v = w3_first_value(a[j]);
         s += '<option value='+ dq(idx++) +' id="id-'+ i +'-'+ j +'">'+ v.toString() +'</option> ';
      }
   }
   */
   
   return w3int_select(psa, label, title, path, sel, s, cb, cb_param);
}

function w3_select_hier(psa, label, title, path, sel, opts, cb, cb_param)
{
   return w3int_select_hier(psa, label, title, path, sel, 0, opts, cb, cb_param);
}

function w3_select_hier_collapse(psa, label, title, path, sel, collapse, opts, cb, cb_param)
{
   return w3int_select_hier(psa, label, title, path, sel, collapse, opts, cb, cb_param);
}

// conditional -- individual menu entries can be enabled/disabled
// [ ['s', 1|0], ... ]
function w3_select_conditional(psa, label, title, path, sel, opts, cb, cb_param)
{
   var s = '';
   var idx = 0;
   if (!isArray(opts)) return;

   opts.forEach(function(el) {
      if (isArray(el))
         s += '<option value='+ dq(idx) +' '+ ((idx == sel)? 'selected':'') +' '+ (el[1]? '':'disabled') +'>'+ el[0] +'</option> ';
         idx++;
   });
   
   return w3int_select(psa, label, title, path, sel, s, cb, cb_param);
}

function w3_select_set_disabled(path, value, disabled, title)
{
   w3_iterate_children(path, function(el, i) {
      //console.log('w3_select_set_disabled['+i+']');
      //console.log(el);
      if (el.value == value) {
         el.disabled = disabled? true:false;
         if (isArg(title)) el.title = title;
      }
   });
}

// used when current value should come from config param
function w3_select_get_param(psa, label, title, path, opts, cb, init_val, cb_param)
{
   var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? 0 : init_val, save, update_path_var);
	return w3_select(psa, label, title, path, cur_val, opts, cb, cb_param);
}

function w3_select_enum(path, func)
{
   path = w3_el(path);
   if (!path) return;
	w3_iterate_children(path, func);
}

function w3_select_match(path, s)
{
   var found = false, idx = -1;
   w3_select_enum(path,
      function(e) {
         if (!found && !e.disabled && e.innerHTML.toLowerCase().startsWith(s)) {
            idx = e.value;
            found = true;
         }
      }
   );
   return { found:found, idx:idx };
}

function w3_select_get_value(path, idx)
{
   var found = null, last_disabled = null;
   w3_select_enum(path,
      function(e) {
         if (e.value == idx)
            found = e.innerHTML;
         if (!found && e.disabled)
            last_disabled = e.innerHTML;
      }
   );
   return { option:found, last_disabled:last_disabled };
}

function w3_select_value(path, idx, opt)
{
   if (w3_opt(opt, 'all')) {
      //console.log('{all:1}');
      w3_els(path, function(el) {
         //console.log(el);
         el.value = idx;
      });
   } else {
      var el = w3_el(path);
      if (!el) return;
      el.value = idx;
   }
}

function w3_select_set_if_includes(path, s, opt)
{
   var found = false;
   var re = RegExp(s);
   w3_select_enum(path, function(option) {
      //console.log('w3_select_set_if_includes s=['+ s +'] consider=['+ option.innerHTML +']');
      if (re.test(option.innerHTML)) {
         if (!found) {
            w3_select_value(path, option.value);
            if (w3_opt(opt, 'cb')) {
               w3_call(opt.cb, path, option.value);
            }
            //console.log('w3_select_set_if_includes TRUE value='+ option.value);
            found = true;
         }
      }
   });
   return found;
}


////////////////////////////////
// slider
////////////////////////////////

function w3int_slider_change(ev, complete, path, cb, cb_param)
{
	var el = ev.currentTarget;
	w3_check_restart_reboot(el);
	
	// cb is a string because can't pass an object to onclick
	if (cb) {
	   //console.log('w3int_slider_change path='+ path +' el.value='+ el.value);
		w3_call(cb, path, el.value, complete, /* first */ false, cb_param);
	}
	
	if (complete) {
	   //console.log('w3int_slider_change COMPLETE path='+ path +' el.value='+ el.value);
	   w3int_post_action();
	}
}

function w3int_slider_wheel(evt, path, cb, cb_param, need_shift_s)
{
   var need_shift = +need_shift_s;
   w3int.rate_limit_evt[cb] = evt;
   w3int.rate_limit_need_shift[cb] = need_shift;
   w3int.rate_limit[cb](cb_param);
   //console.log(cb +' need_shift='+ need_shift +' shiftKey='+ evt.shiftKey);
	if (!need_shift || evt.shiftKey)
	   evt.preventDefault();
}

function w3_slider_wheel(cb, el, cur, slow, fast)
{
   el = w3_el(el);
   if (!el) return null;
   var evt = w3int.rate_limit_evt[cb];
	//event_dump(evt, cb, 1);
   console.log(cb +' need_shift='+ w3int.rate_limit_need_shift[cb] +' shiftKey='+ evt.shiftKey +' button='+ evt.button +' buttons='+ evt.buttons);
	if (w3int.rate_limit_need_shift[cb] && !evt.shiftKey)
	   return null;
   var x = evt.deltaX;
   var y = evt.deltaY;
   var ay = Math.abs(y);
   var up_down = ((x < 0 && y <= 0) || (x >= 0 && y < 0));
   //console.log(x +' '+ y +' '+ (up_down? 'U':'D') +' '+ w3_id(evt.target));
   up_down = up_down? 1 : -1;
   var speed = up_down * ((ay < 50)? slow : fast);
   //console.log('y='+ y +' '+ speed);
   var nval = cur + speed;
   //console.log('w3_wheel '+ cb +' min='+ el.min +' max='+ el.max +' step='+ el.step +' cur='+ cur +' speed='+ speed +' nval='+ nval);
   return w3_clamp(nval, +el.min, +el.max);
}

//function w3int_slider_mobile(evt) { evt.preventDefault(); }

function w3_slider(psa, label, path, val, min, max, step, cb, cb_param)
{
   var dump = psa.includes('w3-dump');
	var id = w3_add_id(path);
	var inline = psa.includes('w3-label-inline');
	var bold = !psa.includes('w3-label-not-bold');
	var spacing = (label != '' && inline)? 'w3-margin-L-8' : '';
	if (inline) spacing += ' w3-margin-left';

	value = (val == null)? '' : w3_strip_quotes(val);
	value = 'value='+ dq(value);
	cb_param = cb_param || 0;
	var oc = ' oninput="w3int_slider_change(event, 0, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';
	// change event fires when the slider is done moving
	var os = ' onchange="w3int_slider_change(event, 1, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';
   //var om = kiwi_isMobile()? ' ontouchstart="w3int_slider_mobile(event)"' : '';
   
   var ow = '';
   if (psa.includes('w3-wheel')) {
      var need_shift = psa.includes('w3-wheel-shift')? 1:0;
      var cb_wheel = removeEnding(cb, '_cb') + '_wheel_cb';
      ow = ' onwheel="w3int_slider_wheel(event, '+ sq(path) +', '+ sq(cb_wheel) +', '+ sq(cb_param) +', '+ sq(need_shift) +')"';
      w3int.rate_limit[cb_wheel] = kiwi_rateLimit(cb_wheel, 170 /* msec */);
   }

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, w3_sb(id, spacing), '', value +
      ' type="range" min='+ dq(min) +' max='+ dq(max) +' step='+ dq(step) + oc + os + ow);

	var s = (label != '') ?
         ('<div '+ psa_outer +'>' +
            w3int_label(psa_label, label, path) +
            // need <br> because both <label> and <input-range> are inline elements
            ((!inline && label != '')? '<br>':'') +
            '<input '+ psa_inner +'>' +
         '</div>')
      :
         ('<input '+ psa_inner +'>');

	// run the callback after instantiation with the initial control value
	if (cb)
		setTimeout(function() {
			//console.log('w3_slider: initial callback: '+ cb +'('+ sq(path) +', '+ val +')');
			w3_call(cb, path, val, /* complete */ true, /* first */ true, cb_param);
		}, 500);

	if (dump) console.log(s);
	return s;
}

function w3_slider_set(path, val, cb)
{
   w3_set_value(path, val);
   w3_call(cb, path, val, /* complete */ true, /* first */ false);
}

function w3_slider_setup(path, min, max, step, val)
{
   var el = w3_el(path);
   if (!el) return el;
   //console.log('w3_slider_setup path='+ path +' min='+ min +' max='+ max +' step='+ step +' val='+ val);
   el.min = min;
   el.max = max;
   el.step = step;
   el.value = val;
   return el;
}

// used when current value should come from config param
function w3_slider_get_param(psa, label, path, min, max, step, cb, cb_param, init_val)
{
   var update_path_var = psa.includes('w3-update');
   var save = psa.includes('w3-defer')? EXT_SAVE_DEFER : undefined;
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? 0 : init_val, save, update_path_var);
	//console.log('w3_slider_get_param path='+ path +' update_path_var='+ update_path_var +' init_val='+ init_val +' cur_val='+ cur_val +' psa='+ psa);
   return w3_slider(psa, label, path, cur_val, min, max, step, cb, cb_param);
}


////////////////////////////////
// menu (Kiwi menu)
////////////////////////////////

function w3_menu(psa, cb)
{
	var id = psa.replace(/\s+/g, ' ').split(' |')[0];
   cb = cb || '';
   //console.log('w3_menu id='+ id +' psa='+ psa);

   var onclick = 'onclick="w3int_menu_onclick(event, '+ sq(id) +', '+ sq(cb) +')"' +
      ' oncontextmenu="w3int_menu_onclick(event, '+ sq(id) +', '+ sq(cb) +')"';
	var p = w3_psa(psa, 'w3-menu w3-menu-container w3-round-large', '', onclick);
   var s = '<div '+ p +'></div>';
   //console.log('w3_menu s='+ s);
   w3_el('id-w3-misc-container').innerHTML += s;
}

// menu items can be in argument list or passed as an array
function w3_menu_items(id, arr, max_vis)
{
   //console.log('w3_menu_items id='+ id);
   if (w3int.menu_debug) canvas_log('w3_menu_items cur='+ w3int.menu_cur_id);
   w3_menu_close('open');     // close any existing first
	w3int.menu_cur_id = id;
   
   var s = '';
   var i, idx = 0, prop, attr;
   var items;

   if (isArray(arr)) {
      items = arr;
   } else {
      // isObject(arr)
      items = [];
      for (i=1; i < arguments.length; i++)
         items.push(arguments[i]);
   }

   if (w3int.menu_debug) canvas_log('#='+ items.length);
   for (i=0; i < items.length; i++) {
   if (w3int.menu_debug) canvas_log('i='+ i +' '+ items[i]);
      if (items[i] == null) continue;
      prop = 'w3-menu ';
      attr = 'id='+ dq(idx);

      if (items[i] == '<hr>') {
         prop += 'w3-menu-item-hr';
         idx++;
      } else
      if (items[i].charAt(0) == '!') {    // first char == '!' hack to disable menu item
         prop += 'w3-menu-item-disabled';
         items[i] = items[i].substr(1);
         idx++;
      } else {
         prop += 'w3-menu-item';
         idx++;
         if (!kiwi_isMobile()) prop += ' w3-menu-item-hover';
      }
      var psa = w3_psa_mix('', prop, '', attr);
      s += w3_div(psa, items[i]);
   }
   
   if (w3int.menu_debug) canvas_log('s#='+ s.length);
   //console.log(s);
   var el = w3_el(id);
   el.innerHTML = s;
   if (w3int.menu_debug)
      canvas_log('BUILD');
   
   // NB: If "overflow-y: auto" is done instead of "overflow-y: scroll" (via w3-scroll-always-y)
   // then the scrollbar will intrude into the width of the menu elements and cause the longest
   // to lose right padding.
   max_vis = max_vis || items.length;
   if (items.length > max_vis) {
      el.style.height = px((max_vis * (4 + 19.5 + 4)) + 2*8);
      w3_add(el, 'w3-scroll-always-y');
   } else {
      el.style.height = '';
      w3_remove(el, 'w3-scroll-always-y');
   }
   
   // ":hover" highlighting doesn't work on mobile devices -- do it manually.
   // But NOT if the menu is long enough to be scrollable since there is no
   // gesture to distinguish mousing from scrolling, as with desktop.
   if (kiwi_isMobile()) {
      if (mobile_laptop_test) console.log('w3_menu_items MOBILE '+ id);
      w3_iterate_children(id, function(el, i) {
         el.addEventListener('touchmove', w3int_menu_touch_move, false);
         if (mobile_laptop_test)
            el.addEventListener('mousemove', w3int_menu_mouse_move, false);
      });
   }
}

// for testing
function w3int_menu_mouse_move(ev)
{
   var x = Math.round(ev.pageX);
   var y = Math.round(ev.pageY);
   w3int_menu_move('MMM', x, y);
}

function w3int_menu_touch_move(ev)
{
   //event_dump(ev, 'w3int_menu_touch_move');
   var x = Math.round(ev.touches[0].pageX);
   var y = Math.round(ev.touches[0].pageY);
   w3int_menu_move('MTM', x, y);
}

function w3int_menu_move(which, x, y)
{
   //console.log('w3int_menu_move '+ which);
   var el = document.elementFromPoint(x, y);
   if (!el) return;
   if (w3_isScrollingY(w3int.menu_cur_id)) return;
   var rtn = (!el || !w3_contains(el, 'w3-menu-item'));
   //console.log(which +' '+ x +':'+ y +':'+ el.id);
   //var el2 = w3_el(w3int.menu_cur_id);
   //console.log(el2.scrollTop +':'+ el2.scrollHeight +':'+ el2.clientHeight);

   if (w3int.prev_menu_hover_el && (rtn || (el != w3int.prev_menu_hover_el))) {
      w3int.prev_menu_hover_el.style.background = '';
      w3int.prev_menu_hover_el.style.color = '';
      if (rtn) {
         //if (w3int.menu_debug) canvas_log('R');
         return;
      }
   }

   el.style.background = 'rgb(79, 157, 251)';
   el.style.color = 'white';
   w3int.prev_menu_hover_el = el;
}

function w3_menu_popup(id, close_func, x, y)
{
   //console.log('w3_menu_popup id='+ id +' x='+ x +' y='+ y);
   var x0 = x, y0 = y;
   var el = w3_el(id);
	x = x || window.innerWidth/2;
	y = y || window.innerHeight/2;

	var slop = 16;
	el.w3_realigned = false;
	
	if ((x + el.clientWidth) + slop > window.innerWidth) {
	   x = window.innerWidth - el.clientWidth - slop;
	   //if (w3int.menu_debug) canvas_log('X:'+ x0 +':'+ y0);
	   el.w3_realigned_time = Date.now();
	   el.w3_realigned = true;
	}
   el.style.left = px(x);

   var yt = y + el.clientHeight + slop;
	//if (w3int.menu_debug) canvas_log('Y:'+ x0 +':'+ y0 +'/'+ yt +'>'+ window.innerHeight +' '+ TF(yt > window.innerHeight));
	if (yt > window.innerHeight) {
	   y = window.innerHeight - el.clientHeight - slop;
	   el.w3_realigned_time = Date.now();
	   el.w3_realigned = true;
	}
   el.style.top = px(y);
   //console.log('w3_menu_popup NEW id='+ id +' x='+ x +' y='+ y);

   w3_visible(el, true);
   if (w3int.menu_debug)
      canvas_log('VIS');
   w3int.menu_active = true;
   el.w3_menu_x = x;

	// close menu if escape key while menu is displayed
	w3int.menu_close_func = close_func;
   w3int.menu_first = true;
	window.addEventListener("keyup", w3int_menu_event, false);
	window.addEventListener("mousedown", w3int_menu_event, false);
	window.addEventListener("touchstart", w3int_menu_event, false);
	window.addEventListener("click", w3int_menu_event, false);
}

function w3_menu_active()
{
   return w3int.menu_active;
}

function w3int_menu_onclick(ev, id, cb, cb_param)
{
   if (w3int.menu_debug)
      canvas_log('menu_onclick '+ id +' from='+ (cb_param || ev.type));
   //console.log('w3int_menu_onclick id='+ id +' cb='+ cb);
   //if (ev != null) event_dump(ev, "MENU");
   var el = w3_el(id);

   // ignore false click from menu re-alignment (only if click is recent enough)
   //console.log('w3int_menu_onclick realigned='+ el.w3_realigned);
   if (el.w3_realigned) {
      el.w3_realigned = false;
      var when = Date.now() - el.w3_realigned_time;
      if (when < 50) {
         if (w3int.menu_debug) canvas_log('XRe'+ when);
         //console.log('w3int_menu_onclick: cancelEvent()');
         return cancelEvent(ev);
      }
      if (w3int.menu_debug) canvas_log('XOK'+ when);
   }

   if (ev != null && cb != null) {
      var _id = ev.target.id;
      var idx = +_id;
      if (_id == '' || isNaN(idx)) _id = ev.target.parentNode.id;
      idx = +_id;
      if (_id == '' || isNaN(idx)) idx = -1;
      //console.log('w3int_menu_onclick CALL idx='+ idx);
      if (w3int.menu_debug)
         canvas_log('CALL: '+ idx);
      if (idx == -1) return;     // clicked/touched top/bottom border -- don't dismiss menu
      w3_call(cb, idx, el.w3_menu_x, cb_param, ev);
   }

   w3_visible(el, false);
   if (w3int.menu_debug)
      canvas_log('NOT_VIS');
   w3int.menu_active = false;
   window.removeEventListener("keyup", w3int_menu_event, false);
   window.removeEventListener("mousedown", w3int_menu_event, false);
   window.removeEventListener("touchstart", w3int_menu_event, false);
   window.removeEventListener("click", w3int_menu_event, false);

   // allow right-button to select menu items
	if (ev != null) {
      if (w3int.menu_debug) canvas_log('Enull');
	   return cancelEvent(ev);
	}
}

// close menu if caller's close_func() returns true
function w3int_menu_event(evt)
{
   if (w3int.menu_debug) canvas_log('ME '+ evt.type);
   var el = w3_el(w3int.menu_cur_id);
   //console.log(el);
   //event_dump(evt, 'MENU-CLOSE', true);

   if (el && w3int.menu_close_func) {
      //console.log(w3int.menu_close_func);
      var close = w3int.menu_close_func(evt, w3int.menu_first);
      w3int.menu_first = false;
      if (w3int.menu_debug) canvas_log('CLOSE='+ close);
      if (close) w3_menu_close(evt.type);
   }
   //else canvas_log('no w3_menu_close()');
}

function w3_menu_close(from)
{
   if (w3int.menu_debug) canvas_log('w3_menu_close '+ from +' id='+ w3int.menu_cur_id);
   if (!w3int.menu_cur_id) return;
   var el = w3_el(w3int.menu_cur_id);
   if (!el) return;
   el.w3_realigned = false;
   w3int_menu_onclick(null, w3int.menu_cur_id, null, 'menu_close');
   w3int.menu_cur_id = null;
}


////////////////////////////////
// standard callbacks -- only set var
////////////////////////////////

function w3_num_cb(path, val)
{
	var v = parseFloat(val);
	if (val == '') v = 0;
	if (isNaN(v)) {
	   //console.log('w3_num_cb path='+ path);
	   w3_set_value(path, 0);
	   v = 0;
	}
	//console.log('w3_num_cb: path='+ path +' val='+ val +' v='+ v);
	setVarFromString(path, v);
}

function w3_bool_cb(path, val)
{
	v = +val;
	//console.log('w3_bool_cb: path='+ path +' val='+ val +' v='+ v);
	setVarFromString(path, v);
}

function w3_string_cb(path, val)
{
	//console.log('w3_string_cb: path='+ path +' val='+ val);
	setVarFromString(path, val.toString());
}


////////////////////////////////
// standard callbacks -- set cfg and var
////////////////////////////////

function w3_num_set_cfg_cb(path, val, first)
{
	var v = parseFloat(val);
	if (isNaN(v)) v = 0;
	
	// if first time don't save, otherwise always save
	var save = isArg(first)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function w3_int_set_cfg_cb(path, val, first)
{
	var v = parseInt(val);
	if (isNaN(v)) v = 0;
	w3_set_value(path, v);     // remove any fractional or non-number portion from field
	
	// if first time don't save, otherwise always save
	var save = isArg(first)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

// limit precision using callback spec: 'admin_float_cb|prec'
function w3_float_set_cfg_cb(path, val, first, cb_a)
{
   var prec = -1;    // default to no precision limiting applied
	//console.log('admin_float_cb '+ path +'='+ val +' cb_a.len='+ cb_a.length);
	if (cb_a && cb_a.length >= 2) {
	   prec = +(cb_a[1]);
	   if (isNaN(prec)) prec = -1;
	   //console.log('admin_float_cb prec='+ prec);
	}
	val = parseFloat(val);
	if (isNaN(val)) {
	   // put old value back
	   val = ext_get_cfg_param(path);
	} else {
	   if (prec != -1) {
         var s = val.toFixed(prec);    // NB: .toFixed() does rounding
         //console.log('admin_float_cb val('+ prec +')='+ s);
         val = +s;
      }
	   // if first time don't save, otherwise always save
	   var save = isArg(first)? (first? false : true) : true;
	   ext_set_cfg_param(path, val, save);
	}
   w3_set_value(path, val);   // remove any non-numeric part from field
}

function w3_bool_set_cfg_cb(path, val, first)
{
	var v;
	if (val == true || val == 1) v = true; else
	if (val == false || val == 0) v = false; else
	   v = false;
	
	// if first time don't save, otherwise always save
	var save = isArg(first)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function w3_string_old_set_cfg_cb(path, val, first)
{
	console.log('w3_string_old_set_cfg_cb: path='+ path +' '+ typeof(val) +' <'+ val +'> first='+ first);
	
	// if first time don't save, otherwise always save
	var save = isArg(first)? (first? false : true) : true;
	ext_set_cfg_param(path, encodeURIComponent(val.toString()), save);
}

function w3_string_set_cfg_cb(path, val, first)
{
	//console.log('w3_string_set_cfg_cb: path='+ path +' '+ typeof(val) +' <'+ val +'> first='+ first);
   w3_json_set_cfg_cb(path, val, first);
}

function w3_json_set_cfg_cb(path, val, first)
{
	//console.log('w3_json_set_cfg_cb: path='+ path +' '+ typeof(val) +' <'+ val +'> first='+ first);

	// verify that value is valid JSON
	var v = val;
	if (isString(v)) v = JSON.stringify(v);
   try {
      JSON.parse(v);
   } catch(ex) {
      console.log('w3_json_set_cfg_cb: path='+ path +' '+ typeof(val) +' <'+ val +'> first='+ first);
      console.log('w3_json_set_cfg_cb: DANGER value did not parse as JSON!');
      return;
   }

	// if first time don't save, otherwise always save
	var save = isArg(first)? (first? false : true) : true;
	ext_set_cfg_param(path, val, save);
}

// like w3_string_set_cfg_cb(), but enforces specification of leading 'http://' or 'https://'
function w3_url_set_cfg_cb(path, val, first)
{
	//console.log('w3_url_set_cfg_cb: path='+ path +' '+ typeof(val) +' <'+ val +'> first='+ first);
	if (val != '' && !val.startsWith('http://') && !val.startsWith('https://')) val = 'http://'+ val;
	w3_string_set_cfg_cb(path, val, first);
	w3_set_value(path, val);
}


// path is structured to be: [el_name][sep][idx]
// e.g. [id-dx.o.ty][_][123]
function w3_remove_trailing_index(path, sep)
{
   sep = sep || '-';    // optional separator, e.g. if negative indicies are used
   var re = new RegExp('(.*)'+ sep +'([-]?[0-9]+)');
	var el = re.exec(path);      // remove trailing -nnn
   //console.log('w3_remove_trailing_index path='+ path +' el='+ el);
	var idx;
	if (!el) {
	   idx = null;
	   el = path;
	} else {
	   idx = el[2];
	   el = el[1];
	}
	return { el:el, idx:idx };
}

var w3_MENU_NEXT = 1;
var w3_MENU_PREV = -1;

// cb_param = { dir:w3_MENU_{NEXT,PREV}, id:'...', func:'...', (optional, to skip non-numeric menu items) isNumeric:true }
function w3_select_next_prev_cb(path, cb_param, first)
{
   var val;
	if (dbgUs) console.log('w3_select_next_prev_cb cb_param='+ decodeURIComponent(cb_param));
   var cbp = w3_cb_param_decode(cb_param);
   if (!isObject(cbp)) {
	   console.log('w3_select_next_prev_cb cb_param is not an object');
	   return;
   }
	if (dbgUs) console.log('w3_select_next_prev_cb dir='+ cbp.dir +' id='+ cbp.id);
	
   // if any menu has a selection value then select next/prev (if any)
   var prev = 0, capture_the_next = 0, captured_next = 0, captured_prev = 0;
   var menu_s, el;
   
   for (var i = 0; (el = w3_el(menu_s = cbp.id + i)) != null; i++) {
      val = el.value;
      if (dbgUs) console.log('menu_s='+ menu_s +' value='+ val);
      if (val == -1) continue;
      
      w3_select_enum(menu_s, function(option) {
         var content_check = (isUndefined(cbp.isNumeric) || (cbp.isNumeric == true && isNumber(parseInt(option.innerText))));
	      if (option.disabled || !content_check) return;
         if (dbgUs) console.log('consider val='+ option.value +' \"'+ option.innerText +'\" content_check='+ content_check);
         if (capture_the_next) {
            captured_next = option.value;
            capture_the_next = 0;
         }
         if (option.value === val) {
            captured_prev = prev;
            capture_the_next = 1;
         }
         prev = option.value;
      });
      break;
   }

	if (dbgUs) console.log('i='+ i +' captured_prev='+ captured_prev +' captured_next='+ captured_next);
	val = 0;
	if (cbp.dir == w3_MENU_NEXT && captured_next) val = captured_next;
	if (cbp.dir == w3_MENU_PREV && captured_prev) val = captured_prev;
	if (dbgUs) console.log('w3_select_next_prev_cb val='+ val);
	//if (val)
	   w3_call(cbp.func, menu_s, val, first);
}


////////////////////////////////
// tables
////////////////////////////////

// caller can choose more specific table type, e.g. w3-table-fixed
// FIXME: deprecated, only still used in admin GPS
function w3_table(psa)
{
	var p = w3_psa(psa, 'w3-table w3-table-default');
	var s = '<table '+ p +'>';
		for (var i=1; i < arguments.length; i++) {
			s += arguments[i];
		}
	s += '</table>';
	//console.log(s);
	return s;
}

function w3_table_heads(psa)
{
	var p = w3_psa(psa, 'w3-table-head');
	var s = '';
	for (var i=1; i < arguments.length; i++) {
	   if (arguments[i] == null) continue;
		s += '<th '+ p +'>';
		s += arguments[i];
		s += '</th>';
	}
	//console.log(s);
	return s;
}

function w3_table_row(psa)
{
	var p = w3_psa(psa, 'w3-table-row');
	var s = '<tr '+ p +'>';
		for (var i=1; i < arguments.length; i++) {
	      if (arguments[i] == null) continue;
			s += arguments[i];
		}
	s += '</tr>';
	//console.log(s);
	return s;
}

function w3_table_cells(psa)
{
	var p = w3_psa(psa, 'w3-table-cell');
	var s = '';
	for (var i=1; i < arguments.length; i++) {
	   if (arguments[i] == null) continue;
		s += '<td '+ p +'>';
		s += arguments[i];
		s += '</td>';
	}
	//console.log(s);
	return s;
}


////////////////////////////////
// containers
////////////////////////////////

// If any psa prop is of the form N:prop then only use prop if N == current arg number
// e.g.
// w3_inline('foo/bar 1:baz', w3_div('zero'), w3_div('one'), w3_div('two')) =>
// <div class="foo">
//    <div class="bar zero"> ... </div>
//    <div class="bar baz one"> ... </div>
//    <div class="bar two"> ... </div>
// </div>
// This is a generalization of the w3-btn-right feature of w3int_button()

function w3_inline(psa, attr)
{
	var narg = arguments.length;
   var dump = psa.includes('w3-dump');
   
   if (psa == '' && attr == '' && narg > 2) {
      console.log('### w3_inline OLD API DEPRECATED');
      var args = Array.prototype.splice.call(arguments, 0);
      args.splice(0, 1);
      return w3_inline.apply(null, args);
   } else {
      var psa3 = w3_psa3(psa);
      var psa_outer = w3_psa(psa3.middle, 'w3-show-inline-new');
      var psa_inner = w3_psa(psa3.right);
         if (dump) console.log('w3_inline psa_outer='+ psa_outer +' psa_inner='+ psa_inner);
      var s = '<div w3d-inlo '+ psa_outer +'>';
      for (var i = 1; i < narg; i++) {
         var a = arguments[i];
         if (!a) continue;
         if (dump) console.log('w3_inline i='+ i +' a='+ a);
         
         // merge a pseudo psa specifier into the next argument
         // e.g. 'w3-*', w3_*('w3-psa') => w3_*('w3-* w3-psa')
         var psa_merge = false;
         if (a.startsWith('w3-') || a.startsWith('id-')) {
            psa = w3_psa(psa3.right, a);
            i++;
            a = arguments[i];
            psa_merge = true;
         } else {
            psa = '';
            if (psa_inner != '') {
               var psa3r = '';
               var a1 = psa3.right.split(' ');
               a1.forEach(
                  function(p1) {
                     //console.log('N:prop i='+ (i-1) +' p1=');
                     var a2 = p1.split(':');
                     //console.log(a2);
                     if (a2.length == 2) {
                        var n = +a2[0];
                        //console.log('i='+ i +' n='+ n +' '+ (n == (i-1)));
                        psa3r = w3_sb(psa3r, (n == (i-1))? a2[1] : '');
                     } else {
                        psa3r = w3_sb(psa3r, p1);
                     }
                  }
               );
               psa = w3_psa(psa3r);
            }
         }
         
         // If the psa is empty, and the arg is a div, don't wrap it with our usual enclosing div.
         // This solves the "w3_inline() + w3-hide" problem where our extra div
         // added by w3_inline isn't the one with the w3-hide, and causes unwanted spacing when
         // using w3_inline('w3-halign-space-between/').
         if (psa3.right == '' && !psa_merge && a.startsWith('<div '))
            s += a;
         else
            s += '<div w3d-inli-'+ w3_sb(i-1, psa) +'>'+ a + '</div>';
      }
      s += '</div>';
      if (dump) console.log(s);
      return s;
   }
}

// call w3_inline when parameters need to be accumulated in an array
function w3_inline_array(psa, ar)
{
	ar.unshift(psa);
	//console.log(ar);
   return w3_inline.apply(null, ar);
}

// see: stackoverflow.com/questions/29885284/how-to-set-a-fixed-width-column-with-css-flexbox
function w3_inline_percent(psa)
{
   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.middle, 'w3-show-inline-new');
	var narg = arguments.length;
	var total = 0;
	var prop = psa.includes('w3-debug')? 'w3_debug ' : '';
	var last_halign_end = psa.includes('w3-last-halign-end');
	var s = '<div w3d-inlpo '+ psa_outer +'>';
		for (var i = 1; i < narg; i += 2) {
		   var style;
		   if (i+1 < narg) {
		      style = 'flex-basis:'+ arguments[i+1] +'%';
		      total += arguments[i+1];
		   } else {
		      style = 'flex-basis:'+ (100 - total) +'%';
		   }
		   if (i == narg-1 && last_halign_end) prop += 'w3-flex w3-halign-end';
			s += '<div w3d-inlpi-'+ ((i-1)/2) +' '+ w3_psa(psa3.right, prop, style) +'>'+ arguments[i] + '</div>';
		}
	s += '</div>';
	//console.log(s);
	return s;
}

/*
// old version using div width:%
function w3_inline_percent_old(psa)
{
   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.middle, 'w3-show-inline-new');
	var narg = arguments.length;
	var total = 0;
	var s = '<div w3d-inlpo '+ psa_outer +'>';
		for (var i = 1; i < narg; i += 2) {
		   var style;
		   if (i+1 < narg) {
		      style = 'width:'+ arguments[i+1] +'%';
		      total += arguments[i+1];
		   } else {
		      style = 'width:'+ (100 - total) +'%';
		   }
			s += '<div w3d-inlpi-'+ ((i-1)/2) +' '+ w3_psa(psa3.right, '', style) +'>'+ arguments[i] + '</div>';
		}
	s += '</div>';
	//console.log(s);
	return s;
}
*/

function w3_div(psa)
{
   var p = w3_psa(psa);
	var s = '<div w3d-div '+ p +'>';
	var narg = arguments.length;
		for (var i=1; i < narg; i++) {
			s += arguments[i];
		}
	s += '</div>';
	//console.log(s);
	return s;
}

function w3_span(psa)
{
   var p = w3_psa(psa, 'w3-show-span');
	var s = '<div w3d-span '+ p +'>';
	var narg = arguments.length;
		for (var i=1; i < narg; i++) {
			s += arguments[i];
		}
	s += '</div>';
	//console.log(s);
	return s;
}

function w3_divs(psa, attr)
{
   var narg = arguments.length;
   var s, args;

   if (psa == '' && attr == '') {
      console.log('### w3_divs OLD API 1 DEPRECATED');
      //console.log('prop=<'+ psa +'> attr=<'+ attr +'>');
      //kiwi_trace();
      s = w3_div.apply(null, Array.prototype.splice.call(arguments, 1));
      //console.log(s);
      return s;
   } else
   if (psa != '' && attr == '' && narg > 2) {
      console.log('### w3_divs OLD API 2 DEPRECATED');
      //console.log('prop=<'+ psa +'> attr=<'+ attr +'>');
      //kiwi_trace();
      args = Array.prototype.splice.call(arguments, 0);
      args.splice(1, 1);
      s = w3_div.apply(null, args);
      //console.log(s);
      return s;
   } else
   if (psa == '' && attr && attr.startsWith('w3-') && narg > 2) {
      console.log('### w3_divs OLD API 3 DEPRECATED');
      //console.log('prop=<'+ psa +'> attr=<'+ attr +'>');
      //kiwi_trace();
      args = Array.prototype.splice.call(arguments, 0);
      args.splice(0, 1);
      s = w3_divs.apply(null, args);
      //console.log(s);
      return s;
   } else
   if (psa != '' && attr && attr.startsWith('w3-') && narg > 2) {
      console.log('### w3_divs OLD API 4 DEPRECATED');
      //console.log('prop=<'+ psa +'> attr=<'+ attr +'>');
      //kiwi_trace();
      args = Array.prototype.splice.call(arguments, 2);
      args.splice(0, 0, arguments[0] +'/'+ arguments[1]);
      s = w3_divs.apply(null, args);
      //console.log(s);
      return s;
   } else {
      var psa3 = w3_psa3(psa);
      var psa_outer = w3_psa(psa3.middle);
      var psa_inner = w3_psa(psa3.right);
      s = '<div w3d-divso '+ psa_outer +'>';
         for (var i=1; i < narg; i++) {
            s += '<div w3d-divsi-'+ (i-1) +' '+ psa_inner +'>'+ arguments[i] + '</div>';
         }
      s += '</div>';
      //console.log(s);
      return s;
   }
}

function w3_col_percent(psa)
{
   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.middle, 'w3-row');
	var narg = arguments.length;
	var s = '<div w3d-colpo '+ psa_outer +'>';
		for (var i = 1; i < narg; i += 2) {
		   var prop, style;
		   if (i+1 < narg) {
		      prop = 'w3-col';
		      style = 'width:'+ arguments[i+1] +'%';
		   } else {
		      prop = 'w3-rest';
		      style = '';
		   }
			s += '<div w3d-colpi-'+ ((i-1)/2) +' '+ w3_psa(psa3.right, prop, style) +'>'+ arguments[i] + '</div>';
		}
	s += '</div>';
	//console.log(s);
	return s;
}

// the w3-text makes it inline-block, so no surrounding w3_inline() needed
function w3_text(psa, text)
{
   var style = (psa.includes('w3-padding') || psa.includes('w3-nopad'))? '' : 'padding:0 4px 0 0';
   if (!psa.includes('w3-nobk')) style += '; background-color:inherit';
	var s = w3_div(w3_psa_mix(psa, 'w3-text', style), text? text:' ');
	//console.log(s);
	return s;
}

function w3_code(psa)
{
   var psa3 = w3_psa3(psa);
   var narg = arguments.length;
	var s = '<pre '+ w3_psa(psa3.left) +'><code '+ w3_psa(psa3.middle) +'>';
		for (var i=1; i < narg; i++) {
			s += '<code '+ w3_psa(psa3.right) +'>'+ arguments[i] + '</code>';
		}
	s += '</code></pre>';
	//console.log(s);
	return s;
}

function w3_header(psa, size)
{
   var psa3 = w3_psa3(psa);
   //console.log(psa3);
   var narg = arguments.length;
	var s = '<header '+ w3_psa(psa3.middle) +'><h'+ size +' '+ w3_psa(psa3.right) +'>';
		for (var i=2; i < narg; i++) {
			s += arguments[i];
		}
	s += '</h'+ size +'></header>';
	//console.log(s);
	return s;
}

function w3_half(prop_row, prop_col, left, right, prop_left, prop_right)
{
	if (prop_left == undefined) prop_left = '';
	if (prop_right == undefined) prop_right = '';

	var s =
	'<div class="w3-row '+ prop_row +'">' +
		'<div class="w3-col w3-half '+ prop_col + prop_left +'">' +
			left +
		'</div>' +
		'<div class="w3-col w3-half '+ prop_col + prop_right +'">' +
			(right? right:'') +
		'</div>' +
	'</div>';
	//console.log(s);
	return s;
}

function w3_third(prop_row, prop_col, left, middle, right)
{
	var s =
	'<div class="w3-row '+ prop_row +'">' +
		'<div class="w3-col w3-third '+ prop_col +'">' +
			left +
		'</div>' +
		'<div class="w3-col w3-third '+ prop_col +'">' +
			(middle? middle:'') +
		'</div>' +
		'<div class="w3-col w3-third '+ prop_col +'">' +
			(right? right:'') +
		'</div>' +
	'</div>';
	//console.log(s);
	return s;
}

function w3_quarter(prop_row, prop_col, left, middleL, middleR, right)
{
	var s =
	'<div class="w3-row '+ prop_row +'">' +
		'<div class="w3-col w3-quarter '+ prop_col +'">' +
			left +
		'</div>' +
		'<div class="w3-col w3-quarter '+ prop_col +'">' +
			(middleL? middleL:'') +
		'</div>' +
		'<div class="w3-col w3-quarter '+ prop_col +'">' +
			(middleR? middleR:'') +
		'</div>' +
		'<div class="w3-col w3-quarter '+ prop_col +'">' +
			(right? right:'') +
		'</div>' +
	'</div>';
	//console.log(s);
	return s;
}

function w3_canvas(psa, w, h, opt)
{
   var s = '';
   var pad = w3_opt(opt, 'pad', null);
   var pad_top = w3_opt(opt, 'pad_top', 0);
   var pad_bot = w3_opt(opt, 'pad_bottom', 0);
   if (!isNull(pad)) pad_top = pad_bot = pad;
   pad = pad_top + pad_bot;
   if (pad) {
      //w -= pad*2; h -= pad*2;
      s += sprintf(' padding-top:%dpx; padding-bottom:%dpx;', pad_top, pad_bot);
   }

   var left = w3_opt(opt, 'left', '');
   if (left != '') s += sprintf(' left:%dpx;', left);
   var right = w3_opt(opt, 'right', '');
   if (right != '') s += sprintf(' right:%dpx;', right);
   var top = w3_opt(opt, 'top', '');
   if (top != '') s += sprintf(' top:%dpx;', top);

   s = '<canvas '+ w3_psa(psa, null, 'position:absolute;'+ s, sprintf('width="%d" height="%d"', w, h)) +'></canvas>';
	//console.log(s);
	return s;
}
