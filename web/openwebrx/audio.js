/*

OpenWebRX (c) Copyright 2013-2014 Andras Retzler <randras@sdr.hu>

This file is part of OpenWebRX.

    OpenWebRX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenWebRX is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenWebRX. If not, see <http://www.gnu.org/licenses/>.

*/

// Copyright (c) 2015 - 2016 John Seamons, ZL/KF6VO

// Note: we don't support older browsers using the Mozilla audio API since it is now depreciated
// see https://wiki.mozilla.org/Audio_Data_API

// Optimise these if audio lags or is choppy:
var audio_better_delay = 4;		// can override with URL 'audio=' parameter
var audio_buffer_size = 8192;		// 2048 was choppy
var audio_buffer_min_length_sec = 1.7/2; // actual number of samples are calculated from sample rate
var audio_buffer_max_length_sec = 3.4; // actual number of samples are calculated from sample rate
var audio_flush_interval_ms = 1000; // the interval in which audio_flush() is called

var audio_stat_input_size = 0;
var audio_stat_total_input_size = 0;
var audio_buffer_current_size = 0;

var audio_context;
var audio_started = false;

var audio_data;
var audio_received = Array();
var audio_buffer_index = 0;
var audio_source, audio_watchdog;
var audio_output_rate = 0;
var audio_input_rate;
var audio_compression = false;

var audio_prepared_buffers = Array();
var audio_prepared_seq = Array();
var audio_prepared_flags = Array();
var audio_last_output_buffer;
var audio_last_output_offset = 0;
var audio_buffering = false;
var audio_silence_buffer;

var audio_resample_ratio = 1;

var audio_interpolation;
var audio_decimation;
var audio_transition_bw;

var audio_convolver_running = false;
var audio_convolver, audio_lpf_buffer, audio_lpf;

var resample_init1 = false;
var resample_init2 = false;
var resample_input_buffer = [];
var resample_input_available = 0;
var resample_input_processed = 0;
var resample_last_taps_delay = 0;
var resample_output_buffer = [];
var resample_output_size;
var resample_taps = [];
var resample_taps_length;
var resample_last = 0;

var resample_new = true;
var resample_old = !resample_new;
var resample_old_lpf_taps_length = 255;

var comp_off_div = 1;
var comp_lpf_freq = 0;
var comp_lpf_taps = [];
var comp_lpf_taps_length = 255;

function audio_init()
{
	if (audio_better_delay == 4) {
		audio_buffer_size = 8192;
		audio_buffer_min_length_sec = 1.7/2;
	} else
	
	if (audio_better_delay == 3) {
		audio_buffer_size = 2048;
		audio_buffer_min_length_sec = 0;
	} else
	
	if (audio_better_delay == 2) {
		audio_buffer_size = 4096;
		audio_buffer_min_length_sec = 0;
	} else
	
	if (audio_better_delay == 1) {
		audio_buffer_size = 8192;
		audio_buffer_min_length_sec = 0;
	}
	
	audio_data = new Int16Array(audio_buffer_size);
	audio_last_output_buffer = new Float32Array(audio_buffer_size)
	audio_silence_buffer = new Float32Array(audio_buffer_size);
	console.log('audio_buffer_size='+ audio_buffer_size +' audio_buffer_min_length_sec='+ audio_buffer_min_length_sec);
	
	setTimeout(function() { setInterval(audio_stats, 1000) }, 1000);

	//https://github.com/0xfe/experiments/blob/master/www/tone/js/sinewave.js
	try {
		window.AudioContext = window.AudioContext || window.webkitAudioContext;
		audio_context = new AudioContext();
		audio_context.sampleRate = 44100;		// attempt to force a lower rate
		audio_output_rate = audio_context.sampleRate;		// see what rate we're actually getting
	} catch(e) {
		kiwi_serious_error("Your browser does not support Web Audio API, which is required for OpenWebRX to run. Please use an HTML5 compatible browser.");
		audio_context = null;
		audio_output_rate = 0;
		return true;
	}

	return false;
}

