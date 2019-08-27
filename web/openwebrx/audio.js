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

// constants
var audio_periodic_interval_ms = 1000; // the interval in which audio_periodic() is called

var audio_flags = {
   SND_FLAG_LPF:        0x01,
   SND_FLAG_ADC_OVFL:   0x02,
   SND_FLAG_NEW_FREQ:   0x04,
   SND_FLAG_MODE_IQ:    0x08,
   SND_FLAG_COMPRESSED: 0x10,
   SND_FLAG_RESTART:    0x20,
   SND_FLAG_MASKED:     0x40
};

// init only once
var audio_ext_sequence = 0;
var audio_meas_dly_ena = 0;
var audio_initial_connect = false;
var audio_watchdog_restart = false;

// stats
var audio_stat_input_size = 0;
var audio_stat_total_input_size = 0;
var audio_reconnect = 0;
var audio_silence_count = 0;
var audio_restart_count = 0;
var audio_stat_output_bufs;
var audio_underrun_errors = 0;
var audio_overrun_errors = 0;
var audio_last_underruns = 0;
var audio_last_reconnect = 0;
var audio_last_restart_count = 0;
var audio_stat_last_time;

// set in audio_init()
var audio_running;
var audio_started;
var audio_last_output_offset;
var audio_mode_iq;
var audio_compression;
var audio_stat_input_epoch;
var audio_prepared_buffers;
var audio_prepared_buffers2;
var audio_prepared_seq;
var audio_prepared_flags;
var audio_prepared_smeter;
var audio_buffering;
var audio_convolver_running;
var audio_meas_dly;
var audio_meas_dly_start;
var resample_new_default = false;
var resample_new;
var resample_old;
var resample_init1;
var resample_init2;
var resample_input_buffer;
var resample_input_available;
var resample_input_processed;
var resample_last_taps_delay;
var resample_output_buffer;
var resample_output_buffer2;
var resample_output_size;
var resample_taps;
var resample_taps_length;
var resample_last;
var resample_last2;
var audio_adpcm = { index:0, previousValue:0 };
var audio_ext_adc_ovfl;
var audio_need_stats_reset;
var audio_change_LPF_latch;
var audio_change_freq_latch;
var audio_panner = null;
var audio_gain;

// LPF tap info for convolver when compression used
var comp_lpf_freq;
var comp_lpf_taps;
var comp_lpf_taps_length;

var audio_buffer_size;
var audio_buffer_min_length_sec; // actual number of samples are calculated from sample rate
var audio_buffer_max_length_sec; // actual number of samples are calculated from sample rate
var audio_data;
var audio_last_output_buffer, audio_last_output_buffer2;
var audio_silence_buffer;
var audio_stats_interval;
var audio_context;
var audio_output_rate;
var audio_last_is_local, audio_last_compression;

// set in audio_rate()
var audio_input_rate;
var audio_interpolation;
var audio_decimation;
var audio_resample_ratio;
var audio_transition_bw;
var audio_min_nbuf;
var audio_max_nbuf;

// set in audio_connect()
var audio_stat_output_epoch;
var audio_channels;
var audio_source;
var audio_watchdog;
var audio_firefox_watchdog = 0;
var audio_change_LPF_delayed;

// set in audio_disconnect()
var audio_disconnected;

// set in audio_recv()
var audio_masked;

// set in audio_prepare()
var audio_convolver;


/*
   init:
      rate()
      started = buffering = false
   rate:
      min_nbuf = , max_nbuf =
   start:
      started = true
      connect()
      setup periodic()
   disconnect:
      ...
   connect:
      if (reconnect) disconnect(), buffering = true
      setup onprocess()
   recv:
      if (->IQ || change comp) connect(reconnect=1)
      prepare()
      if (!started && prepared.len > min_nbuf) start()
   prepare:
      if () splice()
      resample()
      if () dopush: prepared.push()
      if (buffering && prepared.len > min_nbuf) buffering = false;
   onprocess:
      if (started && prepared.len == 0)
         underruns++
         buffering = true
      if (!started || buffering)
         play(silence)
      else
         play(prepared.shift())
   periodic:
      if (prepared.len > max_nbuf) trim(prepared)
      if (overran) overrun()
      if (underruns) underrun()

                  is called by:
audio_init()      when sound thread starts, rx_sound.cpp:c2s_sound_setup()
audio_rate()      right after above, rx_sound.cpp:c2s_sound_setup()
audio_recv()      web socket data received
audio_start()     audio_recv() when enough initially buffered
audio_connect()   audio_start() initially
                  audio_watchdog_process() to recover
                  audio_recv() IQ/comp change with new buf queues, resamp, comp etc.
audio_prepare()   audio_recv()

*/

