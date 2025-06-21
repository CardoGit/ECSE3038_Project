#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"

// WiFi Credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Server Configuration
const char* serverUrl = "https://slovakia-experience-tracked-improve.trycloudflare.com";
String settingsEndpoint = "/settings";
String tempEndpoint = "/temperature"; 

// LED Pins
const int coolingLED = 23;    // Fan control (temperature)
const int lightingLED = 22;   // Light control
const int temp_pin = 4;
const int pir_pin = 15;

// Temperature sensor setup
OneWire oneWire(temp_pin);
DallasTemperature tempSensors(&oneWire);

// Variables to display
float userTempTrigger = 0.0;
String userLightTime = "";
String lightTimeOff = "";
bool isSunsetMode = false;
String originalLightSetting = "";

float currentTemp;     
bool presenceDetected ; 

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;   
const int daylightOffset_sec = 0;

unsigned long lastUpdateTime = 0;
const long updateInterval = 5000; // Update every 5 seconds

void setup() {
  Serial.begin(115200);
  
  // Initialize LED pins
  pinMode(coolingLED, OUTPUT);
  pinMode(lightingLED, OUTPUT);
  pinMode(pir_pin, INPUT);
  digitalWrite(coolingLED, LOW);
  digitalWrite(lightingLED, LOW);
  
  // Initialize temperature sensor
  tempSensors.begin();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

// Helper function to get current time as string
String getCurrentTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00"; //returns the current date and time as an ISO 8601 formatted string
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);  // No microseconds
  return String(timeString);
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Read sensors
  currentTemp = readTemperature();
  presenceDetected = detectPresence();

  // Update settings from server at regular intervals, avoid overwhelming the server.
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;
    
    if (WiFi.status() == WL_CONNECTED) {
      getSettingsFromServer();
      controlLEDs();
      displayStatusAndSettings();

      sendSensorData(currentTemp, presenceDetected);
    } else {
      Serial.println("WiFi Disconnected");
    }
  }
}

float readTemperature() {
  tempSensors.requestTemperatures();
  return tempSensors.getTempCByIndex(0);
}

bool detectPresence() {
  return digitalRead(pir_pin);
}

// void controlLEDs() {
//   // Get current time
//   struct tm timeinfo;
//   if(!getLocalTime(&timeinfo)){
//     Serial.println("Failed to obtain time");
//     return;
//   }
  
//   // Convert current time to seconds since midnight for comparison
//   unsigned long currentTimeSec = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  
//   // Convert light on/off times to seconds since midnight
//   unsigned long lightOnSec = timeStringToSeconds(userLightTime);
//   unsigned long lightOffSec = timeStringToSeconds(lightTimeOff);
  
//   // Determine if we're in the light duration period
//   bool inLightDuration;
//   if (lightOnSec < lightOffSec) {
//     // Normal case (on and off same day)
//     inLightDuration = (currentTimeSec >= lightOnSec && currentTimeSec < lightOffSec);
//   } else {
//     // Wrap around midnight (on today, off tomorrow)
//     inLightDuration = (currentTimeSec >= lightOnSec || currentTimeSec < lightOffSec);
//   }
  
//   // Control cooling LED (pin 22) - temperature and presence
//   digitalWrite(coolingLED, (currentTemp >= userTempTrigger && presenceDetected) ? HIGH : LOW);
  
//   // Control lighting LED (pin 23) - presence and time
//   digitalWrite(lightingLED, (presenceDetected && inLightDuration) ? HIGH : LOW);

// //   if (inLightDuration) {
// //   digitalWrite(lightingLED, HIGH); // Keep light ON during the full duration
// // } else {
// //   digitalWrite(lightingLED, presenceDetected ? HIGH : LOW); // After duration, check for presence
// // }

// }


void controlLEDs() {
  // Get current time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  // Convert current time to seconds since midnight for comparison
  unsigned long currentTimeSec = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
  
  // Convert light on/off times to seconds since midnight
  unsigned long lightOnSec = timeStringToSeconds(userLightTime);
  unsigned long lightOffSec = timeStringToSeconds(lightTimeOff);
  
  // Determine if we're in the light duration period
  bool inLightDuration;
  if (lightOnSec < lightOffSec) {
    // Normal case (on and off same day)
    inLightDuration = (currentTimeSec >= lightOnSec && currentTimeSec < lightOffSec);
  } else {
    // Wrap around midnight (on today, off tomorrow)
    inLightDuration = (currentTimeSec >= lightOnSec || currentTimeSec < lightOffSec);
  }
  
  // Control cooling LED (pin 22) - temperature and presence
  digitalWrite(coolingLED, (currentTemp >= userTempTrigger && presenceDetected) ? HIGH : LOW);
  
  // Control lighting LED (pin 23)
  if (inLightDuration) {
    digitalWrite(lightingLED, HIGH); // Keep light ON during the full duration
  } else {
    digitalWrite(lightingLED, presenceDetected ? HIGH : LOW); // After duration, check for presence
  }
}

