#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>


#define PIR_PIN 15
#define TEMP_PIN 4
#define FAN_PIN 23
#define LIGHT_PIN 22

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* serverUrl = "https://2351-208-131-174-130.ngrok-free.app";

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

unsigned long lastPost = 0;
const long interval = 10000; // 10 seconds

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  
  sensors.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void loop() {
  unsigned long now = millis();
  if (now - lastPost > interval) {
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    bool presence = digitalRead(PIR_PIN);

    Serial.printf("Temp: %.2f, Presence: %d\n", tempC, presence);

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      String body = "{\"temperature\": " + String(tempC, 2) + 
                    ", \"presence\": " + String(presence ? "true" : "false") + "}";

      int httpResponseCode = http.POST(body);
      Serial.println(httpResponseCode); 

      
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Received response:");
        Serial.println(response);

        // Parse instructions (very basic)
        if (response.indexOf("\"fan\":true") > 0) digitalWrite(FAN_PIN, HIGH);
        else digitalWrite(FAN_PIN, LOW);

        if (response.indexOf("\"light\":true") > 0) digitalWrite(LIGHT_PIN, HIGH);
        else digitalWrite(LIGHT_PIN, LOW);
      } else {
        Serial.println("Error sending POST");
      }

      http.end();
    }
    lastPost = now;
  }
}