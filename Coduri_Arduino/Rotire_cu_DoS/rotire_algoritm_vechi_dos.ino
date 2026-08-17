#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>

// Definitie pini
#define MIC_PIN A0 // Out microfon
#define EN_PIN 4 // Enable
#define DIR_PIN 2 // Direcție
#define STEP_PIN 3 // Step
#define CS_PIN 10 // Chip Select pentru SPI
#define R_SENSE 0.11f // Rezistența de detecție standard de pe modul (scrie R110 pe driver)

// Inițializare driver în modul Hardware SPI
TMC2130Stepper driver(CS_PIN);

// Inițializare AccelStepper în modul DRIVER (folosește un pin STEP și un pin DIR)
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

int N = 80; // 80 de esantioane la o citire, in 20 de ms cu sample rate 4000Hz (perioada de esantionare 250us)
int samples[80]; 

float coef[7] = {1.618034, 1.414214, 1.175570, 0.907981, 0.618034, 0.312869, 0.000000}; // coeficientii precalculati - vezi algoritmul Goertzel
int freqs[7] = {400, 500, 600, 700, 800, 900, 1000};
float energies[7];

enum State { IDLE, EXPECT_900_START, RECEIVE_DATA, EXPECT_900_STOP, EXPECT_800_STOP };
State stare = IDLE;

unsigned long state_timer = 0;
unsigned long symbol_timer = 0;

int scor_0 = 0;
int scor_1 = 0;
int biti_receptionati = 0;
String mesaj_binar = "";

// Nivel de zgomot dinamic si praguri decuplate
float noise_floor = 1000.0;
const float k_noise_data = 3.5; // Prag mai relaxat pentru bitii de date (400-700Hz)
const float k_noise_control = 6.0; // Prag sever pentru START/STOP (800-1000Hz)

void setup() {
  Serial.begin(115200);

  // Overclock ADC 
  ADCSRA = (ADCSRA & 0xf8) | 0x04;  // 13us conversie ADC microfon in loc de 110us standard
  
  Serial.println("==========================================================");
  Serial.println("Receptor ACTIV: Mod Funcțional");
  Serial.println("Motorul este conectat. Astept START (1000->900)...");
  Serial.println("==========================================================");

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // Dezactivat temporar

  // --- CONFIGURARE DRIVER TMC2130 PRIN SPI ---
  SPI.begin();
  driver.begin();
  driver.rms_current(800, R_SENSE); // 800mA RMS
  driver.stealthChop(1); // Mod ultra-silențios
  driver.en_pwm_mode(1);
  driver.microsteps(16); // 1/16 micropași (3200 pași = 1 perioada)

  digitalWrite(EN_PIN, LOW); // Activăm driverul

  // --- CONFIGURARE CINEMATICĂ ACCELSTEPPER ---
  stepper.setMaxSpeed(3200); // Viteza max: 3200 pași/sec (1 tură/sec)
  stepper.setAcceleration(1600); // Accelerare: 1600 pași/sec² (ajunge la viteză max în 2 sec)
  
  // Setați poziția inițială ca fiind punctul 0 (Origine / Home)
  stepper.setCurrentPosition(0);

}

