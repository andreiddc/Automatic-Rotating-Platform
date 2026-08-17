#include <stdint.h>
#include <avr/interrupt.h>

#define NO_SAMPLES 250
#define SAMPLING_FREQ 10000
#define TARGET_FREQ 1000

const int16_t COEFF_Q8 = 414; // coeficient precalculat shiftat cu 8 pozitii
const int16_t DC_OFFSET = 338; // valoare medie de offset masurata anterior

volatile int32_t q0 = 0; // valori volatile care se modifica la fiecare intrerupere
volatile int32_t q1 = 0;
volatile int32_t q2 = 0;
volatile uint16_t sample_count = 0;
volatile bool block_ready = false;

int32_t final_q1 = 0; // valori la finalul etapei iterative
int32_t final_q2 = 0;

void setup() {
  Serial.begin(115200);

  ADCSRA &= ~(bit(ADPS2) | bit(ADPS1) | bit(ADPS0)); // facem b2b1b0 = 000
  ADCSRA |= bit(ADPS2) | bit(ADPS0); // facem b2b1b0 = 101 <=> prescaler 32 <=> ceas ADC = 16MHz/32 = 500kHz

  cli(); // Oprim întreruperile temporar ca sa nu porneasca Timer1 in timp ce ii modificam registrele
  TCCR1A = 0; // Setăm registrele de control la 0
  TCCR1B = 0;
  TCNT1  = 0; // Resetăm contorul la 0
  
  // Mod CTC (Clear Timer on Compare Match)
  // OCR1A = (Clock Freq / (Prescaler * Target Freq)) - 1
  // OCR1A = (16.000.000 / (1 * 10000)) - 1 = 199
  OCR1A = 1599;
  
  TCCR1B |= (1 << WGM12); // Activare mod CTC
  TCCR1B |= (1 << CS10); // Prescaler 1 - ceas la 16MHz
  TIMSK1 |= (1 << OCIE1A); // Activare întrerupere Timer1 Compare
  sei(); // Pornim întreruperile
}

ISR(TIMER1_COMPA_vect) { // rutina de intreruperi declansata o data la 100us
  if (block_ready) return; // Dacă loop() încă procesează datele, sărim peste achiziție

  int16_t current_sample = analogRead(A0) - DC_OFFSET;
  q0 = ((COEFF_Q8 * q1) >> 8) - q2 + current_sample;
  q2 = q1;
  q1 = q0;
  sample_count++;

  if (sample_count >= NO_SAMPLES) { // daca am terminat de citit 250 de esantioane, resetam
    final_q1 = q1;
    final_q2 = q2;
    q1 = 0;
    q2 = 0;
    sample_count = 0;
    block_ready = true; 
  }
}

void loop() {
  if (block_ready) {
    uint32_t start_time = micros();

    int64_t q1_sq = (int64_t)final_q1 * final_q1;
    int64_t q2_sq = (int64_t)final_q2 * final_q2;
    int64_t q1_q2_coeff = ((int64_t)COEFF_Q8 * final_q1 * final_q2) >> 8;

    int64_t magnitude_sq = q1_sq + q2_sq - q1_q2_coeff; // Pitagora generalizata
    int32_t display_mag = (int32_t)(magnitude_sq >> 10);
    
    Serial.print("Energie Semnal (Scalată): ");
    Serial.print(display_mag);
    Serial.print(" | Timp executie calcul final: ");
    uint32_t elapsed_time = micros() - start_time;
    Serial.print(elapsed_time);
    Serial.println(" us");

    block_ready = false; 
  }
}