// Copyright (c) 2016-2019 John Seamons, ZL/KF6VO

/*

	///////////////////////////////////////
	// API summary
	///////////////////////////////////////
	
	                           integrated: L=label T=text 3=psa3()
	                           |     callback
	                                 |     first: F=always false, not called on instan
	                                       |
	nav navdef

	label set_label

	link
   
	radio_button               T
	radio_button_get_param     T
	radio_unhighlight
	
	switch                     T     **Needs L added
	switch_set_value
	
	button                     T
	button_text                T
	icon
	
	input                      L3    cb    F
	input_change
	input_get
	
	textarea                   L
	textarea_get_param         L
	
	checkbox                   L3
	checkbox_get_param         L
	checkbox_get
	checkbox_set
	
	select                     L3
	select_hier                L
	select_get_param           L
	select_enum
	select_value
	select_set_if_includes
	
	slider                     L3
	slider_set
	
	menu
	menu_items
	menu_popup
	menu_onclick
	
	inline                     3
	inline_percent             3
	divs                       3
	col_percent                3
	
	
	
	///////////////////////////////////////
	// Useful stuff
	///////////////////////////////////////
	
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


	///////////////////////////////////////
	// Notes about HTML/DOM
	///////////////////////////////////////
	
	"Typically, the styles are merged, but when conflicts arise, the later declared style will generally win
	(unless the !important attribute is specified on one of the styles, in which case that wins).
	Also, styles applied directly to an HTML element take precedence over CSS class styles."
	
	window.
		inner{Width,Height}		does not include tool/scroll bars
		outer{Width,Height}		includes tool/scroll bars
	
	element.
		client{Width,Height}		viewable only; no: border, scrollbar, margin; yes: padding
		offset{Width,Height}		viewable only; includes padding, border, scrollbars

	
	///////////////////////////////////////
	// FIXME cleanups
	///////////////////////////////////////
	
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
	
	
	///////////////////////////////////////
	// API users
	///////////////////////////////////////
	
	1) kiwisdr.com website content
	
	2) antenna switch extension is a user of API:
	   visible_block()         DEP
	   w3_div()
	   w3_divs()
	   w3_inline()                   only in OLDER versions of the ant_switch ext
	   w3_btn()
	   w3_radio_btn(yes/no)    DEP   w3_radio_button
	   w3_input_get_param()    DEP   w3_input_get
	   w3_string_set_cfg_cb()
	   w3_highlight()
	   w3_unhighlight()
	   w3_radio_unhighlight()
	   var w3_highlight_time

*/


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

// a single-argument call that silently continues if func not found
function w3_call(func, arg0, arg1, arg2, arg3, arg4)
{
   var rv = undefined;

   if (!isArg(func)) return rv;
   
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
	   } else
	      console.log('w3_call: func not a string or function');
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

