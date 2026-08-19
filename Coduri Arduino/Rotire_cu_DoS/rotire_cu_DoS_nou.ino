#include <stdint.h>
#include <avr/interrupt.h>
#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>

#define NUM_FREQS 7
#define NO_SAMPLES 250
#define SAMPLING_FREQ 5000

#define MIC_PIN A0 // Pin microfon
#define EN_PIN 4 // Enable
#define DIR_PIN 2 // Direcție
#define STEP_PIN 3 // Step
#define CS_PIN 10 // Chip Select pentru SPI
#define R_SENSE 0.11f // Valoarea rezistentei de detectie de pe placuta

// Indecși pentru frecvențe: 400Hz-0, 500Hz-1, 600Hz-2, 700Hz-3, 800Hz-4, 900Hz-5, 1000Hz-6
const int16_t COEFFS_Q8[NUM_FREQS] = {448, 414, 373, 326, 274, 218, 158}; // Coeficientii shiftati pt 400...1000Hz
const int16_t DC_OFFSET = 338; // Valoare medie de offset masurata anterior

volatile int32_t q0[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0}; 
volatile int32_t q1[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0};
volatile int32_t q2[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0};
volatile uint16_t sample_count = 0;
volatile bool block_ready = false;

int32_t final_q1[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0}; 
int32_t final_q2[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0};

enum State {
  STATE_IDLE,
  STATE_EXPECT_900,
  STATE_COLLECTING,
  STATE_EXPECT_STOP_800
};

State currentState = STATE_IDLE;
uint32_t start_timer = 0; // Cronometru pentru timeout-ul de 500ms

char bitBuffer[16]; // Buffer pentru stocarea biților recepționați
int8_t bitBufferIndex = 0;
int8_t last_processed_index = -1;

TMC2130Stepper driver(CS_PIN); // Inițializare driver în modul Hardware SPI
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN); // Inițializare AccelStepper în modul DRIVER


void setup() {
  Serial.begin(115200);

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // Dezactivat temporar

  SPI.begin();
  driver.begin();
  driver.rms_current(800, R_SENSE); // 800mA RMS
  driver.stealthChop(1); // Mod ultra-silențios
  driver.en_pwm_mode(1);
  driver.microsteps(16); // 1/16 micropași (3200 pași = 1 perioada)

  digitalWrite(EN_PIN, LOW); // Activăm driverul
  
  stepper.setMaxSpeed(3200); // viteza max: 3200 pași/sec (1 tura/sec)
  stepper.setAcceleration(1600); // accelerare: 1600 pași/sec^2 (ajunge la viteză max în 2 sec)
  stepper.setCurrentPosition(0); // setam pozitia initiala (origine/home)

  ADCSRA &= ~(bit(ADPS2) | bit(ADPS1) | bit(ADPS0)); // b2b1b0 = 000
  ADCSRA |= bit(ADPS2) | bit(ADPS0); // b2b1b0 = 101 <=> prescaler 32 <=> ceas ADC = 500kHz

  cli(); 
  TCCR1A = 0; 
  TCCR1B = 0;
  TCNT1 = 0; 
  // Mod CTC (Clear Timer on Compare Match) pentru a obține fs=5000Hz datorită resetului la timer
  OCR1A = 3199; // (16.000.000 / (1 * 5000)) - 1 = 3199
  TCCR1B |= (1 << WGM12); 
  TCCR1B |= (1 << CS10); // Prescaler 1 - ceas la 16MHz
  TIMSK1 |= (1 << OCIE1A); 
  sei(); 
}


ISR(TIMER1_COMPA_vect) { 
  if (block_ready) return; 
  
  int16_t current_sample = analogRead(MIC_PIN) - DC_OFFSET;
  for (int8_t i = 0; i < NUM_FREQS; i++) {
    q0[i] = ((COEFFS_Q8[i] * q1[i]) >> 8) - q2[i] + current_sample;
    q2[i] = q1[i];
    q1[i] = q0[i];
  }
  sample_count++;

  if (sample_count >= NO_SAMPLES) { 
    for (int8_t i = 0; i < NUM_FREQS; i++) {
      final_q1[i] = q1[i];
      final_q2[i] = q2[i];
      q1[i] = 0;
      q2[i] = 0;
    }
    sample_count = 0;
    block_ready = true; 
  }
}


void rotire_DIR_ANG(uint8_t directie, uint16_t unghi) {
  if (unghi > 360) {
    Serial.println("[EROARE] Unghiul este invalid.");
    return;
  }
  int16_t nr_pasi = (80 * unghi) / 9; // nr_pasi = 3200 * unghi / 360
  
  // directie == 1 -> Sens trigonometric (antiorar) -> pași pozitivi
  // directie == 0 -> Sens antitrigonometric (orar) -> pași negativi
  if (directie == 0) {
    nr_pasi = -nr_pasi;
  }

  Serial.print("-> Comanda rotire: ");
  Serial.print(unghi);
  Serial.println(" grade");

  stepper.move(nr_pasi); // doar setam tinta, miscarea propriu-zisa e facuta de stepper.run() in loop()
}


