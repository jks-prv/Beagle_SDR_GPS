% -20 dB over 400 - 4000 Hz (i.e. 20 dB / decade)
%
% from: csdr/predefined.h
% NB we found a bug in the csdr octave code:
%   [start:step:stop] => [start:step:stop-1]
%     i.e. [0:1/sr:size(vect)(1)/sr] => [0:1/sr:(size(vect)(1)-1)/sr]
%   Otherwise octave complains about the vector sizes to dot() differing.
%
% pkg load signal
clear all
clc
function mk(sr, type) 
	% Make NFM deemphasis filter. Parameters: samplerate, filter length, frequency to normalize at (filter gain will be 0dB at this frequency)
    tapnum = 78;
	attn = -80;
	normalize_at_freq = @(vect,freq,i) vect/dot(vect,sin(2*pi*freq*[0:1/sr:(size(vect)(i)-1)/sr])); 
    norm_freq=400;
    if (type == 0)
        % -20 dB / dec (-6.02 dB / oct)
        % rolloff below 400 Hz
	    printf("NFM -LF\n")
	    if (sr == 12000)
            freqvect=   [0,200,   200,400, 400,600, 600,800, 800,1200, 1200,1600, 1600,3200, 3200,4000, 4000,5000, 5000,6000];
            loss=       [-80,-80, -80,0,   0,-3.5,  -3.5,-6, -6,-9.54, -9.54,-12, -12,-18,   -18,-20,   -20,-22,   -22,-23];
        else
            freqvect=   [0,200,   200,400, 400,600, 600,800, 800,1200, 1200,1600, 1600,3200, 3200,4000, 4000,5000, 5000,6000, 6000,10000];
            loss=       [-80,-80, -80,0,   0,-3.5,  -3.5,-6, -6,-9.54, -9.54,-12, -12,-18,   -18,-20,   -20,-22,   -22,-23,   -23,-28];
        endif
        resp=power(10.0,loss/20.0);
        coeffs=firls(tapnum,freqvect/(sr/2),resp);
        i=1;
    elseif (type == 1)
        % -20 dB / dec (-6.02 dB / oct)
        % flat to 0 Hz
	    printf("NFM +LF\n")
	    if (sr == 12000)
            freqvect=   [0,400, 400,600, 600,800, 800,1200, 1200,1600, 1600,3200, 3200,4000, 4000,5000, 5000,6000];
            loss=       [0,0,   0,-3.5,  -3.5,-6, -6,-9.54, -9.54,-12, -12,-18,   -18,-20,   -20,-22,   -22,-23];
        else
            freqvect=   [0,400, 400,600, 600,800, 800,1200, 1200,1600, 1600,3200, 3200,4000, 4000,5000, 5000,6000, 6000,10000];
            loss=       [0,0,   0,-3.5,  -3.5,-6, -6,-9.54, -9.54,-12, -12,-18,   -18,-20,   -20,-22,   -22,-23,   -23,-28];
        endif
        resp=power(10.0,loss/20.0);
        coeffs=firls(tapnum,freqvect/(sr/2),resp);
        i=1;
    elseif (type == 2)
	    printf("AM/SSB 75 uS\n");
	    if (sr == 12000)
            freqvect=   [0,1000, 1000,2000, 2000,3000, 3000,4000, 4000,5000, 5000,sr/2];
            loss=       [0,-0.8, -0.8,-2.5, -2.5,-4.3, -4.3,-5.7, -5.7,-7.0, -7.0,-7.8];
        else
            freqvect=   [0,1000, 1000,2000, 2000,3000, 3000,4000, 4000,5000, 5000,6000, 6000,7000, 7000,8000, 8000,9000, 9000,sr/2];
            loss=       [0,-0.8, -0.8,-2.5, -2.5,-4.3, -4.3,-5.7, -5.7,-7.0, -7.0,-7.8, -7.8,-8.6, -8.6,-9.2, -9.2,-9.6, -9.6,-10.0];
        endif
        resp=power(10.0,loss/20.0);
        coeffs=firls(tapnum,freqvect/(sr/2),resp);
        i=1;
    elseif (type == 3)
	    printf("AM/SSB 50 uS\n");
	    if (sr == 12000)
            freqvect=   [0,1000, 1000,2000, 2000,3000, 3000,4000, 4000,5000, 5000,sr/2];
            loss=       [0,-0.4, -0.4,-1.3, -1.3,-2.4, -2.4,-3.5, -3.5,-4.5, -4.5,-5.4];
        else
            freqvect=   [0,1000, 1000,2000, 2000,3000, 3000,4000, 4000,5000, 5000,6000, 6000,7000, 7000,8000, 8000,9000, 9000,sr/2];
            loss=       [0,-0.4, -0.4,-1.3, -1.3,-2.4, -2.4,-3.5, -3.5,-4.5, -4.5,-5.4, -5.4,-6.1, -6.1,-6.7, -6.7,-7.2, -7.2,-7.6];
        endif
        resp=power(10.0,loss/20.0);
        coeffs=firls(tapnum,freqvect/(sr/2),resp);
	    i=1;
    elseif (type == 4)
	    printf("-20 dB test\n")
        freqvect =      [0,1000, 1000,2000, 2000,3000, 3000,4000, 4000,sr/2];
        mag_resp_dB =   [0,0,    0,-20,     -20,-20,   -20,0,     0,0];
        mag_resp = power(10.0, mag_resp_dB/20.0);
        coeffs=firls(tapnum,freqvect/(sr/2),mag_resp);
        i=1;
    else
        % fir2 experiment
	    printf("fir2\n")
        freqvect=   [0,400, 400,4000, 4000,sr/2];
        loss=       [0,0,   0,attn,   attn,attn];
        resp=power(10.0,loss/20.0);
        coeffs=fir2(tapnum, freqvect/(sr/2), resp);
	    printf("size coeffs fir2: [%d %d]\n",size(coeffs));
        i=2;
    endif
	coeffs=normalize_at_freq(coeffs,norm_freq,i);
	freqz(coeffs);
	printf("size coeffs: [%d %d]\n",size(coeffs));
	printf("%g, ",coeffs);
	printf("\n")
end
