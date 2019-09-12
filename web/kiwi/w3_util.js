// Copyright (c) 2016-2018 John Seamons, ZL/KF6VO

/*

	///////////////////////////////////////
	// API summary
	///////////////////////////////////////
	
	                           integrated: L=label, T=text
	                           3=psa3()
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
	
	input                      L3
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
	   visible_block()
	   w3_divs()
	   w3_inline()                   only in OLDER versions of the ant_switch ext
	   w3_btn()
	   w3_radio_btn(yes/no)    DEP   w3_radio_button
	   w3_input_get_param()    DEP
	   w3_string_set_cfg_cb()
	   w3_highlight()
	   w3_unhighlight()
	   w3_radio_unhighlight()
	   var w3_highlight_time

*/


////////////////////////////////
// deprecated
////////////////////////////////

function visible_block() {}      // FIXME: used by antenna switch ext


////////////////////////////////
// util
////////////////////////////////

function w3_console_obj(obj, prefix)
{
   var s = prefix? (prefix + ' ') : '';
   if (typeof obj == 'object')
      s += JSON.stringify(obj, null, 1);
   else
   if (typeof obj == 'string')
      s += '"'+ obj.toString() +'"';
   else
      s += obj.toString();
   console.log(s);
   if (typeof obj == 'object') console.log(obj);
}