void loop() {
  unsigned long start_time;

  // 1. ACHIZITIE BUFFERIZATA
  for (int i = 0; i < N; i++) {
    start_time = micros();
    samples[i] = analogRead(MIC_PIN);
    while (micros() - start_time < 250); 
  }

  // 2. OFFSET DINAMIC
  long suma = 0;
  for (int i = 0; i < N; i++) suma += samples[i];
  float offset_DC = (float)suma / (float)N;

  // 3. CALCUL GOERTZEL
  for (int f = 0; f < 7; f++) {
    float s_prev = 0.0, s_prev2 = 0.0;
    for (int i = 0; i < N; i++) {
      float x = samples[i] - offset_DC;
      float s = x + coef[f] * s_prev - s_prev2;
      s_prev2 = s_prev;
      s_prev = s;
    }
    energies[f] = (s_prev2 * s_prev2) + (s_prev * s_prev) - (coef[f] * s_prev * s_prev2);
  }

  // 4. IDENTIFICARE FRECVENTA & PRAGURI DUBLE
  float max_E = 0;
  float second_max_E = 0;
  int freq_detectata = 0;

  for (int i = 0; i < 7; i++) {
    if (energies[i] > max_E) {
      second_max_E = max_E;
      max_E = energies[i];
      freq_detectata = freqs[i];
    } else if (energies[i] > second_max_E) {
      second_max_E = energies[i];
    }
  }

  // Modificam pragul dinamic de liniste
  if (max_E < noise_floor * 2.0) {
    noise_floor = 0.9 * noise_floor + 0.1 * max_E; 
  }
  
  // Calculam ambele praguri
  float thresholdData = noise_floor * k_noise_data;
  float thresholdControl = noise_floor * k_noise_control;
  
  // Alegem pragul curent in functie de frecventa detectata
  float prag_curent;
  if (freq_detectata == 800 || freq_detectata == 900 || freq_detectata == 1000) {
    prag_curent = thresholdControl;
  } else {
    prag_curent = thresholdData;
  }

  // Validam folosind pragul ales specific
  if (!(max_E > prag_curent && max_E > 2000.0 && max_E >= 1.5 * second_max_E)) {
    freq_detectata = 0; 
  }

  // 5. MASINA DE STARI
  switch (stare) {
    
    case IDLE:
      if (freq_detectata == 1000) {
        stare = EXPECT_900_START;
        state_timer = millis();
      }
      break;

    case EXPECT_900_START: 
      if (freq_detectata == 900) {
        if (millis() - state_timer <= 250) {
          stare = RECEIVE_DATA;
          biti_receptionati = 0;
          scor_0 = 0; scor_1 = 0;
          mesaj_binar = "";
          symbol_timer = millis(); 
        } else {
          stare = IDLE; 
        }
      } else if (millis() - state_timer > 250) {
        stare = IDLE;
      }
      break;

    case RECEIVE_DATA: 
      if (millis() - symbol_timer >= 200) { 
        if (scor_0 > scor_1) mesaj_binar += "0";
        else if (scor_1 > scor_0) mesaj_binar += "1";
        else mesaj_binar += "E"; // Bit eronat/indecis

        biti_receptionati++;

        scor_0 = 0; scor_1 = 0;
        symbol_timer += 200; 

        // Modificare: Asteptam 30 de biti (3 cadre x 10 biti)
        if (biti_receptionati == 30) {
          stare = EXPECT_900_STOP;
          state_timer = millis();
        }
      } else {
        if (freq_detectata == 400 || freq_detectata == 500) scor_0++;
        if (freq_detectata == 600 || freq_detectata == 700) scor_1++;
      }
      break;

    case EXPECT_900_STOP:
      if (freq_detectata == 900) {
        stare = EXPECT_800_STOP;
        state_timer = millis();
      } else if (millis() - state_timer > 300) {
        stare = IDLE;
      }
      break;

    case EXPECT_800_STOP:
      if (freq_detectata == 800 && (millis() - state_timer <= 250)) {

        // --- LOGICA DE VOT MAJORITAR ---
        String cadru1 = mesaj_binar.substring(0, 10);
        String cadru2 = mesaj_binar.substring(10, 20);
        String cadru3 = mesaj_binar.substring(20, 30);
        String rezultat_final = "";
        bool pachet_valid = true;

        for (int i = 0; i < 10; i++) {
            int voturi_1 = 0;
            int voturi_0 = 0;
            
            if (cadru1[i] == '1') voturi_1++; else if (cadru1[i] == '0') voturi_0++;
            if (cadru2[i] == '1') voturi_1++; else if (cadru2[i] == '0') voturi_0++;
            if (cadru3[i] == '1') voturi_1++; else if (cadru3[i] == '0') voturi_0++;

            if (voturi_1 >= 2) {
                rezultat_final += "1";
            } else if (voturi_0 >= 2) {
                rezultat_final += "0";
            } else {
                pachet_valid = false;
                break;
            }
        }

        if (!pachet_valid) {
            Serial.println("[EROARE CRITICA] Comanda respinsa, pachet invalid.");
        } else {
            // Extragem valorile
            int directie = rezultat_final.charAt(0) - '0';
            String binar_unghi = rezultat_final.substring(1);
            long unghi_zecimal = strtol(binar_unghi.c_str(), NULL, 2);
            
            Serial.println("--- REZULTAT IN URMA VOTULUI ---");
            Serial.print("Pachet Corectat: "); Serial.println(rezultat_final);
            Serial.print("Directie (MSB) : "); Serial.println(directie);
            Serial.print("Unghi Zecimal  : "); Serial.println(unghi_zecimal);

            // 6. TRIMITEREA COMENZII CATRE MOTOR
            if (!(unghi_zecimal >= 0 && unghi_zecimal <= 360)) {
                Serial.println("[EROARE] Unghiul este invalid (>360).");
            } else {
                float com_move = 3200.0 * (float)unghi_zecimal / 360.0;
                
                if (directie == 0) {
                    com_move = com_move * (-1.0);
                }

                stepper.move(com_move);

                while (stepper.distanceToGo() != 0) {
                    stepper.run();
                }
                
                Serial.println("--> Rotatie completa!");
            }
        }

        Serial.println("--------------------------------------\nAstept START nou...");
        stare = IDLE;
      } else if (millis() - state_timer > 250) {
        stare = IDLE;
      }
      break;
  }

}
