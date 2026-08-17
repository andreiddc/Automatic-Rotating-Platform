const int N = 250;
int samples[N]; 

void setup() {
  Serial.begin(115200);
  // 1. Curățarea biților de prescaler existenți în registrul ADCSRA
  ADCSRA &= ~(bit(ADPS2) | bit(ADPS1) | bit(ADPS0)); 

  // 2. Setarea prescaler-ului la valoarea 64
  ADCSRA |= bit(ADPS2) | bit(ADPS1); 
}

void loop() {
  unsigned long start_time;
  unsigned long end_time;

  start_time = micros();
  for (int i = 0; i < N; i++) {
    samples[i] = analogRead(A0); //52 us pe citire
  }
  end_time = micros();

  Serial.print("Perioada de esantionare: "); Serial.print(end_time - start_time); Serial.println(" us");
  Serial.print("Numar de esantioane citite intr-o perioada: "); Serial.println(N);
  float sample_rate = (float)N / (float)(end_time - start_time);
  Serial.print("Frecventa de esantionare: "); Serial.print(sample_rate * 1000000); Serial.println("Hz");
  Serial.println("----------------------------------------------------------------");

  delay(2000);
}