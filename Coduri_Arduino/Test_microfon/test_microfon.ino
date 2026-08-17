void setup() {
  Serial.begin(115200);
}

void loop() {
  int minVal = 1023;
  int maxVal = 0;
  unsigned long startMillis = millis();

  // Citim senzorul timp de 40ms (suficient pentru a prinde cateva unde complete)
  while (millis() - startMillis < 40) {
    int val = analogRead(A0);
    if (val < minVal) {
      minVal = val;
    }
    if (val > maxVal) {
      maxVal = val;
    }
  }

  // Amplitudinea este diferenta dintre cel mai inalt si cel mai de jos punct
  int amplitudine = maxVal - minVal;

  // Trimitem 3 valori catre Plotter: Limita Jos (0), Limita Sus (1023) si Amplitudinea
  Serial.print("0 ");
  Serial.print("1023 ");
  Serial.println(amplitudine);
}
