#include <SPI.h>
#include <TMC2130Stepper.h>
#include <AccelStepper.h>

// Definitie pini
#define EN_PIN 4 // Enable
#define DIR_PIN 2 // Direcție
#define STEP_PIN 3 // Step
#define CS_PIN 10 // Chip Select pentru SPI
#define R_SENSE 0.11f // Rezistența de detecție standard de pe modul (nu stiu ce inseamna valoarea, ar trebui sa caut)

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
  stepper.setMaxSpeed(3200);     // Viteza max: 3200 pași/sec (1 turație/sec)
  stepper.setAcceleration(1600); // Accelerare: 1600 pași/sec² (ajunge la viteză max în 2 sec)
  
  // Setați poziția inițială ca fiind punctul 0 (Origine / Home)
  stepper.setCurrentPosition(0);
}

void loop() {
  stepper.move(800); //asta e distanta noua propusa
  // 800 inseamna 3200 * (90/360), adica rotire cu 90 de grade
  // move cu nr pozitiv este rotire in sens trigonometric (antiorar) - vom folosi 1 la DIR
  // move cu nr negativ este rotire in sens antitrigonometric (orar) - vom folosi 0 la DIR
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  delay(10000);
  /*
  stepper.move(-800);
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  delay(10000);*/

}