function w3_strip_quotes(s)
{
	if ((typeof s == "string") && (s.indexOf('\'') != -1 || s.indexOf('\"') != -1))
		return s.replace(/'/g, '').replace(/"/g, '') + ' [quotes stripped]';
	return s;
}

// a single-argument call that silently continues if func not found
function w3_call(func, arg0, arg1, arg2, arg3)
{
   var rv = undefined;

   if (func == null || func == undefined) return rv;
   
	try {
	   if (typeof(func) === 'string') {
         var f = getVarFromString(func);
         //console.log('w3_call: '+ func +'() = '+ (typeof f));
         if (typeof(f) === 'function') {
            //var args = Array.prototype.slice.call(arguments);
            rv = f(arg0, arg1, arg2, arg3);
         } else {
            //console.log('w3_call: getVarFromString(func) not a function: '+ func +' ('+ typeof(f) +')');
         }
      } else
	   if (typeof(func) === 'function') {
         rv = func(arg0, arg1, arg2, arg3);
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
   var to_v = typeof v;
   if (to_v === 'number' || to_v === 'boolean' || to_v === 'string') {
      //console.log('w3_first_value prim');
      rv = v;
   } else
   if (Array.isArray(v)) {
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

function w3_obj_seq_el(o, idx)
{
   var keys = Object.keys(o);
   return o[keys[idx]];
}

function w3_obj_enum_data(obj, data, func)
{
   var keys = Object.keys(obj);
   for (var i=0, len = keys.length; i < len; i++) {
      var key = keys[i];
      if (data == null || obj[key] == data) func(i, key);
   }
}

function w3_obj_enum_key(obj, key, func)
{
   var keys = Object.keys(obj);
   for (var i=0, len = keys.length; i < len; i++) {
      var k = keys[i];
      if (key == null || k == key) func(i, obj[key]);
   }
}

function w3_ext_param_array_match_str(arr, s, func)
{
   var found = false;
   arr.forEach(function(a_el, i) {
      var el = a_el.toString().toLowerCase();
      //console.log('w3_ext_param_array_match_str CONSIDER '+ i +' '+ el +' '+ s);
      if (!found && el.startsWith(s)) {
         //console.log('w3_ext_param_array_match_str MATCH '+ i);
         func(i, a_el.toString());
         found = true;
      }
   });
   return found;
}

function w3_ext_param_array_match_num(arr, n, func)
{
   var found = false;
   arr.forEach(function(a_el, i) {
      var a_num = parseFloat(a_el);
      //console.log('w3_ext_param_array_match_num CONSIDER '+ i +' '+ a_num +' '+ n);
      if (!found && a_num == n) {
         //console.log('w3_ext_param_array_match_num MATCH '+ i);
         func(i, n);
         found = true;
      }
   });
   return found;
}

function w3_ext_param(s, param)
{
   p = param.toLowerCase().split(':');
   if (s.startsWith(p[0])) {
      rv = { match: true };
      if (p.length > 1) {
         rv.num = parseFloat(p[1]);
         rv.string = p[1];
      }
      return rv;
   }
   return { match: false }
}

function w3_clamp(v, min, max, clamp_val)
{
   if (clamp_val == undefined)
      v = Math.max(min, Math.min(max, v));
   else
      if (v < min || v > max) v = clamp_val;
   return v;
}


////////////////////////////////
// HTML
////////////////////////////////

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

// allow an element-obj or element-id to be used
// try id without, then with, leading 'id-'; then including cfg prefix as a last resort
function w3_el(el_id)
{
	if (typeof el_id == "string") {
	   if (el_id == '') return null;
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

// w3_el(), but silently failing if element doesn't exist
function w3_el_softfail(id)
{
	var el = w3_el(id);
	var debug;
	try {
		debug = el.innerHTML;
	} catch(ex) {
		console.log('w3_el_softfail("'+ id +'") SOFTFAIL');
		/*
		if (dbgUs && dbgUsFirst) {
			kiwi_trace();
			dbgUsFirst = false;
		}
		*/
	}
	if (el == null) {
	   el = document.createElement('div');		// allow failures to proceed, e.g. assignments to innerHTML
	   el.id = id;
	   el.classList.add('NB-SOFTFAIL');
	   el.style.display = 'none';
	   document.body.appendChild(el);
	}
	return el;
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
	var el = w3_el_softfail(id);
	var s = '';
	var narg = arguments.length;
   for (var i=1; i < narg; i++) {
      s += arguments[i];
   }
	el.innerHTML = s;
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
	for (var i=0; i < els.length; i++) {
		func(els[i], i);
	};
}

function w3_iterate_classList(el_id, func)
{
	var el = w3_el(el_id);
	if (el) for (var i = 0; i < el.classList.length; i++) {
		func(el.classList.item(i), i);
	}
	return el;
}

function w3_appendElement(el_parent, el_type, html)
{
   var el_child = document.createElement(el_type);
   w3_innerHTML(el_child, html);
	w3_el(el_parent).appendChild(el_child);
	return el_child;
}

function w3_iterate_children(el_id, func)
{
	var el = w3_el(el_id);
	
	for (var i=0; i < el.children.length; i++) {
		var child_el = el.children[i];
		func(child_el, i);
	}
}

function w3_iterateDeep_children(el_id, func)
{
	var el = w3_el(el_id);
	
	for (var i=0; i < el.children.length; i++) {
		var child_el = el.children[i];
		func(child_el);
		if (child_el.hasChildNodes)
			w3_iterateDeep_children(child_el, func);
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
	if (debug) console.log('BBOX x1='+ bbox.x1 +' x2='+ bbox.x2 +' y1='+ bbox.y1 +' y2='+ bbox.y2 +' w='+ bbox.w +' h='+ bbox.h);
	return bbox;
}

function w3_center_in_window(el_id)
{
	var el = w3_el(el_id);
	var rv = window.innerHeight/2 - el.clientHeight/2;
	//console.log('w3_center_in_window wh='+ window.innerHeight +' ch='+ el.clientHeight +' rv='+ rv);
	return rv;
}

// conditionally select field element
function w3_field_select(el_id, opts)
{
	var el = w3_el(el_id);
	el = (el && typeof el.select == 'function')? el : null;

   var trace = 0;
   if (trace) {
      var id = (typeof el_id == 'object')? el_id.id : el_id;
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
	var el = w3_el(el_id);
	//console.log('w3_add <'+ props +'>');
	if (!el || !props) return null;
	props = props.replace(/\s+/g, ' ').split(' ');
	props.forEach(function(p) {
	   el.classList.add(p);
	});
	return el;
}

function w3_remove(el_id, props)
{
	var el = w3_el(el_id);
	//console.log('w3_remove <'+ props +'>');
	if (!el || !props) return null;
	props = props.replace(/\s+/g, ' ').split(' ');
	props.forEach(function(p) {
	   el.classList.remove(p);
	});
	return el;
}

function w3_remove_wildcard(el_id, prefix)
{
	var el = w3_el(el_id);
	//console.log('w3_remove_wildcard <'+ prefix +'>');
	if (!el) return null;
	el.classList.forEach(function(cl) {
	   if (cl.startsWith(prefix)) el.classList.remove(cl);
	});
	return el;
}

function w3_set_props(el_id, props, val)
{
	if (val)
	   w3_add(el_id, props);
	else
	   w3_remove(el_id, props);
	return el_id;
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
	w3_remove(el, 'w3-hide');
	w3_add(el, display? display : 'w3-show-inline-block');
	return el;
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

function w3_hide(el)
{
   //w3_console_obj(el, 'w3_hide BEGIN');
	var el = w3_iterate_classList(el, function(className, idx) {
      //console.log('w3_hide CONSIDER '+ className);
	   if (className.startsWith('w3-show-')) {
         //console.log('w3_hide REMOVE '+ className);
	      w3_remove(el, className);
	   }
	});
	w3_add(el, 'w3-hide');
   //w3_console_obj(el, 'w3_hide END');
	return el;
}

function w3_show_hide(el, show, display)
{
   var rv;
   //w3_console_obj(el, 'w3_show_hide BEGIN show='+ show);
   if (show) {
      rv = w3_show(el, display? display : 'w3-show-block');
   } else {
      rv = w3_hide(el);
   }
   //w3_console_obj(el, 'w3_show_hide END');
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

function w3_highlight(el_id)
{
	var el = w3_el(el_id);
	//console.log('w3_highlight '+ el.id);
	w3_add(el, w3_highlight_color);
}

function w3_unhighlight(el_id)
{
	var el = w3_el(el_id);
	//console.log('w3_unhighlight '+ el.id);
	w3_remove(el, w3_highlight_color);
}

function w3_isHighlighted(el_id)
{
	var el = w3_el(el_id);
	return w3_contains(el, w3_highlight_color);
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
function w3_color(el_id, color, bkgColor)
{
	var el = w3_el(el_id);
	if (!el) return null;
	var prev = el.style.color;
	if (color) el.style.color = color;
	if (bkgColor) el.style.backgroundColor = bkgColor;
	return prev;
}

// returns previous color
function w3_background_color(el_id, color)
{
	var el = w3_el(el_id);
	var prev = el.style.backgroundColor;
	if (color != undefined && color != null) el.style.backgroundColor = color;
	return prev;
}

function w3_check_restart_reboot(el_id)
{
	var el = w3_el(el_id);
   if (!el) return;
	
	do {
		if (w3_contains(el, 'w3-restart')) {
			w3_restart_cb();
			break;
		}
		if (w3_contains(el, 'w3-reboot')) {
			w3_reboot_cb();
			break;
		}
		el = el.parentNode;
	} while (el);
}

function w3_set_value(path, val)
{
	var el = w3_el(path);
	if (el) el.value = val;
}

function w3_set_decoded_value(path, val)
{
	//console.log('w3_set_decoded_value: path='+ path +' val='+ val);
	var el = w3_el(path);
	if (el) el.value = decodeURIComponent(val);
}

function w3_get_value(path)
{
	var el = w3_el(path);
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
   //console.log('w3_psa3 in='+ psa3);
   psa3 = psa3 || '';
   arr = psa3.split('/');
   //console.log(arr);
   if (arr.length == 1)
      return { left:'', middle:'', right:arr[0] };
   else
   if (arr.length == 2)
      return { left:'', middle:arr[0], right:arr[1] };
   else
   if (arr.length == 3)
      return { left:arr[0], middle:arr[1], right:arr[2] };
   else
      return { left:'', middle:'', right:'' };
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
   
   var hasPSA = function(s) { return (s && s != '')? s : ''; };
   var needsSP = function(s) { return (s && s != '')? ' ' : ''; };
	var a = psa? psa.split('|') : [];
	psa = '';

	var prop = hasPSA(a[0]);
	if (extra_prop) prop += needsSP(prop) + extra_prop;
	if (prop != '') psa = 'class='+ dq(prop);

	var style = hasPSA(a[1]);
	if (extra_style) style += needsSP(style) + extra_style;
	if (style != '') psa += needsSP(psa) +'style='+ dq(style);

	var attr = hasPSA(a[2]);
	if (extra_attr) attr += needsSP(attr) + extra_attr;
	if (attr != '') psa += needsSP(psa) + attr;

	//console.log('psa_out=['+ psa +']');
	return psa;
}

// like w3_psa() except returns in original psa format (i.e. not expanded to "class=...")
function w3_psa_mix(psa, extra_prop, extra_style, extra_attr)
{
	//console.log('mix_in=['+ psa +']');
	//console.log('extra_prop=['+ extra_prop +']');
	//console.log('extra_style=['+ extra_style +']');
	//console.log('extra_attr=['+ extra_attr +']');

   var hasPSA = function(s) { return (s && s != '')? s : ''; };
   var needsSP = function(s) { return (s && s != '')? ' ' : ''; };
   var needsSemi = function(s) { return (s && s != '')? '; ' : ''; };
	var a = psa? psa.split('|') : [];
	psa = '';

	var prop = hasPSA(a[0]);
	if (extra_prop) prop += needsSP(prop) + extra_prop;
	if (prop != '') psa += prop;

	var style = hasPSA(a[1]);
	if (extra_style) style += needsSemi(style) + extra_style;
	if (style != '') psa += '|'+ style;

	var attr = hasPSA(a[2]);
	if (extra_attr) attr += needsSP(attr) + extra_attr;
	if (attr != '') psa += '|' + attr;

	//console.log('mix_out=['+ psa +']');
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

function w3_fillText(ctx, x, y, text, color, font, align, baseline)
{
   if (color) ctx.fillStyle = color;
   if (font) ctx.font = font;
   if (align) ctx.textAlign = align;
   if (baseline) ctx.textBaseline = baseline;
   var tw = ctx.measureText(text).width;
   ctx.fillText(text, x-tw/2, y);
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


////////////////////////////////
// nav
////////////////////////////////

function w3_click_nav(next_id, cb_next, cb_arg)
{
   //console.log('w3_click_nav '+ next_id +' cb_next='+ cb_next +' cb_arg='+ cb_arg);
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
      w3_call(cb_next +'_focus', next_id, cb_arg);
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
   if ((!psa || psa == '') && (!text || text == '') && (!extension || extension == '')) return '';
   
   // most likely already an embedded w3_label()
   if (text && text.startsWith('<label ')) return text;
   
   path = path? ('id-'+ path +'-label') : '';   // so w3_set_label() can find label
	//var inline = psa.includes('w3-label-inline');
	var p = w3_psa(psa, path);
	text = text? text : '';
	var s = '<label '+ p +'>'+ text + (extension? extension : '') +'</label>';
	//var s = '<label '+ p +'>'+ text + (extension? extension : '') + (inline? '':'<br>') +'</label>';
	//console.log('LABEL: psa='+ psa +' text=<'+ text +'> text?='+ (text?1:0) +' s=<'+ s +'>');
	return s;
}

function w3_set_label(label, path)
{
	w3_el(path +'-label').innerHTML = label;
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
      w3_call(cb, el, cb_param, /* first */ false);
   }

   w3int_post_action();
}

function w3_link(psa, url, inner, title, cb, cb_param)
{
   var qual_url = url;
   if (!url.startsWith('http://') && !url.startsWith('https://'))
      qual_url = 'http://'+ url;
   title = (title && title != '')? (' title='+ dq(title)) : '';

   // by default use pointer cursor if there is a callback
	var pointer = (cb && cb != '')? 'w3-pointer':'';
	cb_param = cb_param || 0;
	var onclick = cb? (' onclick="w3int_link_click(event, '+ sq(cb) +', '+ sq(cb_param) +')"') : '';

	var p = w3_psa(psa, pointer, '', 'href='+ dq(qual_url) +' target="_blank"'+ title + onclick);
	var s = '<a '+ p +'>'+ inner +'</a>';
	//console.log(s);
	return s;
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

function w3int_radio_click(ev, path, cb)
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
		w3_call(cb, path, idx, /* first */ false);
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

function w3_radio_button(psa, text, path, isSelected, cb)
{
	var onclick = cb? ('onclick="w3int_radio_click(event, '+ sq(path) +', '+ sq(cb) +')"') : '';
	var p = w3_psa(psa, 'id-'+ path + (isSelected? (' '+ w3_highlight_color) : '') +' w3-btn w3-ext-btn', '', onclick);
	var s = '<button '+ p +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

// used when current value should come from config param
function w3_radio_button_get_param(psa, text, path, selected_if_val, init_val, save_cb)
{
	//console.log('w3_radio_button_get_param: '+ path);
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	
	// set default selection of button based on current value
	var isSelected = (cur_val == selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
	return w3_radio_button(psa, text, path, isSelected, save_cb);
}


////////////////////////////////
// buttons: two button switch
////////////////////////////////

var w3_SWITCH_YES_IDX = 0, w3_SWITCH_NO_IDX = 1;

function w3_switch(psa, text_0, text_1, path, text_0_selected, save_cb)
{
   //console.log('w3_switch psa='+ psa);
	var s =
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-0'), text_0, path, text_0_selected? 1:0, save_cb) +
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-1'), text_1, path, text_0_selected? 0:1, save_cb);
	return s;
}

// used when current value should come from config param
function w3_switch_get_param(psa, text_0, text_1, path, text_0_selected_if_val, init_val, save_cb)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);

	// set default selection of button based on current value
	var text_0_selected = (cur_val == text_0_selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
	var s =
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-0'), text_0, path, text_0_selected? 1:0, save_cb) +
		w3_radio_button(w3_psa_mix(psa, 'w3int-switch-1'), text_1, path, text_0_selected? 0:1, save_cb);
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
         w3_call(cb, path, cb_param, /* first */ false);
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

function w3_button(psa, text, cb, cb_param)
{
	var path = 'id-btn-grp-'+ w3int_btn_grp_uniq.toString();
	w3int_btn_grp_uniq++;
	cb_param = cb_param || 0;
	var onclick = cb? ('onclick="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"') : '';
	if (cb && psa.includes('w3-momentary')) {
	   onclick += ' onmousedown="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', 0)"';
	   onclick += ' ontouchstart="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', 0)"';
	}
	
	// w3-round-large listed first so its '!important' can be overriden by subsequent '!important's
	var default_style = psa.includes('w3-round-')? '' : ' w3-round-large';
   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left);
	var psa_inner = w3_psa(psa3.right, path +' w3-btn w3-ext-btn'+ default_style, '', onclick);
	var s = '<button '+ psa_inner +'>'+ text +'</button>';
	if (psa_outer != '') s = '<div '+ psa_outer +'>'+ s +'</div>';
	//console.log(s);
	return s;
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
	if (typeof size == 'number' && size >= 0) font_size = px(size);
	else
	if (typeof size == 'string') font_size = size;
	font_size = font_size? (' font-size:'+ font_size +';') : '';

	color = (color && color != '')? (' color:'+ color) : '';
	var onclick = cb? ('onclick="w3int_button_click(event, '+ sq(path) +', '+ sq(cb) +', '+ sq(cb_param) +')"') : '';
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
	if (typeof size == 'number' && size >= 0) font_size = px(size);
	else
	if (typeof size == 'string') font_size = size;
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
function w3int_input_key(ev, path, cb)
{
   var k = ev.key;
   var ctl = ev.ctrlKey;
	var el = w3_el(path);
   if (!el) return;
   //console.log('w3int_input_key k='+ k + (ctl? ' CTL ':'') +' val=<'+ el.value +'> cb='+ cb);
   cb = cb.split('|');

   if (ctl && k == 'c' && cb[1]) {
      //console.log('w3int_input_key ^C cb='+ cb[1]);
      w3_call(cb[1]);
   }

   if (ctl && k == 'd' && cb[2]) {
      //console.log('w3int_input_key ^D cb='+ cb[2]);
      w3_call(cb[2]);
   }

   if (ctl && k == '\\' && cb[3]) {
      //console.log('w3int_input_key ^\\ cb='+ cb[3]);
      w3_call(cb[3]);
   }

   var input_any_change = w3_contains(el, 'w3-input-any-change');
	if (ev.key == 'Enter') {
      if (input_any_change) {
         // consider unchanged input value followed by Enter to be an input change
         //console.log('w3int_input_key: w3-input-any-change + Enter');
         w3_input_change(path, cb[0]);
      } else
	   if (el.value == '') {
         // cause empty input lines followed by Enter to send empty command to shell
         //console.log('w3int_input_key: empty line + Enter');
         w3_input_change(path, cb[0]);
      }
	}

   // if Delete key (Backspace) when entire value is selected then consider it an input change
	if (ev.key == 'Backspace' && input_any_change && el.selectionStart == 0 && el.selectionEnd == el.value.length) {
      //console.log('w3int_input_key Delete: len='+ el.value.length +' ss='+ el.selectionStart +' se='+ el.selectionEnd);
      el.value = '';
      w3_input_change(path, cb[0]);
	}
}

function w3_input_change(path, cb, cb_param)
{
	var el = w3_el(path);
	if (el) {
      //console.log('w3_input_change path='+ path);
      w3_check_restart_reboot(el);
      
      w3_highlight(el);
      setTimeout(function() {
         w3_unhighlight(el);
      }, w3_highlight_time);

      // cb is a string because can't pass an object to onclick
      if (cb) {
         cb = cb.split('|');
         //el.select();
         w3_call(cb[0], path, el.value, /* first */ false, cb_param);
      }
   }
	
   if (w3_contains(el, 'w3-retain-input-focus'))
	   w3_field_select(path, {mobile:1});     // select the field
   else
      w3int_post_action();
}

// no cb_param here because w3_input_change() passes the input value as the callback parameter
function w3_input(psa, label, path, val, cb, placeholder)
{
	var id = path? ('id-'+ path) : '';
	cb = cb || '';
	var phold = placeholder? (' placeholder="'+ placeholder +'"') : '';
	var onchange = path? (' onchange="w3_input_change('+ sq(path) +', '+ sq(cb) +')" onkeydown="w3int_input_key(event, '+ sq(path) +', '+ sq(cb) +')"') : '';
	var val = ' value='+ dq(val || '');
	var inline = psa.includes('w3-label-inline');
	var bold = !psa.includes('w3-label-not-bold');
	var spacing = (label != '' && inline)? ' w3int-margin-input' : '';

	// type="password" in no good because it forces the submit to be https which we don't support
	var type = 'type='+ (psa.includes('w3-password')? '"password"' : '"text"');

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, 'w3-input w3-border w3-hover-shadow '+ id + spacing, '', type + phold);

	var s =
	   '<div '+ psa_outer +'>' +
         w3_label(psa_label, label, path) +
		   // NB: include id in an id= for benefit of keyboard shortcut field detection
         '<input id='+ dq(id) +' '+ psa_inner + val + onchange +'>' +
      '</div>';
	//if (path == 'Title') console.log(s);
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
	cur_val = decodeURIComponent(cur_val);
	//console.log('w3_input_get: path='+ path +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input(psa, label, path, cur_val, cb, placeholder);
}

// DEPRECATED
function w3_input_get_param(label, path, cb, init_val, placeholder)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	cur_val = decodeURIComponent(cur_val);
	//console.log('w3_input_get_param: path='+ path +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input('', label, path, cur_val, cb, placeholder);
}


////////////////////////////////
// textarea
////////////////////////////////

function w3_textarea(psa, label, path, val, rows, cols, cb)
{
	var id = path? (' id-'+ path) : '';
	var spacing = (label != '')? ' w3-margin-T-8' : '';
	var onchange = ' onchange="w3_input_change('+ sq(path) +', '+ sq(cb) +')" onkeydown="w3int_input_key(event, '+ sq(path) +', '+ sq(cb) +')"';
	var val = val || '';
	var p = w3_psa(psa, 'w3-input w3-border w3-hover-shadow'+ id + spacing, '', 'rows="'+ rows +'" cols="'+ cols +'"');

	var s =
	   w3_div('',
	      label,
		   '<textarea '+ p + onchange +'>'+ val +'</textarea>'
		);
	//if (label == 'Title') console.log(s);
	return s;
}

// used when current value should come from config param
function w3_textarea_get_param(psa, label, path, rows, cols, cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	cur_val = decodeURIComponent(cur_val);
	//console.log('w3_textarea_get_param: path='+ path +' cur_val="'+ cur_val +'"');
	return w3_textarea(psa, label, path, cur_val, rows, cols, cb);
}


////////////////////////////////
// checkbox
////////////////////////////////

function w3int_checkbox_change(path, save_cb)
{
	var el = w3_el(path);
	w3_check_restart_reboot(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
	   //console.log('w3int_checkbox_change el='+ el +' checked='+ el.checked);
		//el.select();
		w3_highlight(el);
		setTimeout(function() {
			w3_unhighlight(el);
		}, w3_highlight_time);
		w3_call(save_cb, path, el.checked, /* first */ false);
	}

   if (w3_contains(el, 'w3-retain-input-focus'))
	   w3_field_select(path, {mobile:1});     // select the field
   else
      w3int_post_action();
}

function w3_checkbox(psa, label, path, checked, cb)
{
	var id = path? ('id-'+ path) : '';
	var onchange = ' onchange="w3int_checkbox_change('+ sq(path) +', '+ sq(cb) +')"';
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
			w3_call(cb, path, checked, /* first */ true);
		}, 500);

	//if (label == 'Title') console.log(s);
	return s;
}

// used when current value should come from config param
function w3_checkbox_get_param(psa, label, path, save_cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	//console.log('w3_checkbox_get_param: path='+ path +' cur_val="'+ cur_val +'"');
	return w3_checkbox(psa, label, path, cur_val, save_cb);
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
   if (!el) return;
	el.checked = checked? true:false;
}


////////////////////////////////
// select
////////////////////////////////

var W3_SELECT_SHOW_TITLE = -1;

function w3int_select_change(ev, path, save_cb)
{
	var el = ev.currentTarget;
	w3_check_restart_reboot(el);

	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		w3_call(save_cb, path, el.value, /* first */ false);
	}
	
   w3int_post_action();
}

function w3int_select(psa, label, title, path, sel, opts_s, cb)
{
	var id = path? ('id-'+ path) : '';
	var first = '';

	if (title != '') {
		first = '<option value="-1" '+ ((sel == W3_SELECT_SHOW_TITLE)? 'selected':'') +' disabled>' + title +'</option>';
	} else {
		if (sel == W3_SELECT_SHOW_TITLE) sel = 0;
	}
	
	var inline = psa.includes('w3-label-inline');
	var bold = !psa.includes('w3-label-not-bold');
	var spacing = (label != '' && !inline)? ' w3-margin-T-8' : '';
	if (inline) spacing += ' w3-margin-left';
	if (cb == undefined) cb = '';
	var onchange = 'onchange="w3int_select_change(event, '+ sq(path) +', '+ sq(cb) +')"';

   var psa3 = w3_psa3(psa);
   var psa_outer = w3_psa(psa3.left, inline? 'w3-show-inline-new':'');
   var psa_label = w3_psa_mix(psa3.middle, (label != '' && bold)? 'w3-bold':'');
	var psa_inner = w3_psa(psa3.right, id +' w3-pointer'+ spacing, '', onchange);

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
			w3_call(cb, path, sel, /* first */ true);
		}, 500);

	//console.log(s);
	return s;
}

function w3int_select_options(sel, opts)
{
   var s = '';
   
   if (typeof opts == 'string') {
      // range of integers (increment one assumed)
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
   if (Array.isArray(opts)) {
      // array of strings and/or numbers or take first object key as option
      for (var i=0; i < opts.length; i++) {
         var obj = opts[i];
         if (typeof obj == 'object') {
            var keys = Object.keys(obj);
            obj = obj[keys[0]];
         }
         s += '<option value='+ dq(i) +' '+ ((i == sel)? 'selected':'') +'>'+ obj +'</option>';
      }
   } else
   if (typeof opts == 'object') {
      // object: enumerate sequentially like an array
      // allows object to serve a dual purpose by having non-integer keys
      w3_obj_enum_data(opts, null, function(i, key) {
         s += '<option value='+ dq(i) +' '+ ((i == sel)? 'selected':'') +'>'+ opts[key] +'</option>';
      });
      
      /*
      var keys = Object.keys(opts);
      for (var i=0; i < keys.length; i++) {
         var key = keys[i];
         s += '<option value='+ dq(i) +' '+ ((i == sel)? 'selected':'') +'>'+ opts[key] +'</option>';
      }
      */
   }
   
   return s;
}

function w3_select(psa, label, title, path, sel, opts, save_cb)
{
   var s = w3int_select_options(sel, opts);
   return w3int_select(psa, label, title, path, sel, s, save_cb);
}

// hierarchical -- menu entries interspersed with disabled (non-selectable) headers
function w3_select_hier(psa, label, title, path, sel, opts, cb)
{
   var s = '';
   var idx = 0;
   if (typeof opts != 'object') return;

   w3_obj_enum_data(opts, null, function(i, key) {
      s += '<option value='+ dq(idx++) +' disabled>'+ key +'</option> ';
      var a = opts[key];
      if (!Array.isArray(a)) return;

      for (var j=0; j < a.length; j++) {
         var v = w3_first_value(a[j]);
         s += '<option value='+ dq(idx++) +' id="id-'+ i +'-'+ j +'">'+ v.toString() +'</option> ';
      }
   });
   
   /*
   var keys = Object.keys(opts);
   for (var i=0; i < keys.length; i++) {
      var key = keys[i];
      s += '<option value='+ dq(idx++) +' disabled>'+ key +'</option> ';
      var a = opts[key];
      if (!Array.isArray(a)) continue;

      for (var j=0; j < a.length; j++) {
         var v = w3_first_value(a[j]);
         s += '<option value='+ dq(idx++) +' id="id-'+ i +'-'+ j +'">'+ v.toString() +'</option> ';
      }
   }
   */
   
   return w3int_select(psa, label, title, path, sel, s, cb);
}

// used when current value should come from config param
function w3_select_get_param(psa, label, title, path, opts, save_cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? 0 : init_val);
	return w3_select(psa, label, title, path, cur_val, opts, save_cb);
}

function w3_select_enum(path, func)
{
   var path = w3_el(path);
   if (!path) return;
	w3_iterate_children(path, func);
}

function w3_select_value(path, idx)
{
   var el = w3_el(path);
   if (!el) return;
	el.value = idx;
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

function w3int_slider_change(ev, complete, path, save_cb)
{
	var el = ev.currentTarget;
	w3_check_restart_reboot(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
	   //console.log('w3int_slider_change path='+ path +' el.value='+ el.value);
		w3_call(save_cb, path, el.value, complete, /* first */ false);
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

function w3_slider(psa, label, path, val, min, max, step, save_cb)
{
	var id = path? ('id-'+ path) : '';
	var inline = psa.includes('w3-label-inline');
	var bold = !psa.includes('w3-label-not-bold');
	var spacing = (label != '' && inline)? ' w3-margin-L-8' : '';
	if (inline) spacing += ' w3-margin-left';

	value = (val == null)? '' : w3_strip_quotes(val);
	value = 'value='+ dq(value);
	var oc = ' oninput="w3int_slider_change(event, 0, '+ sq(path) +', '+ sq(save_cb) +')"';
	// change event fires when the slider is done moving
	var os = ' onchange="w3int_slider_change(event, 1, '+ sq(path) +', '+ sq(save_cb) +')"';

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
	if (save_cb)
		setTimeout(function() {
			//console.log('w3_slider: initial callback: '+ save_cb +'('+ sq(path) +', '+ val +')');
			w3_call(save_cb, path, val, /* complete */ true, /* first */ true);
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

function w3_menu_items(id)
{
   //console.log('w3_menu_items id='+ id);

   var s = '';
   var idx = 0, prop, attr;

   for (var i=1; i < arguments.length; i++) {
      if (arguments[i] == '<hr>') {
         prop = 'w3-menu-item-hr';
         attr = '';
      } else {
         prop = 'w3-menu-item';
         attr = 'id='+ dq(idx);
         idx++;
      }
      s += w3_div(prop +'||'+ attr, arguments[i]);
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
   el.style.top = px(y);
   el.style.left = px(x);
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
   el.style.visibility = 'hidden';

   window.removeEventListener("keyup", w3int_menu_close, false);
   window.removeEventListener("click", w3int_menu_close, false);

   if (ev != null && cb != null) {
      var _id = ev.target.id;
      var idx = +_id;
      if (_id == '' || isNaN(idx)) _id = ev.target.parentNode.id;
      idx = +_id;
      if (_id == '' || isNaN(idx)) idx = -1;
      w3_call(cb, idx, el.w3_menu_x);
   }

   // allow right-button to select menu items
	if (ev != null) {
      //console.log('w3int_menu_onclick: cancelEvent()');
      canvas_ignore_mouse_event = true;
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
	//console.log('w3_string_set_cfg_cb: path='+ path +' '+ typeof val +' "'+ val +'" first='+ first);
	
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, encodeURIComponent(val.toString()), save);
}

function w3_remove_trailing_index(path, sep)
{
   sep = sep || '-';    // optional separator, e.g. if negative indicies are used
   var re = new RegExp('(.*)'+ sep +'([-]?[0-9]+)');
	var el = re.exec(path);      // remove trailing -nnn
   //console.log('w3_remove_trailing_index path='+ path +' el='+ el);
	var idx;
	if (!el) {
	   idx = -1;
	   el = path;
	} else {
	   idx = el[2];
	   el = el[1];
	}
	return { el:el, idx:idx };
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

function w3_inline_percent(psa)
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
	var s = w3_div(w3_psa_mix(psa, 'w3-text', 'padding:0 4px 0 0; background-color:inherit'), text);
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
			right +
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
			middle +
		'</div>' +
		'<div class="w3-col w3-third '+ prop_col +'">' +
			right +
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
			middleL +
		'</div>' +
		'<div class="w3-col w3-quarter '+ prop_col +'">' +
			middleR +
		'</div>' +
		'<div class="w3-col w3-quarter '+ prop_col +'">' +
			right +
		'</div>' +
	'</div>';
	//console.log(s);
	return s;
}