function audio_init(is_local, less_buffering, compression)
{
   audio_running = false;
   
   console.log('--------------------------');
   //console.log('AUDIO audio_init CALLED is_local='+ is_local +' less_buffering='+ less_buffering +' compression='+ compression);

   less_buffering = false;    // DEPRECATED
   
   //console.log('AUDIO audio_init LAST audio_last_is_local='+ audio_last_is_local +' audio_last_compression='+ audio_last_compression);
   if (is_local == null) is_local = audio_last_is_local;
   audio_last_is_local = is_local;
   if (compression == null) compression = audio_last_compression;
   audio_last_compression = compression;

   console.log('AUDIO audio_init FINAL is_local='+ is_local +' less_buffering='+ less_buffering +' compression='+ compression);

   if (audio_source != undefined) {
      //console.log('AUDIO audio_init audio_disconnect');
      audio_disconnect();
   }

   // reset globals
   audio_started = false;
   audio_last_output_offset = 0;
   audio_mode_iq = false;
   audio_compression = compression? true:false;
   audio_stat_input_epoch = -1;
   audio_prepared_buffers = Array();
   audio_prepared_buffers2 = Array();
   audio_prepared_seq = Array();
   audio_prepared_flags = Array();
   audio_prepared_smeter = Array();
   audio_buffering = false;
   audio_convolver_running = false;
   audio_meas_dly = 0;
   audio_meas_dly_start = 0;
   resample_new = kiwi_isMobile()? false : resample_new_default;
   resample_old = !resample_new;
   resample_init1 = false;
   resample_init2 = false;
   resample_input_buffer = [];
   resample_input_available = 0;
   resample_input_processed = 0;
   resample_last_taps_delay = 0;
   resample_output_buffer = [];
   resample_output_buffer2 = [];
   resample_taps = [];
   resample_last = 0;
   resample_last2 = 0;
   comp_lpf_freq = 0;
   comp_lpf_taps = [];
   comp_lpf_taps_length = 255;
   audio_adpcm.index = 0;
   audio_adpcm.previousValue = 0;
   audio_ext_adc_ovfl = false;
   audio_need_stats_reset = true;
   audio_change_LPF_latch = false;
   audio_change_freq_latch = false;
   
   var buffering_scheme = 0;
   var scheme_s;
	var a = kiwi_url_param('abuf', null, null);
   var abuf = 0;
   
   if (a != null) {
      var a2 = a.split(',');
      abuf = parseFloat(a2[0]);
      if (!isNaN(abuf) && abuf >= 0.25 && abuf <= 5.0) {
         console.log('AUDIO override abuf='+ a);
         var max = abuf * 3;
         if (a2.length >= 2) {
            var m = parseFloat(a2[1]);
            if (!isNaN(m) && m >= 0.25 && m <= 5.0 && m > abuf) {
               max = m;
            }
         } else {
            max = abuf * 3;
         }
         audio_buffer_min_length_sec = abuf;
         audio_buffer_max_length_sec = max;
         audio_buffer_size = 8192;
         buffering_scheme = 9;
         scheme_s = 'abuf=';
      } else {
         abuf = 0;
      }
   }
   
   if (abuf == 0) {
      if (less_buffering) {
         if (is_local)
            buffering_scheme = 2;
         else
            buffering_scheme = 1;
      } else {
         buffering_scheme = 0;
      }

      // 2048 =  46 ms/buf 21.5 /sec @ 44.1 kHz
      // 4096 =  93 ms/buf 10.8 /sec @ 44.1 kHz
      // 8192 = 186 ms/buf  5.4 /sec @ 44.1 kHz
   
      if (buffering_scheme == 2) {
         audio_buffer_size = 8192;
         audio_buffer_min_length_sec = 0.37;    // min_nbuf = 2 @ 44.1 kHz
         audio_buffer_max_length_sec = 2.00;
         scheme_s = 'less buf, local';
      } else
      
      if (buffering_scheme == 1) {
         audio_buffer_size = 8192;
         audio_buffer_min_length_sec = 0.74;    // min_nbuf = 4 @ 44.1 kHz
         audio_buffer_max_length_sec = 3.00;
         scheme_s = 'less buf, remote';
      } else
      
      if (buffering_scheme == 0) {
         audio_buffer_size = 8192;
         audio_buffer_min_length_sec = 0.85;    // min_nbuf = 5 @ 44.1 kHz
         audio_buffer_max_length_sec = 3.40;
         scheme_s = 'more buf';
      }
   }
   
	audio_data = new Int16Array(audio_buffer_size);
	audio_last_output_buffer = new Float32Array(audio_buffer_size)
	audio_last_output_buffer2 = new Float32Array(audio_buffer_size)
	audio_silence_buffer = new Float32Array(audio_buffer_size);
	console.log('AUDIO buffer_size='+ audio_buffer_size +' buffering_scheme: '+ scheme_s);
	
	kiwi_clearInterval(audio_stats_interval);
	audio_stats_interval = setInterval(audio_stats, 1000);

	//https://github.com/0xfe/experiments/blob/master/www/tone/js/sinewave.js
	try {
		window.AudioContext = window.AudioContext || window.webkitAudioContext;
		audio_context = new AudioContext();
		audio_context.sampleRate = 44100;		// attempt to force a lower rate
		audio_output_rate = audio_context.sampleRate;		// see what rate we're actually getting
		
		try {
		   audio_panner = audio_context.createStereoPanner();
		   audio_panner_ui_init();
		} catch(e) {
		   audio_panner = null;
		}
		
      if (kiwi_isSmartTV()) audio_gain = audio_context.createGain();
	} catch(e) {
		kiwi_serious_error("Your browser does not support Web Audio API, which is required for OpenWebRX to run. Please use an HTML5 compatible browser.");
		audio_context = null;
		audio_output_rate = 0;
		return true;
	}

   audio_running = true;
   return false;
}

