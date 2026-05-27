#include <GxEPD2_3C.h> //GxEPD2
#include <Fonts/FreeMonoBold9pt7b.h>
#include <DHT.h> //DHT sensor library
#include "pitches.h"
#include <WiFi.h>
#include "time.h"
#include <TimeLib.h> //Time (timelib)
#include <HTTPClient.h>
#include <ArduinoJson.h> //ArduinoJson
#include <ESP32Servo.h>  //ESP32Servo

// ================= DISPLAY =================
#define EPD_SS 5
#define EPD_DC 17
#define EPD_RST 16
#define EPD_BUSY 4

#define MAX_DISPLAY_BUFFER_SIZE 800
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))

GxEPD2_3C<GxEPD2_290_C90c, MAX_HEIGHT(GxEPD2_290_C90c)>
    display(GxEPD2_290_C90c(EPD_SS, EPD_DC, EPD_RST, EPD_BUSY));

volatile int page = 0;

// ================= DHT SENSOR =================
#define DHTPIN 32
#define DHTTYPE DHT11 // use DHT22 if needed

DHT dht(DHTPIN, DHTTYPE);

// ================= DC MOTOR =================
#define ENABLE 27
#define DIRA 25
#define DIRB 26

// ================= Servo MOTOR =================
#define LDRPIN 34 // analog pin für licht abhängiger wiederstand
Servo myservo;

// ================= TEMPERATUR SENSOR =================
volatile float temperature = 0;
volatile float humidity = 0;
String fanStatus = "Ventilaror aus";

// timing (non-blocking style)
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 8000; // 8 seconds

// ================= SOUND =================
// notes in the melody:
int melody[] = {
    NOTE_C5, NOTE_D5, NOTE_E5, NOTE_F5, NOTE_G5, NOTE_A5, NOTE_B5, NOTE_C6};
int duration = 500; // 500 miliseconds
#define SPEAKERPIN 12

// ================= WIFI + Time =================

const char *ssid = "BensGalaxy";
const char *password = "bla12345";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// ================= Alarm =================

int weckStunde = 16;
int weckMinute = 24;
static bool alarmOff = false;
#define ALARM_OFF_BUTTON 33
unsigned long lastReadTimeAlarm = 0;
const unsigned long READ_INTERVAL_ALARM = 1000; // 1 seconds

//================= weather =================
volatile float maxTemp = 0.0;
volatile float minTemp = 0.0;
volatile float rainSum = 0.0;
volatile float snowfallSum = 0.0;
// Aalen
String lat = "48.8378";
String lon = "10.0933";
String urlWeatherForcast = "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon + "&daily=temperature_2m_max,temperature_2m_min,rain_sum,snowfall_sum&timezone=Europe%2FBerlin&forecast_days=3";

// ================= Tasks =================

TaskHandle_t displayHandle = NULL;
TaskHandle_t forcastHandle = NULL;
TaskHandle_t readSensorsHandle = NULL;
TaskHandle_t fanHandle = NULL;
TaskHandle_t servoHandle = NULL;
TaskHandle_t alarmHandle = NULL;

int DISPLAY_DELAY = 8000;
int FORCAST_DELAY = 1000 * 60 * 60 * 24; // once a day
int READ_SENSORS_DELAY = 1000;
int FAN_DELAY = 1000;
int SERVO_DELAY = 1000;
int ALARM_DELAY = 1000;

// ================= DEBUGGING =================
// 0x01 sensors; 0x02 fan; 0x04 servo; 0x08 Display; 0x10 localTime; 0x20 alarm; 0x40 forcast

int print = 0 | 0x04;

// ================= SENSOR =================
void readSensor(void *parameter)
{
  while (true)
  {
    if (print & 0x01)
    {
      Serial.println("Reading sensor...");
    }

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h))
    {
      if (print & 0x01)
      {
        Serial.println("DHT read failed!");
      }
    }

    temperature = t;
    humidity = h;

    if (print & 0x01)
    {
      Serial.print("Temp: ");
      Serial.print(temperature);
      Serial.print(" °C | Hum: ");
      Serial.println(humidity);
    }

    vTaskDelay(pdMS_TO_TICKS(READ_SENSORS_DELAY));
  }
}