void displayStatusAndSettings() {
  // Get current time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  char currentTime[9];
  strftime(currentTime, sizeof(currentTime), "%H:%M:%S", &timeinfo);
  
  // Print header
  Serial.println("\n========================================");
  Serial.println("         SMART HUB STATUS REPORT");
  Serial.println("========================================");
  
  // Current Sensor Readings Section
  Serial.println("\n--- CURRENT SENSOR READINGS ---");
  Serial.print("Time:            ");
  Serial.println(currentTime);
  Serial.print("Temperature:     ");
  Serial.print(currentTemp);
  Serial.println("°C");
  Serial.print("Presence:        ");
  Serial.println(presenceDetected ? "Detected" : "Not Detected");
  
  // Current Settings Section
  Serial.println("\n--- CURRENT SETTINGS ---");
  Serial.print("Temperature Trigger: ");
  Serial.print(userTempTrigger);
  Serial.println("°C");
  Serial.print("Light Trigger Time:  ");
  Serial.println(userLightTime);
  Serial.print("Light Turn Off Time: ");
  Serial.println(lightTimeOff);
  Serial.print("Original Light Setting: ");
  Serial.println(originalLightSetting);
  Serial.print("Sunset Mode:       ");
  Serial.println(isSunsetMode ? "Active" : "Inactive");
  
  // Device Control Section
  Serial.println("\n--- DEVICE CONTROL STATUS ---");
  Serial.print("Cooling/Fan (Pin 22): ");
  Serial.print(digitalRead(coolingLED) ? "ON" : "OFF");
  Serial.print(" - Condition: ");
  Serial.print(currentTemp);
  Serial.print("°C ");
  Serial.print((currentTemp >= userTempTrigger) ? "≥ " : "< ");
  Serial.print(userTempTrigger);
  Serial.print("°C and presence ");
  Serial.println(presenceDetected ? "detected" : "not detected");
  
  Serial.print("Lighting (Pin 23):    ");
  Serial.print(digitalRead(lightingLED) ? "ON" : "OFF");
  Serial.print(" - Condition: ");
  Serial.print(presenceDetected ? "Presence detected" : "No presence");
  Serial.print(" and current time (");
  Serial.print(currentTime);
  Serial.print(") is ");
  Serial.print(digitalRead(lightingLED) ? "within " : "outside ");
  Serial.print(userLightTime);
  Serial.print(" to ");
  Serial.println(lightTimeOff);
  
  // Footer
  Serial.println("========================================");
  Serial.println("Next update in 5 seconds...");
  Serial.println("========================================\n");
}

unsigned long timeStringToSeconds(String timeStr) {
  int hours = timeStr.substring(0, 2).toInt();
  int minutes = timeStr.substring(3, 5).toInt();
  int seconds = timeStr.substring(6, 8).toInt();
  return hours * 3600 + minutes * 60 + seconds;
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}


void sendSensorData(float temperature, bool presence) {
  HTTPClient http;
  
  String fullUrl = serverUrl + tempEndpoint;
  http.begin(fullUrl);
  
  // Create JSON payload with both temperature and presence
  DynamicJsonDocument doc(128);
  doc["temperature"] = temperature;
  doc["presence"] = presence;
  doc["timestamp"] = getCurrentTimeString();
  
  String payload;
  serializeJson(doc, payload);
  
  // Send HTTP POST request
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);
  
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Sensor data sent successfully");
  } else {
    Serial.print("Error sending sensor data. HTTP code: ");
    Serial.println(httpCode);
  }
  
  http.end();
}


void getSettingsFromServer() {
  HTTPClient http;
  
  String fullUrl = serverUrl + settingsEndpoint;
  http.begin(fullUrl);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    userTempTrigger = doc["user_temp"].as<float>();
    userLightTime = doc["user_light"].as<String>();
    lightTimeOff = doc["light_time_off"].as<String>();
    
    // Check if sunset mode is active
    isSunsetMode = (doc.containsKey("original_light") && doc["original_light"] == "sunset");
    originalLightSetting = isSunsetMode ? "sunset" : userLightTime;
    
  } else {
    Serial.print("Error getting settings. HTTP code: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