function w3_opt(opt, elem, default_val)
{
   //console.log('w3_opt opt:');
   //console.log(opt);
   if (isDefined(opt) && isDefined(opt[elem])) {
      //console.log('w3_opt elem='+ elem +' DEFINED rv='+ opt[elem]);
      return opt[elem];
   } else {
      //console.log('w3_opt elem='+ elem +' NOT DEFINED rv='+ default_val);
      return default_val;
   }
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
// param:   name[:val]
// returns: { match:true|false, has_value:true|false, num:parseFloat(val), string:val }
function w3_ext_param(s, param)
{
   var pu = param.split(':');
   var pl = param.toLowerCase().split(':');
   if (s.startsWith(pl[0])) {
      rv = { match: true };
      if (pl.length > 1) {
         rv.has_value = true;
         rv.num = parseFloat(pl[1]);
         rv.string = pl[1];
         rv.string_case = pu[1];
      } else {
         rv.has_value = false;
         rv.num = 0;
         rv.string = '';
      }
      return rv;
   }
   return { match: false }
}

function w3_clamp(v, min, max, clamp_val)
{
   if (isNaN(v)) return (isDefined(clamp_val)? clamp_val : Math.NaN);
   
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

function w3_clamp3(v, min, max, clamp_min, clamp_max, clamp_NaN)
{
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
   path = path || '';
   if (path == '') return '';
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
			el = w3int_w3_el('id-'+ el_id);
			if (el == null) {
				el_id = w3_add_toplevel(el_id);
				el = w3int_w3_el(el_id);
				if (el == null) {
					el = w3int_w3_el('id-'+ el_id);
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
			els = w3int_w3_els('id-'+ el_id);
			if (els == null) {
				el_id = w3_add_toplevel(el_id);
				els = w3int_w3_els(el_id);
				if (els == null) {
					els = w3int_w3_els('id-'+ el_id);
				}
			}
		}
      if (els && func) for (var i = 0; i < els.length; i++) {
         func(els[i], i);
      }
		return els;
	}
	return (w3_el(el_id));
}

// return id of element (i.e. 'id-*') if found
function w3_id(el_id)
{
	var el = w3_el(el_id);
	if (!el) return null;
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
	return id;
}

// assign innerHTML, silently failing if element doesn't exist
function w3_innerHTML(id)
{
	//var el = w3_el_softfail(id);
	var el = w3_el(id);
	if (!el) return null;
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
	var els = document.getElementsByClassName(cname);
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
	var el = w3_el(el_id);
	
	for (var i=0; i < el.children.length; i++) {    // el.children is a collection, can't use forEach()
		var child_el = el.children[i];
		func(child_el, i);
	}
}

// excludes text and comment nodes
function w3_iterateDeep_children(el_id, func)
{
	var el = w3_el(el_id);
	
	for (var i=0; i < el.children.length; i++) {    // el.children is a collection, can't use forEach()
		var child_el = el.children[i];
		func(child_el);
		if (child_el.hasChildNodes)
			w3_iterateDeep_children(child_el, func);
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

// bounding box measured from the origin of parent
function w3_boundingBox_children(el_id, debug)
{
	var bbox = { x1:1e99, x2:0, y1:1e99, y2:0, w:0, h:0 };
	w3_iterateDeep_children(el_id, function(el) {
		if (el.nodeName != 'DIV' && el.nodeName != 'IMG')
			return;
		var position = css_style(el, 'position');
		if (position == 'static')
		   return;
		if (el.offsetHeight == 0)
		   return;
		if (debug) console.log(el);
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
	if (debug) w3_console.log(bbox, 'w3_boundingBox_children');
	return bbox;
}

function w3_center_in_window(el_id, id)
{
	var el = w3_el(el_id);
	var rv = window.innerHeight/2 - el.clientHeight/2;
	//console.log('w3_center_in_window wh='+ window.innerHeight +' ch='+ el.clientHeight +' rv='+ rv + (id? (' '+ id) : ''));
	return rv;
}

// conditionally select field element
function w3_field_select(el_id, opts)
{
	var el = w3_el(el_id);
	el = (el && isFunction(el.select))? el : null;

   var trace = 0;
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
	   el.setAttribute(name, val)    // repeated sets only update (i.e. don't create duplicate attrs)
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
	var el = w3_iterate_classList(el, function(className, idx) {
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

function w3_show_hide(el, show, display)
{
   var rv;
   //w3_console.log(el, 'w3_show_hide BEGIN show='+ show);
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

function w3_visible(el_id, visible)
{
	var el = w3_el(el_id);
	el.style.visibility = visible? 'visible' : 'hidden';
	return el;
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

// colors = "bgColor/w3-bg-color, fgColor/w3-fg-color"
// color '' to revert to default color
function w3_colors(el_id, colors)
{
   var el = w3_el(el_id);
   if (!el) return null;
   var ar = colors? colors.split(',') : null;
   var bg = null, fg = null;
   if (ar && ar.length > 0) bg = ar[0];
   if (ar && ar.length > 1) fg = ar[1];
   if (bg && bg.startsWith('w3-'))
      w3_add(el, bg);
   else
      el.style.backgroundColor = bg;
   if (fg && fg.startsWith('w3-'))
      w3_add(el, fg);
   else
      el.style.color = fg;
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

function w3_fillText_shadow(canvas, text, x, y, font, fontSize, color, strokeWidth) {
   strokeWidth = strokeWidth || Math.floor(fontSize / 3);   // rule of thumb for good looking shadow
   var ctx = canvas.getContext("2d");
   ctx.font = px(fontSize) +' '+ font;
   ctx.miterLimit = 2;
   ctx.lineJoin = "circle";

   ctx.strokeStyle = "black";
   ctx.lineWidth = strokeWidth;
   ctx.strokeText(text, x, y);

   ctx.fillStyle = color;
   ctx.lineWidth = 1;
   ctx.fillText(text, x, y);
};

function w3_fillText(ctx, x, y, text, color, font, lineWidth, align, baseline)
{
   if (color) ctx.fillStyle = color;
   if (font) ctx.font = font;
   if (lineWidth) ctx.lineWidth = lineWidth;
   if (align) ctx.textAlign = align;
   if (baseline) ctx.textBaseline = baseline;
   var tw = ctx.measureText(text).width;
   ctx.fillText(text, x-tw/2, y);
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

function w3_set_value(path, val)
{
	var el = w3_el(path);
/*
	if (val == '(encrypted)' && w3_contains(el, 'w3-encrypted'))
	   w3_add(el, 'kiwi-pw');
*/
	if (el) el.value = val;
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
   var a0 = (a[0] || '').replace('&slash;', '/');
   var a1 = (a[1] || '').replace('&slash;', '/');
   var a2 = (a[2] || '').replace('&slash;', '/');
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
      s = isString(s)? s.trim() : '';
      if (s != '') a.push(s);
   }
	return a.length? a.join(' ') : '';
}

function w3_sbc(c)
{
   var a = [];
   for (var i = 1; i < arguments.length; i++) {
      var s = arguments[i];
      s = isString(s)? s.trim() : '';
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
      s = isString(s)? s.trim() : '';
      if (s != '') {
         if (s[s.length-1] != ew) s = s + ew;
         a.push(s);
      }
   }
	return a.length? a.join(' ') : '';
}

// psa = prop|style|attr
// => <div [class="[prop] [extra_prop]"] [style="[style] [extra_style]"] [attr] [extra_attr]>
function w3_psa(psa, extra_prop, extra_style, extra_attr)
{
	//console.log('psa_in=['+ psa +']');
	//console.log('extra_prop=['+ extra_prop +']');
	//console.log('extra_style=['+ extra_style +']');
	//console.log('extra_attr=['+ extra_attr +']');

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

	var attr = (a[2] || '').replace('&vbar;', '|');    // e.g. title="foo&vbar;bar" => title="foo|bar"
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

// see: developer.mozilla.org/en-US/docs/Web/API/Element/scrollHeight#determine_if_an_element_has_been_totally_scrolled
function w3_isScrolledDown(id)
{
   var el = w3_el(id);
   return (Math.abs(el.scrollHeight - el.clientHeight - el.scrollTop) < 1)
}

// returns 0 to 1.0 (-1 on error)
function w3_scrolledPosition(id)
{
   var el = w3_el(id);
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
   if (!isArg(el)) {
      setTimeout(function () {
         w3_do_when_rendered(id, func, arg, poll_ms);
      }, poll_ms);
      return;
   }
   
   func(el, arg);
}

// calls func(arg) when cond_func() becomes true
function w3_do_when_cond(cond_func, func, arg, poll_ms)
{
   poll_ms = isNumber(poll_ms)? poll_ms : 500;
   if (!cond_func()) {
      setTimeout(function () {
         w3_do_when_cond(cond_func, func, arg, poll_ms);
      }, poll_ms);
      return;
   }
   
   func(arg);
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

function w3_click_nav(next_id, cb_next, cb_param)
{
   //console.log('w3_click_nav '+ next_id +' cb_next='+ cb_next +' cb_param='+ cb_param);
   //kiwi_trace();
	var next_id_nav = 'id-nav-'+ next_id;		// to differentiate the nav anchor from the nav container
	var cur_id = null;
	var next_el = null;
   var cb_prev = null;

	w3_iterate_children(w3_el(next_id_nav).parentNode, function(el, i) {
	   //console.log('w3_click_nav consider: nodename='+ el.nodeName);
	   if (el.nodeName != 'A') return;
	   //if (el.nodeName != 'DIV') return;

		//console.log('w3_click_nav consider: id='+ el.id +' ==? next_id_nav='+ next_id_nav +' el.className="'+ el.className +'"');
		if (w3_contains(el, 'w3int-cur-sel')) {
			cur_id = el.id.substring(7);		// remove 'id-nav-' added by w3int_anchor()

		   //console.log('w3_click_nav FOUND cur_id='+ cur_id);
			w3_remove(el, 'w3int-cur-sel');
			w3_iterate_classList(el, function(s, i) {
			   if (s.startsWith('id-nav-cb-'))
			      cb_prev = s.substring(10);
			});
		}

		if (el.id == next_id_nav) {
			next_el = el;
		}
	});

   // toggle visibility of current content
	if (cur_id)
		w3_toggle(cur_id, 'w3-show-block');
	if (cur_id && cb_prev) {
		//console.log('w3_click_nav BLUR cb_prev='+ cb_prev +' cur_id='+ cur_id);
		w3_call(cb_prev +'_blur', cur_id);
	}

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
	//console.log('w3_click_nav cb_prev='+ cb_prev +' cur_id='+ cur_id +' cb_next='+ cb_next +' next_id='+ next_id);
}

// id = unique, cb = undefined => cb = id
// id = unique, cb = func
// id = unique, cb = null => don't want focus/blur callbacks
function w3int_anchor(psa, text, id, cb, isSelected)
{
   if (cb === undefined) cb = id;
   var nav_cb = cb? ('id-nav-cb-'+ cb) : '';
   //console.log('w3int_anchor id='+ id +' cb='+ cb +' nav_cb='+ nav_cb);

	// store id prefixed with 'id-nav-' so as not to collide with content container id prefixed with 'id-'
	var attr = 'id="id-nav-'+ id +'" onclick="w3_click_nav('+ sq(id) +', '+ sq(cb) +')"';
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

function w3_nav(psa, text, id, cb, isSelected)
{
	return w3int_anchor(psa, text, id, cb, isSelected);
}

function w3_navdef(psa, text, id, cb)
{
	// must wait until instantiated before manipulating 
	setTimeout(function() {
		//console.log('w3_navdef instantiate focus');
		w3_toggle(id, 'w3-show-block');
	}, w3_highlight_time);
	
	return w3int_anchor(psa, text, id, cb, true);
}


////////////////////////////////
// labels
////////////////////////////////

function w3_label(psa, text, path, extension)
{
   if (arguments.length >= 4) console.log('### w3_label ext='+ extension);
   psa = psa || '';
   text = text || '';
   extension = extension || '';
   if (psa == '' && text == '' && extension == '') return '';
   
   // most likely already an embedded w3_label()
   if (text.startsWith('<label ')) return text;
   
   var id = w3_add_id(path, '-label');    // so w3_set_label() can find label
	//var inline = psa.includes('w3-label-inline');
	var p = w3_psa(psa, id);
	var s = '<label '+ p +'>'+ text + extension +'</label>';
	//var s = '<label '+ p +'>'+ text + extension + (inline? '':'<br>') +'</label>';
	//console.log('LABEL: psa='+ psa +' text=<'+ text +'> s=<'+ s +'>');
	return s;
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
   console.log('w3int_link_click cb='+ cb +' cb_param='+ cb_param);
   console.log(ev);
   var el = ev.currentTarget;
   console.log(el);
   w3_check_restart_reboot(el);

   // cb is a string because can't pass an object to onclick
   if (cb) {
      w3_call(cb, el, cb_param, /* first */ false);   // links don't really have first callback
   }

   w3int_post_action();
}

function w3_link(psa, url, inner, title, cb, cb_param)
{
   var qual_url = url;
   var target;
   if (url.startsWith('javascript:')) {
      target = '';
   } else {
      if (url != '' && !url.startsWith('http://') && !url.startsWith('https://'))
         qual_url = 'http://'+ url;
      target = ' target="_blank"';
   }
   inner = inner || '';
   title = title || '';
   if (title != '') title = ' title='+ dq(title);

   // by default use pointer cursor if there is a callback
	var pointer = (cb && cb != '')? 'w3-pointer':'';
	cb_param = cb_param || 0;
	var onclick = cb? (' onclick="w3int_link_click(event, '+ sq(cb) +', '+ sq(cb_param) +')"') : '';

	var p = w3_psa(psa, pointer, '', 'href='+ dq(qual_url) + target + title + onclick);
	var s = '<a '+ p +'>'+ inner +'</a>';
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
   }
}


////////////////////////////////
// buttons: radio
////////////////////////////////

var w3_SELECTED = true;
var w3_NOT_SELECTED = false;

function w3_radio_unhighlight(path)
{
	w3_iterate_classname('id-'+ path, function(el) { w3_unhighlight(el); });
}

function w3int_radio_click(ev, path, cb, cb_param)
{
	w3_radio_unhighlight(path);
	w3_highlight(ev.currentTarget);

	var idx = -1;
	w3_iterate_classname('id-'+ path, function(el, i) {
		if (w3_isHighlighted(el))
			idx = i;
		//console.log('w3int_radio_click CONSIDER path='+ path +' el='+ el +' idx='+ idx);
	});

	w3_check_restart_reboot(ev.currentTarget);

	// cb is a string because can't pass an object to onclick
	if (cb) {
		w3_call(cb, path, idx, /* first */ false, cb_param);   // radio buttons don't really have first callback
	}

   w3int_post_action();
}

// deprecated (still used by antenna switch ext)
function w3_radio_btn(text, path, isSelected, save_cb, prop)
{
	var prop = (arguments.length > 4)? arguments[4] : null;
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
	var p = w3_psa(psa, 'id-'+ path + (isSelected? (' '+ w3_highlight_color) : '') +' w3-btn w3-ext-btn', '', onclick);
	var s = '<button '+ p +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

// used when current value should come from config param
function w3_radio_button_get_param(psa, text, path, selected_if_val, init_val, cb, cb_param)
{
	//console.log('w3_radio_button_get_param: '+ path);
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	
	// set default selection of button based on current value
	var isSelected = (cur_val == selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
	return w3_radio_button(psa, text, path, isSelected, cb, cb_param);
}


////////////////////////////////
// buttons: two button switch
////////////////////////////////

var w3_SWITCH_YES_IDX = 0, w3_SWITCH_NO_IDX = 1;

function w3_switch(psa, text_0, text_1, path, text_0_selected, cb, cb_param)
{
   //console.log('w3_switch psa='+ psa);
	var s =
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-0'), text_0, path, text_0_selected? 1:0, cb, cb_param) +
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-1'), text_1, path, text_0_selected? 0:1, cb, cb_param);
	return s;
}

// used when current value should come from config param
function w3_switch_get_param(psa, text_0, text_1, path, text_0_selected_if_val, init_val, cb, cb_param)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);

	// set default selection of button based on current value
	//if (w3_contains(psa, 'id-net-ssl'))
	//   console.log('w3_switch_get_param cur_val='+ cur_val +' text_0_selected_if_val='+ text_0_selected_if_val);
	var text_0_selected = (cur_val == text_0_selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
	var s =
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-0'), text_0, path, text_0_selected? 1:0, cb, cb_param) +
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-1'), text_1, path, text_0_selected? 0:1, cb, cb_param);
	return s;
}

function w3_switch_set_value(path, switch_idx)
{
   var sw = 'w3int-switch-'+ switch_idx;
   //console.log('w3_switch_set_value: switch='+ sw +' path='+ path);
	w3_iterate_classname('id-'+ path, function(el, i) {
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

function w3int_button_click(ev, path, cb, cb_param)
{
   if (!w3_contains(path, 'w3-disabled')) {
      //console.log('w3int_button_click path='+ path +' cb='+ cb +' cb_param='+ cb_param);
      w3_check_restart_reboot(ev.currentTarget);
   
      // cb is a string because can't pass an object to onclick
      if (cb) {
         w3_call(cb, path, cb_param, /* first */ false);    // buttons don't really have first callback
      }
   }

   w3int_post_action();
}

var w3int_btn_grp_uniq = 0;

// deprecated (still used by older versions of antenna switch ext)
function w3_btn(text, cb)
{
   console.log('### DEPRECATED: w3_btn');
   return w3_button('', text, cb);
}

function w3_cb_param_encode(cbp)
{
   if (!isArg(cbp)) {
      return 0;
   }
   
   if (isObject(cbp)) {
      return encodeURIComponent(JSON.stringify(cbp));
   }
   
   return cbp;
}

function w3_cb_param_decode(cbp)
{
   if (!isArg(cbp)) {
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
	//console.log('w3_button cb_param='+ cb_param);
	cb_param = w3_cb_param_encode(cb_param);
	//console.log('w3_button cb_param='+ cb_param);

	var onclick = cb? ('onclick="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"') : '';
	if (cb && psa.includes('w3-momentary')) {
	   onclick += ' onmousedown="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', 0)"';
	   onclick += ' ontouchstart="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', 0)"';
	}
	
	// w3-round-large listed first so its '!important' can be overriden by subsequent '!important's
	var default_style = (psa.includes('w3-round') || psa.includes('w3-circle'))? '' : ' w3-round-large';
   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left);
	var psa_inner = w3_psa(psa3.right, path +' w3-btn w3-ext-btn'+ default_style, '', onclick);
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
	var path = 'id-btn-grp-'+ w3int_btn_grp_uniq.toString();
	w3int_btn_grp_uniq++;
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

function w3_icon(psa, fa_icon, size, color, cb, cb_param)
{
   // by default use pointer cursor if there is a callback
	var pointer = (cb && cb != '')? ' w3-pointer':'';
	var path = 'id-btn-grp-'+ w3int_btn_grp_uniq.toString();
	w3int_btn_grp_uniq++;
	cb_param = cb_param || 0;

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
	var onclick = cb? ('onclick="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"') : '';
	if (cb && psa.includes('w3-momentary')) {
	   onclick += ' onmousedown="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', 0)"';
	   onclick += ' ontouchstart="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', 0)"';
	}

	var p = w3_psa(psa, path + pointer +' fa '+ fa_icon, font_size + color, onclick);
	var s = '<i '+ p +'></i>';
	//console.log(s);
	return s;
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
	var path = 'id-btn-grp-'+ w3int_btn_grp_uniq.toString();
	w3int_btn_grp_uniq++;
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
   var ctl = ev.ctrlKey;
	var el = w3_el(path);
   if (!el) return;
   //w3_remove(el, 'kiwi-pw');
   var trace = w3_contains(el, 'w3-trace');
   if (trace) console.log('w3int_input_keydown k='+ k + (ctl? ' CTL ':'') +' val=<'+ el.value +'> cb='+ cb);
   var cb_a = cb.split('|');

   if (ctl && 'CD\\'.includes(k) && cb_a[1]) {
      //console.log('w3int_input_keydown ^'+ k +' cb_a[1]='+ cb_a[1]);
      w3_call(cb_a[1], k);
   }

   var input_any_change = w3_contains(el, 'w3-input-any-change');
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

// 'from' arg (that is appended to w3_call() cb_a[] arg) is:
//    'ev' if called from normal onchange event
//    'kd' or 'ku' if 'w3-input-any-change' is specified and the "any change" criteria is met
function w3_input_change(path, cb, from)
{
	var el = w3_el(path);
	if (el) {
      var trace = w3_contains(el, 'w3-trace');
      if (trace) console.log('w3_input_change path='+ path);
      w3_check_restart_reboot(el);
      w3_schedule_highlight(el);
      
      // cb is a string because can't pass an object to onclick
      if (cb) {
         var cb_a = cb.split('|');
         if (isArg(from)) cb_a.push(from);
         //el.select();
         w3_call(cb_a[0], path, el.value, /* first */ false, cb_a);
      }

/*
      if (el.value != '' && w3_contains(el, 'w3-encrypted')) {
         w3_add(el, 'kiwi-pw');
         el.value = '(encrypted)';
      }
*/
   }
	
   if (w3_contains(el, 'w3-retain-input-focus') || w3_contains(el, 'w3-ext-retain-input-focus'))
	   w3_field_select(path, {mobile:1});     // select the field
   else
      w3int_post_action();
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
	var id = w3_add_id(path);
	cb = cb || '';
	var phold = placeholder? ('placeholder="'+ placeholder +'"') : '';
	var custom = psa.includes('w3-custom-events');
	var onchange = ' onchange="w3_input_change('+ sq(path) +', '+ sq(cb) +', '+ sq('ev') +')"';
	var onkeydown = ' onkeydown="w3int_input_keydown(event, '+ sq(path) +', '+ sq(cb) +')"';
	var onkeyup = ' onkeyup="w3int_input_keyup(event, '+ sq(path) +', '+ sq(cb) +')"';
	var events = (path && !custom)? (onchange + onkeydown + onkeyup) : '';
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

	var s =
	   '<div '+ psa_outer +'>' +
         w3_label(psa_label, label, path) +
		   // NB: include id in an id= for benefit of keyboard shortcut field detection
         '<input id='+ dq(id) +' '+ psa_inner + val + events +'>' +
      '</div>';
	//if (path == 'freq-input') console.log(s);
	//w3int_input_set_id(id);
	return s;
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
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	cur_val = kiwi_decodeURIComponent('w3_input_get:'+ path, cur_val);
	//console.log('w3_input_get: path='+ path +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input(psa, label, path, cur_val, cb, placeholder);
}

// DEPRECATED (still used by ant switch ext)
function w3_input_get_param(label, path, cb, init_val, placeholder)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	cur_val = kiwi_decodeURIComponent('w3_input_get_param:'+ path, cur_val);
	//console.log('w3_input_get_param: path='+ path +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input('', label, path, cur_val, cb, placeholder);
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
	var onchange = psa.includes('w3-input-any-change')? '' : (' onchange="w3_input_change('+ sq(path) +', '+ sq(cb) +')"');
	var onkeydown = ' onkeydown="w3int_input_keydown(event, '+ sq(path) +', '+ sq(cb) +')"';
	var onkeyup = ' onkeyup="w3int_input_keyup(event, '+ sq(path) +', '+ sq(cb) +')"';
	var val = val || '';
	var bold = !psa.includes('w3-label-not-bold');
   var psa3 = w3_psa3(psa);
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, 'w3-input w3-border w3-hover-shadow '+ id + spacing, '', 'rows='+ dq(rows) +' cols='+ dq(cols));

	var s =
	   w3_div(psa3.left,
         w3_label(psa_label, label, path) +
		   '<textarea '+ psa_inner + onchange + onkeydown + onkeyup +'>'+ val +'</textarea>'
		);
	//if (psa.includes('w3-dump')) console.log('$');
	//if (psa.includes('w3-dump')) console.log(psa3);
	//if (psa.includes('w3-dump')) console.log(psa_inner);
	//if (psa.includes('w3-dump')) console.log('label='+ label);
	//if (psa.includes('w3-dump')) console.log('val='+ val);
	//if (psa.includes('w3-dump')) console.log('s='+ s);
	return s;
}

// used when current value should come from config param
function w3_textarea_get_param(psa, label, path, rows, cols, cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
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

   if (w3_contains(el, 'w3-retain-input-focus') || w3_contains(el, 'w3-ext-retain-input-focus'))
	   w3_field_select(path, {mobile:1});     // select the field
   else
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

   var ls = w3_label(psa_label, label, path);
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
function w3_checkbox_get_param(psa, label, path, cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	//console.log('w3_checkbox_get_param: path='+ path +' cur_val="'+ cur_val +'"');
	return w3_checkbox(psa, label, path, cur_val, cb);
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
// select
////////////////////////////////

var W3_SELECT_SHOW_TITLE = -1;

function w3int_select_change(ev, path, cb, cb_param)
{
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
	var spacing = (label != '' && !inline)? ' w3-margin-T-8' : '';
	if (inline) spacing += ' w3-margin-left';
	if (cb == undefined) cb = '';
	var onchange = 'onchange="w3int_select_change(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, id +' w3-select-menu'+ spacing, '', onchange);

	var s =
	   '<div '+ psa_outer +'>' +
         w3_label(psa_label, label, path) +
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

	//console.log(s);
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
      w3_obj_enum(opts, function(key, i, o) {
         var value, text, disabled = false;
         if (isObject(o)) {
            value = isDefined(o.value)? o.value : i;
            var hasName = isDefined(obj.name);
            if (hasName && !show_empty && obj.name == '') return;
            text = hasName? o.name : key;
            if (isDefined(o.disabled) && o.disabled) disabled = true;
         } else {
            value = i;
            text = o;
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
         s += '<option value='+ dq(idx++) +' '+ (el[1]? '':'disabled') +'>'+ el[0] +'</option> ';
   });
   
   return w3int_select(psa, label, title, path, sel, s, cb, cb_param);
}

function w3_select_set_disabled(path, value, disabled)
{
   w3_iterate_children('id-'+ path, function(el, i) {
      //console.log('w3_select_set_disabled['+i+']');
      //console.log(el);
      if (el.value == value) el.disabled = disabled? true:false;
   });
}

// used when current value should come from config param
function w3_select_get_param(psa, label, title, path, opts, cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? 0 : init_val);
	return w3_select(psa, label, title, path, cur_val, opts, cb);
}

function w3_select_enum(path, func)
{
   var path = w3_el(path);
   if (!path) return;
	w3_iterate_children(path, func);
}

function w3_select_value(path, idx, opt)
{
   if (w3_opt(opt, 'all')) {
      w3_els(path, function(el) {
         el.value = idx;
      });
   } else {
      var el = w3_el(path);
      if (!el) return;
      el.value = idx;
   }
}

function w3_select_set_if_includes(path, s)
{
   var found = false;
   var re = RegExp(s);
   w3_select_enum(path, function(option) {
      //console.log('w3_select_set_if_includes s=['+ s +'] consider=['+ option.innerHTML +']');
      if (re.test(option.innerHTML)) {
         if (!found) {
            w3_select_value(path, option.value);
            //console.log('w3_select_set_if_includes TRUE');
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
	
	if (complete)
	   w3int_post_action();
}

// deprecated
function w3_slider_old(label, path, val, min, max, step, save_cb)
{
	if (val == null)
		val = '';
	else
		val = w3_strip_quotes(val);
	var oc = 'oninput="w3int_slider_change(event, 0, '+ sq(path) +', '+ sq(save_cb) +')" ';
	// change event fires when the slider is done moving
	var os = 'onchange="w3int_slider_change(event, 1, '+ sq(path) +', '+ sq(save_cb) +')" ';
	var label_s = w3_label('w3-bold', label, path);
	var s =
		label_s +'<br>'+
		'<input id="id-'+ path +'" class="" value=\''+ val +'\' ' +
		'type="range" min="'+ min +'" max="'+ max +'" step="'+ step +'" '+ oc + os +'>';

	// run the callback after instantiation with the initial control value
	if (save_cb)
		setTimeout(function() {
			//console.log('w3_slider: initial callback: '+ save_cb +'('+ sq(path) +', '+ val +')');
			w3_call(save_cb, path, val, /* complete */ true, /* first */ true);
		}, 500);

	//if (path == 'iq.pll_bw') console.log(s);
	return s;
}

function w3_slider(psa, label, path, val, min, max, step, cb, cb_param)
{
	var id = w3_add_id(path);
	var inline = psa.includes('w3-label-inline');
	var bold = !psa.includes('w3-label-not-bold');
	var spacing = (label != '' && inline)? ' w3-margin-L-8' : '';
	if (inline) spacing += ' w3-margin-left';

	value = (val == null)? '' : w3_strip_quotes(val);
	value = 'value='+ dq(value);
	cb_param = cb_param || 0;
	var oc = ' oninput="w3int_slider_change(event, 0, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';
	// change event fires when the slider is done moving
	var os = ' onchange="w3int_slider_change(event, 1, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"';

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, id + spacing, '', value +
      ' type="range" min='+ dq(min) +' max='+ dq(max) +' step='+ dq(step) + oc + os);

	var s =
	   '<div '+ psa_outer +'>' +
         w3_label(psa_label, label, path) +
         // need <br> because both <label> and <input-range> are inline elements
         ((!inline && label != '')? '<br>':'') +
         '<input '+ psa_inner +'>' +
      '</div>';

	// run the callback after instantiation with the initial control value
	if (cb)
		setTimeout(function() {
			//console.log('w3_slider: initial callback: '+ cb +'('+ sq(path) +', '+ val +')');
			w3_call(cb, path, val, /* complete */ true, /* first */ true, cb_param);
		}, 500);

	//if (path == 'iq.pll_bw') console.log(s);
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


////////////////////////////////
// menu
////////////////////////////////

function w3_menu(psa, cb)
{
	var id = psa.replace(/\s+/g, ' ').split(' |')[0];
   cb = cb || '';
   //console.log('w3_menu id='+ id +' psa='+ psa);

   var onclick = 'onclick="w3int_menu_onclick(event, '+ sq(id) +', '+ sq(cb) +')"' +
      ' oncontextmenu="w3int_menu_onclick(event, '+ sq(id) +', '+ sq(cb) +')"';
	var p = w3_psa(psa, 'w3-menu w3-round-large', '', onclick);
   var s = '<div '+ p +'></div>';
   //console.log('w3_menu s='+ s);
   w3_el('id-w3-main-container').innerHTML += s;
}

// menu items can be in argument list or passed as an array
function w3_menu_items(id, arr)
{
   //console.log('w3_menu_items id='+ id);

   var s = '';
   var idx = 0, prop, attr;
   var items;

   if (isArray(arr)) {
      items = arr;
   } else {
      // isObject(arr)
      items = [];
      for (var i=1; i < arguments.length; i++)
         items.push(arguments[i]);
   }

   for (var i=0; i < items.length; i++) {
      if (items[i] == '<hr>') {
         prop = 'w3-menu-item-hr';
         attr = 'id='+ dq(idx);
         idx++;
      } else
      if (items[i].charAt(0) == '!') {    // first char == '!' hack to disable menu item
         prop = 'w3-menu-item-disabled';
         attr = 'id='+ dq(idx);
         items[i] = items[i].substr(1);
         idx++;
      } else {
         prop = 'w3-menu-item';
         attr = 'id='+ dq(idx);
         idx++;
      }
      s += w3_div(prop +'||'+ attr, items[i]);
   }
   
   //console.log(s);
   w3_el(id).innerHTML = s;
}

function w3_menu_popup(id, x, y)
{
   //console.log('w3_menu_popup id='+ id +' x='+ x +' y='+ y);
   var el = w3_el(id);
	if (x == -1) x = window.innerWidth/2;
	if (y == -1) y = window.innerHeight/2;

	var slop = 16;
	el.w3_realigned = false;
	
	if (x + el.clientWidth + slop > window.innerWidth) {
	   x = window.innerWidth - el.clientWidth - slop;
	   el.w3_realigned = true;
	}
   el.style.left = px(x);

	if (y + el.clientHeight + slop > window.innerHeight) {
	   y = window.innerHeight - el.clientHeight - slop;
	   el.w3_realigned = true;
	}
   el.style.top = px(y);
   //console.log('w3_menu_popup NEW id='+ id +' x='+ x +' y='+ y);

   el.style.visibility = 'visible';
   el.w3_menu_x = x;

	// close menu if escape key while menu is displayed
	w3int_menu_close_cur_id = id;
	window.addEventListener("keyup", w3int_menu_close, false);
	window.addEventListener("click", w3int_menu_close, false);
}

function w3int_menu_onclick(ev, id, cb)
{
   //console.log('w3int_menu_onclick id='+ id +' cb='+ cb);
   //if (ev != null) event_dump(ev, "MENU");
   var el = w3_el(id);
   //console.log('w3int_menu_onclick realigned='+ el.w3_realigned);
   if (el.w3_realigned) {
      // ignore false click from menu re-alignment
      el.w3_realigned = false;
      return cancelEvent(ev);
   }

   el.style.visibility = 'hidden';
   window.removeEventListener("keyup", w3int_menu_close, false);
   window.removeEventListener("click", w3int_menu_close, false);

   if (ev != null && cb != null) {
      var _id = ev.target.id;
      var idx = +_id;
      if (_id == '' || isNaN(idx)) _id = ev.target.parentNode.id;
      idx = +_id;
      if (_id == '' || isNaN(idx)) idx = -1;
      //console.log('w3int_menu_onclick CALL idx='+ idx);
      w3_call(cb, idx, el.w3_menu_x);
   }

   // allow right-button to select menu items
	if (ev != null) {
      //console.log('w3int_menu_onclick: cancelEvent()');
	   return cancelEvent(ev);
	}
}

var w3int_menu_close_cur_id;

// close menu if escape key pressed or a click outside of the menu
function w3int_menu_close(evt)
{
   //event_dump(evt, 'MENU-CLOSE');
   if ((evt.type == 'keyup' && evt.key == 'Escape') ||
      (evt.type == 'click' && evt.button != 2 )) {
      //console.log('w3int_menu_close '+ evt.type +' button='+ evt.button);
      w3int_menu_onclick(null, w3int_menu_close_cur_id);
   }
}


////////////////////////////////
// standard callbacks
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

function w3_num_set_cfg_cb(path, val, first)
{
	var v = parseFloat(val);
	if (isNaN(v)) v = 0;
	
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function w3_int_set_cfg_cb(path, val, first)
{
	var v = parseInt(val);
	if (isNaN(v)) v = 0;
	w3_set_value(path, v);     // remove any fractional or non-number portion from field
	
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
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
	   ext_set_cfg_param(path, val, true);
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
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function w3_string_set_cfg_cb(path, val, first)
{
	//console.log('w3_string_set_cfg_cb: path='+ path +' '+ typeof(val) +' "'+ val +'" first='+ first);
	
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, encodeURIComponent(val.toString()), save);
}

// unlike w3_string_set_cfg_cb(), saving json doesn't need encodeURIComponent()
// because JSON.stringify() escapes everything properly
function w3_json_set_cfg_cb(path, val, first)
{
	//console.log('w3_json_set_cfg_cb: path='+ path +' '+ typeof(val) +' "'+ val +'" first='+ first);
	
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, val, save);
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
      var val = el.value;
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

function w3_inline(psa, attr)
{
	var narg = arguments.length;
   
   if (psa == '' && attr == '' && narg > 2) {
      console.log('### w3_inline OLD API DEPRECATED');
      var args = Array.prototype.splice.call(arguments, 0);
      args.splice(0, 1);
      return w3_inline.apply(null, args);
   } else {
      var psa3 = w3_psa3(psa);
      var psa_outer = w3_psa(psa3.middle, 'w3-show-inline-new');
      var psa_inner = w3_psa(psa3.right);
      var s = '<div w3d-inlo '+ psa_outer +'>';
      for (var i = 1; i < narg; i++) {
         var psa;
         var a = arguments[i];
         //console.log('w3_inline i='+ i +' a='+ a);
         
         // merge a pseudo psa specifier into the next argument
         // i.e. 'w3-*', w3_*('w3-psa') => w3_*('w3-* w3-psa')
         var psa_merge = false;
         if (a.startsWith('w3-') || a.startsWith('id-')) {
            psa = w3_psa(psa3.right, a);
            i++;
            a = arguments[i];
            psa_merge = true;
         } else {
            psa = psa_inner;
         }
         
         // If the psa is null, and the arg is a div, don't wrap it with our usual enclosing div.
         // This solves the "w3_inline() + w3-hide" problem where our extra div
         // added by w3_inline isn't the one with the w3-hide, and causes unwanted spacing when
         // using w3_inline('w3-halign-space-between/').
         if (psa3.right == '' && !psa_merge && a.startsWith('<div '))
            s += a;
         else
            s += '<div w3d-inli-'+ (i-1) +' '+ psa +'>'+ a + '</div>';
      }
      s += '</div>';
      //console.log(s);
      return s;
   }
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
   var s;

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
      var args = Array.prototype.splice.call(arguments, 0);
      args.splice(1, 1);
      s = w3_div.apply(null, args);
      //console.log(s);
      return s;
   } else
   if (psa == '' && attr && attr.startsWith('w3-') && narg > 2) {
      console.log('### w3_divs OLD API 3 DEPRECATED');
      //console.log('prop=<'+ psa +'> attr=<'+ attr +'>');
      //kiwi_trace();
      var args = Array.prototype.splice.call(arguments, 0);
      args.splice(0, 1);
      s = w3_divs.apply(null, args);
      //console.log(s);
      return s;
   } else
   if (psa != '' && attr && attr.startsWith('w3-') && narg > 2) {
      console.log('### w3_divs OLD API 4 DEPRECATED');
      //console.log('prop=<'+ psa +'> attr=<'+ attr +'>');
      //kiwi_trace();
      var args = Array.prototype.splice.call(arguments, 2);
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
   var pad = (psa.includes('w3-padding') || psa.includes('w3-nopad'))? '' : 'padding:0 4px 0 0; ';
	var s = w3_div(w3_psa_mix(psa, 'w3-text', pad +'background-color:inherit'), text? text:' ');
	//console.log(s);
	return s;
}

function w3_code(prop_outer, prop_inner)
{
	var narg = arguments.length;
	var s = '<pre class="'+ prop_outer +'"><code>';
		for (var i=2; i < narg; i++) {
			s += '<div class="'+ prop_inner +'">'+ arguments[i] + '</div>';
		}
	s += '</code></pre>';
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
