/*
	FIXME input validation issues:
		data out-of-range
		data missing
		what should it mean? delete button, but params have been changed (e.g. freq)
		SECURITY: can eval arbitrary code input?
*/

function dx_get_value(v) { return w3_get_value('dx.o.'+ v +'_'+ dx.o.gid); }

function dx_update_check(idx, upd, isPassband)
{
   // called from user connection label edit panel, not admin edit tab
   if (idx == dx.IDX_USER) return;

   idx = +idx;
   dx.o.gid = idx;
	console.log('### dx_update_check gid(idx)='+ idx +' '+ ((upd == dx.UPD_ADD)? 'ADD' : 'MODIFY'));

   dx.o.fr = +dx_get_value('fr');
   dx.o.fm = +dx_get_value('fm');
   dx.o.ft = +dx_get_value('ft');

   dx.o.fd = dx.o.last_fd[idx];
   dx.o.b = +dx_get_value('b');
   dx.o.e = +dx_get_value('e');

   dx.o.o = +dx_get_value('o');
   dx.o.i =  dx_get_value('i');
   dx.o.n =  dx_get_value('n');
   dx.o.p =  dx_get_value('p');
   
   // dx.o.{lo,hi} handled in dx_passband_cb() if isPassband set
   if (!isPassband) {
      if (idx >= 0 && dx.o.last_pb[idx]) {
         dx.o.lo = dx.o.last_pb[idx][0];
         dx.o.hi = dx.o.last_pb[idx][1];
      } else {
         dx.o.lo = dx.o.hi = 0;
      }
   }

   var flags = (dx.o.fd << dx.DX_DOW_SFT) | (dx.o.ft << dx.DX_TYPE_SFT) | dx.o.fm;
	if (dx.o.fr < 0) dx.o.fr = 0;
	
	// delay setting to -1 until here so dx.o.* params are copied from cur gid/idx
	if (upd == dx.UPD_ADD)
	   dx.o.gid = -1;
	else
	   dx.ignore_dx_update = true;      // prevent tabbing between fields being lost by re-render
	
   console.log('dx.o:');
   console.log(dx.o);
   var begin = +(dx.o.b), end = +(dx.o.e);
   if (begin == 0 && end == 0) end = 2400;
	console.log('SET DX_UPD g='+ dx.o.gid +' f='+ dx.o.fr +' lo='+ dx.o.lo +' hi='+ dx.o.hi +
	   ' o='+ dx.o.o.toFixed(0) +' fl='+ flags +'(dow='+ dx.o.fd +'|ty='+ dx.o.ft +'|mo='+ dx.o.fm +') b='+ begin +' e='+ end +' i='+ dx.o.i +' n='+ dx.o.n +' p='+ dx.o.p);
	ext_send('SET DX_UPD g='+ dx.o.gid +' f='+ dx.o.fr +' lo='+ dx.o.lo.toFixed(0) +' hi='+ dx.o.hi.toFixed(0) +
      ' o='+ dx.o.o.toFixed(0) +' fl='+ flags +' b='+ begin +' e='+ end +
      ' i='+ encodeURIComponent(dx.o.i +'x') +' n='+ encodeURIComponent(dx.o.n +'x') +' p='+ encodeURIComponent(dx.o.p +'x'));

   // indicate that we've saved due to the immediate nature of DX_UPD
   w3_call('dx_sched_save', 'id-dx-list-saved', 1000);      // w3_call() because dx_save() only exists in admin
}

// CAUTION: the following routines are called from both user and admin code

function dx_num_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	console.log('dx_num_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
	w3_num_cb(o.el, val);
	dx_update_check(o.idx, dx.UPD_MOD);
}

function dx_freq_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	val = val.parseFloatWithUnits('kM', 1e-3);
	console.log('dx_num_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
   w3_set_value(path, val);      // NB: keep field numeric!
	w3_num_cb(o.el, val);
	dx_update_check(o.idx, dx.UPD_MOD);
}

function dx_sel_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	console.log('dx_sel_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
	w3_string_cb(o.el, val);
	dx_update_check(o.idx, dx.UPD_MOD);
}

function dx_string_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	console.log('dx_string_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
	w3_string_cb(o.el, val);
	dx_update_check(o.idx, dx.UPD_MOD);
}

function dx_dow_button(path, day_i)
{
	var o = w3_remove_trailing_index(path, '_');
   console.log('dx_dow_button path='+ path +' o.idx='+ o.idx);
   var user = (o.idx == dx.IDX_USER);
   var dow_b = 1 << (6 - (+day_i));
   var dow = user? dx.o.fd : dx.o.last_fd[o.idx];
   var before = dow;
   dow ^= dow_b;
   if (user)
      dx.o.fd = dow;
   else
      dx.o.last_fd[o.idx] = dow;
   var sel = dow & dow_b;
   console.log('dx_dow_button day_i='+ day_i +' dow_b='+ dow_b.toHex(2) +' sel='+ sel.toHex(2) +
         ' before='+ before.toHex(2) +' after='+ dow.toHex(2) +' o.idx='+ o.idx);
   w3_color(path, sel? 'black' : 'white', sel? dx.dow_colors[day_i] : 'darkGrey');
	dx_update_check(o.idx, dx.UPD_MOD);
}

function dx_sched_time_cb(path, val, first)
{
   val = (+val).toFixed(0).leadingZeros(4);
   dx_string_cb(path, val, first);
   w3_set_value(path, val);      // put back value with leading zeros
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
   if (len >= 1 && a[0] != '' && a[len-1] != '') {    // len-1 because ", " from split() above becomes 2 array elements
      dx.o.lo = Math.round(a[0].parseFloatWithUnits('kM'));
      dx.o.hi = Math.round(a[len-1].parseFloatWithUnits('kM'));
   } else {
      dx.o.lo = dx.o.hi = 0;
   }

	var o = w3_remove_trailing_index(path, '_');
	console.log('dx_passband_cb lo='+ dx.o.lo +' hi='+ dx.o.hi +' o.idx='+ o.idx);
	dx_update_check(o.idx, dx.UPD_MOD, true);
}
