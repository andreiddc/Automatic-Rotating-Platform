#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>

// Definitie pini
#define EN_PIN 4 // Enable
#define DIR_PIN 2 // Direcție
#define STEP_PIN 3 // Step
#define CS_PIN 10 // Chip Select pentru SPI
#define R_SENSE 0.11f // Rezistența de detecție standard de pe modul (nu stiu ce inseamna valoarea, ar trebui sa caut)

// --- Variabile pentru Testul de Stres ---
int iteratia_curenta = 0;
const int MAX_ITERATII = 400; // 400 de rotiri x 90 grade = 100 de ture complete

// Inițializare driver în modul Hardware SPI
TMC2130Stepper driver(CS_PIN);

// 2. Inițializare AccelStepper în modul DRIVER (folosește un pin STEP și un pin DIR)
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

void setup() {
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // Dezactivat temporar

  // --- CONFIGURARE DRIVER TMC2130 PRIN SPI ---
  SPI.begin();
  driver.begin();
  driver.rms_current(800, R_SENSE); // 800mA RMS
  driver.stealthChop(1);            // Mod ultra-silențios
  driver.en_pwm_mode(1);
  driver.microsteps(16);            // 1/16 micropași (3200 pași = 1 perioada)

  digitalWrite(EN_PIN, LOW); // Activăm driverul

  // --- CONFIGURARE CINEMATICĂ ACCELSTEPPER ---
  stepper.setMaxSpeed(3200); // Viteza max: 3200 pași/sec (1 tură/sec)
  stepper.setAcceleration(1600); // Accelerare: 1600 pași/sec² (ajunge la viteză max în 2 sec)
  stepper.setCurrentPosition(0); // Setați poziția inițială ca fiind punctul 0 (Origine / Home)

  Serial.println("=========================================");
  Serial.println("START TEST STRES: 400 x 90 Grade");
  Serial.println("Motorul va porni in 3 secunde...");
  Serial.println("=========================================");
  delay(3000); 
}

void loop() {
  // Executăm doar atâta timp cât nu am atins limita de 400 de mișcări
  if (iteratia_curenta < MAX_ITERATII) {
    
    // Setăm următoarea destinație relativă (+800 pași = +90 grade)
    stepper.move(800);
    
    // Afișăm progresul în Serial Monitor
    Serial.print("Execut iteratia: ");
    Serial.print(iteratia_curenta + 1);
    Serial.println(" / 400");

    // Mișcăm platforma până ajunge la destinație
    while (stepper.distanceToGo() != 0) {
      stepper.run();
    }
    
    iteratia_curenta++;
    
    // Pauză de 0.5 secunde între mișcări pentru a simula oprirea la măsurătoare
    delay(500); 
    
  } else if (iteratia_curenta == MAX_ITERATII) {
    // Am terminat testul
    Serial.println("=========================================");
    Serial.println("TEST FINALIZAT CU SUCCES!");
    Serial.println("Au fost executate 100 de ture complete.");
    Serial.println("Poti verifica acum alinierea fizica a platformei.");
    Serial.println("=========================================");
    
    // Incrementăm pentru a bloca intrarea repetată în acest mesaj
    iteratia_curenta++; 
  }
}
