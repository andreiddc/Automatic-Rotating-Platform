#include <stdint.h>
#include <avr/interrupt.h>

#define NUM_FREQS 2
#define NO_SAMPLES 250
#define SAMPLING_FREQ 10000

const int16_t COEFFS_Q8[NUM_FREQS] = {496, 476}; // coeficientii shiftati pt 400Hz si 600 Hz
const int16_t DC_OFFSET = 338; // valoare medie de offset măsurată anterior

volatile int32_t q0[NUM_FREQS] = {0, 0}; // Vectori volatili care se modifică la fiecare întrerupere
volatile int32_t q1[NUM_FREQS] = {0, 0};
volatile int32_t q2[NUM_FREQS] = {0, 0};
volatile uint16_t sample_count = 0;
volatile bool block_ready = false;

int32_t final_q1[NUM_FREQS] = {0, 0}; // Valori la finalul etapei iterative
int32_t final_q2[NUM_FREQS] = {0, 0};

void setup() {
  Serial.begin(115200);

  ADCSRA &= ~(bit(ADPS2) | bit(ADPS1) | bit(ADPS0)); // facem b2b1b0 = 000
  ADCSRA |= bit(ADPS2) | bit(ADPS0); // facem b2b1b0 = 101 <=> prescaler 32 <=> ceas ADC = 16MHz/32 = 500kHz

  cli(); // Oprim întreruperile temporar
  TCCR1A = 0; // Setăm registrele de control la 0
  TCCR1B = 0;
  TCNT1  = 0; // Resetăm contorul la 0
  
  // Mod CTC (Clear Timer on Compare Match)
  OCR1A = 1599; // OCR1A = (16.000.000 / (1 * 10000)) - 1 = 1599
  
  TCCR1B |= (1 << WGM12); // Activare mod CTC
  TCCR1B |= (1 << CS10); // Prescaler 1 - ceas la 16MHz
  TIMSK1 |= (1 << OCIE1A); // Activare întrerupere Timer1 Compare
  sei(); // Pornim întreruperile
}

ISR(TIMER1_COMPA_vect) { 
  if (block_ready) return; // Dacă loop() încă procesează datele, sărim peste achiziție
  //uint32_t start_time = micros();
  int16_t current_sample = analogRead(A0) - DC_OFFSET;
  for (int8_t i = 0; i < NUM_FREQS; i++) {
    q0[i] = ((COEFFS_Q8[i] * q1[i]) >> 8) - q2[i] + current_sample;
    q2[i] = q1[i];
    q1[i] = q0[i];
  }
  sample_count++;

  if (sample_count >= NO_SAMPLES) { 
    for (int i = 0; i < NUM_FREQS; i++) {
      final_q1[i] = q1[i];
      final_q2[i] = q2[i];
      q1[i] = 0;
      q2[i] = 0;
    }
    sample_count = 0;
    block_ready = true; 
  }
  //uint32_t elapsed_time = micros() - start_time;
  //Serial.println(elapsed_time);
}

void loop() {

  if (block_ready) {
    int64_t max_magnitude = -1;
    int8_t max_index = -1;

    // Calculăm energia pentru ambele frecvențe și determinăm maximul
    for (int i = 0; i < NUM_FREQS; i++) {
      int64_t q1_sq = (int64_t)final_q1[i] * final_q1[i];
      int64_t q2_sq = (int64_t)final_q2[i] * final_q2[i];
      int64_t q1_q2_coeff = ((int64_t)COEFFS_Q8[i] * final_q1[i] * final_q2[i]) >> 8;

      int64_t magnitude_sq = q1_sq + q2_sq - q1_q2_coeff; // Pitagora generalizată
      
      if (magnitude_sq > max_magnitude) {
        max_magnitude = magnitude_sq;
        max_index = i;
      }
    }

    int32_t display_mag = (int32_t)(max_magnitude >> 10);

    if(display_mag > 2000) {
        if (max_index == 0) {
            Serial.print("BIT 0 (400Hz) | Energie max: ");
        }   else if (max_index == 1) {
            Serial.print("BIT 1 (600Hz) | Energie max: ");
        }
        Serial.println(display_mag);
    }
    block_ready = false;
    }
}