#include <WiFi.h>
#include <MQTT.h>
#include <NusabotSimpleTimer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHTesp.h>
#include <Wire.h>
#include <BH1750.h>

// ================= PENGATURAN PIN BARU =================
#define DHTPIN 33           // DHT11 Data
#define ONE_WIRE_BUS 14     // DS18B20 Data
#define TRIG_PIN 5          // HC-SR04 Trig (Pengganti waterPin analog)
#define ECHO_PIN 18         // HC-SR04 Echo                 
#define TDS_PIN 34          // TDS Analog Read
#define TDS_POWER_PIN 32    // TDS VCC (Untuk nyala/mati otomatis)
#define VOLTAGE_PIN 35      // Voltage Sensor Signal
#define RELAY_A_PIN 27 // Pompa Nutrisi A (20 detik)
#define RELAY_B_PIN 26 // Pompa Nutrisi B (5 detik)

// ================= OBJEK SENSOR =================
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHTesp dht;
BH1750 lightMeter;

// ================= KONFIGURASI WIFI & MQTT =================
const char ssid[] = "."; //[cite: 1]
const char pass[] = "88888888"; //[cite: 1]
const char broker[] = "broker.emqx.io"; //[cite: 1]
const char clientID[] = "ESP32_Alden_12345A"; //[cite: 1]

WiFiClient net; //[cite: 1]c
MQTTClient client; //[cite: 1]
NusabotSimpleTimer timer; //[cite: 1]
const float VOLTAGE_MULTIPLIER = 5.0; // Kalibrasi sensor baterai

void publishWaterLevel() {
  // Menggunakan sensor Ultrasonik sebagai pengganti analog pin
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  float distance = pulseIn(ECHO_PIN, HIGH) * 0.034 / 2;

  char msg[10];
  snprintf(msg, sizeof(msg), "%.1f", distance); //[cite: 1]

  Serial.print("Jarak Air: ");
  Serial.print(msg); Serial.println(" cm");
  client.publish("pstdh/air", msg, strlen(msg)); //[cite: 1]
}

void publishWaterTemp() {
  sensors.requestTemperatures(); //[cite: 1]
  float Air = sensors.getTempCByIndex(0); //[cite: 1]
  // if (Air == DEVICE_DISCONNECTED_C) return; //[cite: 1]

  char msg[10];
  snprintf(msg, sizeof(msg), "%.1f", Air); //[cite: 1]
  Serial.print("Suhu Air: ");
  Serial.print(msg); Serial.println(" C");
  client.publish("pstdh/suhuair", msg, strlen(msg)); //[cite: 1]
}

void publishAirTemp() {
  // Menggunakan DHTesp untuk mencegah crash
  TempAndHumidity dhtData = dht.getTempAndHumidity();
  if (isnan(dhtData.temperature)) return;

  char msg[10];
  snprintf(msg, sizeof(msg), "%.1f", dhtData.temperature); //[cite: 1]
  Serial.print("Suhu Udara: ");
  Serial.print(msg); Serial.println(" C");
  client.publish("pstdh/suhuudara", msg, strlen(msg)); //[cite: 1]
}

void publishTDS() {
  // 1. Nyalakan TDS
  digitalWrite(TDS_POWER_PIN, HIGH);
  delay(500); // Jeda stabilisasi
  
  // 2. Baca Data
  int adcValue = analogRead(TDS_PIN); //[cite: 1]
  
  // 3. Matikan TDS langsung
  digitalWrite(TDS_POWER_PIN, LOW);

  // Rumus kalibrasi milikmu 
  float voltage = adcValue * (3.3 / 4095.0); //[cite: 1]
  float tdsValue = (750.0 * voltage) / 2.0; //[cite: 1]
  float tdsvlu = 2.2 * tdsValue; //[cite: 1]

  char msg[10];
  snprintf(msg, sizeof(msg), "%.1f", tdsvlu); //[cite: 1]

  Serial.print("TDS: ");
  Serial.print(msg); Serial.println(" ppm");
  client.publish("pstdh/tds", msg, strlen(msg)); //[cite: 1]
}

