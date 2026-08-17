#include <stdint.h>
#include <avr/interrupt.h>

#define NUM_FREQS 7
#define NO_SAMPLES 250
#define SAMPLING_FREQ 5000

const int16_t COEFFS_Q8[NUM_FREQS] = {448, 414, 373, 326, 274, 218, 158}; // Coeficientii shiftati pt 400...1000Hz
const int16_t DC_OFFSET = 338; // Valoare medie de offset masurata anterior

// Vectori volatili care se modifică la fiecare întrerupere
volatile int32_t q0[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0}; 
volatile int32_t q1[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0};
volatile int32_t q2[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0};
volatile uint16_t sample_count = 0;
volatile bool block_ready = false;

int32_t final_q1[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0}; 
int32_t final_q2[NUM_FREQS] = {0, 0, 0, 0, 0, 0, 0};

// Stările mașinii de stări
enum State {
  STATE_IDLE,
  STATE_EXPECT_900,
  STATE_COLLECTING,
  STATE_EXPECT_STOP_800
};

State currentState = STATE_IDLE;
uint32_t start_timer = 0; // Cronometru pentru timeout-ul de 500ms

// Buffer pentru stocarea biților recepționați
char bitBuffer[64];
int8_t bitBufferIndex = 0;
int8_t last_processed_index = -1;

void setup() {
  Serial.begin(115200);

  ADCSRA &= ~(bit(ADPS2) | bit(ADPS1) | bit(ADPS0)); // b2b1b0 = 000
  ADCSRA |= bit(ADPS2) | bit(ADPS0); // b2b1b0 = 101 <=> prescaler 32 <=> ceas ADC = 500kHz

  cli(); 
  TCCR1A = 0; 
  TCCR1B = 0;
  TCNT1 = 0; 
  
  // Mod CTC (Clear Timer on Compare Match) pentru 5000Hz
  OCR1A = 3199; // (16.000.000 / (1 * 5000)) - 1 = 3199
  
  TCCR1B |= (1 << WGM12); 
  TCCR1B |= (1 << CS10); // Prescaler 1 - ceas la 16MHz
  TIMSK1 |= (1 << OCIE1A); 
  sei(); 
}

ISR(TIMER1_COMPA_vect) { 
  if (block_ready) return; 
  
  int16_t current_sample = analogRead(A0) - DC_OFFSET;
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

void loop() {
  if (block_ready) {
    int64_t max_magnitude = -1;
    int8_t index_frec_detectat = -1;

    // Calculăm energia pentru toate cele 7 frecvențe
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
    bool signal_detected = (display_mag > 2000); // Prag pentru filtrarea zgomotului

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
            // STOP confirmat! Afișăm mesajul final
            Serial.print("Mesaj transmis: ");
            Serial.println(bitBuffer);
            
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