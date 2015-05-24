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

// Note: we don't support older browsers using the Mozilla audio API since it is now depreciated
// see https://wiki.mozilla.org/Audio_Data_API

var audio_buffer_current_size_debug=0;
var audio_buffer_all_size_debug=0;
var audio_buffer_current_count_debug=0;
var audio_buffer_current_size=0;

var audio_context;
var audio_started = false;

var audio_received = Array();
var audio_buffer_index = 0;
var audio_node, audio_FFnode;
var audio_output_rate;
var audio_input_rate;

// Optimalise these if audio lags or is choppy:
var audio_buffer_size = 8192;//2048 was choppy
var audio_buffer_maximal_length_sec=1.7; //actual number of samples are calculated from sample rate
var audio_flush_interval_ms=250; //the interval in which audio_flush() is called

var audio_prepared_buffers = Array();
var audio_last_output_buffer = new Float32Array(audio_buffer_size);
var audio_last_output_offset = 0;
var audio_buffering = false;
var audio_buffering_fill_to=10; //on audio underrun we wait until this n*audio_buffer_size samples are present

var audio_resample_ratio = 1;

var audio_interpolation;
var audio_decimation;
var audio_transition_bw;

var audio_convolver, audio_lpf_buffer, audio_lpf;

var resample_init = false;
//var resample_interp = true;
var resample_interp = false;
var resample_input_buffer = [];
var resample_input_available = 0;
var resample_input_processed = 0;
var resample_last_taps_delay = 0;
var resample_output_buffer = [];
var resample_output_size;
var resample_taps = [];
var resample_taps_length;
var resample_last = 0;

function audio_init()
{
	
	//divlog('onaudioprocess: <span id="id-OAP">0</span>');
	
	audio_debug_time_start=(new Date()).getTime();
	audio_debug_time_last_start=audio_debug_time_start;

	//https://github.com/0xfe/experiments/blob/master/www/tone/js/sinewave.js
	try 
	{
		//console.log("AudioContext="+window.AudioContext);
		//console.log("webkitAudioContext="+window.webkitAudioContext);
		window.AudioContext = window.AudioContext || window.webkitAudioContext;
		audio_context = new AudioContext();
		audio_output_rate = audio_context.sampleRate;
	}
	catch(e) 
	{
		divlog('Your browser does not support Web Audio API, which is required for OpenWebRX to run. Please upgrade to a HTML5 compatible browser.', 1);
	}
}

function audio_connect()
{
	//on Chrome v36, createJavaScriptNode has been replaced by createScriptProcessor
	var createScriptNode = (audio_context.createJavaScriptNode == undefined)? audio_context.createScriptProcessor.bind(audio_context) : audio_context.createJavaScriptNode.bind(audio_context);
	audio_node = createScriptNode(audio_buffer_size, 0, 1);
	audio_node.onaudioprocess = audio_onprocess;
	audio_node.connect(audio_context.destination);
	
	// workaround for Firefox problem where audio goes silent after a while
	if (wrx_isFirefox()) {
		audio_FFnode = createScriptNode(audio_buffer_size, 1, 0);
		audio_FFnode.onaudioprocess = audio_watchdog;
		audio_node.connect(audio_FFnode);	// send audio_node output to audio_FFnode as well
	}
}

// onaudioprocess -> (audio_node) audio_context.createScriptProcessor -> audio_context.destination
function audio_start()
{
	audio_connect();
	window.setInterval(audio_flush,audio_flush_interval_ms);
	//divlog('Web Audio API succesfully initialized, sample rate: '+audio_output_rate.toString()+ " sps");
	/*audio_source=audio_context.createBufferSource();
   audio_buffer = audio_context.createBuffer(xhr.response, false);
	audio_source.buffer = buffer;
	audio_source.noteOn(0);*/
	demodulator_analog_replace('am'); //needs audio_output_rate to exist
	
	audio_started = true;
}

var silence_count = 0, restart_count = 0;