// audio_source.onaudioprocess() -> audio_convolver -> audio_context.destination
//                                                  -> audio_watchdog.audio_watchdog_process()
//
// called only once when enough network input has been received
function audio_start()
{
	if (audio_context == null) return;

	ws_aud_send("SET dbgAudioStart=1");
	audio_connect(0);
	ws_aud_send("SET dbgAudioStart=2");
	window.setInterval(audio_flush, audio_flush_interval_ms);
	//divlog('Web Audio API succesfully initialized, sample rate: '+audio_output_rate.toString()+ " sps");

	ws_aud_send("SET dbgAudioStart=3 aor="+audio_output_rate);
	try {
		demodulator_analog_replace(init_mode);		//needs audio_output_rate to exist
	} catch(ex) {
		ws_aud_send("SET x-DEBUG: audio_start.demodulator_analog_replace: catch: "+ ex.toString());

		// message too big -- causes server crash
		//ws_aud_send("SET x-DEBUG: audio_start.demodulator_analog_replace: catch: "+ ex.stack);
	}
	ws_aud_send("SET dbgAudioStart=4");
	
	audio_started = true;
}

var audio_reconnect = 0;

function audio_connect(reconnect)
{
	if (audio_context == null) return;
	
	if (reconnect) {
		audio_source.disconnect(); audio_source.onaudioprocess = null;
		if (audio_convolver_running)
			audio_convolver.disconnect();
		if (audio_watchdog) {
			audio_watchdog.disconnect(); audio_watchdog.onaudioprocess = null;
		}
		audio_reconnect++;
	}
	
	audio_stat_output_epoch = -1;
	audio_source = audio_context.createScriptProcessor(audio_buffer_size, 0, 1);		// in=0, out=1
	audio_source.onaudioprocess = audio_onprocess;
	
	if (audio_convolver_running) {
		audio_source.connect(audio_convolver);
		audio_convolver.connect(audio_context.destination);
	} else {
		audio_source.connect(audio_context.destination);
	}
	
	// workaround for Firefox problem where audio goes silent after a while (bug seems less frequent now?)
	if (kiwi_isFirefox()) {
		audio_watchdog = audio_context.createScriptProcessor(audio_buffer_size, 1, 0);	// in=1, out=0
		audio_watchdog.onaudioprocess = audio_watchdog_process;
		
		// send audio to audio_watchdog as well
		if (audio_convolver_running)
			audio_convolver.connect(audio_watchdog);
		else
			audio_source.connect(audio_watchdog);
	}
}

var audio_silence_count = 0, audio_restart_count = 0;

// NB never put any console.log()s in here
function audio_watchdog_process(ev)
{
	if (muted || audio_buffering) {
		audio_silence_count = 0;
		return;
	}
	
	var data = ev.inputBuffer.getChannelData(0);
	var silent = (data[0] == 0);
	audio_silence_count = silent? audio_silence_count+1 : 0;

	if (audio_silence_count > 16) {
		audio_connect(1);
		audio_silence_count = 0;
		audio_restart_count++;
	}
}

var audio_change_LPF_delayed = false;
var audio_stat_output_epoch;
var audio_stat_output_bufs;
var audio_underrun_errors = 0;
var audio_overrun_errors = 0;

var audio_sequence = 0;

// NB never put any console.log()s in here
function audio_onprocess(ev)
{
	if (audio_stat_output_epoch == -1) {
		audio_stat_output_epoch = (new Date()).getTime();
		audio_stat_output_bufs = 0;
	}

	/*
	// simulate Firefox "goes silent" problem
	if (dbgUs && kiwi_isFirefox() && audio_stat_output_bufs >= 100) {
		ev.outputBuffer.copyToChannel(audio_silence_buffer,0);
		return;
	}
	*/

	if (audio_prepared_buffers.length == 0) {
		if (!inactivity_timeout_msg) {
			audio_underrun_errors++;
		}
		audio_buffering = true;
	}

	if (audio_buffering) {
		audio_need_stats_reset = true;
		ev.outputBuffer.copyToChannel(audio_silence_buffer,0);
		return;
	}

	ev.outputBuffer.copyToChannel(audio_prepared_buffers.shift(),0);
	audio_stat_output_bufs++;
	
	audio_sequence = audio_prepared_seq.shift();

	if (audio_change_LPF_delayed) {
		audio_recompute_LPF();
		audio_change_LPF_delayed = false;
	}
	
	var flags_smeter = audio_prepared_flags.shift();
	sMeter_dBm_biased = (flags_smeter & 0xfff) / 10;

	if (flags_smeter & audio_flags.AUD_FLAG_LPF) {
		audio_change_LPF_delayed = true;
	}
}

