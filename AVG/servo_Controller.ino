// Dereceyi PWM duty'ye çeviren yardımcı fonksiyon
void setServoAngle(int channel, int angle) {
  angle = constrain(angle, 0, 180);
  int us = map(angle, 0, 180, Servo_minUs, Servo_maxUs);
  int duty = (int)(us * ((1 << Servo_resolution) - 1) / (1000000.0 / Servo_freq));
  ledcWrite(channel, duty);
}

// Servo motoru yavaş yavaş hareket ettir
void moveServoSlow(int channel, int startAngle, int endAngle, int delayMs) {
  int step = (startAngle < endAngle) ? 1 : -1;
  for (int angle = startAngle; angle != endAngle; angle += step) {
    setServoAngle(channel, angle);
    delay(delayMs);
  }
  setServoAngle(channel, endAngle); // Son açıyı da ver
}

// PWM sinyalini durdurarak servo motoru serbest bırak
void stopServo(int channel) {
  ledcWrite(channel, 0);
}

void solYukAl() {
  moveServoSlow(Servo_pwmChannel2, 90, 0, 10);
  stopServo(Servo_pwmChannel2);
  delay(2000);

  moveServoSlow(Servo_pwmChannel1, 90, 150, 10);
  delay(2000);

  moveServoSlow(Servo_pwmChannel1, 150, 90, 10);
  delay(2000);

  moveServoSlow(Servo_pwmChannel2, 0, 90, 10);
  stopServo(Servo_pwmChannel2);
}