function audio_watchdog(ev)
{
	if (muted) { silence_count = 0; return; }
	var data = ev.inputBuffer.getChannelData(0);
	silence_count = (data[0] == 0)? silence_count+1 : 0;
	if (silence_count > 16) {
		audio_node.disconnect(); audio_node.onaudioprocess = null;
		audio_connect();
		silence_count = 0;
		restart_count++;
		html('id-ff-wdog').innerHTML = ', restart '+restart_count.toString();
		//console.log("*** Firefox restart_count="+restart_count);
	}
}

function audio_rate(input_rate)
{
	audio_input_rate = input_rate;

	if (input_rate == 8250) {
		audio_interpolation = 294;		// 8250 -> 44.1K
		audio_decimation = 55;
		audio_transition_bw = 0.001;
		audio_resample_ratio = audio_output_rate / audio_input_rate;
		divlog("Network audio rate: "+audio_input_rate.toString()+" sps");
	} else
		divlog("unsupported audio rate", 1);
}

var audio_data = new Int16Array(8192);

function audio_recv(evt)
{
	var sm8 = new Uint8Array(evt.data,4,2);
	sMeter_dBm_biased = ((sm8[0]<<8) | sm8[1]) / 10;

	var ad8 = new Uint8Array(evt.data,6);
	var i, bytes = ad8.length;
	for (i=0; i<bytes; i+=2) {
		audio_data[i/2] = (ad8[i]<<8) | ad8[i+1];		// convert from network byte-order
	}
	
	audio_prepare(audio_data, bytes/2);
	audio_buffer_current_size_debug+=bytes/2;
	audio_buffer_all_size_debug+=bytes/2;
	if(audio_started == false && audio_prepared_buffers.length > audio_buffering_fill_to) audio_start();
}