var audio_last_inderruns = 0;
var audio_last_reconnect = 0;
var audio_last_restart_count = 0;

function audio_flush()
{
	var flushed = false;
	
	while (audio_prepared_buffers.length * audio_buffer_size > audio_buffer_max_length_sec * audio_output_rate) {
		flushed = true;
		audio_prepared_buffers.shift();
		audio_prepared_seq.shift();
		audio_prepared_flags.shift();
	}
	
	if (flushed) {
		add_problem("audio overrun");
		audio_overrun_errors++;
	}
	
	if (audio_last_inderruns != audio_underrun_errors) {
			add_problem("audio underrun");
			ws_aud_send("SET underrun="+ audio_underrun_errors);
		audio_last_inderruns = audio_underrun_errors;
	}
	
	/*
	if (dbgUs && kiwi_isFirefox()) {
		if (audio_last_reconnect != audio_reconnect) {
			console.log('FF audio_reconnect='+ audio_reconnect);
			audio_last_reconnect = audio_reconnect;
		}
		
		if (audio_last_restart_count != audio_restart_count) {
			console.log('FF restart_count='+ audio_restart_count);
			audio_last_restart_count = audio_restart_count;
		}

		var s = 'ob='+ audio_stat_output_bufs +' ab='+ audio_buffering +' len='+ audio_prepared_buffers.length +
			' recon='+ audio_reconnect +' resta='+ audio_restart_count + ' und='+ audio_underrun_errors + ' ovr='+ audio_overrun_errors;
		ws_aud_send('SET FF-0 '+ s);
	}
	*/
}

function audio_rate(input_rate)
{
	audio_input_rate = input_rate;

	if (audio_input_rate == 0) {
		divlog("browser doesn\'t support WebAudio", 1);
		ws_aud_send("SET zero audio_input_rate?");
	} else
	if (audio_output_rate == 0) {
		divlog("browser doesn\'t support WebAudio", 1);
		ws_aud_send("SET no WebAudio");
	} else {
		if (resample_old) {
			audio_interpolation = audio_output_rate / audio_input_rate;		// needed by rational_resampler_get_lowpass_f()
			audio_decimation = 1;
		} else {
			audio_interpolation = 0;
			if (input_rate == 8250) {
				if (audio_output_rate == 44100) {
					audio_interpolation = 294;		// 8250 -> 44.1k
					audio_decimation = 55;
				} else
				if (audio_output_rate == 48000) {
					audio_interpolation = 64;		// 8250 -> 48k
					audio_decimation = 11;
				} else
				if (audio_output_rate == 22500) {
					audio_interpolation = 147;		// 8250 -> 22.5k
					audio_decimation = 55;
				} else
				if (audio_output_rate == 24000) {
					audio_interpolation = 32;		// 8250 -> 24k
					audio_decimation = 11;
				} else
				if (audio_output_rate == 96000) {
					audio_interpolation = 128;		// 8250 -> 96k
					audio_decimation = 11;
				} else
				if (audio_output_rate == 32000) {
					audio_interpolation = 128;		// 8250 -> 32k
					audio_decimation = 33;
				} else
				if (audio_output_rate == 64000) {
					audio_interpolation = 256;		// 8250 -> 64k
					audio_decimation = 33;
				} else
				if (audio_output_rate == 16000) {
					audio_interpolation = 64;		// 8250 -> 16k
					audio_decimation = 33;
				} else
				if (audio_output_rate == 192000) {
					audio_interpolation = 256;		// 8250 -> 192k
					audio_decimation = 11;
				}
			}
			
			// An unknown input or output rate.
			// Try to find common denominators by brute force.
			if (audio_interpolation == 0) {
				var interp, i_decim;
				for (interp = 2; interp <= 1024; interp++) {
					var decim = (input_rate * interp) / audio_output_rate;
					i_decim = Math.floor(decim);
					var frac = Math.abs(decim - i_decim);
					if (frac < 0.00001) break;
				}
				if (interp > 1024) {
					//divlog("unsupported audio output rate: "+audio_output_rate, 1);
					ws_aud_send("SET UAR in="+input_rate+" out="+audio_output_rate);
					kiwi_serious_error("Your system uses an audio output rate of "+audio_output_rate+" sps which we do not support.");
				} else {
					audio_interpolation = interp;
					audio_decimation = i_decim;
					//console.log("brute force calc: aor="+audio_output_rate+" interp="+interp+" decim="+i_decim);
				}
			}
		}
	}

	if (audio_interpolation != 0) {
		audio_transition_bw = 0.001;
		audio_resample_ratio = audio_output_rate / audio_input_rate;
		ws_aud_send("SET AR OK in="+ input_rate +" out="+ audio_output_rate);
		//divlog("Network audio rate: "+audio_input_rate.toString()+" sps");
	}
}

