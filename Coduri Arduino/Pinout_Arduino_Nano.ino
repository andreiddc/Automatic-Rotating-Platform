// Definitie pini
#define EN_PIN 4 // Enable
#define DIR_PIN 2 // Direcție
#define STEP_PIN 3 // Step
#define CS_PIN 10 // Chip Select pentru SPI

#define R_SENSE       0.11f // Rezistența de detecție standard de pe modul

// Inițializare driver în modul Hardware SPI
TMC2130Stepper driver(CS_PIN);