function audio_prepare(data, data_len)
{
	//console.log("audio_prepare :: "+data_len.toString());
	//console.log("data.len = "+data_len.toString());
	var dopush=function()
	{
		audio_prepared_buffers.push(audio_last_output_buffer);
		audio_last_output_offset=0;
		audio_last_output_buffer=new Float32Array(audio_buffer_size);
		audio_buffer_current_count_debug++;
	};

	var copy = function(d, di, s, si, len)
	{
		var i;
		for (i=0; i<len; i++) d[di+i] = s[si+i];
	};
	
	var idata, idata_length;
	if(data_len==0) return;
	
	// --- Resampling ---
	if (audio_resample_ratio != 1) {
	
		// initialization
		if (resample_init == false && (!resample_interp || (resample_interp && audio_node != undefined))) {
			resample_taps_length = resample_interp? 255 : Math.round(4.0/audio_transition_bw);
			if (resample_taps_length%2==0) resample_taps_length++; //number of symmetric FIR filter taps should be odd
			rational_resampler_get_lowpass_f(resample_taps, resample_taps_length, audio_interpolation, audio_decimation);
			//console.log("audio_resample_ratio "+audio_resample_ratio+" resample_taps_length "+resample_taps_length+" osize "+data_len+'/'+Math.round(data_len * audio_resample_ratio));
			
			//var middle=Math.floor(resample_taps_length/2); for(var i=middle; i<=middle+64; i++) console.log("tap"+i+": "+resample_taps[i]);

			if (resample_interp) {
				audio_convolver = audio_context.createConvolver();
				audio_convolver.normalize = false;
				audio_lpf_buffer = audio_context.createBuffer(1, resample_taps_length, audio_output_rate);
				audio_lpf = audio_lpf_buffer.getChannelData(0);
				audio_lpf.set(resample_taps);
				audio_convolver.buffer = audio_lpf_buffer;
				audio_node.disconnect();
				audio_node.connect(audio_convolver);
				audio_convolver.connect(audio_context.destination);
			}

			resample_init = true;
		}

		if (resample_interp) {
			// our traditional linear interpolator
			resample_output_size = Math.round(data_len * audio_resample_ratio);
			var incr = 1.0 / audio_resample_ratio;
			var di = 0;
			var frac = 0;
			for (i=0; i<resample_output_size; i++) {
			
				// new = cur*frac + last*(1-frac)  [0 <= frac <= 1]  i.e. incr = old/new
				// new = cur*frac + last - last*frac
				// new = (cur-last)*frac + last  [only one multiply]
				//assert(di < data_len);
				var xc = data[di];
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
				var new_available = resample_input_available-resample_input_processed;
				copy(resample_input_buffer, 0, resample_input_buffer, resample_input_processed, new_available);
				resample_input_available= new_available;
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
	if(audio_last_output_offset+idata_length<=audio_buffer_size)
	{	//array fits into output buffer
		for(var i=0;i<idata_length;i++) audio_last_output_buffer[i+audio_last_output_offset]=idata[i]/32768*f_volume;
		audio_last_output_offset+=idata_length;
		//console.log("fits into; offset="+audio_last_output_offset.toString());
		if(audio_last_output_offset==audio_buffer_size) dopush();
	}
	else
	{	//array is larger than the remaining space in the output buffer
		var copied=audio_buffer_size-audio_last_output_offset;
		var remain=idata_length-copied;
		for(var i=0;i<audio_buffer_size-audio_last_output_offset;i++) //fill the remaining space in the output buffer
			audio_last_output_buffer[i+audio_last_output_offset] = idata[i]/32768*f_volume;
		dopush();//push the output buffer and create a new one

		//console.log("larger than; copied half: "+copied.toString()+", now at: "+audio_last_output_offset.toString());
		do {
			var i;
			for (i=0; i<remain; i++) { //copy the remaining input samples to the new output buffer
				if (i == audio_buffer_size) {
					dopush();
					break;
				}
				audio_last_output_buffer[i]=idata[i+copied]/32768*f_volume;
			}
			remain -= i;
			copied += i;
		} while (remain);
		audio_last_output_offset+=i;
		//console.log("larger than; remained: "+remain.toString()+", now at: "+audio_last_output_offset.toString());
	}
	if(audio_buffering && audio_prepared_buffers.length>audio_buffering_fill_to) audio_buffering=false;
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
	//See 4.1.6 at: http://www.dspguru.com/dsp/faqs/multirate/resampling
	var cutoff_for_interpolation=1.0/interpolation;
	var cutoff_for_decimation=1.0/decimation;
	var cutoff = (cutoff_for_interpolation<cutoff_for_decimation)?cutoff_for_interpolation:cutoff_for_decimation; //get the lower
	firdes_lowpass_f(output, output_size, cutoff/2);
}

function firdes_lowpass_f(output, length, cutoff_rate)
{
	//Generates symmetric windowed sinc FIR filter real taps
	//	length should be odd
	//	cutoff_rate is (cutoff frequency/sampling frequency)
	//Explanation at Chapter 16 of dspguide.com
	var i;
	var middle=Math.floor(length/2);
	output[middle]=2*Math.PI*cutoff_rate*firdes_wkernel_hamming(0);
	for(i=1; i<=middle; i++) // calculate taps
	{
		output[middle-i]=output[middle+i]=(Math.sin(2*Math.PI*cutoff_rate*i)/i)*firdes_wkernel_hamming(i/middle);
		//printf("%g %d %d %d %d | %g\n",output[middle-i],i,middle,middle+i,middle-i,sin(2*Math.PI*cutoff_rate*i));
	}
	
	//Normalize filter kernel
	var sum=0;
	for(i=0;i<length;i++) // normalize pass 1
	{
		sum+=output[i];
	}
	for(i=0;i<length;i++) // normalize pass 2
	{
		output[i]/=sum;
	}
}

function firdes_wkernel_hamming(rate)
{
	//Explanation at Chapter 16 of dspguide.com, page 2
	//Hamming window has worse stopband attentuation and passband ripple than Blackman, but it has faster rolloff.
	rate=0.5+rate/2;
	return 0.54-0.46*Math.cos(2*Math.PI*rate);
}

if (!AudioBuffer.prototype.copyToChannel)
{ //Chrome 36 does not have it, Firefox does
	AudioBuffer.prototype.copyToChannel=function(input,channel) //input is Float32Array
	{
		var cd=this.getChannelData(channel);
		for(var i=0;i<input.length;i++) cd[i]=input[i];
	}
}

//var OAP = 0;

function audio_onprocess(ev)
{
	if(audio_buffering) return;
	if(audio_prepared_buffers.length==0) { add_problem("audio underrun"); audio_buffering=true; }
	else ev.outputBuffer.copyToChannel(audio_prepared_buffers.shift(),0);
	//else audio_prepared_buffers.shift();	//jks no audio
	//html('id-OAP').innerHTML = OAP.toString(); OAP++;
}

function audio_flush()
{
	flushed=false;
	while (audio_buffer_maximal_length_sec*audio_output_rate < audio_prepared_buffers.length*audio_buffer_size)
	{
		flushed=true;
		audio_prepared_buffers.shift();
	}
	if(flushed) add_problem("audio overrun");
}


function audio_onprocess_notused(e) 
{
	//https://github.com/0xfe/experiments/blob/master/www/tone/js/sinewave.js
	if(audio_received.length==0) 
	{ add_problem("audio underrun"); return; }
	output = e.outputBuffer.getChannelData(0);
	int_buffer = audio_received[0];
	read_remain = audio_buffer_size;
	//audio_buffer_maximal_length=120;

	obi=0; //output buffer index
	debug_str=""
	while(1)	
	{
		if(int_buffer.length-audio_buffer_index>read_remain)
		{
			for (i=audio_buffer_index; i<audio_buffer_index+read_remain; i++)
				output[obi++] = int_buffer[i]/32768;
			//debug_str+="added whole ibl="+int_buffer.length.toString()+" abi="+audio_buffer_index.toString()+" "+(int_buffer.length-audio_buffer_index).toString()+">"+read_remain.toString()+" obi="+obi.toString()+"\n";
			audio_buffer_index+=read_remain;
			break;
		}
		else
		{	
			for (i=audio_buffer_index; i<int_buffer.length; i++)
				output[obi++] = int_buffer[i]/32768;
			read_remain-=(int_buffer.length-audio_buffer_index);
			audio_buffer_current_size-=audio_received[0].length;
			/*if (audio_received.length>audio_buffer_maximal_length)
			{
				add_problem("audio overrun");
				audio_received.splice(0,audio_received.length-audio_buffer_maximal_length);
			}
			else*/
				audio_received.splice(0,1);
			//debug_str+="added remain, remain="+read_remain.toString()+" abi="+audio_buffer_index.toString()+" alen="+int_buffer.length.toString()+" i="+i.toString()+" arecva="+audio_received.length.toString()+" obi="+obi.toString()+"\n";
			audio_buffer_index = 0;			
			if(audio_received.length == 0 || read_remain == 0) return;
			int_buffer = audio_received[0];
		}
	}
	//debug_str+="obi="+obi.toString();
	//alert(debug_str);
}

function audio_flush_notused()
{
	if (audio_buffer_current_size>audio_buffer_maximal_length_sec*audio_output_rate)
	{ 
		add_problem("audio overrun");
		console.log("audio_flush() :: size: "+audio_buffer_current_size.toString()+" allowed: "+(audio_buffer_maximal_length_sec*audio_output_rate).toString());
		while (audio_buffer_current_size>audio_buffer_maximal_length_sec*audio_output_rate*0.5)
		{
			audio_buffer_current_size-=audio_received[0].length;
			audio_received.splice(0,1);
		}
	}
}

var audio_debug_time_start=0;
var audio_debug_time_last_start=0;

function debug_audio()
{
	if(audio_debug_time_start==0) return; //audio_init has not been called
	time_now=(new Date()).getTime();
	audio_debug_time_since_last_call=(time_now-audio_debug_time_last_start)/1000;
	audio_debug_time_last_start=time_now; //now
	audio_debug_time_taken=(time_now-audio_debug_time_start)/1000;
	html("id-audio-sps").innerHTML=
		"audio rx "+(audio_buffer_current_size_debug/audio_debug_time_since_last_call).toFixed(0)+" sps ("+
		(audio_buffer_all_size_debug/audio_debug_time_taken).toFixed(0)+" avg), output "+
		((audio_buffer_current_count_debug*audio_buffer_size)/audio_debug_time_taken).toFixed(0)+' sps';
	audio_buffer_current_size_debug=0;
}
