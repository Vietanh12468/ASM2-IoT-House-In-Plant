
#define BLYNK_TEMPLATE_ID "**********"
#define BLYNK_TEMPLATE_NAME "**********"
#define BLYNK_AUTH_TOKEN "**********"

#define BLYNK_PRINT Serial

#include <DHT.h>  
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

char ssid[] = "HHBC";
char pass[] = "0904612256vn";

// char ssid[] = "iPhone";
// char pass[] =  "hoze404lol";

#define sensorSoil D2
#define sensorTemp 14
#define sensorPin A0
#define relay D6
#define LED D1

#define DHTTYPE DHT11
DHT dht(sensorTemp, DHTTYPE);

int dailyWater = 5000;
int lightMode;
int waterMode;
int moistPercentage;
int record;
float temperature;
String tempDate;
String tempTime;

void setup() {
  dht.begin();  
  SPIFFS.begin();

  pinMode(LED, OUTPUT);
  pinMode(sensorSoil, OUTPUT);
  pinMode(relay, OUTPUT);

  digitalWrite(sensorSoil, LOW);
  digitalWrite(LED, LOW);
  digitalWrite(relay, LOW);

  Serial.begin(9600);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();
  analogReadD5();
  analogReadD2();
  GetCurrentTime();
  if (tempTime.substring(3).equals("00")) {
    addLineToFile("/Moist.txt", String(moistPercentage));
    addLineToFile("/Temp.txt", String(temperature));
  }
  if (tempTime.equals("18:00")) {
    float Avgmoist = CalculateTotal("/Moist.txt") / record;
    float Avgtemp = CalculateTotal("/Temp.txt") / record;
    float Totalwateruse = CalculateTotal("/WaterUse.txt") / 100;

    String report = "In: " + tempDate + ", Average moisture (%): " + String(Avgmoist) + ", Average temperature (C): " + String(Avgtemp) + "C" + ", Total water use: " + String(Totalwateruse);
    Serial.print("Plant report: ");
    Serial.println(report);
    resetAllFile();

    Blynk.logEvent("report", report);
  }
  if (tempTime.equals("06:00")) {
    GetTempNextDay();
    if(waterMode == 1 && moistPercentage < 70){
      Serial.println("Start daily water");
      digitalWrite(relay, HIGH);
      delay(dailyWater);
      digitalWrite(relay, LOW);
      addLineToFile("/WaterUse.txt", String(dailyWater));
      delay(30000);
    }
  }
  if (tempTime.compareTo("06:00") > 0 && tempTime.compareTo("18:00") < 0 && lightMode == 1) {
    digitalWrite(LED, HIGH);
  }
  else{
    digitalWrite(LED, LOW);
  }
  delay(30000);
}

void resetAllFile() {
  SPIFFS.remove("/Moist.txt");
  SPIFFS.remove("/Temp.txt");
  SPIFFS.remove("/WaterUse.txt");
  File file = SPIFFS.open("/WaterUse.txt", "a");
  file.close();
}

void addLineToFile(String filename, String line) {
  File file = SPIFFS.open(filename, "a");
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  file.println(line);
  file.close();
}

float CalculateTotal(String filename) {
  record = 0;
  float total = 0;
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return 0;
  }
  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line);
    if (!isnan(line.toFloat())) {
      total = total + line.toFloat();
      record = record + 1;
    } 
  }
  file.close();
  return total;
}

void GetCurrentTime(){
  WiFiClient client;
  HTTPClient http;

  // Make a GET request to the WorldTime API
  http.begin(client, "http://worldtimeapi.org/api/ip");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.println("Failed to parse JSON");
      return;
    }
    const char* datetime = doc["datetime"];
    // Extract the date and hour-minute from datetime
    String datetimeString = String(datetime);
    tempDate = datetimeString.substring(0, 10);
    tempTime = datetimeString.substring(11, 16);

    Serial.println("Time: " + tempTime);
    Serial.println("Date: " + tempDate);
  }
  else {
    Serial.print("HTTP request failed with error code: ");
    Serial.println(httpCode);
  }
  http.end();
}

void GetTempNextDay(){
  WiFiClient client;
  HTTPClient http;

  // Make a GET request to the Weather API
  http.begin(client, "http://api.weatherapi.com/v1/forecast.json?key=**********&q=HaNoi&dt=" + tempDate + "&hour=12");
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {

      DynamicJsonDocument jsonDoc(4096); 
      DeserializationError error = deserializeJson(jsonDoc, http.getString());

      if (!error) {
        JsonArray hourlyArray = jsonDoc["forecast"]["forecastday"][0]["hour"];

        for (JsonObject hour : hourlyArray) {
          String time = hour["time"];
          float tempC = hour["temp_c"];
          int chanceOfRain = hour["chance_of_rain"];
          int humidity = hour["humidity"];
          int lightStrength = hour["uv"];
          if (humidity >= 80){
            dailyWater = 3000;
          }
          else {
            dailyWater = 5000;
          }

          String report = "In: " + time + ", Temperature (C): " + String(tempC) + ", Chance of Rain: " + String(chanceOfRain) + "%" + ", Humidity: " + String(humidity) + "%" + ", Light Strength: " + String(lightStrength);
          Serial.print("weather report: ");
          Serial.println(report);

          Blynk.logEvent("report", report);
        }
      } else {
        Serial.print("Error during parsing: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.println("HTTP request failed");
    }
  } else {
    Serial.println("Unable to connect");
  }
  http.end();
}

void analogReadD2() {
  digitalWrite(sensorSoil, HIGH); 
  delay(10);  
  int moisture = analogRead(sensorPin);
  digitalWrite(sensorSoil, LOW); 
  moistPercentage = map(moisture, 200, 800, 100, 0);
  Serial.print("Soil moist: ");
  Serial.println(moistPercentage);
  Blynk.virtualWrite(V2, moistPercentage);
  if (moistPercentage <= 10){
    Blynk.logEvent("plant_problem", "Plant is low on water, need water now!");
  }
}

void analogReadD5() {
  digitalWrite(sensorTemp, HIGH);
  delay(10);  
  temperature = dht.readTemperature();

  if (isnan(temperature)) {
    Serial.println("Failed to read data from DHT sensor!");
  }
  else {
    Blynk.virtualWrite(V3, temperature);
    if (temperature <= 10){
      Blynk.logEvent("plant_problem", "Plant is too cold, Need a warmer place");
    }
    else if (temperature >= 30){
      Blynk.logEvent("plant_problem", "Plant is too hot, Need a cooler place");
    }
    Serial.print("Temperature  (C): ");
    Serial.println(temperature);  
  }
  digitalWrite(sensorTemp, LOW);
}

BLYNK_WRITE(V4) {
  if(moistPercentage < 70){
    int wateringState = param.asInt();
    int watering = 1000 * wateringState;
    Serial.println(watering);
    digitalWrite(relay, HIGH);
    Serial.println("Water pump: On");
    delay(watering); 
    addLineToFile("/WaterUse.txt", String(watering));
    digitalWrite(relay, LOW);
    Serial.println("Water pump: Off");
  }
}

BLYNK_WRITE(V1)
{
  lightMode = param.asInt();
  Serial.print("LightMode: ");
  Serial.println(lightMode);
}

BLYNK_WRITE(V6)
{
  waterMode = param.asInt();
  Serial.print("WaterMode: ");
  Serial.println(waterMode);
}
