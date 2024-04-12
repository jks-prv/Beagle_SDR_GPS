/*
	FIXME input validation issues:
		data out-of-range
		data missing
		what should it mean? delete button, but params have been changed (e.g. freq)
		SECURITY: can eval arbitrary code input?
*/

function dx_get_value(v) { return w3_get_value('dx.o.'+ v +'_'+ dx.o.gid); }

// dx_mode(fm) to menu_mode(mm) mapping: (see kiwi.js)
// fm: am amn usb lsb  cw cwn nbfm  iq  drm  usn  lsn sam sau sal sas qam nnfm amw  modes_{lc,uc,idx}[]
//      0   1   2   3   4   5    6   7    8    9   10  11  12  13  14  15   16  17
// mm: AM AMN AMW USB USN LSB  LSN  CW  CWN NBFM NNFM  IQ DRM SAM SAU SAL  SAS QAM  mode_menu[]

function dx_update_check(idx, upd, isPassband)
{
   var user = (idx == dx.IDX_USER);
   if (!user) dx.o.gid = idx = +idx;

   // convert from mode menu order to dx mode order
   var dx_mode_idx, mode_menu_idx, mode_s;
   if (user) {
      mode_menu_idx = +dx.o.mm;
      mode_s = kiwi.mode_menu[mode_menu_idx];
      dx_mode_idx = w3_array_el_seq(kiwi.modes_uc, mode_s);
      console.log('dx_update_check USER dx_mode_idx='+ dx_mode_idx +' mode_menu_idx='+ mode_menu_idx +' '+ mode_s);
   } else {
      mode_menu_idx = +dx_get_value('mm');
      mode_s = kiwi.mode_menu[mode_menu_idx];
      dx_mode_idx = w3_array_el_seq(kiwi.modes_uc, mode_s);
      console.log('dx_update_check ADMIN dx_mode_idx='+ dx_mode_idx +' mode_menu_idx='+ mode_menu_idx +' '+ mode_s);
   }
   dx.o.fm = +dx_mode_idx;

   // Called from user connection label edit panel, not admin edit tab.
   // User label edit panel has a separate commit mechanism.
   if (idx == dx.IDX_USER) return true;

	console.log('### dx_update_check gid(idx)='+ idx +' '+ ((upd == dx.UPD_ADD)? 'ADD' : 'MODIFY'));

   dx.o.fr = +dx_get_value('fr');
   dx.o.ft = +dx_get_value('ft');

   dx.o.fd = dx.o.last_fd[idx];
   dx.o.b = +dx_get_value('b');
   dx.o.e = +dx_get_value('e');

   dx.o.o = +dx_get_value('o');
   dx.o.s = +dx_get_value('s');
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

   var flags = (dx.o.fd << dx.DX_DOW_SFT) | (dx.o.ft << dx.DX_TYPE_SFT) | dx_encode_mode(dx.o.fm);
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
	   ' o='+ dx.o.o.toFixed(0) +' s='+ dx.o.s.toFixed(0) +' fl='+ flags +'(dow='+ dx.o.fd +'|ty='+ dx.o.ft +'|mo='+ dx.o.fm +') b='+ begin +' e='+ end +' i='+ dx.o.i +' n='+ dx.o.n +' p='+ dx.o.p);
	ext_send('SET DX_UPD g='+ dx.o.gid +' f='+ dx.o.fr +' lo='+ dx.o.lo.toFixed(0) +' hi='+ dx.o.hi.toFixed(0) +
      ' o='+ dx.o.o.toFixed(0) +' s='+ dx.o.s.toFixed(0) +' fl='+ flags +' b='+ begin +' e='+ end +
      ' i='+ encodeURIComponent(dx.o.i +'x') +' n='+ encodeURIComponent(dx.o.n +'x') +' p='+ encodeURIComponent(dx.o.p +'x'));

   // indicate that we've saved due to the immediate nature of DX_UPD
   w3_call('dx_sched_save', 'id-dx-list-saved', 1000);      // w3_call() because dx_save() only exists in admin
   return false;
}

