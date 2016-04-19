function mfg_interface()
{
	visible_block('id-kiwi-container-1', true);
	
	var mfg = html("id-mfg");
	mfg.innerHTML =
		'<div><strong> Manufacturing interface </strong></div><br>' +
		'<div>' +
			'EEPROM serial number: <div id="id-ee-status" class="class-inline-block"></div>' +
			'<div id="id-ee-write" class="class-ee-button" onclick="mfg_ee_write()"></div>' +
		'</div><br>' +
		'<div>' +
			'<div id="id-seq-override" class="class-ee-button" onclick="mfg_seq_override()"' +
				'style="background-color:red">override serno<br>sequence</div>' +
			'<form id="id-seq-form" class="class-inline-block" name="form_freq" action="#" onsubmit="mfg_seq_set(); return false">' +
				'<input id="id-seq-input" type="text" size=9></form>' +
		'</div><br>' +
		'<div id="id-status-msg" class="class-inline-block"></div>' +
		'';
	mfg.style.top = mfg.style.left = '10px';
	visible_inline_block('id-ee-write', false);
	visible_inline_block('id-seq-form', false);
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
				var serno = param[1];
				console.log("MFG next_serno="+ next_serno + " serno="+ serno);
				
				var serno_text, button_text;
				if (serno < 0) {
					serno_text = 'invalid';
					button_text = 'write EEPROM with<br>next serno: '+ next_serno;
					write_button_vis = true;
				} else {
					serno_text = 'valid = '+ serno.toString();
					button_text = '';
					write_button_vis = false;
				}
				
				html('id-ee-status').innerHTML = serno_text;
				html('id-ee-write').innerHTML = button_text;
				visible_inline_block('id-ee-write', write_button_vis);
				visible_inline_block('id-seq-form', false);
				html('id-seq-input').value = next_serno;
				break;

			case "next_serno":
				next_serno = param[1];

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
