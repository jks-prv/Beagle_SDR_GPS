%******************************************************************************\
%* Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
%* Copyright (c) 2002
%*
%* Author:
%*	Volker Fischer
%*
%* Description:
%* 	Generating resample filter taps
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

% Filter for ratios close to 1 -------------------------------------------------
% Fixed for sample-rate conversiones of R ~ 1
I = 10; % D = I

% Number of taps per poly-phase
NumTapsP = 12;

% Cut-off frequency
fc = 0.97 / I;

% MMSE filter-design and windowing
h = I * firls(I * NumTapsP - 1, [0 fc fc 1], [1 1 0 0]) .* kaiser(I * NumTapsP, 5)';


% Export coefficiants to file ****************************************
fid = fopen('ResampleFilter.h', 'w');

fprintf(fid, '/* Automatically generated file with GNU Octave */\n');
fprintf(fid, '/* File name: "ResampleFilter.m" */\n');
fprintf(fid, '/* Filter taps in time-domain */\n\n');

fprintf(fid, '#ifndef _RESAMPLEFILTER_H_\n');
fprintf(fid, '#define _RESAMPLEFILTER_H_\n\n');

fprintf(fid, '#define RES_FILT_NUM_TAPS_PER_PHASE  ');
fprintf(fid, int2str(NumTapsP));
fprintf(fid, '\n');
fprintf(fid, '#define INTERP_DECIM_I_D             ');
fprintf(fid, int2str(I));
fprintf(fid, '\n\n\n');
fprintf(fid, 'extern float fResTaps1To1[INTERP_DECIM_I_D][RES_FILT_NUM_TAPS_PER_PHASE];\n');

fprintf(fid, '#endif	/* _RESAMPLEFILTER_H_ */\n');
fclose(fid);

fid = fopen('ResampleFilter.cpp', 'w');

fprintf(fid, '/* Automatically generated file with GNU Octave */\n');
fprintf(fid, '/* File name: "ResampleFilter.m" */\n');
fprintf(fid, '/* Filter taps in time-domain */\n\n');
fprintf(fid, '#include "ResampleFilter.h"\n\n');

% Write filter taps
fprintf(fid, '/* Filter for ratios close to 1 */\n');
fprintf(fid, 'float fResTaps1To1[INTERP_DECIM_I_D][RES_FILT_NUM_TAPS_PER_PHASE] = {\n');
for i = 1:I
	hTemp = h(i:I:end) ;
	fprintf(fid, '{\n');
	fprintf(fid, '	%.20ff,\n', hTemp(1:end - 1));
	fprintf(fid, '	%.20ff\n', hTemp(end));
    if (i < I)
        fprintf(fid, '},\n');
    else
        fprintf(fid, '}\n');        
    end    
end
fprintf(fid, '};\n\n\n');
fclose(fid);
