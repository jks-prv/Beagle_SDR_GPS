/*
	FIXME input validation issues:
		data out-of-range
		data missing
		what should it mean? delete button, but params have been changed (e.g. freq)
		SECURITY: can eval arbitrary code input?
*/

var DX_MODE = 0xf;

var DX_TYPE = 0xf0;
var DX_TYPE_SFT = 4;

var DX_FLAG = 0xff00;

var dxo = { };

function dx_update_check(idx)
{
   idx = +idx;
   if (idx == -1) return;     // called from user connection label edit panel, not admin edit tab

	console.log('### dx_update_check idx='+ idx);
   dxo.gid = idx;
   var tag = dxo.tags[idx];
   dxo.tag = tag;
   var value = function(v) { return w3_get_value('dxo.'+ v +'_'+ idx); }
   dxo.f = +value('f');
   dxo.o = +value('o');
   console.log('dxo:');
   console.log(dxo);
   
	var mode = dxo.m;
	var type = dxo.y << DX_TYPE_SFT;
	mode |= type;
	dxo.f -= cfg.freq_offset;
	if (dxo.f < 0) dxo.f = 0;
	
	console.log('SET DX_UPD g='+ dxo.gid +' f='+ dxo.f +' lo='+ dxo.lo.toFixed(0) +' hi='+ dxo.hi.toFixed(0) +' o='+ dxo.o.toFixed(0) +' m='+ mode +' t='+ tag +
		' i='+ dxo.i +' n='+ dxo.n +' p='+ dxo.p);
	//wf_send('SET DX_UPD g='+ dxo.gid +' f='+ dxo.f +' lo='+ dxo.lo.toFixed(0) +' hi='+ dxo.hi.toFixed(0) +' o='+ dxo.o.toFixed(0) +' m='+ mode +
	//	' i='+ encodeURIComponent(dxo.i +'x') +' n='+ encodeURIComponent(dxo.n +'x') +' p='+ encodeURIComponent(dxo.p +'x'));
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
      var hbw = Math.round(parseInt(a[0]) / 2);
      dxo.lo = -hbw;
      dxo.hi =  hbw;
   } else
   if (len > 1) {
      dxo.lo = Math.round(parseInt(a[0]));
      dxo.hi = Math.round(parseInt(a[len-1]));
   } else {
      dxo.lo = dxo.hi = 0;
   }

	var o = w3_remove_trailing_index(path, '_');
	dx_update_check(o.idx);
}
