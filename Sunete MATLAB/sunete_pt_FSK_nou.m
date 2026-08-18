% ==========================================
% GENERATOR ȘI INTERPRETOR DE SEMNAL AUDIO FSK
% ==========================================
% --- Setări Transmisie ---
Fs = 44100;                 % Frecvența de eșantionare (44.1 kHz)
durata_simbol = 0.1;        % Durata fiecărui simbol: 100 ms
t = 0 : 1/Fs : durata_simbol - 1/Fs; % Vectorul de timp
% Pachetul de date (1 bit direcție + 9 biți unghi)
date = [1 0 0 0 0 0 1 0 1 0];
% ==========================================
% 1. Generarea secvenței de frecvențe
% ==========================================
frecvente = [];
% Marker de START
frecvente = [frecvente, 1000, 900];
% Maparea biților pe frecvențe (inclusiv regula cu *)
for i = 1:length(date)
   bit_curent = date(i);
  
   if i == 1
       % Primul bit
       if bit_curent == 0
           frecvente = [frecvente, 400];
       else
           frecvente = [frecvente, 600];
       end
   else
       if bit_curent == date(i-1)
           % Avem 2 biți identici consecutivi
           if bit_curent == 0
               if frecvente(end) == 400
                   frecvente = [frecvente, 500]; % BIT 0*
               else
                   frecvente = [frecvente, 400]; % BIT 0
               end
           else
               if frecvente(end) == 600
                   frecvente = [frecvente, 700]; % BIT 1*
               else
                   frecvente = [frecvente, 600]; % BIT 1
               end
           end
       else
           % Biți diferiți
           if bit_curent == 0
               frecvente = [frecvente, 400];
           else
               frecvente = [frecvente, 600];
           end
       end
   end
end
% Marker de STOP
frecvente = [frecvente, 900, 800];
% ==========================================
% 2. Generarea Semnalului Audio
% ==========================================
semnal_final = [];
for i = 1:length(frecvente)
   % Generăm tonul sinusoidal de 100 ms pentru frecvența curentă
   ton = sin(2 * pi * frecvente(i) * t);
  
   % Înlănțuim tonurile pentru a forma pachetul audio complet
   semnal_final = [semnal_final, ton];
end
% ==========================================
% 3. Interpretare și Afișare în Consolă
% ==========================================
clc;
disp('========================================');
disp('            DATE TRANSMISIE             ');
disp('========================================');
fprintf('Biții din pachet : %s\n', num2str(date, '%d '));
fprintf('Frecvențe (Hz)   : %s\n', num2str(frecvente, '%d '));
disp(' ');
disp('========================================');
disp('          DECODIFICARE LOGICĂ           ');
disp('========================================');
% Extragere direcție
if date(1) == 1
   text_directie = 'dreapta';
else
   text_directie = 'stânga';
end
% Extragere și conversie unghi (biții 2-10)
biti_unghi = date(2:end);
puteri = 2.^(length(biti_unghi)-1 : -1 : 0); % [256, 128, 64, ...]
unghi_zecimal = sum(biti_unghi .* puteri);
fprintf('--> COMANDĂ: Rotește la %s cu un unghi de %d de grade.\n', text_directie, unghi_zecimal);
disp('========================================');
% ==========================================
% 4. Redare Sunet și Vizualizare
% ==========================================
disp('Se redă sunetul generat...');
sound(semnal_final, Fs); % Redă semnalul în difuzoare
