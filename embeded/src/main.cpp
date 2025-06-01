#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi Credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Server Configuration
const char* serverUrl = "https://7dd8-208-131-174-130.ngrok-free.app";

String settingsEndpoint = "/settings";
String sensorDataEndpoint = "/sensor-data";
String graphEndpoint = "/graph";

// Pin assignments
#define TEMP_PIN 4          // DS18B20 temperature sensor pin
#define PIR_PIN 15          // PIR sensor pin
#define fanPin 23           // Fan control pin
#define lightPin 22         // Light control pin

// Temperature sensor setup
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensors(&oneWire);

// System state variables
float currentTemp = 0.0;
float userTempTrigger = 25.0;    // Default value
bool fanState = false;
bool presenceState = false;
bool lightState = false;
String userLightTime = "18:30:00";
String lightTimeOff = "22:30:00";
String currentTime = "00:00:00";  // Will be updated in loop

// Timing variables
unsigned long lastUpdateTime = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastDataSendTime = 0;
const long updateInterval = 5000; // Update every 5 seconds
const long timeUpdateInterval = 60000; // Update time every minute
const long dataSendInterval = 5000; // Send sensor data every 30 seconds

void setup() {
  Serial.begin(115200);
  
  // Initialize control pins
  pinMode(fanPin, OUTPUT);
  pinMode(lightPin, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  
  // Initialize temperature sensor
  tempSensors.begin();
  
  // Start with devices off
  digitalWrite(fanPin, LOW);
  digitalWrite(lightPin, LOW);
  
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

  // Initialize current time (in real implementation, get from NTP)
  updateCurrentTime();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Read sensors
  currentTemp = readTemperature();
  presenceState = detectPresence();
  
  // Update current time periodically
  if (currentMillis - lastTimeUpdate >= timeUpdateInterval) {
    lastTimeUpdate = currentMillis;
    updateCurrentTime();
  }
  
  // Send sensor data to server periodically
  if (currentMillis - lastDataSendTime >= dataSendInterval) {
    lastDataSendTime = currentMillis;
    if (WiFi.status() == WL_CONNECTED) {
      sendSensorData();
    }
  }
  
  // Update settings from server at regular intervals
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;
    
    if (WiFi.status() == WL_CONNECTED) {
      getSettingsFromServer();
      
      // Display current status
      printSystemStatus();
      
      // Control devices based on combined logic
      controlDevices();
    } else {
      Serial.println("WiFi Disconnected");
      // Safety measure - turn devices off if WiFi disconnects
      emergencyShutdown();
    }
  }
}

void sendSensorData() {
  HTTPClient http;
  
  String fullUrl = serverUrl + sensorDataEndpoint;
  http.begin(fullUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  DynamicJsonDocument doc(256);
  doc["temperature"] = currentTemp;
  doc["presence"] = presenceState;
  doc["datetime"] = getIsoTimestamp();
  
  String payload;
  serializeJson(doc, payload);
  
  int httpCode = http.POST(payload);
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("Sensor data sent successfully");
  } else {
    Serial.print("Error sending sensor data. HTTP code: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

String getIsoTimestamp() {
  // Simulate ISO timestamp (in real implementation, use actual time)
  // Format: "2023-02-23T18:22:28"
  return "2023-" + String(random(1, 13)) + "-" + String(random(1, 29)) + "T" + 
         currentTime.substring(0, 2) + ":" + currentTime.substring(3, 5) + ":00";
}

void updateCurrentTime() {
  // Simulate time progression (in real implementation, use NTP or RTC)
  static int hours = 0;
  static int minutes = 0;
  
  minutes++;
  if (minutes >= 60) {
    minutes = 0;
    hours++;
    if (hours >= 24) {
      hours = 0;
    }
  }
  
  // Format as HH:MM:SS
  currentTime = String(hours < 10 ? "0" + String(hours) : String(hours)) + ":" +
                String(minutes < 10 ? "0" + String(minutes) : String(minutes)) + ":00";
}

float readTemperature() {
  tempSensors.requestTemperatures();
  return tempSensors.getTempCByIndex(0);
}

bool detectPresence() {
  return digitalRead(PIR_PIN);
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
    
    Serial.println("Successfully updated settings from server");
  } else {
    Serial.print("Error getting settings. HTTP code: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

void controlDevices() {
  // FAN CONTROL: Only turn on if temperature > threshold AND someone is present
  if (currentTemp > userTempTrigger && presenceState) {
    if (!fanState) {
      digitalWrite(fanPin, HIGH);
      fanState = true;
      Serial.println("Conditions met - turning fan ON (Temp high + Presence detected)");
    }
  } else {
    if (fanState) {
      digitalWrite(fanPin, LOW);
      fanState = false;
      Serial.println("Conditions not met - turning fan OFF");
    }
  }

  // LIGHT CONTROL: Only turn on if during light time AND someone is present
  bool isLightTime = isTimeInRange(currentTime, userLightTime, lightTimeOff);
  
  if (isLightTime && presenceState) {
    if (!lightState) {
      digitalWrite(lightPin, HIGH);
      lightState = true;
      Serial.println("Conditions met - turning lights ON (Correct time + Presence detected)");
    }
  } else {
    if (lightState) {
      digitalWrite(lightPin, LOW);
      lightState = false;
      if (!isLightTime) {
        Serial.println("Turning lights OFF - Outside scheduled time");
      } else {
        Serial.println("Turning lights OFF - No presence detected");
      }
    }
  }
}

bool isTimeInRange(String checkTime, String startTime, String endTime) {
  // Convert all times to seconds since midnight
  int checkSec = timeToSeconds(checkTime);
  int startSec = timeToSeconds(startTime);
  int endSec = timeToSeconds(endTime);
  
  // Handle overnight ranges (like 22:00 to 06:00)
  if (startSec < endSec) {
    return (checkSec >= startSec && checkSec < endSec);
  } else {
    return (checkSec >= startSec || checkSec < endSec);
  }
}

int timeToSeconds(String timeStr) {
  // Convert HH:MM:SS to total seconds
  int hours = timeStr.substring(0, 2).toInt();
  int minutes = timeStr.substring(3, 5).toInt();
  int seconds = timeStr.substring(6, 8).toInt();
  
  return hours * 3600 + minutes * 60 + seconds;
}

void printSystemStatus() {
  Serial.println("\n===== System Status =====");
  Serial.print("Current Time: ");
  Serial.println(currentTime);
  
  Serial.print("Current Temperature: ");
  Serial.print(currentTemp);
  Serial.print("°C | Trigger Temp: ");
  Serial.print(userTempTrigger);
  Serial.println("°C");
  
  Serial.print("Presence: ");
  Serial.println(presenceState ? "Detected" : "Not detected");
  
  Serial.print("Light Schedule: ");
  Serial.print(userLightTime);
  Serial.print(" to ");
  Serial.println(lightTimeOff);
  
  Serial.print("Is Light Time Now: ");
  Serial.println(isTimeInRange(currentTime, userLightTime, lightTimeOff) ? "Yes" : "No");
  
  Serial.print("Fan State: ");
  Serial.println(fanState ? "ON (Temp high + Presence)" : "OFF (Conditions not met)");
  
  Serial.print("Light State: ");
  Serial.println(lightState ? "ON (Correct time + Presence)" : "OFF (Conditions not met)");
  Serial.println("=======================\n");
}

void emergencyShutdown() {
  if (fanState) {
    digitalWrite(fanPin, LOW);
    fanState = false;
  }
  if (lightState) {
    digitalWrite(lightPin, LOW);
    lightState = false;
  }
}
