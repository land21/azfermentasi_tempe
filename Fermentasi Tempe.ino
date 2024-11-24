#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define KIPAS_PIN 13
#define BME280_ADDRESS 0x76
const char* ssid = "me";
const char* password = "land1322";
const char* mqtt_server = "aztempe.cloud.shiftr.io";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme;

float suhu; // input temperature
float kelembapan; // input humidity
float fan; // output fan speed

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print(".");
    if (client.connect("Alat", "aztempe", "9FPE13J9yYJGmxzY")) {
      Serial.println("Connected to MQTT broker");
      client.subscribe("SRT/#");
    } else {
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  if (!bme.begin(BME280_ADDRESS)) {
    Serial.println("BME280 sensor not detected");
    while (1);
  }  
  pinMode(KIPAS_PIN, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  suhu = bme.readTemperature();
  kelembapan = bme.readHumidity();

  // Fuzzy membership values
  float mx11, mx12, mx13, mx21, mx22, mx23;

  // Suhu Dingin
  mx11 = (suhu <= 33) ? 1 : (suhu >= 36 ? 0 : (36 - suhu) / 3);

  // Suhu Normal
  mx12 = (suhu == 36) ? 1 : (suhu >= 33 && suhu <= 36) ? (suhu - 33) / 3 : (suhu >= 36 && suhu <= 39) ? (39 - suhu) / 3 : 0;

  // Suhu Panas
  mx13 = (suhu >= 39) ? 1 : (suhu <= 36 ? 0 : (suhu - 36) / 3);

  // Kelembapan Rendah
  mx21 = (kelembapan <= 60) ? 1 : (kelembapan >= 65 ? 0 : (65 - kelembapan) / 5);

  // Kelembapan Normal
  mx22 = (kelembapan == 65) ? 1 : (kelembapan >= 60 && kelembapan <= 65) ? (kelembapan - 60) / 5 : (kelembapan >= 65 && kelembapan <= 70) ? (70 - kelembapan) / 5 : 0;

  // Kelembapan Tinggi
  mx23 = (kelembapan >= 70) ? 1 : (kelembapan <= 65 ? 0 : (kelembapan - 65) / 5);

  // Inferensi
  float ar1 = min(mx11, mx21);
  float ar2 = min(mx11, mx22);
  float ar3 = min(mx11, mx23);
  float ar4 = min(mx12, mx21);
  float ar5 = min(mx12, mx22);
  float ar6 = min(mx12, mx23);
  float ar7 = min(mx13, mx21);
  float ar8 = min(mx13, mx22);
  float ar9 = min(mx13, mx23);

  // Defuzzification for fan speed output
  float zr1 = (ar1 == 1) ? 10 : (ar1 == 0 ? 100 : 100 - (ar1 * (100 - 10)));
  float zr2 = (ar2 == 1) ? 10 : (ar2 == 0 ? 100 : 100 - (ar2 * (100 - 10)));
  float zr3 = (ar3 == 1) ? 10 : (ar3 == 0 ? 100 : 100 - (ar3 * (100 - 10)));
  float zr4 = (ar4 == 1) ? 10 : (ar4 == 0 ? 100 : 100 - (ar4 * (100 - 10)));
  float zr5 = (ar5 == 1) ? 10 : (ar5 == 0 ? 100 : 100 - (ar5 * (100 - 10)));
  float zr6 = (ar6 == 1) ? 245 : (ar6 == 0 ? 90 : (ar6 * (245 - 90)) + 90);
  float zr7 = (ar7 == 1) ? 245 : (ar7 == 0 ? 90 : (ar7 * (245 - 90)) + 90);
  float zr8 = (ar8 == 1) ? 245 : (ar8 == 0 ? 90 : (ar8 * (245 - 90)) + 90);
  float zr9 = (ar9 == 1) ? 245 : (ar9 == 0 ? 90 : (ar9 * (245 - 90)) + 90);

  fan = ((ar1 * zr1) + (ar2 * zr2) + (ar3 * zr3) + (ar4 * zr4) + (ar5 * zr5) + (ar6 * zr6) + (ar7 * zr7) + (ar8 * zr8) + (ar9 * zr9)) / 
        (ar1 + ar2 + ar3 + ar4 + ar5 + ar6 + ar7 + ar8 + ar9);

  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.print(" C, Kelembaban: ");
  Serial.print(kelembapan);
  Serial.print(" %, Kecepatan Kipas: ");
  Serial.print(fan);
  Serial.println(" %");

  analogWrite(KIPAS_PIN, fan); // Control the fan speed

  // MQTT Publish
  char msg[10];

  snprintf(msg, 10, "%d", (int)suhu);
  client.publish("SRT/Suhu", msg); 

  snprintf(msg, 10, "%d", (int)kelembapan);
  client.publish("SRT/Kelembapan", msg); 

  snprintf(msg, 10, "%d", (int)fan);
  client.publish("SRT/Kipas", msg);

  delay(10000);
}
