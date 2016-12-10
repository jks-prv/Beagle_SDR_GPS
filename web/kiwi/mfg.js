// Copyright (c) 2016 John Seamons, ZL/KF6VO

var ws_mfg;
var ver_maj, ver_min;

function mfg_main()
{
}

function kiwi_ws_open(conn_type, cb, cbp)
{
	return open_websocket(conn_type, cb, cbp, null, mfg_recv);
}

function mfg_draw()
{
	var mfg = html("id-mfg");
	var s =
	'<div class="w3-container w3-section w3-dark-grey w3-half">' +
		'<div><h4><strong>Manufacturing interface</strong>&nbsp;&nbsp;' +
			'<small>(software version v'+ ver_maj +'.'+ ver_min +')</small></h4></div>';
	
	s += ver_maj?
		'<div style="color:lime"><h2>OKAY to make customer SD cards using this.</h2></div>' :
		'<div style="color:red"><h2>WARNING: Use for testing only!<br>Do not make customer SD cards with this yet.</h2></div>';

	s +=
		'<div>' +
			'EEPROM serial number: ' +
			'<div id="id-ee-write" class="w3-btn w3-round-large" onclick="mfg_ee_write()"></div>' +
		'</div><br>' +

		w3_col_percent('', 'w3-hcenter',
			'<div id="id-seq-override" class="w3-btn w3-round-large w3-yellow" onclick="mfg_seq_override()"></div>', 40,

			'<input id="id-seq-input" class="w3-input w3-border w3-width-auto w3-hover-shadow w3-hidden" type="text" size=9 onchange="mfg_seq_set()">', 20,

			'<div id="id-power-off" class="w3-btn w3-round-large" style="background-color:fuchsia" onclick="mfg_power_off()"></div>', 40
		) +
		'<hr>' +

		w3_third('', '',
			'<div id="id-sd-write" class="w3-btn w3-round-large w3-aqua" onclick="mfg_sd_write()"></div>',

			'<div class="w3-progress-container w3-round-large w3-white w3-show-inline-block">' +
				'<div id="id-progress" class="w3-progressbar w3-round-large w3-light-green" style="width:0%">' +
					'<div id="id-progress-text" class="w3-container w3-text-black"></div>' +
				'</div>' +
			'</div>' +
			'<span id="id-progress-time"></span>' +
			'<span id="id-progress-icon" class="w3-margin-left"></span>',

			'<div id="id-sd-status" class="class-sd-status"></div>'
		) +
		'<hr>' +

		'<div id="id-status-msg" class="class-mfg-status" data-scroll-down="true"></div>' +
	'</div>' +
	'';
	
	mfg.innerHTML = s;
	mfg.style.top = mfg.style.left = '10px';

	var el = html('id-sd-write');
	el.innerHTML = "click to write<br>micro-SD card";

	var el = html('id-power-off');
	el.innerHTML = "click to halt<br>and power off";

	visible_block('id-mfg', true);
	
	//setTimeout(function() { setInterval(update_TOD, 1000); }, 1000);
}

function mfg_recv(data)
{
	var next_serno = -1;
	var stringData = arrayBufferToString(data);
	params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		param = params[i].split("=");
		
		switch (param[0]) {
			case "ver_maj":
				ver_maj = parseFloat(param[1]);
				break;

			case "ver_min":
				ver_min = parseFloat(param[1]);
				mfg_draw();
				break;

			case "serno":
				var serno = parseFloat(param[1]);
				console.log("MFG next_serno="+ next_serno + " serno="+ serno);
				
				var button_text;
				if (serno <= 0) {		// write serno = 0 to force invalid for testing
					button_text = 'invalid, click to write EEPROM<br>with next serial number: '+ next_serno;
					button_color = 'red';
				} else {
					button_text = 'valid = '+ serno.toString() +'<br>click to change to '+ next_serno;
					button_color = '#4CAF50';		// w3-green
				}
				
				html('id-ee-write').innerHTML = button_text;
				html('id-ee-write').style.backgroundColor = button_color;

				html('id-seq-override').innerHTML = 'next serial number = '+ next_serno +'<br>click to override';
				w3_unclass(w3_el_id('id-seq-input'), ' w3-visible');
				html('id-seq-input').value = next_serno;
				break;

			case "next_serno":
				next_serno = parseFloat(param[1]);
				break;

			case "microSD_done":
				mfg_sd_write_done(parseFloat(param[1]));
				break;

			default:
				console.log('MFG UNKNOWN: '+ param[0] +'='+ param[1]);
				break;
		}
	}
}

function mfg_ee_write()
{
	ext_send("SET write");
}

function mfg_seq_override()
{
	w3_class(w3_el_id('id-seq-input'), ' w3-visible');
}

function mfg_seq_set()
{
	var next_serno = parseFloat(html('id-seq-input').value).toFixed(0);
	if (next_serno >= 0 && next_serno <= 9999) {
		ext_send("SET set_serno="+ next_serno);
	}
}

var sd_progress, sd_progress_max = 4*60;		// measured estimate -- in secs (varies with SD card write speed)
var mfg_sd_interval;
var refresh_icon = '<i class="fa fa-refresh fa-spin" style="font-size:24px"></i>';

function mfg_sd_write()
{
	var el = html('id-sd-write');
	w3_class(el, ' w3-override-yellow');
	el.innerHTML = "writing the<br>micro-SD card...";

	var el = html('id-sd-status');
	el.innerHTML = '';

	html('id-progress-text').innerHTML = html('id-progress').style.width = '0%';

	sd_progress = -1;
	mfg_sd_progress();
	mfg_sd_interval = setInterval('mfg_sd_progress()', 1000);

	html('id-progress-icon').innerHTML = refresh_icon;

	ext_send("SET microSD_write");
}

function mfg_sd_progress()
{
	sd_progress++;
	var pct = ((sd_progress / sd_progress_max) * 100).toFixed(0);
	if (pct <= 95) {	// stall updates until we actually finish in case SD is writing slowly
		html('id-progress-text').innerHTML = html('id-progress').style.width = pct +'%';
	}
	html('id-progress-time').innerHTML =
		((sd_progress / 60) % 60).toFixed(0) +':'+ (sd_progress % 60).toFixed(0).leadingZeros(2);
}

function mfg_sd_write_done(err)
{
	var el = html('id-sd-write');
	w3_unclass(el, ' w3-override-yellow');
	el.innerHTML = "click to write<br>micro-SD card";

	var el = html('id-sd-status');
	var msg = err? ('FAILED error '+ err.toString()) : 'WORKED';
	if (err == 1) msg += '<br>No SD card inserted?';
	if (err == 15) msg += '<br>rsync I/O error';
	el.innerHTML = msg;
	el.style.color = err? 'red':'lime';

	if (!err) {
		// force to max in case we never made it during updates
		html('id-progress-text').innerHTML = html('id-progress').style.width = '100%';
	}
	kiwi_clearInterval(mfg_sd_interval);
	html('id-progress-icon').innerHTML = '';
}

function mfg_power_off()
{
	var el = html('id-power-off');
	el.style.backgroundColor = 'red';
	el.innerHTML = "halting and<br>powering off...";
	ext_send("SET mfg_power_off");
}