var audio_adpcm = { index:0, previousValue:0 };
var audio_flags = { AUD_FLAG_SMETER: 0x0000, AUD_FLAG_LPF: 0x1000 };
var audio_need_stats_reset = true;

function audio_recv(data)
{
	var u32View = new Uint32Array(data, 4, 1);
	var seq = u32View[0];
	
	var fs8 = new Uint8Array(data, 8, 2);
	var flags_smeter = (fs8[0] << 8) | fs8[1];
	
	var ad8 = new Uint8Array(data, 10);
	var i, bytes = ad8.length, samps;
	
	if (!audio_compression) {
		for (i=0; i<bytes; i+=2) {
			audio_data[i/2] = (ad8[i]<<8) | ad8[i+1];		// convert from network byte-order
		}
		samps = bytes/2;		// i.e. 1024 8b bytes -> 512 16b samps, 1KB -> 1KB, 1:1 no compression
	} else {
		decode_ima_adpcm_u8_i16(ad8, audio_data, bytes, audio_adpcm);
		samps = bytes*2;		// i.e. 1024 8b bytes -> 2048 16b samps, 1KB -> 4KB, 4:1 over uncompressed
	}
	
	audio_prepare(audio_data, samps, seq, flags_smeter);
	var enough_buffered = audio_prepared_buffers.length * audio_buffer_size > audio_buffer_min_length_sec * audio_output_rate;

	if (!audio_started && enough_buffered) {
		audio_start();
	}

	if (audio_need_stats_reset)
		audio_stats_reset();

	audio_stat_input_size += samps;
	audio_stat_total_input_size += samps;
}

