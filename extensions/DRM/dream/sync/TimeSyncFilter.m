%******************************************************************************\
%* Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
%* Copyright (c) 2003
%*
%* Author:
%*	Alexander Kurpiers,
%*      modified by David Flamand to support more sample rates
%*      modified by Julian Cable to provide a wider filter for Mode E
%*
%* Description:
%* 	Hilbert Filter for timing acquisition
%*  Runs at 48 kHz, can be downsampled to 48 kHz / 8 = 6 kHz
%*
%******************************************************************************
%*
%* This program is free software; you can redistribute it and/or modify it under
%* the terms of the GNU General Public License as published by the Free Software
%* Foundation; either version 2 of the License, or (at your option) any later 
%* version.
%*
%* This program is distributed in the hope that it will be useful, but WITHOUT 
%* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
%* FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
%* details.
%*
%* You should have received a copy of the GNU General Public License along with
%* this program; if not, write to the Free Software Foundation, Inc., 
%* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
%*
%******************************************************************************/

% Hilbert filter characteristic frequencies
% 5 kHz bandwidth
global fstart5 = 800;
global fstop5 = 5200;
global ftrans5 = 800; % Size of transition region
% 10 kHz bandwidth
global fstart10 = 800;
global fstop10 = 10200;
global ftrans10 = 800; % Size of transition region
% 50 kHz bandwidth
global fstart50 = 800;
global fstop50 = 50200;
global ftrans50 = 800; % Size of transition region

function [b] = DesignFilter(B, ftrans, nhil, fs)
    % Parks-McClellan optimal equiripple FIR filter design
    f = [0 B B + 2*ftrans fs];
    m = [2 2 0 0];
    b = remez(nhil - 1, f / fs, m, [1 10]);
endfunction

function MakeFilter(fid_cpp, fid_h, sample_rate, fstart, fstop, ftrans, bw)

    % Length of hilbert filter
    ntaps = 80 * sample_rate / 48000 + 1;

    % Actual filter design
    b = DesignFilter(fstop-fstart, ftrans, ntaps, sample_rate);

    % Export coefficients to file ****************************************

    f = sample_rate / 1000;

    % Write header file
    fprintf(fid_h, '#define NUM_TAPS_HILB_FILT_%i_%i            %i\n', bw, f, ntaps);
    fprintf(fid_h, '/* Low pass prototype for Hilbert-filter %i kHz bandwidth */\n', bw);
    fprintf(fid_h, 'extern const float fHilLPProt%i_%i[NUM_TAPS_HILB_FILT_%i_%i];\n', bw, f, bw, f);
    fprintf(fid_h, '\n');

    % Write filter taps
    fprintf(fid_cpp, '/* Low pass prototype for Hilbert-filter %i kHz bandwidth */\n', bw);
    fprintf(fid_cpp, 'const float fHilLPProt%i_%i[NUM_TAPS_HILB_FILT_%i_%i] =\n', bw, f, bw, f);
    fprintf(fid_cpp, '{\n');
    fprintf(fid_cpp, '	%.18ef,\n', b(1:end - 1));
    fprintf(fid_cpp, '	%.18ef\n', b(end));
    fprintf(fid_cpp, '};\n');
    fprintf(fid_cpp, '\n');
endfunction

function MakeFilter1(fid_cpp, fid_h, sample_rate)
    global fstart5;
    global fstop5;
    global ftrans5;
    global fstart10;
    global fstop10;
    global ftrans10;
    fprintf(fid_h, '/* Filter parameters for %i Hz sample rate */\n', sample_rate);
    fprintf(fid_cpp, '/*********************************************************/\n');
    fprintf(fid_cpp, '/* Filter taps for %-6i Hz sample rate                 */\n', sample_rate);
    MakeFilter(fid_cpp, fid_h, sample_rate, fstart5, fstop5, ftrans5, 5);
    MakeFilter(fid_cpp, fid_h, sample_rate, fstart10, fstop10, ftrans10, 10);
endfunction

fid_cpp = fopen('TimeSyncFilter.cpp', 'w');
fprintf(fid_cpp, '/* Automatically generated file with GNU Octave */\n');
fprintf(fid_cpp, '\n');
fprintf(fid_cpp, '/* File name: "TimeSyncFilter.m" */\n');
fprintf(fid_cpp, '/* Filter taps in time-domain */\n');
fprintf(fid_cpp, '\n');
fprintf(fid_cpp, '#include "TimeSyncFilter.h"\n');
fprintf(fid_cpp, '\n');

fid_h = fopen('TimeSyncFilter.h', 'w');
fprintf(fid_h, '/* Automatically generated file with GNU Octave */\n');
fprintf(fid_h, '/* File name: "TimeSyncFilter.m" */\n');
fprintf(fid_h, '/* Filter taps in time-domain */\n');
fprintf(fid_h, '\n');
fprintf(fid_h, '#ifndef _TIMESYNCFILTER_H_\n');
fprintf(fid_h, '#define _TIMESYNCFILTER_H_\n');
fprintf(fid_h, '\n');
fprintf(fid_h, '/* Filter bandwidths */\n');
fprintf(fid_h, 'enum EHilbFiltBw { ');
fprintf(fid_h, 'HILB_FILT_BNDWIDTH_5 = %i, ', fstop5 - fstart5 + ftrans5);
fprintf(fid_h, 'HILB_FILT_BNDWIDTH_10 = %i, ', fstop10 - fstart10 + ftrans10);
fprintf(fid_h, 'HILB_FILT_BNDWIDTH_50 = %i ', fstop50 - fstart50 + ftrans50);
fprintf(fid_h, '};\n\n');

MakeFilter1(fid_cpp, fid_h, 24000);
MakeFilter1(fid_cpp, fid_h, 48000);
MakeFilter1(fid_cpp, fid_h, 96000);
MakeFilter1(fid_cpp, fid_h, 192000);
MakeFilter(fid_cpp, fid_h, 192000, fstart50, fstop50, ftrans50, 50);
MakeFilter1(fid_cpp, fid_h, 384000);
MakeFilter(fid_cpp, fid_h, 384000, fstart50, fstop50, ftrans50, 50);

fprintf(fid_h, '#endif	/* _TIMESYNCFILTER_H_ */\n');

fclose(fid_h);
fclose(fid_cpp);
