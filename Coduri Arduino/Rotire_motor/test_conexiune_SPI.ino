#include <SPI.h>
#include <TMC2130Stepper.h>

#define EN_PIN           4
#define DIR_PIN          2
#define STEP_PIN         3
#define CS_PIN          10
#define R_SENSE       0.11f

TMC2130Stepper driver(CS_PIN);

void setup() {
  Serial.begin(9600);
  while(!Serial); // Așteaptă deschiderea Serial Monitor

  pinMode(EN_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // Dezactivat inițial

  SPI.begin();
  driver.begin();
  driver.rms_current(700, R_SENSE);
  driver.stealthChop(1);
  driver.en_pwm_mode(1);
  driver.microsteps(16);

  digitalWrite(EN_PIN, LOW); // Activăm driverul

  Serial.println("=================================");
  Serial.println("   DIAGNOZĂ COMUNICAȚIE SPI      ");
  Serial.println("=================================");

  // Citim registrul de stare al driverului
  uint32_t drv_status = driver.DRV_STATUS();
  
  Serial.print("Răspuns raw DRV_STATUS: 0x");
  Serial.println(drv_status, HEX);

  if (drv_status == 0x00000000 || drv_status == 0xFFFFFFFF) {
    Serial.println("\n❌ EROARE: Comunicație SPI eșuată!");
    Serial.println("Arduino NU recepționează niciun semnal de la TMC2130.");
  } else {
    Serial.println("\n✅ SUCCES: Driverul TMC2130 răspunde prin SPI!");
  }
}

void loop() {
  // Trimitem câțiva pași de test
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(500);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(500);
}