// Calculează energia și returnează true dacă s-a detectat un semnal peste prag
bool proceseaza_goertzel(int8_t &index_frec_detectat) {
  int64_t max_magnitude = -1;
  index_frec_detectat = -1;

  for (int8_t i = 0; i < NUM_FREQS; i++) {
    int64_t q1_sq = (int64_t)final_q1[i] * final_q1[i];
    int64_t q2_sq = (int64_t)final_q2[i] * final_q2[i];
    int64_t q1_q2_coeff = ((int64_t)COEFFS_Q8[i] * final_q1[i] * final_q2[i]) >> 8;

    int64_t magnitude_sq = q1_sq + q2_sq - q1_q2_coeff; 
    
    if (magnitude_sq > max_magnitude) {
      max_magnitude = magnitude_sq;
      index_frec_detectat = i;
    }
  }

  int32_t display_mag = (int32_t)(max_magnitude >> 10);
  return (display_mag > 2000); // Prag pentru filtrarea zgomotului
}


void loop() {
  stepper.run(); // APELEAZĂ MEREU stepper.run() PENTRU MIȘCARE NON-BLOCANTĂ!

  if (block_ready) {
    int8_t index_frec_detectat = -1;
    bool signal_detected = proceseaza_goertzel(index_frec_detectat);

    // Mașina de stări asincronă
    switch (currentState) {
      case STATE_IDLE:
        // Așteptăm prima parte a START-ului: 1000Hz (Index 6)
        if (signal_detected && index_frec_detectat == 6) {
          currentState = STATE_EXPECT_900;
          start_timer = millis(); // Pornim cronometrul de 500ms
          last_processed_index = 6;
        }
        break;

      case STATE_EXPECT_900:
        // Verificăm timeout-ul de 500ms
        if (millis() - start_timer > 500) {
          currentState = STATE_IDLE;
          last_processed_index = -1;
          break;
        }
        if (signal_detected) {
          if (index_frec_detectat == 5) { // A doua parte a START-ului: 900Hz (Index 5)
            if (index_frec_detectat != last_processed_index) {
              currentState = STATE_COLLECTING;
              bitBufferIndex = 0;
              bitBuffer[0] = '\0';
              last_processed_index = 5;
            }
          } else if (index_frec_detectat != 6) {
            // Dacă apare altceva în fereastră, revenim în IDLE
            currentState = STATE_IDLE;
            last_processed_index = -1;
          }
        }
        break;

      case STATE_COLLECTING:
        if (signal_detected) {
          if (index_frec_detectat == 5) { // 900Hz - Prima parte a secvenței de STOP
            if (index_frec_detectat != last_processed_index) {
              currentState = STATE_EXPECT_STOP_800;
              last_processed_index = 5;
            }
          } else if (index_frec_detectat >= 0 && index_frec_detectat <= 3) { // Frecvențe de date (400Hz - 700Hz)
            if (index_frec_detectat != last_processed_index) {
              if (bitBufferIndex < 63) {
                if (index_frec_detectat == 0 || index_frec_detectat == 1) {
                  bitBuffer[bitBufferIndex++] = '0'; // Bit 0 sau 0*
                } else if (index_frec_detectat == 2 || index_frec_detectat == 3) {
                  bitBuffer[bitBufferIndex++] = '1'; // Bit 1 sau 1*
                }
                bitBuffer[bitBufferIndex] = '\0';
              }
              last_processed_index = index_frec_detectat;
            }
          }
        } else {
          // Resetăm indexul când nu e semnal, permițând reluarea aceluiași tip de bit ulterior
          last_processed_index = -1;
        }
        break;

      case STATE_EXPECT_STOP_800:
        if (signal_detected) {
          if (index_frec_detectat == 4) { // A doua parte a STOP-ului: 800Hz (Index 4)
            // STOP confirmat! Verificam regula stricta de 10 biti.
            if (bitBufferIndex == 10) {
              Serial.print("Mesaj transmis (Binar): ");
              Serial.println(bitBuffer);
              
              // DECODIFICARE PAYLOAD
              uint8_t directie = bitBuffer[0] - '0'; // Primul caracter devine numar (0 sau 1)
              uint16_t unghi_zecimal = strtol(bitBuffer + 1, NULL, 2); // Citim restul string-ului ca Baza 2
              
              // Executam rotatia (non-blocant)
              rotire_DIR_ANG(directie, unghi_zecimal);
            } else {
              Serial.print("[EROARE] Payload invalid! Numar curent de biti: ");
              Serial.println(bitBufferIndex);
            }
            
            currentState = STATE_IDLE;
            last_processed_index = -1;
          } else if (index_frec_detectat == 5) {
            // Ignorăm al doilea bloc al aceluiași ton de 900Hz
            break;
          } else {
            // Dacă survine altă frecvență, ne întoarcem la colectare
            currentState = STATE_COLLECTING;
            last_processed_index = -1;
          }
        } else {
          currentState = STATE_COLLECTING;
          last_processed_index = -1;
        }
        break;
    }

    block_ready = false;
  }
}
