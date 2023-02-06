% see: dsp.stackexchange.com/questions/34605/biquad-cookbook-formula-for-broadcast-fm-de-emphasis
%
% pkg load signal
clear all;
clc;

%fs = 44100;
fs = 12000;
%fs = 20250;
samplingtime = 1/fs;
tau = 75;  % us
%tau = 50;  % us
%tau = 500;  % us
tau = tau/1000000;

% analog prototype
A2 = [1];
B2 = [tau 1];
%Ds = tf(B2, A2); % swap between A2 and B2 to select pre- or de-emphasis
Ds = tf(A2, B2); % swap between A2 and B2 to select pre- or de-emphasis
Ds = Ds/dcgain(Ds);

% MZT
T1 = tau;
z1 = -exp(-1.0/(fs*T1));
p1 = 1+z1;

a0 = 1.0;
a1 = p1;
a2 = 0;

b0 = 1.0;
b1 = z1;
b2 = 0;

% swap between a1, b1 to select pre- or de-emphasis

%Bmzt = [b0 a1 b2]
%Amzt = [a0 b1 a2]
Bmzt = [b0 b1 b2]
Amzt = [a0 a1 a2]

% tf(<numer>, <denom>)
% tf([A B C], [X Y Z]) => y1(s): (As^2 + Bs + C) / (Xs^2 + Ys + Z)
DzMZT = tf(Amzt, Bmzt, samplingtime);
DzMZT = DzMZT/dcgain(DzMZT);

%% Plot
wmin = 2 * pi * 20.0; % 20Hz
%wmax = 2 * pi * ((fs/2.0) - (fs/2 - 20000));    % 20kHz
wmax = 2 * pi * ((fs/2.0) - (fs/2 - 10000));    % 10kHz

figure(1);
bode(Ds, 'b', DzMZT, 'c', {wmin, wmax});
legend('Analog prototype', 'MZT 2nd order','location', 'northwest');
grid on;
