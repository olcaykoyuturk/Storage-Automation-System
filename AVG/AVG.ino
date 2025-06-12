// LIBRARY
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

// Web
const char* ssid = "ESP32-AP";
const char* password = "12345678";

WebSocketsClient webSocket;

const char* deviceID = "avg1";  // Cihaz kimliği

// 74HC4051 Multiplexer pinleri
#define S0 21
#define S1 22
#define S2 23
#define Z_PIN 36

// Motor A pinleri
const int PWMA = 14;
const int AIN1 = 12;
const int AIN2 = 13;

// Motor B pinleri
const int PWMB = 25;
const int BIN1 = 26;
const int BIN2 = 27;

const int STBY = 33;

// PWM ayarları
const int freq = 1000;
const int pwmChannelA = 0;
const int pwmChannelB = 1;
const int resolution = 8;

// Servo motor pinleri
const int servoYukariAsagi = 17;
const int servoSagSol = 16;

// PWM ayarları
const int Servo_pwmChannel1 = 2;  // Yukarı-Aşağı
const int Servo_pwmChannel2 = 3;  // Sağ-Sol
const int Servo_freq = 50;        // Servo için 50 Hz
const int Servo_resolution = 12;  // 12 bit çözünürlük (0-4095)
const int Servo_minUs = 500;      // 0 derece için mikro saniye
const int Servo_maxUs = 2500;     // 180 derece için mikro saniye

// PID
float Kp = 0.06;
float Kd = 0.12;
int oncekiHata = 0;
unsigned int speed = 120;

// MAP
int8_t x = 0;
int8_t y = 0;
int8_t target_x = 0;
int8_t target_y = 0;

bool hedefeUlasildi = true;

enum Yon { KUZEY,
           GUNEY,
           DOGU,
           BATI };

Yon yon = KUZEY;

// LINE POZISYON
unsigned int sonPozisyon = 3500;

// TIME
unsigned long sonCollisionZamani = 0;

// LINE PINS
uint16_t sensorDegerleri[8];

void setup() {
  Serial.begin(115200);

  pinMode(2, OUTPUT);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  // MOTOR
  ledcSetup(pwmChannelA, freq, resolution);
  ledcAttachPin(PWMA, pwmChannelA);

  ledcSetup(pwmChannelB, freq, resolution);
  ledcAttachPin(PWMB, pwmChannelB);

  digitalWrite(STBY, HIGH);

  // SERVO
  ledcSetup(Servo_pwmChannel1, Servo_freq, Servo_resolution);
  ledcAttachPin(servoSagSol, Servo_pwmChannel1);

  ledcSetup(Servo_pwmChannel2, Servo_freq, Servo_resolution);
  ledcAttachPin(servoYukariAsagi, Servo_pwmChannel2);

  // Servo başlangıç pozisyonları
  setServoAngle(Servo_pwmChannel1, 90);  // Yukarı-Aşağı ortada
  setServoAngle(Servo_pwmChannel2, 90);  // Sağ-Sol ortada
  delay(500); // Pozisyona gelmesi için bekle
  stopServo(Servo_pwmChannel1);
  stopServo(Servo_pwmChannel2);

  delay(100);
  connectToWiFi();
  delay(100);
  setupWebSocket();

  // BEKLEME NOKTASI
  for (int i = 0; i < 10; i++) {
    digitalWrite(2, HIGH);
    delay(200);
    digitalWrite(2, LOW);
    delay(200);
  }
}

void loop() {
  webSocket.loop();

  if (hedefeUlasildi) {
    motorAll_dur();
    return;
  }

  // CIZGI SENSORU
  sensorleriOku();
  unsigned int cizgiPozisyonu = readLineBlack(sensorDegerleri);

  // HAREKET
  collision();
  PID(cizgiPozisyonu);
}