function audio_prepare(data, data_len, seq, flags_smeter)
{
	var resample_decomp = resample_new && audio_compression && comp_lpf_taps_length;

	//console.log("audio_prepare :: "+data_len.toString());
	//console.log("data.len = "+data_len.toString());

	var dopush = function()
	{
		audio_prepared_buffers.push(audio_last_output_buffer);
		audio_prepared_seq.push(seq);

		// delay changing LPF until point at which buffered audio changed
		var fs = flags_smeter & 0xfff;
		if (resample_decomp && (flags_smeter & audio_flags.AUD_FLAG_LPF)) fs |= audio_flags.AUD_FLAG_LPF;
		audio_prepared_flags.push(fs);
		audio_last_output_offset = 0;
		audio_last_output_buffer = new Float32Array(audio_buffer_size);
	};

	var copy = function(d, di, s, si, len)
	{
		var i;
		for (i=0; i<len; i++) d[di+i] = s[si+i];
	};
	
	var idata, idata_length;
	if (data_len == 0) return;
	
	// --- Resampling ---
	
	if (audio_resample_ratio != 1) {
	
		// Need a convolver-based LPF in two cases:
		//		1) filter high-frequency artifacts from using decompression with new resampler
		//			(built-in LPF of resampler without compression is sufficient)
		//		2) filter high-frequency artifacts from using old resampler
		// Use the firdes_lowpass_f() routine of the new sampler code to construct the filter for the convolver
	
		// initialization
		if (!resample_init1) {
			resample_taps_length = resample_new? Math.round(4.0/audio_transition_bw) : resample_old_lpf_taps_length;
			if (resample_taps_length%2 == 0) resample_taps_length++;	// number of symmetric FIR filter taps should be odd
			rational_resampler_get_lowpass_f(resample_taps, resample_taps_length, audio_interpolation, audio_decimation);
			//console.log("audio_resample_ratio "+audio_resample_ratio+" resample_taps_length "+resample_taps_length+" osize "+data_len+'/'+Math.round(data_len * audio_resample_ratio));
			//var middle=Math.floor(resample_taps_length/2); for(var i=middle; i<=middle+64; i++) console.log("tap"+i+": "+resample_taps[i]);

			resample_init1 = true;
		}

		if (!resample_init2 && (resample_decomp || resample_old) && (audio_source != undefined)) {
			var lpf_taps, lpf_taps_length;
			
			if (resample_decomp) {
				audio_recompute_LPF();
				lpf_taps = comp_lpf_taps;
				lpf_taps_length = comp_lpf_taps_length;
			} else {
				lpf_taps = resample_taps;
				lpf_taps_length = resample_taps_length;
			}

			console.log('COMP *** installing a convolver');
			audio_convolver = audio_context.createConvolver();
			audio_convolver.normalize = false;
			
			audio_lpf_buffer = audio_context.createBuffer(1, lpf_taps_length, audio_output_rate);
			audio_lpf = audio_lpf_buffer.getChannelData(0);
			audio_lpf.set(lpf_taps);
			audio_convolver.buffer = audio_lpf_buffer;
			
			// splice the convolver in-between the audio source and destination
			audio_source.disconnect();
			audio_source.connect(audio_convolver);
			audio_convolver.connect(audio_context.destination);
			
			if (kiwi_isFirefox()) {
				//audio_source.connect(audio_watchdog);
				audio_convolver.connect(audio_watchdog);
			}
			
			audio_convolver_running = true;

			resample_init2 = true;
		}

		if (resample_old) {
		
			// our traditional linear interpolator
			resample_output_size = Math.round(data_len * audio_resample_ratio);
			var incr = 1.0 / audio_resample_ratio;
			var di = 0;
			var frac = 0;
			var xc;
			for (i=0; i<resample_output_size; i++) {
			
				// new = cur*frac + last*(1-frac)  [0 <= frac <= 1]  i.e. incr = old/new
				// new = cur*frac + last - last*frac
				// new = (cur-last)*frac + last  [only one multiply]
				//assert(di < data_len);
				xc = data[di];
				var xl = resample_last;
				resample_output_buffer[i] = (xc-xl)*frac + xl;
				frac += incr;
				if (frac >= 1) {
					frac -= 1;
					resample_last = xc;
					di++;
				}
			}
			resample_last = xc;
		} else {
			if (resample_input_processed != 0) {
				var new_available = resample_input_available - resample_input_processed;
				copy(resample_input_buffer, 0, resample_input_buffer, resample_input_processed, new_available);
				resample_input_available = new_available;
				resample_input_processed = 0;
			}
			copy(resample_input_buffer, resample_input_available, data, 0, data_len);
			resample_input_available += data_len;
			rational_resampler_ff(resample_input_buffer, resample_output_buffer, resample_input_available, audio_interpolation, audio_decimation, resample_taps, resample_taps_length, resample_last_taps_delay);
		}
		
		idata = resample_output_buffer;
		idata_length = resample_output_size;
	} else {
		idata = data;
		idata_length = data_len;
	}
	
	//console.log("idata_length "+idata_length);
	if (audio_last_output_offset+idata_length <= audio_buffer_size)
	{	//array fits into output buffer
		for (var i=0; i<idata_length; i++) audio_last_output_buffer[i+audio_last_output_offset]=idata[i]/32768*f_volume;
		audio_last_output_offset+=idata_length;
		//console.log("fits into; offset="+audio_last_output_offset.toString());
		if (audio_last_output_offset == audio_buffer_size) dopush();
	}
	else
	{	//array is larger than the remaining space in the output buffer
		var copied = audio_buffer_size-audio_last_output_offset;
		var remain = idata_length-copied;
		for (var i=0;i<audio_buffer_size-audio_last_output_offset;i++) //fill the remaining space in the output buffer
			audio_last_output_buffer[i+audio_last_output_offset] = idata[i]/32768*f_volume;
		dopush();	//push the output buffer and create a new one

		//console.log("larger than; copied half: "+copied.toString()+", now at: "+audio_last_output_offset.toString());
		do {
			var i;
			for (i=0; i<remain; i++) { //copy the remaining input samples to the new output buffer
				if (i == audio_buffer_size) {
					dopush();
					break;
				}
				audio_last_output_buffer[i] = idata[i+copied]/32768*f_volume;
			}
			remain -= i;
			copied += i;
		} while (remain);
		audio_last_output_offset+=i;
		//console.log("larger than; remained: "+remain.toString()+", now at: "+audio_last_output_offset.toString());
	}
	
	var enough_buffered = audio_prepared_buffers.length * audio_buffer_size > audio_buffer_min_length_sec * audio_output_rate;

	if (audio_buffering && enough_buffered) {
		audio_buffering = false;
	}
}


