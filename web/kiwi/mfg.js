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
	var s =
		w3_div('',
		   '<h4><strong>Manufacturing interface</strong>&nbsp;&nbsp;' +
			'<small>(software version v'+ ver_maj +'.'+ ver_min +')</small></h4>'
		);
	
	s += ver_maj?
		w3_div('|color:lime', '<h2>OKAY to make customer SD cards using this.</h2>') :
		w3_div('|color:red"', '<h2>WARNING: Use for testing only!<br>Do not make customer SD cards with this yet.</h2>');

	s +=
		w3_div('',
			'EEPROM serial number: ',
			w3_div('id-ee-write w3-btn w3-round-large||onclick="mfg_ee_write()"')
		) +
		'<br>' +

		w3_col_percent('/w3-hcenter',
			w3_div('id-seq-override w3-btn w3-round-large w3-yellow||onclick="mfg_seq_override()"'), 40,
			'<input id="id-seq-input" class="w3-input w3-border w3-width-auto w3-hover-shadow w3-hidden" type="text" size=9 onchange="mfg_seq_set()">', 20,
			w3_div('id-power-off w3-btn w3-round-large|background-color:fuchsia||onclick="mfg_power_off()"'), 40
		) +
		'<hr>' +

		w3_third('', '',
			w3_div('id-sd-write w3-btn w3-round-large w3-aqua||onclick="mfg_sd_write()"'),

			w3_div('',
            w3_div('w3-progress-container w3-round-large w3-white w3-show-inline-block',
               w3_div('id-progress w3-progressbar w3-round-large w3-light-green w3-width-zero',
                  w3_div('id-progress-text w3-container w3-text-black')
               )
            ),
            
            w3_div('w3-margin-T-8',
               w3_div('id-progress-time w3-show-inline-block'),
               w3_div('id-progress-icon w3-show-inline-block w3-margin-left')
            )
         ),

			w3_div('id-sd-status class-sd-status')
		) +
		'<hr>' +

		w3_div('id-output-msg class-mfg-status w3-scroll-down w3-small');
	
	var el = w3_innerHTML('id-mfg',
	   w3_div('w3-container w3-section w3-dark-grey w3-half',
	      s
	   )
	);
	el.style.top = el.style.left = '10px';

	var el2 = w3_innerHTML('id-sd-write', 'click to write<br>micro-SD card');

	el2 = w3_innerHTML('id-power-off', 'click to halt<br>and power off');

	w3_show_block(el);
	
	//setTimeout(function() { setInterval(status_periodic, 5000); }, 1000);
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
				
				var el = w3_innerHTML('id-ee-write', button_text);
				w3_background_color(el, button_color);

				w3_innerHTML('id-seq-override', 'next serial number = '+ next_serno +'<br>click to override');
				el = w3_remove('id-seq-input', 'w3-visible');
				el.value = next_serno;
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
	w3_add('id-seq-input', 'w3-visible');
}

function mfg_seq_set()
{
	var next_serno = parseFloat(w3_el('id-seq-input').value).toFixed(0);
	if (next_serno >= 0 && next_serno <= 9999) {
		ext_send("SET set_serno="+ next_serno);
	}
}

var sd_progress, sd_progress_max = 4*60;		// measured estimate -- in secs (varies with SD card write speed)
var mfg_sd_interval;
var refresh_icon = '<i class="fa fa-refresh fa-spin" style="font-size:24px"></i>';

function mfg_sd_write()
{
	var el = w3_innerHTML('id-sd-write', 'writing the<br>micro-SD card...');
	w3_add(el, 'w3-override-yellow');

	el = w3_innerHTML('id-sd-status', 'writing micro-SD card...');
	w3_color(el, 'white');

	w3_el('id-progress-text').innerHTML = w3_el('id-progress').style.width = '0%';

	sd_progress = -1;
	mfg_sd_progress();
	mfg_sd_interval = setInterval(mfg_sd_progress, 1000);

	w3_innerHTML('id-progress-icon', refresh_icon);

	ext_send("SET microSD_write");
}

function mfg_sd_progress()
{
	sd_progress++;
	var pct = ((sd_progress / sd_progress_max) * 100).toFixed(0);
	if (pct <= 95) {	// stall updates until we actually finish in case SD is writing slowly
		w3_el('id-progress-text').innerHTML = w3_el('id-progress').style.width = pct +'%';
	}
	var secs = (sd_progress % 60).toFixed(0).leadingZeros(2);
	var mins = Math.floor(sd_progress / 60).toFixed(0);
	w3_innerHTML('id-progress-time', mins +':'+ secs);
}

function mfg_sd_write_done(err)
{
	var el = w3_innerHTML('id-sd-write', 'click to write<br>micro-SD card');
	w3_remove(el, 'w3-override-yellow');

	var msg = err? ('FAILED error '+ err.toString()) : 'WORKED';
	if (err == 1) msg += '<br>No SD card inserted?';
	if (err == 15) msg += '<br>rsync I/O error';
	var el = w3_innerHTML('id-sd-status', msg);
	w3_color(el, err? 'red':'lime');

	if (!err) {
		// force to max in case we never made it during updates
		w3_el('id-progress-text').innerHTML = w3_el('id-progress').style.width = '100%';
	}
	kiwi_clearInterval(mfg_sd_interval);
	w3_innerHTML('id-progress-icon', '');
}

function mfg_power_off()
{
	var el = w3_innerHTML('id-power-off', 'halting and<br>powering off...');
	w3_background_color(el, 'red');
	ext_send("SET mfg_power_off");
}
