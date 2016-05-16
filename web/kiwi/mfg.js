sd_progress_max = 200;

function mfg_interface()
{
	var mfg = html("id-mfg");
	mfg.innerHTML =
		'<div><strong> Manufacturing interface </strong></div><br>' +
		'<div>' +
			'EEPROM serial number: <div id="id-ee-status" class="class-inline-block"></div>' +
			'<div id="id-ee-write" class="class-mfg-button" onclick="mfg_ee_write()"></div>' +
		'</div><br>' +
		'<div>' +
			'<div id="id-seq-override" class="class-mfg-button" onclick="mfg_seq_override()"' +
				'style="background-color:yellow"></div>' +
			'<form id="id-seq-form" class="class-inline-block" name="form_freq" action="#" onsubmit="mfg_seq_set(); return false">' +
				'<input id="id-seq-input" type="text" size=9></form>' +
		'</div><br>' +
		'<hr>' +
		'<div id="id-microsd" class="class-inline-block">' +
			'<div id="id-sd-write" class="class-mfg-button" style="background-color:cyan" onclick="mfg_sd_write()"></div>' +
			'<progress id="id-progress" value="0" max="'+ sd_progress_max +'"></progress>' +
		'</div>' +
		'<hr>' +
		'<div id="id-status-msg" class="class-mfg-status" data-scroll-down="true"></div>' +
		'';
	mfg.style.top = mfg.style.left = '10px';
	visible_inline_block('id-ee-write', false);
	visible_inline_block('id-seq-form', false);
	var el = html('id-sd-write');
	el.innerHTML = "click to write<br>micro-SD card";
	visible_block('id-mfg', true);
	
	ws_mfg = open_websocket("MFG", timestamp);
	setTimeout(function() { setInterval(function() { ws_mfg.send("SET keepalive") }, 5000) }, 5000);
	//setTimeout(function() { setInterval(update_TOD, 1000); }, 1000);
}

function mfg_recv(data)
{
	var s = '', next_serno = -1;
	var stringData = arrayBufferToString(data);
	params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		s += ' '+params[i];
		param = params[i].split("=");
		var write_button_vis;
		
		switch (param[0]) {
			case "serno":
				var serno = parseFloat(param[1]);
				console.log("MFG next_serno="+ next_serno + " serno="+ serno);
				
				var serno_text, button_text;
				if (serno <= 0) {		// write serno = 0 to force invalid for testing
					serno_text = '';
					button_text = 'invalid, click to write EEPROM<br>with next serno: '+ next_serno;
					button_color = 'red';
					write_button_vis = true;
				} else {
					serno_text = '';
					button_text = 'valid = '+ serno.toString() +'<br>click to change';
					button_color = 'lime';
					write_button_vis = true;
				}
				
				html('id-ee-status').innerHTML = serno_text;
				html('id-ee-write').innerHTML = button_text;
				html('id-ee-write').style.backgroundColor = button_color;
				visible_inline_block('id-ee-write', write_button_vis);

				html('id-seq-override').innerHTML = 'next serno = '+ next_serno +'<br>click to override';
				visible_inline_block('id-seq-form', false);
				html('id-seq-input').value = next_serno;
				break;

			case "next_serno":
				next_serno = parseFloat(param[1]);
				break;

			case "microSD_done":
				mfg_sd_write_done(parseFloat(param[1]));
				break;

			default:
				s += " ???";
				break;
		}
	}
	//console.log('MFG: '+s);
}

function mfg_ee_write()
{
	ws_mfg.send("SET write");
}

function mfg_seq_override()
{
	visible_inline_block('id-seq-form', true);
}

function mfg_seq_set()
{
	var next_serno = parseFloat(html('id-seq-input').value).toFixed(0);
	if (next_serno >= 0 && next_serno <= 9999) {
		ws_mfg.send("SET set_serno="+ next_serno);
	}
}

var mfg_sd_interval;

function mfg_sd_write()
{
	var el = html('id-sd-write');
	el.style.backgroundColor = 'yellow';
	el.innerHTML = "writing micro-SD card...";
	html('id-progress').value = 0;
	mfg_sd_interval = setInterval(function() {
		var v = html('id-progress').value + 1;
		if (v < sd_progress_max)	// stall updates until we actually finish
			html('id-progress').value = v;
	}, 1000);
	ws_mfg.send("SET microSD_write");
}

function mfg_sd_write_done(err)
{
	var el = html('id-sd-write');
	el.innerHTML = "click to write<br>micro-SD card";
	var color = err? 'red':'lime';
	el.style.backgroundColor = color;
	html('id-progress').value = sd_progress_max;
	kiwi_clearInterval(mfg_sd_interval);
}