try {
	if (!AudioBuffer.prototype.copyToChannel) { // Chrome 36 does not have it, Firefox does
		AudioBuffer.prototype.copyToChannel = function(input,channel) //input is Float32Array
			{
				var cd=this.getChannelData(channel);
				for(var i=0;i<input.length;i++) cd[i]=input[i];
			}
	}
} catch(ex) { console.log("CATCH: AudioBuffer.prototype.copyToChannel"); }

var audio_stat_input_epoch = -1;
var audio_stat_last_time;

function audio_stats_reset()
{
	audio_stat_input_epoch = (new Date()).getTime();
	audio_stat_last_time = audio_stat_input_epoch;

	audio_stat_input_size = audio_stat_total_input_size = 0;
	audio_need_stats_reset = false;
}

function audio_stats()
{
	if (audio_stat_input_epoch == -1 || audio_stat_output_epoch == -1)
		return;

	var time_now = (new Date()).getTime();
	var secs_since_last_call = (time_now - audio_stat_last_time)/1000;
	var secs_since_reset = (time_now - audio_stat_input_epoch)/1000;
	var secs_since_first_output = (time_now - audio_stat_output_epoch)/1000;
	audio_stat_last_time = time_now;

	var net_sps = audio_stat_input_size / secs_since_last_call;
	var net_avg = audio_stat_total_input_size / secs_since_reset;
	var out_sps = (audio_stat_output_bufs * audio_buffer_size) / secs_since_first_output;

	html("id-msg-audio").innerHTML = "Audio: network "+
		net_sps.toFixed(0) +" sps ("+
		net_avg.toFixed(0) +" avg), output "+
		out_sps.toFixed(0) +" sps";

	html("id-msg-audio").innerHTML += ', Qlen '+audio_prepared_buffers.length;

	if (audio_underrun_errors)
		html("id-msg-audio").innerHTML += ', underruns '+audio_underrun_errors.toString();

	if (audio_restart_count)
		html("id-msg-audio").innerHTML += ', restart '+audio_restart_count.toString();

	audio_stat_input_size = 0;
}

// FIXME
// To eliminate the clicking when switching filter buffers, consider fading between new & old convolvers.

function audio_recompute_LPF()
{
	var lpf_freq = 4000;
	if (typeof demodulators[0] != "undefined") {
		var hcut = Math.abs(demodulators[0].high_cut);
		var lcut = Math.abs(demodulators[0].low_cut);
		lpf_freq = Math.max(hcut, lcut);
	}
	
	if (lpf_freq != comp_lpf_freq) {
		var cutoff = lpf_freq / audio_output_rate;
		//console.log('COMP_LPF cutoff='+ lpf_freq +'/'+ cutoff.toFixed(3) +'/'+ audio_output_rate +' ctaps='+ comp_lpf_taps_length +' cdiv='+ comp_off_div);
		firdes_lowpass_f(comp_lpf_taps, comp_lpf_taps_length, cutoff/comp_off_div);
		comp_lpf_freq = lpf_freq;

	/*
	var sum=0, max=0;;
	var i;
	for (i=0; i<comp_lpf_taps_length; i++) {
		var t = comp_lpf_taps[i];
		sum += t;
		if (t > max) max = t;
	}
	//console.log("COMP_LPF sum="+ sum +" max="+ max);
	*/

		// reload buffer if convolver already running
		if (audio_convolver_running) {
			//console.log("COMP_LPF reload convolver");
			audio_lpf_buffer = audio_context.createBuffer(1, comp_lpf_taps_length, audio_output_rate);
			audio_lpf = audio_lpf_buffer.getChannelData(0);
			audio_lpf.set(comp_lpf_taps);
			audio_convolver.buffer = audio_lpf_buffer;
		}
	}
}