function audio_rate(input_rate)
{
	audio_input_rate = input_rate;

	if (audio_input_rate == 0) {
		snd_send("SET x-DEBUG user's browser gave zero audio_input_rate?");
		kiwi_serious_error("Audio initialization problem.");
	} else
	if (audio_output_rate == 0) {
		snd_send("SET x-DEBUG user's browser doesn't support WebAudio");
		snd_send("SET x-DEBUG "+ navigator.userAgent);
		kiwi_serious_error("Browser doesn\'t support WebAudio:<br>"+ navigator.userAgent +"<br><br>"+
		   "Please update to the latest version of your browser.");
	} else {
		if (resample_old) {
			audio_interpolation = audio_output_rate / audio_input_rate;		// needed by rational_resampler_get_lowpass_f()
			audio_decimation = 1;
		} else {
			audio_interpolation = 0;
			
			// Try to find common denominators by brute force.
			if (audio_interpolation == 0) {
				var interp, i_decim;
				for (interp = 2; interp <= 1024; interp++) {
					var decim = (input_rate * interp) / audio_output_rate;
					i_decim = Math.floor(decim);
					var frac = Math.abs(decim - i_decim);
					if (frac < 0.00001) break;
				}
            //console.log('### input_rate='+ input_rate +' audio_output_rate='+ audio_output_rate +' interp='+ interp);
				if (interp > 1024) {
					//divlog("unsupported audio output rate: "+audio_output_rate, 1);
					snd_send("SET UAR in="+input_rate+" out="+audio_output_rate);
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
		snd_send("SET AR OK in="+ input_rate +" out="+ audio_output_rate);
		//divlog("Network audio rate: "+audio_input_rate.toString()+" sps");
	} else {
	   audio_resample_ratio = 1;
	}

   audio_min_nbuf = Math.ceil((audio_buffer_min_length_sec * audio_output_rate) / audio_buffer_size);
   audio_max_nbuf = Math.ceil((audio_buffer_max_length_sec * audio_output_rate) / audio_buffer_size);
	console.log('AUDIO audio_input_rate='+ audio_input_rate +' audio_output_rate='+ audio_output_rate);
	console.log('AUDIO min_length_sec='+ audio_buffer_min_length_sec +'('+ audio_min_nbuf +' bufs) max_length_sec='+ audio_buffer_max_length_sec +'('+ audio_max_nbuf +' bufs)');
}

// audio_source.onaudioprocess() -> audio_convolver -> audio_context.destination
//                                                  -> audio_watchdog.audio_watchdog_process()
//
// called only once when enough network input has been received

function audio_start()
{
   //console.log('AUDIO audio_start');
	if (audio_context == null) return;

	audio_started = true;
	audio_connect(0);
	window.setInterval(audio_periodic, audio_periodic_interval_ms);

	try {
		demodulator_analog_replace(init_mode);		//needs audio_output_rate to exist
	} catch(ex) {
		snd_send("SET x-DEBUG audio_start.demodulator_analog_replace: catch: "+ ex.toString());

		// message too big -- causes server crash
		//snd_send("SET x-DEBUG audio_start.demodulator_analog_replace: catch: "+ ex.stack);
	}
}

function audio_disconnect()
{
   //console.log('AUDIO audio_disconnect');
   if (audio_source) {
      audio_disconnected = true;
      audio_source.disconnect(); audio_source.onaudioprocess = null;
   }
   if (audio_convolver_running) {
      audio_convolver.disconnect();
      audio_convolver_running = false;
   }
   if (audio_watchdog) {
      audio_watchdog.disconnect(); audio_watchdog.onaudioprocess = null;
   }
}

function audio_connect_destination(src)
{
   // SmartTV browser won't play audio unless there is a gain block in the chain!
   if (kiwi_isSmartTV()) {
      audio_gain.gain.value = 0.3;     // don't blow out the TV speakers when they're set at 50% volume
      src.connect(audio_gain);
      src = audio_gain;
	}
	
	if (audio_panner) {
      src.connect(audio_panner);
      audio_panner.connect(audio_context.destination);
	} else {
      src.connect(audio_context.destination);
   }
}

function audio_set_pan(pan)
{
   if (audio_panner) {
      try {
         audio_panner.pan.value = pan;
      } catch(ex) {}
   }
}

// NB: always use kiwi_log() instead of console.log() in here
function audio_connect(reconnect)
{
   //console.log('AUDIO audio_connect reconnect='+ reconnect);
	if (audio_context == null) return;
	if (!audio_initial_connect && reconnect) {
	   kiwi_log('AUDIO audio_connect reconnect attempt too early -- IGNORED');
	   return;
	}
	if (!reconnect) audio_initial_connect = true;
	
	if (reconnect) {
	   audio_disconnect();
	   resample_init1 = resample_init2 = false;     // make sure convolver gets restarted
		audio_reconnect++;
      kiwi_log('AUDIO reconnect BUFFERING true');
		audio_buffering = true;
	}
	
	audio_stat_output_epoch = -1;
   audio_change_LPF_delayed = false;

	audio_channels = audio_mode_iq? 2 : 1;
	kiwi_log('audio_connect: reconnect='+ reconnect +' audio_mode_iq='+ audio_mode_iq +' audio_channels='+ audio_channels +' audio_compression='+ audio_compression);
	audio_source = audio_context.createScriptProcessor(audio_buffer_size, 0, audio_channels);		// in_nch=0, out_nch=audio_channels
	audio_source.onaudioprocess = audio_onprocess;
   audio_disconnected = false;
	
	if (audio_convolver_running) {
		audio_source.connect(audio_convolver);
		audio_connect_destination(audio_convolver);
	} else {
		audio_connect_destination(audio_source);
	}
	
	// workaround for Firefox problem where audio goes silent after a while (bug seems less frequent now?)
	if (kiwi_isFirefox()) {
		audio_watchdog = audio_context.createScriptProcessor(audio_buffer_size, audio_channels, 0);	// in_nch=audio_channels, out_nch=0
		audio_watchdog.onaudioprocess = audio_watchdog_process;
		
		// send audio to audio_watchdog as well
		if (audio_convolver_running)
			audio_convolver.connect(audio_watchdog);
		else
			audio_source.connect(audio_watchdog);
	}
}

// NB: always use kiwi_log() instead of console.log() in here
function audio_watchdog_process(ev)
{
	if (muted || audio_buffering || audio_masked) {
		audio_silence_count = 0;
		return;
	}
	
	var silence_buf = ev.inputBuffer.getChannelData(0);
	var silent = (silence_buf[0] == 0);
	if (kiwi_gc_snd) silence_buf = null;	// gc
	audio_silence_count = silent? audio_silence_count+1 : 0;

	if (audio_silence_count > 16) {
		audio_connect(1);
		audio_silence_count = 0;
		audio_restart_count++;
      add_problem("FF silence");
      kiwi_log('AUDIO FF SILENCE');
	}
}

// NB: always use kiwi_log() instead of console.log() in here
function audio_onprocess(ev)
{
   audio_firefox_watchdog++;

   if (audio_disconnected) return;
   
   //if (!audio_started) { kiwi_log('audio_onprocess audio_started='+ audio_started +' ql='+ audio_prepared_buffers.length  +' ----------------'); }
	if (audio_stat_output_epoch == -1) {
		audio_stat_output_epoch = (new Date()).getTime();
		audio_stat_output_bufs = 0;
	}

	/*
	// simulate Firefox "goes silent" problem
	if (dbgUs && kiwi_isFirefox() && ((audio_stat_output_bufs & 0x1f) == 0x1f)) {
		ev.outputBuffer.copyToChannel(audio_silence_buffer,0);
      if (audio_channels == 2) ev.outputBuffer.copyToChannel(audio_silence_buffer, 1);
		return;
	}
	*/
	
	/*
	if (dbgUs && ((audio_stat_output_bufs & 0x1f) == 0x1f)) {
      kiwi_log('AUDIO force underrun');
	   audio_prepared_buffers = [];
	}
	*/

	audio_stat_output_bufs++;

	if (audio_started && audio_prepared_buffers.length == 0) {
		audio_underrun_errors++;
      //kiwi_log('AUDIO UNDERRUN BUFFERING');
      audio_buffering = true;
	}

	if (!audio_started || audio_buffering) {
		audio_need_stats_reset = true;
		ev.outputBuffer.copyToChannel(audio_silence_buffer, 0);
      if (audio_channels == 2) ev.outputBuffer.copyToChannel(audio_silence_buffer, 1);
		return;
	}

	ev.outputBuffer.copyToChannel(audio_prepared_buffers.shift(), 0);
	if (audio_channels == 2) ev.outputBuffer.copyToChannel(audio_prepared_buffers2.shift(), 1);
	
	audio_ext_sequence = audio_prepared_seq.shift();

	if (audio_change_LPF_delayed) {
		audio_recompute_LPF(0);
		audio_change_LPF_delayed = false;
	}
	
	sMeter_dBm_biased = audio_prepared_smeter.shift() / 10;
	
	var flags = audio_prepared_flags.shift();
	audio_ext_adc_ovfl = (flags & audio_flags.SND_FLAG_ADC_OVFL)? true:false;

	if (flags & audio_flags.SND_FLAG_LPF) {
		audio_change_LPF_delayed = true;
	}
	
	if (audio_meas_dly_ena && audio_meas_dly_start && (flags & audio_flags.SND_FLAG_NEW_FREQ)) {
		audio_meas_dly = (new Date()).getTime() - audio_meas_dly_start;
      kiwi_log('AUDIO dly='+ audio_meas_dly);
		audio_meas_dly_start = 0;
	}
}

//setInterval(function() { audio_ext_adc_ovfl = audio_ext_adc_ovfl? false:true; }, 1000);

var audio_watchdog_restart_cnt = 0;

function audio_periodic()
{
   // Workaround for latest Firefox audio problem.
   // Detect when audio_onprocess() stops getting called and restart it. An audio_connect() alone is insufficient.
   // The entire audio connection must be rebuilt by also calling audio_init()
   // Because this discards input buffers the compression must be restarted to avoid a noise burst.
   // Do this by asking the server to restart the audio stream with a reset compression state.

   if (audio_firefox_watchdog == 0 && kiwi_isFirefox() && !cfg.disable_recent_changes) {
      add_problem("FF watchdog");
      console.log('AUDIO FF WATCHDOG ============================================');
      audio_init(null, false, null);
      audio_started = true;
      audio_initial_connect = true;
      audio_watchdog_restart = true;
      snd_send("SET restart");
   } else {
      audio_firefox_watchdog = 0;
   }
   
   //if (audio_watchdog_restart) { console.log('audio_watchdog_restart '+ audio_watchdog_restart_cnt); audio_watchdog_restart_cnt++; }

   //console.log('AUDIO FLUSH');
	var overran = false;
	//var audio_buffer_mid_length_sec = audio_buffer_min_length_sec + ((audio_buffer_max_length_sec - audio_buffer_min_length_sec) /2);
	
	while (audio_prepared_buffers.length > audio_max_nbuf) {
		overran = true;
		audio_prepared_buffers.shift();
		if (audio_channels == 2) audio_prepared_buffers2.shift();
		audio_prepared_seq.shift();
		audio_prepared_flags.shift();
		audio_prepared_smeter.shift();
	}
	
	if (overran) {
		add_problem("audio overrun");
		audio_overrun_errors++;
	}
	
	if (audio_last_underruns != audio_underrun_errors) {
			add_problem("audio underrun");
			snd_send("SET underrun="+ audio_underrun_errors);
		audio_last_underruns = audio_underrun_errors;
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
		snd_send('SET FF-0 '+ s);
	}
	*/
}

function audio_recv(data)
{
   //if (!audio_running) console.log('AUDIO audio_recv running='+ audio_running);
   if (!audio_running) return;
   
	var f8 = new Uint8Array(data, 0, 4);   // data, offset, length
   var flags = f8[3];
   
	var u32View = new Uint32Array(data, 4, 1);
	var seq = u32View[0];
	
	var sm8 = new Uint8Array(data, 8, 2);
	var smeter = (sm8[0] << 8) | sm8[1];
	
	var offset = 10;
	if (flags & audio_flags.SND_FLAG_MODE_IQ)
		offset = 20;
	
	audio_masked = (flags & audio_flags.SND_FLAG_MASKED);

	var ad8 = new Uint8Array(data, offset);
	var i, bytes = ad8.length, samps;
	
	if (audio_watchdog_restart) {
	   if (!(flags & audio_flags.SND_FLAG_RESTART)) return;
	   audio_watchdog_restart = false;
	   
      audio_prepared_buffers = [];
      audio_prepared_buffers2 = [];
      audio_prepared_seq = [];
      audio_prepared_flags = [];
      audio_prepared_smeter = [];
      
      if (audio_mode_iq) {
         // because we haven't figured out how to make rational_resampler_cc() work yet
         // punt and just use old resampler for IQ mode
         resample_new = false; resample_old = !resample_new;
      } else {
         audio_adpcm.index = audio_adpcm.previousValue = 0;
         resample_new = kiwi_isMobile()? false : resample_new_default; resample_old = !resample_new;
      }

      audio_connect(1);
	} else
	if (flags & audio_flags.SND_FLAG_MODE_IQ) {
	
	   // current buffer flag is IQ mode
	   if (!audio_mode_iq) {    // !IQ -> IQ transition
         audio_prepared_buffers = [];
         audio_prepared_buffers2 = [];
         audio_prepared_seq = [];
         audio_prepared_flags = [];
         audio_prepared_smeter = [];
         
         // because we haven't figured out how to make rational_resampler_cc() work yet
         // punt and just use old resampler for IQ mode
         resample_new = false; resample_old = !resample_new;
         audio_mode_iq = true;
         //console.log('AUDIO IQ mode');
         audio_connect(1);
	   }
	   audio_last_compression = audio_compression = false;
	   audio_mode_iq = true;
	} else {
	
	   // current buffer flag is not IQ mode
	   // need to restart audio in two cases:
	   //    transition from IQ -> !IQ
	   //    when in !IQ there is a change in compression flag
	   var compressed = (flags & audio_flags.SND_FLAG_COMPRESSED)? true:false;
	   if (audio_mode_iq || (audio_compression != compressed)) {
         audio_prepared_buffers = [];
         audio_prepared_buffers2 = [];
         audio_prepared_seq = [];
         audio_prepared_flags = [];
         audio_prepared_smeter = [];
         audio_adpcm.index = audio_adpcm.previousValue = 0;
         resample_new = kiwi_isMobile()? false : resample_new_default; resample_old = !resample_new;
         audio_mode_iq = false;
         //console.log('AUDIO compression change='+ (audio_compression != compressed) +' now='+ compressed);
         audio_last_compression = audio_compression = compressed;
         audio_connect(1);
	   }
	   
      audio_last_compression = audio_compression = compressed;
	   audio_mode_iq = false;
	}

	if (audio_compression) {
		decode_ima_adpcm_e8_i16(ad8, audio_data, bytes, audio_adpcm);
		samps = bytes*2;		// i.e. 1024 8b bytes -> 2048 16b real samps, 1KB -> 4KB, 4:1 over uncompressed
	} else {
		for (i=0; i<bytes; i+=2) {
			audio_data[i/2] = (ad8[i]<<8) | ad8[i+1];		// convert from network byte-order
		}
		samps = bytes/2;		// i.e. non-IQ: 1024 8b bytes ->  512 16b real samps,                     1KB -> 1KB, 1:1 no compression
		                     // i.e.     IQ: 2048 8b bytes -> 1024 16b  I,Q samps (512 IQ samp pairs), 2KB -> 2KB, 1:1 never compression
	}
	
	audio_prepare(audio_data, samps, seq, flags, smeter);

	if (!audio_started) {
	   var enough_buffered = audio_prepared_buffers.length > audio_min_nbuf;
	   //console.log('ASTART eb='+ enough_buffered +' len='+ audio_prepared_buffers.length);
	   if (enough_buffered)
		   audio_start();
	}

	if (audio_need_stats_reset)
		audio_stats_reset();

	audio_stat_input_size += samps;
	audio_stat_total_input_size += samps;

	extint_audio_data(audio_data, samps);


   // audio FFT hook
   if (rx_chan >= wf_chans) {
      wf_audio_FFT(audio_data, samps);
   }


	// Recording hooks
	if (window.recording) {
		var samples = audio_mode_iq ? 1024 : (compressed ? 2048 : 512);

		// There are 2048 or 512 little-endian samples in each audio_data, the rest of the elements are zeroes
		for (var i = 0; i < samples; ++i) {
			window.recording_meta.data.setInt16(window.recording_meta.offset, audio_data[i], true);
			window.recording_meta.offset += 2;
		}
		window.recording_meta.total_size += samples * 2;

		// Check if it's time for a new buffer yet
		if (window.recording_meta.offset == 65536) {
			window.recording_meta.buffers.push(new ArrayBuffer(65536));
			window.recording_meta.data = new DataView(window.recording_meta.buffers[window.recording_meta.buffers.length - 1]);
			window.recording_meta.offset = 0;
		}
	}
}

var audio_push_ct = 0;

function audio_prepare(data, data_len, seq, flags, smeter)
{
	var resample_new_decomp = resample_new && audio_compression && comp_lpf_taps_length;

	//console.log("audio_prepare :: "+data_len.toString());
	//console.log("data.len = "+data_len.toString());

	var dopush = function()
	{
		audio_prepared_buffers.push(audio_last_output_buffer);
		if (audio_channels == 2) audio_prepared_buffers2.push(audio_last_output_buffer2);
		audio_prepared_seq.push(seq);

		// delay changing LPF until point at which buffered audio changed
		// don't miss any SND_FLAG_LPF flags because dopush() isn't invoked for every call to audio_prepare()
		if (audio_change_LPF_latch) {
		   audio_change_LPF_latch = false;
			flags |= audio_flags.SND_FLAG_LPF;
		}

		// don't miss any SND_FLAG_NEW_FREQ flags because dopush() isn't invoked for every call to audio_prepare()
		if (audio_change_freq_latch) {
		   audio_change_freq_latch = false;
			flags |= audio_flags.SND_FLAG_NEW_FREQ;
		}
		
		audio_prepared_flags.push(flags);
		audio_prepared_smeter.push(smeter);
		audio_last_output_offset = 0;
		audio_last_output_buffer = new Float32Array(audio_buffer_size);
		if (audio_channels == 2) audio_last_output_buffer2 = new Float32Array(audio_buffer_size);
		//if (audio_push_ct < 16) console.log('AUDIO push='+ audio_push_ct); audio_push_ct++;
	};

	var copy = function(d, di, s, si, len)
	{
		var i;
		for (i=0; i<len; i++) d[di+i] = s[si+i];
	};
	
	var idata, idata2, idata_length;
	if (data_len == 0) return;
	
	// --- Resampling ---
	
	if (audio_resample_ratio != 1) {
	
		// setup LPF for rational resampler
		if (!resample_init1) {
			resample_taps_length = Math.round(4.0/audio_transition_bw);
			if (resample_taps_length%2 == 0) resample_taps_length++;	// number of symmetric FIR filter taps should be odd
			rational_resampler_get_lowpass_f(resample_taps, resample_taps_length, audio_interpolation, audio_decimation);
			//console.log("audio_resample_ratio "+audio_resample_ratio+" resample_taps_length "+resample_taps_length+" osize "+data_len+'/'+Math.round(data_len * audio_resample_ratio));
			//var middle=Math.floor(resample_taps_length/2); for(var i=middle; i<=middle+64; i++) console.log("tap"+i+": "+resample_taps[i]);
			//console.log('AUDIO INIT rational resampler: resample_taps_length='+ resample_taps_length);

			resample_init1 = true;
		}

		// Need a convolver-based LPF in two cases:
		//		1) filter high-frequency artifacts from using decompression with new resampler
		//			(built-in LPF of new resampler without compression is, by definition, sufficient)
		//		2) filter high-frequency artifacts from using old resampler (interpolator), independent of decompression
		//       (old resampler is currently used with IQ mode)
		// LPF must track passband in all cases via audio_recompute_LPF().
		// Use the firdes_lowpass_f() routine of the new resampler code to construct the filter for the convolver.
	
		if (!resample_init2 && resample_init1 && (resample_new_decomp || resample_old) && (audio_source != undefined)) {
			var lpf_taps, lpf_taps_length;
			
         audio_recompute_LPF(1);
         lpf_taps = comp_lpf_taps;
         lpf_taps_length = comp_lpf_taps_length;
			//console.log('AUDIO INIT convolver: resample_new_decomp='+ resample_new_decomp +' lpf_taps_length='+ lpf_taps_length);
			audio_convolver = audio_context.createConvolver();
			audio_convolver.normalize = false;
			audio_reload_convolver_buffer(lpf_taps, lpf_taps_length);
			
			// splice the convolver in-between the audio source and destination
			//console.log(audio_source);
			//console.log(audio_convolver);
			//console.log(audio_context);
			//console.log('AUDIO splice in convolver');
			audio_source.disconnect();
			audio_source.connect(audio_convolver);
		   audio_connect_destination(audio_convolver);
			
			if (kiwi_isFirefox()) {
				//audio_source.connect(audio_watchdog);
				audio_convolver.connect(audio_watchdog);
			}
			
			audio_convolver_running = true;

			resample_init2 = true;
		}

		if (resample_old) {
		
			// Our traditional linear interpolator.
			// Because this is an interpolator, and not a decimator, the post-LPF is only needed
			// to clean up the high-frequency junk left above the input passband (input sample rate).
			if (audio_channels == 2) {
            resample_output_size = Math.round(data_len/2 * audio_resample_ratio);
            var incr = 1.0 / audio_resample_ratio;
            var di = 0;
            var frac = 0;
            var xc, xc2, xl;
            for (i=0; i < resample_output_size; i++) {
            
               // new = cur*frac + last*(1-frac)  [0 <= frac <= 1]  i.e. incr = old/new
               // new = cur*frac + last - last*frac
               // new = (cur-last)*frac + last  [only one multiply]
               //assert(di < data_len);
               xc = data[di*2];
               xl = resample_last;
               resample_output_buffer[i] = (xc-xl)*frac + xl;
               xc2 = data[di*2+1];
               xl = resample_last2;
               resample_output_buffer2[i] = (xc2-xl)*frac + xl;
               frac += incr;
               if (frac >= 1) {
                  frac -= 1;
                  resample_last = xc;
                  resample_last2 = xc2;
                  di++;
               }
            }
            resample_last = xc;
            resample_last2 = xc2;
			} else {
            resample_output_size = Math.round(data_len * audio_resample_ratio);
            var incr = 1.0 / audio_resample_ratio;
            var di = 0;
            var frac = 0;
            var xc, xl;
            for (i=0; i < resample_output_size; i++) {
            
               // new = cur*frac + last*(1-frac)  [0 <= frac <= 1]  i.e. incr = old/new
               // new = cur*frac + last - last*frac
               // new = (cur-last)*frac + last  [only one multiply]
               //assert(di < data_len);
               xc = data[di];
               xl = resample_last;
               resample_output_buffer[i] = (xc-xl)*frac + xl;
               frac += incr;
               if (frac >= 1) {
                  frac -= 1;
                  resample_last = xc;
                  di++;
               }
            }
            resample_last = xc;
         }
		} else {
			if (resample_input_processed != 0) {
				var new_available = resample_input_available - resample_input_processed;
				copy(resample_input_buffer, 0, resample_input_buffer, resample_input_processed, new_available);
				resample_input_available = new_available;
				resample_input_processed = 0;
			}
			copy(resample_input_buffer, resample_input_available, data, 0, data_len);
			resample_input_available += data_len;
			
			if (audio_channels == 2) {
			   rational_resampler_cc(resample_input_buffer, resample_output_buffer, resample_output_buffer2, resample_input_available,
			      audio_interpolation, audio_decimation, resample_taps, resample_taps_length, resample_last_taps_delay);
			} else {
			   rational_resampler_ff(resample_input_buffer, resample_output_buffer, resample_input_available,
			      audio_interpolation, audio_decimation, resample_taps, resample_taps_length, resample_last_taps_delay);
			}
		}
		
		idata = resample_output_buffer;
		idata2 = resample_output_buffer2;
		idata_length = resample_output_size;
	} else {
		idata = data;
		idata_length = data_len;
	}
	
   if (flags & audio_flags.SND_FLAG_LPF) {
      audio_change_LPF_latch = true;
   }

	// reduce latency during freq or mode change by trimming most recent buffers back to minimum
   if (flags & audio_flags.SND_FLAG_NEW_FREQ) {
      audio_change_freq_latch = true;
      //console.log('NEW_FREQ audio_meas_dly_ena='+ audio_meas_dly_ena +' audio_meas_dly_start='+ audio_meas_dly_start);
      var len = audio_prepared_buffers.length;
      var min = audio_min_nbuf;
      var pop = (len > min)? (len - min) : 0;
      //if (audio_meas_dly_ena) console.log('AUDIO NEW_FREQ Qlen='+ len +' min='+ min +' pop='+ pop);

      while (audio_prepared_buffers.length > min) {
         audio_prepared_buffers.pop();
		   if (audio_channels == 2) audio_prepared_buffers2.pop();
         audio_prepared_seq.pop();
         audio_prepared_flags.pop();
         audio_prepared_smeter.pop();
      }
   }
	
	//console.log("idata_length "+ idata_length);
	if (audio_last_output_offset + idata_length <= audio_buffer_size) {
	   // array fits into output buffer
	   if (audio_channels == 2) {
         for (var i=0; i < idata_length; i++) {
            audio_last_output_buffer[i+audio_last_output_offset] = idata[i] / 32768 * f_volume;
            audio_last_output_buffer2[i+audio_last_output_offset] = idata2[i] / 32768 * f_volume;
         }
	   } else {
         for (var i=0; i < idata_length; i++)
            audio_last_output_buffer[i+audio_last_output_offset] = idata[i] / 32768 * f_volume;
      }
		audio_last_output_offset += idata_length;
		//console.log("fits into; offset="+ audio_last_output_offset.toString());
		if (audio_last_output_offset == audio_buffer_size) dopush();
	} else {
	   // array is larger than the remaining space in the output buffer
		var copied = audio_buffer_size - audio_last_output_offset;
		var remain = idata_length - copied;
	   if (audio_channels == 2) {
         for (var i=0; i < audio_buffer_size - audio_last_output_offset; i++) {  // fill the remaining space in the output buffer
            audio_last_output_buffer[i+audio_last_output_offset] = idata[i] / 32768 * f_volume;
            audio_last_output_buffer2[i+audio_last_output_offset] = idata2[i] / 32768 * f_volume;
         }
	   } else {
         for (var i=0; i < audio_buffer_size - audio_last_output_offset; i++)    // fill the remaining space in the output buffer
            audio_last_output_buffer[i+audio_last_output_offset] = idata[i] / 32768 * f_volume;
      }
		dopush();	// push the output buffer and create a new one

		//console.log("larger than; copied half: "+copied.toString()+", now at: "+audio_last_output_offset.toString());
		do {
			var i;
			for (i=0; i < remain; i++) {
			   // copy the remaining input samples to the new output buffer
				if (i == audio_buffer_size) {
					dopush();
					break;
				}
				audio_last_output_buffer[i] = idata[i+copied] / 32768 * f_volume;
				if (audio_channels == 2) audio_last_output_buffer2[i] = idata2[i+copied] / 32768 * f_volume;
			}
			remain -= i;
			copied += i;
		} while (remain);
		
		audio_last_output_offset += i;
		//console.log("larger than; remained: "+remain.toString()+", now at: "+audio_last_output_offset.toString());
	}

	if (audio_buffering) {
	   var enough_buffered = audio_prepared_buffers.length > audio_min_nbuf;
	   //console.log('BUFFERING eb='+ enough_buffered +' len='+ audio_prepared_buffers.length +' audio_started='+ audio_started);
	   if (enough_buffered) {
	      //console.log('AUDIO BUFFERING ('+ audio_min_nbuf +') complete');
		   audio_buffering = false;
		}
	}
}


try {
	if (!AudioBuffer.prototype.copyToChannel) { // Chrome 36 does not have it, Firefox does
		AudioBuffer.prototype.copyToChannel = function(input, channel) //input is Float32Array
			{
				var cd = this.getChannelData(channel);
				for (var i=0; i < input.length; i++) cd[i] = input[i];
			}
	}
} catch(ex) { console.log("CATCH: AudioBuffer.prototype.copyToChannel"); }

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
	
	var s = "Audio: network "+
		net_sps.toFixed(0) +" sps ("+
		net_avg.toFixed(0) +" avg), output "+
		out_sps.toFixed(0) +" sps";

	s += ', Qlen '+audio_prepared_buffers.length;
	if (audio_underrun_errors) s += ', underruns '+audio_underrun_errors.toString();
	if (audio_restart_count) s += ', restart '+audio_restart_count.toString();
   w3_innerHTML('id-msg-audio', s);
   
   w3_innerHTML('id-status-audio',
      w3_text(optbar_prefix_color, 'Audio'),
      w3_text('', net_sps.toFixed(0) +'|'+ out_sps.toFixed(0) +' sps, Qlen '+ audio_prepared_buffers.length)
   );

	audio_stat_input_size = 0;
}

// FIXME
// To eliminate the clicking when switching filter buffers, consider fading between new & old convolvers.

// NB: always use kiwi_log() instead of console.log() in here
function audio_recompute_LPF(force)
{
	var lpf_freq = 4000;    // default if no modulator currently defined
	if (typeof demodulators[0] != "undefined") {
		var hcut = Math.abs(demodulators[0].high_cut);
		var lcut = Math.abs(demodulators[0].low_cut);
		lpf_freq = Math.max(hcut, lcut);
	}
	
   if (force || lpf_freq != comp_lpf_freq) {
		var cutoff = lpf_freq / audio_output_rate;
		//kiwi_log('COMP_LPF force='+ force +' resample_new='+ resample_new +' cutoff: '+ comp_lpf_freq +' -> '+ lpf_freq +' '+ cutoff.toFixed(3) +'/'+ audio_output_rate +' ctaps='+ comp_lpf_taps_length);
		firdes_lowpass_f(comp_lpf_taps, comp_lpf_taps_length, cutoff);
		comp_lpf_freq = lpf_freq;

		// reload buffer if convolver already running
		if (audio_convolver_running) {
		   //kiwi_log('COMP_LPF force='+ force +' reload convolver running');
		   audio_reload_convolver_buffer(comp_lpf_taps, comp_lpf_taps_length);
		}
	} else {
		//kiwi_log('COMP_LPF no change required force='+ force +' lpf_freq='+ lpf_freq);
	}
}

// NB: always use kiwi_log() instead of console.log() in here
function audio_reload_convolver_buffer(lpf_taps, lpf_taps_length)
{
   // always using 2 channels of LPF data seems to work fine
   var audio_lpf_buffer = audio_context.createBuffer(2, lpf_taps_length, audio_output_rate);
   var audio_lpf = audio_lpf_buffer.getChannelData(0);
   audio_lpf.set(lpf_taps);
   audio_lpf = audio_lpf_buffer.getChannelData(1);
   audio_lpf.set(lpf_taps);
   audio_convolver.buffer = audio_lpf_buffer;
   //kiwi_log('audio_reload_convolver_buffer lpf_taps_length='+ lpf_taps_length);
}

function rational_resampler_ff(input, output, input_size, interpolation, decimation, taps, taps_length, last_taps_delay)
{
	// Theory: http://www.dspguru.com/dsp/faqs/multirate/resampling
	// oi: output index, i: tap index
	var output_size = Math.round(input_size * interpolation/decimation);
	var i, oi;
	var startingi, delayi, last_delayi = -1;

	for (oi=0; oi < output_size; oi++) //@rational_resampler_ff (outer loop)
	{
		startingi = Math.floor((oi*decimation + interpolation-1-last_taps_delay) / interpolation); //index of first input item to apply FIR on
		delayi = Math.floor((last_taps_delay + startingi*interpolation - oi*decimation) % interpolation); //delay on FIR taps

		if (startingi + taps_length/interpolation +1 > input_size)
		   break; //we can't compute the FIR filter to some input samples at the end

		var end = Math.floor((taps_length-delayi) / interpolation);

		var acc = 0;
		for (i=0; i < end; i++) {  //@rational_resampler_ff (inner loop)
			acc += input[startingi+i] * taps[delayi + i*interpolation];
		}
		output[oi] = acc*interpolation;
	}

	resample_input_processed = startingi;
	resample_output_size = oi;
	resample_last_taps_delay = delayi;
}

function rational_resampler_cc(input, outputI, outputQ, input_size, interpolation, decimation, taps, taps_length, last_taps_delay)
{
	// Theory: http://www.dspguru.com/dsp/faqs/multirate/resampling
	// oi: output index, i: tap index
	consoled.log('WARNING rational_resampler_cc() not working yet!');
   return;
/*
	input_size /= 2;
	var output_size = Math.round(input_size * interpolation/decimation);
	var i, oi;
	var startingi, delayi, last_delayi = -1;

	for (oi=0; oi < output_size; oi++) //@rational_resampler_ff (outer loop)
	{
		startingi = Math.floor((oi*decimation + interpolation-1-last_taps_delay) / interpolation); //index of first input item to apply FIR on
		delayi = Math.floor((last_taps_delay + startingi*interpolation - oi*decimation) % interpolation); //delay on FIR taps

		if (startingi + taps_length/interpolation +1 > input_size)
		   break; //we can't compute the FIR filter to some input samples at the end

		var end = Math.floor((taps_length-delayi) / interpolation);

		var accI = 0, accQ = 0;
		for (i=0; i < end; i++) {  //@rational_resampler_ff (inner loop)
			accI += input[(startingi+i)*2] * taps[delayi + i*interpolation];
			accQ += input[(startingi+i)*2+1] * taps[delayi + i*interpolation];
		}
		outputI[oi] = accI*interpolation;
		outputQ[oi] = accQ*interpolation;
	}

	resample_input_processed = startingi;
	resample_output_size = oi;
	resample_last_taps_delay = delayi;
*/
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
