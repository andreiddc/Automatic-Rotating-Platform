%% Clear all
clc; clear; close all;

%% Sinus pur de x Hz 
% Parametri
fs = 44100; % Frecvența de eșantionare
f = 1000; % Frecvența de test
durata = 5; % Durata testului în secunde

% Generarea undei
t = 0 : (1/fs) : (durata - 1/fs);
y = sin(2 * pi * f * t);

% Redarea sunetului
fprintf('Redare ton %.2f Hz...\n', f);
%nume_fisier = sprintf('Sunet_%.2fHz.wav', f);
%audiowrite(nume_fisier, y, fs);
sound(y, fs);


%% FSK codare asicrona

fs = 44100;              
amp = 0.9;
t = 0 : 1/fs : (0.1 - 1/fs);

f_400 = 400; f_500 = 500; f_600 = 600; f_700 = 700;
f_800 = 800; f_900 = 900; f_1000 = 1000;



%% Script MATLAB FSK + Vot Majoritar (3 Cadre)
% =========================================================================
directie_motor = 1;      
unghi_zecimal = 90;      

fs = 44100;              
amp = 0.9; 

f_400 = 400; f_500 = 500; f_600 = 600; f_700 = 700;
f_800 = 800; f_900 = 900; f_1000 = 1000;

% 1. Piese Control
t_100 = 0 : 1/fs : (0.1 - 1/fs);
s_800_100 = amp * sin(2 * pi * f_800 * t_100);
s_900_100 = amp * sin(2 * pi * f_900 * t_100);
s_1000_100 = amp * sin(2 * pi * f_1000 * t_100);

ton_start = [s_1000_100, s_900_100]; 
ton_stop  = [s_900_100, s_800_100];

% 2. Piese Date
t_20 = 0 : 1/fs : (0.02 - 1/fs);
s_400_20 = amp * sin(2 * pi * f_400 * t_20);
s_500_20 = amp * sin(2 * pi * f_500 * t_20);
s_600_20 = amp * sin(2 * pi * f_600 * t_20);
s_700_20 = amp * sin(2 * pi * f_700 * t_20);

bit0 = repmat([s_400_20, s_500_20], 1, 5); 
bit1 = repmat([s_600_20, s_700_20], 1, 5); 

% =========================================================================
% CONSTRUIREA MESAJULUI AUDIO (Cadru Triplu)
% =========================================================================
binar_unghi = dec2bin(unghi_zecimal, 9);
mesaj_baza = [num2str(directie_motor), binar_unghi];

% Repetam mesajul de 10 biti de 3 ori consecutiv (30 de biti in total)
mesaj_binar_triplu = repmat(mesaj_baza, 1, 3);

disp('====================================================');
disp(['Comanda Zecimala : Dir ', num2str(directie_motor), ', Unghi ', num2str(unghi_zecimal)]);
disp(['Cadru de Baza    : ', mesaj_baza]);
disp(['Payload (3x)     : ', mesaj_binar_triplu]);
disp('====================================================');

audio_final = ton_start;

for i = 1:length(mesaj_binar_triplu)
    if mesaj_binar_triplu(i) == '0'
        audio_final = [audio_final, bit0];
    else
        audio_final = [audio_final, bit1];
    end
end

audio_final = [audio_final, ton_stop];

nume_fisier = sprintf('comanda_DIR%d_ANG%d_TRIPLU.wav', directie_motor, unghi_zecimal);
audiowrite(nume_fisier, audio_final, fs);
disp(['SUCCES: Fisierul "', nume_fisier, '" a fost creat (Durata: ', num2str(length(audio_final)/fs), ' sec).']);