// ================= FAN CONTROL =================
void controlFan(void *parameter)
{
  while (true)
  {
    if (print & 0x02)
    {
      Serial.println("Checking temp and setting fan status");
    }

    if (temperature > 28.0)
    {
      digitalWrite(ENABLE, HIGH);
      digitalWrite(DIRA, HIGH);
      digitalWrite(DIRB, LOW);
      fanStatus = "Ventilaror an";
      if (print & 0x02)
      {
        Serial.println(fanStatus);
      }
    }
    else
    {
      digitalWrite(ENABLE, LOW);
      fanStatus = "Ventilaror aus";
      if (print & 0x02)
      {
        Serial.println(fanStatus);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(FAN_DELAY));
  }
}

// ================= Servo CONTROL =================
void controlServo(void *parameter)
{
  while (true)
  {
    if (print & 0x04)
    {
      Serial.println("Handling the servo motor");
    }

    // because of fluctuations adding 3 messurements together
    int value = analogRead(LDRPIN);
    delay(SERVO_DELAY);
    value = +analogRead(LDRPIN);
    delay(SERVO_DELAY);
    value = +analogRead(LDRPIN);
    if (print & 0x04)
    {
      Serial.print("Value of brightness is ");
      Serial.println(value);
    }
    delay(SERVO_DELAY);

    int servoStatus = myservo.read();
    if (print & 0x04)
    {
      Serial.print("Servo read() ");
      Serial.println(servoStatus);
    }

    if (value < 1200 && servoStatus < 175)
    {
      myservo.write(180);
    }
    else if (value > 1200 && value < 2000 && servoStatus < 85 && servoStatus > 95)
    {
      myservo.write(90);
    }
    else if (value > 2000 && servoStatus > 5)
    {
      myservo.write(0);
    }

    vTaskDelay(pdMS_TO_TICKS(SERVO_DELAY));
  }
}

// ================= DISPLAY =================
void updateDisplay(void *parameter)
{
  while (true)
  {
    if (print & 0x08)
    {
      Serial.println("Updating display...");
    }

    display.setFullWindow();
    display.firstPage();

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      display.println("Konnte die Zeit nicht abfragen.");
    }

    do
    {
      if (page == 0)
      {
        if (print & 0x08)
        {
          Serial.println("Printing temp...");
        }

        display.fillScreen(GxEPD_WHITE);

        display.setCursor(20, 40);
        display.print("Temp: ");
        display.print(temperature, 1);
        display.print("C");

        display.setCursor(20, 80);
        display.print("Luftf: ");
        display.print(humidity, 1);
        display.print("%");

        display.setCursor(20, 120);
        display.print(fanStatus);
      }
      else if (page == 1)
      {
        if (print & 0x08)
        {
          Serial.println("Printing time...");
        }

        display.fillScreen(GxEPD_WHITE);

        display.setCursor(20, 40);
        display.println("Heute ist der:");

        display.setCursor(20, 80);
        display.println(&timeinfo, "%A, %d.%m.%Y");

        display.setCursor(20, 120);
        display.println(&timeinfo, "%H:%M:%S Uhr");

        if (print & 0x08)
        {
          Serial.println(&timeinfo, "%A, %d.%m.%Y");
          Serial.println(&timeinfo, "%H:%M:%S Uhr");
        }
      }
      else if (page == 2)
      {
        if (print & 0x08)
        {
          Serial.println("Printing alarm...");
        }

        display.fillScreen(GxEPD_WHITE);

        display.setCursor(20, 40);
        display.println("Der Alarm ist um ");
        display.setCursor(20, 80);

        if (weckStunde < 10)
        {
          display.print("0");
        }
        display.print(weckStunde, 1);
        display.print(":");
        if (weckMinute < 10)
        {
          display.print("0");
        }
        display.print(weckMinute, 1);
        display.println(" Uhr");
        display.setCursor(20, 120);
        display.println("gestellt");
      }
      else if (page == 3)
      {
        if (print & 0x08)
        {
          Serial.println("Printing weather forcast...");
        }

        display.fillScreen(GxEPD_WHITE);

        display.setCursor(20, 40);
        display.print("Das Wetter am ");
        time_t now = time(nullptr);
        time_t tomorrow = now + 60 * 60 * 24;
        struct tm *tomorrow_info = localtime(&tomorrow);
        if (print & 0x08)
        {
          Serial.println(tomorrow_info, "tomorrow: %d.%m.%Y");
        }

        display.println(tomorrow_info, "%d.%m.%Y");

        display.setCursor(20, 80);
        display.print("Max:");
        display.print(maxTemp, 1);
        display.print("C Min:");
        display.print(minTemp, 1);
        display.print("C");

        display.setCursor(20, 120);
        display.print("Nschl.:");
        display.print(rainSum, 1);
        display.print(" Schneef:");
        display.print(snowfallSum, 1);
      }
      else if (page == 4)
      {
        if (print & 0x08)
        {
          Serial.println("Printing Servo status...");
        }

        display.fillScreen(GxEPD_WHITE);

        display.setCursor(20, 40);
        int servoStatus = myservo.read();
        if (print & 0x08)
        {
          Serial.print("Display Servo status is ");
          Serial.println(servoStatus);
        }

        if (servoStatus < 5)
        {
          display.println("Die Jalousie ist offen");
        }
        else if (servoStatus > 85 && servoStatus < 95)
        {
          display.println("Die Jalousie ist");
          display.setCursor(20, 80);
          display.println("halb geschlossen");
        }
        else if (servoStatus > 175)
        {
          display.println("Die Jalousie ist ");
          display.setCursor(20, 80);
          display.println("geschlossen");
        }
        else
        {
          display.println("Jalousie status unbekannt");
        }
      }
    } while (display.nextPage());
    if (page == 0)
    {
      page = 1;
    }
    else if (page == 1)
    {
      page = 2;
    }
    else if (page == 2)
    {
      page = 3;
    }
    else if (page == 3)
    {
      page = 4;
    }
    else if (page == 4)
    {
      page = 0;
    }
    Serial.println("Display updated");
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY));
  }
}

