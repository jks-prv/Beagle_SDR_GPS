var s;

function gps_interface()
{
	ws_gps = open_websocket("GPS", timestamp, gps_recv);
	setTimeout(function() { setInterval(function() { ws_gps.send("SET keepalive") }, 5000) }, 5000);
	setTimeout(function() { setInterval(function() { s=""; ws_gps.send("SET update") }, 1000) }, 5000);
}

var gps_chans;

function gps_init()
{
	var i;
	var el = html("id-gps");
	s = '';

	s += '<div class="w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue">';
		s += '<table id="id-gps-ch" class="w3-table w3-striped"> </table>';
	s += '</div>';

	s += '<div class="w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue">';
		s += '<table id="id-gps-info" class="w3-table"> </table>';
	s += '</div>';

	el.innerHTML = s;
	visible_block('id-gps', true);
}

var FFTch, prn, snr, rssi, gain, hold, wdog, unlock, parity, sub, sub_renew, novfl;
var acq, track, good, fixes, run, ttff, gpstime, adc_clk, adc_corr;
var lat, lon, alt, map;

function gps_update_init(p)
{
	FFTch = p;
}

var SUBFRAMES = 5;
var max_rssi = 1;

var refresh_icon = '<i class="material-icons">refresh</i>';

var sub_colors = [ 'w3-red', 'w3-green', 'w3-blue', 'w3-yellow', 'w3-orange' ];

function gps_define_chan(ch)
{
	var i;
	if (rssi > max_rssi)
		max_rssi = rssi;

	s += '<tr>';
		s += '<td class="w3-right-align">'+ ch +'</td>';
		s += '<td>'+ ((ch == FFTch)? refresh_icon:'') +'</td>';
		s += '<td class="w3-right-align">'+ (prn? prn:'') +'</td>';
		s += '<td class="w3-right-align">'+ (snr? snr:'') +'</td>';
		s += '<td class="w3-right-align">'+ (rssi? gain:'') +'</td>';
		s += '<td class="w3-right-align">'+ (hold? hold:'') +'</td>';
		s += '<td class="w3-right-align">'+ (rssi? wdog:'') +'</td>';
		
		s += '<td>';
		s += '<span class="w3-tag '+ (unlock? 'w3-red':'w3-white') +'">U</span>';
		s += '<span class="w3-tag '+ (parity? 'w3-red':'w3-white') +'">P</span>';
		s += '</td>';

		s += '<td>';
		for (i = SUBFRAMES-1; i >= 0; i--) {
			var sub_color;
			if (sub_renew & (1<<i)) {
				sub_color = 'w3-grey';
			} else {
				sub_color = (sub & (1<<i))? sub_colors[i]:'w3-white';
			}
			s += '<span class="w3-tag '+ sub_color +'">'+ (i+1) +'</span>';
		}
		s += '</td>';

		s += '<td class="w3-right-align">'+ (novfl? novfl:'') +'</td>';
		
		s += '<td> <div class="w3-progress-container w3-round-xlarge w3-white">';
			var pct = ((rssi / max_rssi) * 100).toFixed(0);
			s += '<div class="w3-progressbar w3-round-xlarge w3-light-green" style="width:'+ pct +'%">';
				s += '<div class="w3-container w3-text-white">'+ rssi +'</div>';
			s += '</div>';
		s += '</div> </td>';
	s += '</tr>';
}

function gps_update(p)
{
	var el = html("id-gps-ch");
	var h =
		'<th>ch</th>'+
		'<th>acq</th>'+
		'<th>PRN</th>'+
		'<th>SNR</th>'+
		'<th>gain</th>'+
		'<th>hold</th>'+
		'<th>wdog</th>'+
		'<th class="w3-center">err</th>'+
		'<th class="w3-center">subframe</th>'+
		'<th>novfl</th>'+
		'<th style="width:50%">RSSI</th>'+
		'';
	el.innerHTML = h + s;

	el = html("id-gps-info");
	s =
		'<th>acq</th>'+
		'<th>tracking</th>'+
		'<th>good</th>'+
		'<th>fixes</th>'+
		'<th>run</th>'+
		'<th>TTFF</th>'+
		'<th>GPS time</th>'+
		'<th>ADC clock</th>'+
		'<th>lat</th>'+
		'<th>lon</th>'+
		'<th>alt</th>'+
		'<th>map</th>'+
		'<tr>'+
			'<td>'+ (acq? 'yes':'no') +'</td>'+
			'<td>'+ (track? track:'') +'</td>'+
			'<td>'+ (good? good:'') +'</td>'+
			'<td>'+ (fixes? fixes.toUnits():'') +'</td>'+
			'<td>'+ run +'</td>'+
			'<td>'+ ttff +'</td>'+
			'<td>'+ gpstime +'</td>'+
			'<td>'+ adc_clk.toFixed(6) +' ('+ adc_corr.toUnits() +') </td>'+
			'<td>'+ lat +'</td>'+
			'<td>'+ lon +'</td>'+
			'<td>'+ alt +'</td>'+
			'<td>'+ map +'</td>'+
		'</tr>'+
		'';
	el.innerHTML = s;
}

function gps_recv(data)
{
	var perr = '', next_serno = -1;
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		perr += ' '+params[i];
		param = params[i].split("=");
		var write_button_vis;
		
		switch (param[0]) {
			case "gps_chans":
				var gps_chans = parseFloat(param[1]);
				console.log("GPS gps_chans="+ gps_chans);
				gps_init();
				break;

			case "update_init":
				gps_update_init(parseFloat(param[1]));
				break;

			case "prn": prn = parseFloat(param[1]); break;
			case "snr": snr = parseFloat(param[1]); break;
			case "rssi": rssi = parseFloat(param[1]); break;
			case "gain": gain = parseFloat(param[1]); break;
			case "hold": hold = parseFloat(param[1]); break;
			case "wdog": wdog = parseFloat(param[1]); break;
			case "unlock": unlock = parseFloat(param[1]); break;
			case "parity": parity = parseFloat(param[1]); break;
			case "sub": sub = parseFloat(param[1]); break;
			case "sub_renew": sub_renew = parseFloat(param[1]); break;
			case "novfl": novfl = parseFloat(param[1]); break;

			case "define_chan":
				gps_define_chan(parseFloat(param[1]));
				break;

			case "acq": acq = parseFloat(param[1]); break;
			case "track": track = parseFloat(param[1]); break;
			case "good": good = parseFloat(param[1]); break;
			case "fixes": fixes = parseFloat(param[1]); break;
			case "run": run = param[1]; break;
			case "ttff": ttff = param[1]; break;
			case "gpstime": gpstime = decodeURIComponent(param[1]); break;
			case "adc_clk": adc_clk = parseFloat(param[1]); break;
			case "adc_corr": adc_corr = parseFloat(param[1]); break;
			case "lat": lat = decodeURIComponent(param[1]); break;
			case "lon": lon = decodeURIComponent(param[1]); break;
			case "alt": alt = decodeURIComponent(param[1]); break;
			case "map": map = decodeURIComponent(param[1]); break;

			case "update":
				gps_update(parseFloat(param[1]));
				break;

			default:
				perr += " ???";
				break;
		}
	}
	//console.log('GPS: '+perr);
}