// CAUTION: the following routines are called from both user and admin code

function dx_num_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	console.log('dx_num_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
	w3_num_cb(o.el, val);
   if (dx_update_check(o.idx, dx.UPD_MOD)) dx_button_highlight();
}

function dx_freq_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	val = val.parseFloatWithUnits('kM', 1e-3);
	console.log('dx_num_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
   w3_set_value(path, val);      // NB: keep field numeric!
	w3_num_cb(o.el, val);
   if (dx_update_check(o.idx, dx.UPD_MOD)) dx_button_highlight();

   // with the admin DX tab make sure a changed frequency maintains a properly sorted list
   if (o.idx != dx.IDX_USER) {
      dx_update_admin();   // update (possibly re-sort) the list
      ext_send('SET MARKER search_freq='+ val);    // force a freq search in case the entry is now off-screen
   }
}

function dx_sel_cb(path, val, first)
{
   if (first) return;
   // e.g. path = "dx.o.mm_123", o = { el: "dx.o.mm", idx: "123" }
	var o = w3_remove_trailing_index(path, '_');
	console.log('dx_sel_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
	w3_num_cb(o.el, +val);     // e.g. sets dx.o.mm
	if (dx_update_check(o.idx, dx.UPD_MOD)) dx_button_highlight();
}

function dx_string_cb(path, val, first)
{
   if (first) return;
	var o = w3_remove_trailing_index(path, '_');
	console.log('dx_string_cb path='+ path +' val='+ val +' o.idx='+ o.idx);
	w3_string_cb(o.el, val);
	if (dx_update_check(o.idx, dx.UPD_MOD)) dx_button_highlight();
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
	if (dx_update_check(o.idx, dx.UPD_MOD)) dx_button_highlight();
}

function dx_sched_time_cb(path, val, first)
{
   val = (+val).toFixed(0).leadingZeros(4);
   dx_string_cb(path, val, first);
   w3_set_value(path, val);      // put back value with leading zeros
	var o = w3_remove_trailing_index(path, '_');
   if (o.idx == dx.IDX_USER) dx_button_highlight();
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
   dx.o.last_pb[o.idx] = [dx.o.lo, dx.o.hi];
	console.log('dx_passband_cb lo='+ dx.o.lo +' hi='+ dx.o.hi +' o.idx='+ o.idx);
   if (dx_update_check(o.idx, dx.UPD_MOD, true)) dx_button_highlight();
}


// common sd backup code used by admin and mfg pages

function sd_backup_init()
{
   kiwi.backup_size = kiwi.backup_progress = null;
   kiwi.sd_backup_icon = w3_icon('', 'fa-refresh fa-spin', 20);
}

function sd_backup_focus()
{
   var re_rootfs_size = new RegExp('^rootfs_size ([0-9]*)M');
   var re_progress = new RegExp('^\\s*([0-9.]*)([MG])\\s*([0-9]*%).*$');

   kiwi_output_msg_p.intercept = function(s) {
      s = s.replace(/\r/g, ' ');
      s = s.replace(/\n/g, ' ');
      var e = re_rootfs_size.exec(s);
      if (e && e.length >= 2) {
         //console.log(e);
         kiwi.backup_size = +e[1];
         if (e[2] == 'G') kiwi.backup_size *= 1024;
         console.log('backup_size='+ kiwi.backup_size);
      }
      e = re_progress.exec(s);
      if (e && e.length >= 3) {
         //console.log(e);
         kiwi.backup_progress = +e[1];
         if (e[2] == 'G') kiwi.backup_progress *= 1024;
         //console.log('backup_progress='+ kiwi.backup_progress);
      }
   };

   w3_do_when_cond(
      function() { return isNumber(kiwi.debian_maj); },
      function() {
         /*
            var ng_D11 = (!dbgUs && kiwi.debian_maj == 11);
            var ng_D12 = (kiwi.debian_maj > 12 || (kiwi.debian_maj == 12 && kiwi.debian_min < 4));
            if (ng_D11 || ng_D12) {
               w3_innerHTML('id-sd-backup-container',
                  w3_div('w3-container w3-text w3-red',
                     'Debian '+ kiwi.debian_maj +'.'+  kiwi.debian_min +' does not support the backup function.'));
            }
         */
         w3_show('id-sd-backup-container', 'w3-show-inline');
      }, null,
      250
   );
   // REMINDER: w3_do_when_cond() returns immediately
}

function sd_backup_blur()
{
   kiwi_output_msg_p.intercept = null;
}

function sd_backup_click_cb(id, idx)
{
   console.log('sd_backup_click_cb debian_maj='+ kiwi.debian_maj);
   /*
      if (!dbgUs && (kiwi.debian_maj == 11 || (kiwi.debian_maj == 12 && kiwi.debian_min < 4))) {
         w3_innerHTML('id-sd-backup-msg', 'SD write not supported yet');
         return;
      }
   */
   
   kiwi.backup_size = kiwi.backup_progress = null;
   w3_innerHTML('id-sd-backup-msg', 'formatting micro-SD card');
   w3_color('id-sd-backup-msg', '');
   
	w3_innerHTML('id-sd-progress-text', '');
	w3_el('id-sd-progress').style.width = '0%';

	kiwi.sd_progress = -1;
	kiwi.sd_interval = kiwi_setInterval(sd_backup_progress, 1000);

	w3_innerHTML('id-sd-backup-icon', kiwi.sd_backup_icon);

	ext_send("SET microSD_write");
}

function sd_backup_progress()
{
   if (kiwi.backup_size && kiwi.backup_progress) {
      if (kiwi.backup_size == 1) {
		   w3_el('id-sd-progress-text').innerHTML = w3_el('id-sd-progress').style.width = '100%';
         w3_innerHTML('id-sd-backup-msg', 'finalizing micro-SD card');
      } else {
         var pct = ((kiwi.backup_progress / kiwi.backup_size) * 100).toFixed(0);
         if (pct == 0) pct = 1;
         if (pct <= 99) {	// stall updates until we actually finish in case SD is writing slowly
            w3_el('id-sd-progress-text').innerHTML = w3_el('id-sd-progress').style.width = pct +'%';
         }
         w3_innerHTML('id-sd-backup-msg', 'copied '+ kiwi.backup_progress.toFixed(0) +' of '+ kiwi.backup_size.toFixed(0) +' MB');
      }
   } else {
      w3_innerHTML('id-sd-backup-msg', 'formatting micro-SD card');
   }

	kiwi.sd_progress++;
	var secs = (kiwi.sd_progress % 60).toFixed(0).leadingZeros(2);
	var mins = Math.floor(kiwi.sd_progress / 60).toFixed(0);
	w3_innerHTML('id-sd-backup-time', mins +':'+ secs);
}

function sd_backup_write_done(err)
{
	var msg = 'backup complete';
	var e;
	switch (err) {
	   case  0: e = null; break;
	   case  1: e = 'no SD card inserted?'; break;
	   case 15: e = 'SD card I/O error'; break;
	   case 30: e = 'SD card already mounted?'; break;
	   case 31: e = 'SD card format error'; break;
	   case 87: e = 'Backups not supported on this version of Debian'; break;
	   case 88: e = '(BBAI-64) must first update to latest Debian version'; break;
	   case 89: e = 'SD card flasher enable failed!'; break;
	   default: e = 'code '+ err; break;
	}
	if (e) msg = '<b>ERROR: '+ e +'</b>';

	if (!err) {
		// force to max in case we never made it during updates
		w3_el('id-sd-progress-text').innerHTML = w3_el('id-sd-progress').style.width = '100%';
	}
	kiwi_clearInterval(kiwi.sd_interval);
	w3_innerHTML('id-sd-backup-icon', '');
   w3_innerHTML('id-sd-backup-msg', msg);
   w3_color('id-sd-backup-msg', 'red', null, err);
}