// ================= DEBUG OUTPUT =================
void printLocalTime()
{
  if (print & 0x10)
  {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time");
      return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.print("Day of week: ");
    Serial.println(&timeinfo, "%A");
    Serial.print("Month: ");
    Serial.println(&timeinfo, "%B");
    Serial.print("Day of Month: ");
    Serial.println(&timeinfo, "%d");
    Serial.print("Year: ");
    Serial.println(&timeinfo, "%Y");
    Serial.print("Hour: ");
    Serial.println(&timeinfo, "%H");
    Serial.print("Hour (12 hour format): ");
    Serial.println(&timeinfo, "%I");
    Serial.print("Minute: ");
    Serial.println(&timeinfo, "%M");
    Serial.print("Second: ");
    Serial.println(&timeinfo, "%S");

    Serial.println("Time variables");
    char timeHour[3];
    strftime(timeHour, 3, "%H", &timeinfo);
    Serial.println(timeHour);
    char timeWeekDay[10];
    strftime(timeWeekDay, 10, "%A", &timeinfo);
    Serial.println(timeWeekDay);
    Serial.println();
  }
}

// ================= ALARM =================
void handleAlarm(void *parameter)
{
  while (true)
  {
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (print & 0x20)
    {
      Serial.print("Handling alarm...");
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      display.println("Konnte die Zeit nicht abfragen.");
    }
    if (alarmOff == true)
    {
      if (print & 0x20)
      {
        Serial.println("Alarm is off...");
      }
    }
    if (timeinfo.tm_hour == weckStunde && timeinfo.tm_min == weckMinute && alarmOff == false)
    {
      if (print & 0x20)
      {
        Serial.println("Alarm is buzzing...");
      }

      int thisNote = 0;
      for (int thisNote = 0; thisNote < 8 && alarmOff == false; thisNote++)
      {
        if (alarmOff == false)
        {
          alarmOff = digitalRead(ALARM_OFF_BUTTON) == 1 ? false : true;
        }
        tone(SPEAKERPIN, melody[thisNote], duration);
        vTaskDelay(pdMS_TO_TICKS(500));
      }
    }
    if (alarmOff == true)
    {
      if (timeinfo.tm_hour == weckStunde && timeinfo.tm_min == weckMinute + 1)
      {
        alarmOff = false;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(ALARM_DELAY));
  }
}

// ================= WEATHER FORCAST =================
void readWeatherForcast(void *parameter)
{
  while (true)
  {
    if (print & 0x40)
    {
      Serial.println("Getting weather forcast");
    }

    HTTPClient client;

    client.begin(urlWeatherForcast);
    int httpResonseStatus = client.GET();

    if (httpResonseStatus > 0)
    {
      if (httpResonseStatus == HTTP_CODE_OK)
      {
        String response = client.getString();
        if (print & 0x40)
        {
          Serial.println(response);
        }

        JsonDocument doc;
        deserializeJson(doc, response.c_str());

        maxTemp = doc["daily"]["temperature_2m_max"][1];
        minTemp = doc["daily"]["temperature_2m_min"][1];
        rainSum = doc["daily"]["rain_sum"][1];
        snowfallSum = doc["daily"]["snowfall_sum"][1];
      }
    }
    vTaskDelay(pdMS_TO_TICKS(FORCAST_DELAY));
  }
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  // DC Motor
  pinMode(ENABLE, OUTPUT);
  pinMode(DIRA, OUTPUT);
  pinMode(DIRB, OUTPUT);

  // Servo Motor
  myservo.attach(13);
  myservo.write(0);

  // Sensor
  dht.begin();

  // Display
  display.init();
  display.setRotation(3);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  // alarm button
  pinMode(ALARM_OFF_BUTTON, INPUT_PULLUP);

  // Threads
  xTaskCreatePinnedToCore(readWeatherForcast, "readWeatherTask", 16384, NULL, 1, &forcastHandle, 1);
  xTaskCreatePinnedToCore(updateDisplay, "DisplayTask", 16384, NULL, 2, &displayHandle, 1);
  xTaskCreatePinnedToCore(readSensor, "readSensorTask", 16384, NULL, 1, &readSensorsHandle, 1);
  xTaskCreatePinnedToCore(controlFan, "fanControlTask", 16384, NULL, 1, &fanHandle, 1);
  xTaskCreatePinnedToCore(controlServo, "fanControlTask", 16384, NULL, 1, &servoHandle, 1);
  xTaskCreatePinnedToCore(handleAlarm, "alarmTask", 16384, NULL, 1, &alarmHandle, 1);

  Serial.println("System initialized");
}

// ================= LOOP =================
// Not needed anymore because of threading
void loop()
{
}

/*
  Wetter forcast, Wecker mit lautsprecher, jalousie mit dashboard einstellen + uhr + Sonnenstand
  wetter = https://api.open-meteo.com/v1/forecast?latitude=48.5&longitude=10.6&daily=temperature_2m_max,temperature_2m_min,snowfall_sum,rain_sum&current=rain,showers,snowfall&timezone=Europe%2FBerlin
*/