void publishLux() {
  float lux = lightMeter.readLightLevel();
  char msg[10];
  snprintf(msg, sizeof(msg), "%.1f", lux);
  
  Serial.print("Cahaya: ");
  Serial.print(msg); Serial.println(" Lux");
  client.publish("pstdh/cahaya", msg, strlen(msg));
}

void publishBattery() {
  int adcVoltase = analogRead(VOLTAGE_PIN);
  float teganganPin = (adcVoltase / 4095.0) * 3.3;
  float teganganBaterai = teganganPin * VOLTAGE_MULTIPLIER;
  
  char msg[10];
  snprintf(msg, sizeof(msg), "%.2f", teganganBaterai);
  
  Serial.print("Baterai: ");
  Serial.print(msg); Serial.println(" V");
  client.publish("pstdh/baterai", msg, strlen(msg));
}

void messageReceived(String &topic, String &payload) {
  Serial.println("Pesan masuk [" + topic + "]: " + payload);

  // Jika pesan masuk ke topik isi pupuk
  if (topic == "pstdh/isi_pupuk") {
    
    // Cek kecocokan password
    if (payload == "ISI50ML") {
      Serial.println("KODE BENAR! Menyalakan pompa pupuk 20 detik.");
      
      // Nyalakan Relay (Berdasar
      digitalWrite(RELAY_A_PIN, LOW);
      digitalWrite(RELAY_B_PIN, LOW);
      delay(30000);
      digitalWrite(RELAY_A_PIN, HIGH);
      digitalWrite(RELAY_B_PIN, HIGH);
      

    } else {
      Serial.println("KODE SALAH! Pompa tidak diaktifkan.");
    }
  }
}

void connect() { //[cite: 1]
  Serial.print("Menghubungkan ke WiFi...");
  while (WiFi.status() != WL_CONNECTED) { //[cite: 1]
    delay(500); //[cite: 1]
    Serial.print("."); //[cite: 1]
  }
  Serial.println("\nWiFi Terhubung!");

  Serial.print("Menghubungkan ke MQTT...");
  while (!client.connect(clientID)) { //[cite: 1]
    delay(500); //[cite: 1]
    Serial.print("."); //[cite: 1]
  }
  Serial.println("\nMQTT Terhubung!");

  client.subscribe("pstdh/#"); //[cite: 1]
  client.publish("pstdh/12345A", "Terhubung", true, 1); //[cite: 1]
}

void setup() { //[cite: 1]
  Serial.begin(115200); //[cite: 1]
  Wire.begin(21, 22); // Untuk I2C BH1750

  client.onMessage(messageReceived);

  // Inisialisasi Perangkat Keras
  dht.setup(DHTPIN, DHTesp::DHT11);
  sensors.begin(); //[cite: 1]
  lightMeter.begin();
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  pinMode(TDS_POWER_PIN, OUTPUT);
  digitalWrite(TDS_POWER_PIN, LOW); // TDS mati secara default
  
  pinMode(RELAY_A_PIN, OUTPUT);
  pinMode(RELAY_B_PIN, OUTPUT);
  digitalWrite(RELAY_A_PIN, HIGH);
  digitalWrite(RELAY_B_PIN, HIGH);
  
  // Inisialisasi Jaringan
  WiFi.begin(ssid, pass); //[cite: 1]
  client.begin(broker, net); //[cite: 1]
  client.setWill("pstdh/12345A", "Terputus", true, 1); //[cite: 1]

  connect(); //[cite: 1]

  // Pengaturan Timer Publikasi MQTT
  timer.setInterval(2000, publishWaterLevel); //[cite: 1]
  timer.setInterval(2000, publishTDS); //[cite: 1]
  timer.setInterval(2000, publishWaterTemp); //[cite: 1]
  timer.setInterval(2000, publishAirTemp); //[cite: 1]
  timer.setInterval(2000, publishLux);
  timer.setInterval(2000, publishBattery);
}

void loop() { //[cite: 1]
  client.loop(); //[cite: 1]
  timer.run(); //[cite: 1]

  if (!client.connected()) { //[cite: 1]
    connect(); //[cite: 1]
  }
  delay(10); //[cite: 1]
}