function rational_resampler_ff(input, output, input_size, interpolation, decimation, taps, taps_length, last_taps_delay)
{
	//Theory: http://www.dspguru.com/dsp/faqs/multirate/resampling
	//oi: output index, i: tap index
	var output_size = Math.round(input_size*interpolation/decimation);
	var i, oi;
	var startingi, delayi, last_delayi = -1;

	for (oi=0; oi<output_size; oi++) //@rational_resampler_ff (outer loop)
	{
		startingi = Math.floor((oi*decimation+interpolation-1-last_taps_delay)/interpolation); //index of first input item to apply FIR on
		delayi = Math.floor((last_taps_delay+startingi*interpolation-oi*decimation)%interpolation); //delay on FIR taps

		if (startingi+taps_length/interpolation+1 > input_size) break; //we can't compute the FIR filter to some input samples at the end

		var end;
		if (0) {
			if (delayi != last_delayi) {
				end = Math.floor((taps_length-delayi)/interpolation);
				last_delayi = delayi;
			}
		} else {
			end = Math.floor((taps_length-delayi)/interpolation);
		}

		var acc=0;
		for(i=0; i<end; i++)	//@rational_resampler_ff (inner loop)
		{
			acc+=input[startingi+i]*taps[delayi+i*interpolation];
		}
		output[oi]=acc*interpolation;
	}

	resample_input_processed=startingi;
	resample_output_size=oi;
	resample_last_taps_delay=delayi;
}

function rational_resampler_get_lowpass_f(output, output_size, interpolation, decimation)
{
	// See 4.1.6 at: http://www.dspguru.com/dsp/faqs/multirate/resampling
	var cutoff_for_interpolation = 1.0/interpolation;
	var cutoff_for_decimation = 1.0/decimation;
	var cutoff = (cutoff_for_interpolation < cutoff_for_decimation)? cutoff_for_interpolation : cutoff_for_decimation; //get the lower
	firdes_lowpass_f(output, output_size, cutoff/2);
}

function firdes_lowpass_f(output, length, cutoff_rate)
{
	//Generates symmetric windowed sinc FIR filter real taps
	//	length should be odd
	//	cutoff_rate is (cutoff frequency/sampling frequency)
	//Explanation at Chapter 16 of dspguide.com
	var i;
	var middle = Math.floor(length/2);
	output[middle] = 2*Math.PI*cutoff_rate * firdes_wkernel_hamming(0);
	for (i=1; i<=middle; i++) // calculate taps
	{
		output[middle-i] = output[middle+i] = (Math.sin(2*Math.PI*cutoff_rate*i)/i) * firdes_wkernel_hamming(i/middle);
		//printf("%g %d %d %d %d | %g\n",output[middle-i],i,middle,middle+i,middle-i,sin(2*Math.PI*cutoff_rate*i));
	}
	
	//Normalize filter kernel
	var sum=0;
	for (i=0; i<length; i++) // normalize pass 1
	{
		sum += output[i];
	}
	for (i=0; i<length; i++) // normalize pass 2
	{
		output[i] /= sum;
	}
}

function firdes_wkernel_hamming(rate)
{
	//Explanation at Chapter 16 of dspguide.com, page 2
	//Hamming window has worse stopband attentuation and passband ripple than Blackman, but it has faster rolloff.
	rate=0.5+rate/2;
	return 0.54-0.46*Math.cos(2*Math.PI*rate);
}
