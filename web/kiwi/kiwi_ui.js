/*
	FIXME input validation issues:
		data out-of-range
		data missing
		what should it mean? delete button, but params have been changed (e.g. freq)
		SECURITY: can eval arbitrary code input?
*/

function dx_update_check(idx)
{
   idx = +idx;
   if (idx == -1) return;     // called from user connection label edit panel, not admin edit tab

	console.log('### dx_update_check idx='+ idx);
   dx.o.gid = idx;
   var tag = dx.o.tags[idx];
   dx.o.tag = tag;
   var value = function(v) { return w3_get_value('dx.o.'+ v +'_'+ idx); }
   dx.o.f = +value('f');
   dx.o.o = +value('o');
   console.log('dx.o:');
   console.log(dx.o);
   
	var mode = dx.o.m;
	var type = dx.o.y << dx.DX_TYPE_SFT;
	mode |= type;
	dx.o.f -= cfg.freq_offset;
	if (dx.o.f < 0) dx.o.f = 0;
	
	console.log('SET DX_UPD g='+ dx.o.gid +' f='+ dx.o.f +' lo='+ dx.o.lo.toFixed(0) +' hi='+ dx.o.hi.toFixed(0) +' o='+ dx.o.o.toFixed(0) +' m='+ mode +' t='+ tag +
		' i='+ dx.o.i +' n='+ dx.o.n +' p='+ dx.o.p);
	//wf_send('SET DX_UPD g='+ dx.o.gid +' f='+ dx.o.f +' lo='+ dx.o.lo.toFixed(0) +' hi='+ dx.o.hi.toFixed(0) +' o='+ dx.o.o.toFixed(0) +' m='+ mode +
	//	' i='+ encodeURIComponent(dx.o.i +'x') +' n='+ encodeURIComponent(dx.o.n +'x') +' p='+ encodeURIComponent(dx.o.p +'x'));
}

function dx_num_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	w3_num_cb(o.el, val);
	dx_update_check(o.idx);
}

function dx_sel_cb(path, val, first)
{
   if (first) return;
	console.log('dx_sel_cb path='+ path +' val='+ val +' first='+ first);
	var o = w3_remove_trailing_index(path, '_');
	w3_string_cb(o.el, val);
	dx_update_check(o.idx);
}

function dx_string_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	w3_string_cb(o.el, val);
	dx_update_check(o.idx);
}

function dx_passband_cb(path, val, first)
{
   if (first) return;

   // pbw
   // lo,hi
   // lo hi
   // lo, hi
   var a = val.split(/[,\s]/);
   var len = a.length;
	console.log('dx_passband_cb path='+ path +' val='+ val +' a.len='+ len);
	console.log(a);
   if (len == 1 && a[0] != '') {
      var hbw = Math.round(a[0].parseFloatWithUnits('kM') / 2);
      dx.o.lo = -hbw;
      dx.o.hi =  hbw;
   } else
   if (len >= 1 && a[0] != '' && a[len-1] != '') {    // len-1 because ", " becomes 2 array elements
      dx.o.lo = Math.round(a[0].parseFloatWithUnits('kM'));
      dx.o.hi = Math.round(a[len-1].parseFloatWithUnits('kM'));
   } else {
      dx.o.lo = dx.o.hi = 0;
   }
	console.log('dx_passband_cb lo='+ dx.o.lo +' hi='+ dx.o.hi);

	var o = w3_remove_trailing_index(path, '_');
	dx_update_check(o.idx);
}
