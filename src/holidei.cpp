#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <time.h>
#include <list>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ArduinoJson.h>

#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

time_t now;
struct tm tmo;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char days[][8] = {
  "Minggu",
  "Senin",
  "Selasa",
  "Rabu",
  "Kamis",
  "Jumat",
  "Sabtu"
};

struct Holiday {
  String name;
  struct tm date;
};

struct Holiday closestHoliday;

std::list<Holiday> holidays;

String padZero(int num);
void updateHolidays(int year);
bool updateClosestHoliday(struct tm referenceDate);
String tmToStringDate(struct tm tmStruct);

void setup() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    for(;;);
  }
  delay(2000);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);

  configTime("WIB-7", "pool.ntp.org");

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while(WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, SCREEN_HEIGHT / 2);
    display.println("Connecting to WiFi...");
    display.display();
    delay(500);
  }

  display.clearDisplay();
}

void loop() {
  while (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, SCREEN_HEIGHT / 2);
    display.println("WiFi disconnected");
    display.display();
    delay(500);
  }
  
  display.clearDisplay();

  time(&now);
  localtime_r(&now, &tmo);

  int currentYear = tmo.tm_year+1900;
  String currentDate = tmToStringDate(tmo);

  String currentTime = padZero(tmo.tm_hour) + ":" + padZero(tmo.tm_min) + ":" + padZero(tmo.tm_sec);

  display.setCursor(0, 0);
  display.print(days[tmo.tm_wday]);
  display.print(", ");
  display.print(currentDate);
  display.setCursor(0, 10);
  display.print(currentTime);

  if (holidays.size() == 0) updateHolidays(currentYear);
  if (closestHoliday.date.tm_year == 0) {
    bool found = updateClosestHoliday(tmo);
    if (!found) {
      updateHolidays(currentYear + 1);
      updateClosestHoliday(tmo);
    }
  }

  display.setCursor(0, 20);
  display.print("Hari libur terdekat: ");
  display.setCursor(0, 30);
  display.print(tmToStringDate(closestHoliday.date));
  display.setCursor(0, 40);
  display.print(closestHoliday.name);

  display.display();
  delay(1000);
}

String padZero(int num) {
  String strNum = String(num);
  if (strNum.length() < 2) {
    return "0" + strNum;
  }
  return strNum;
}

String tmToStringDate(struct tm tmStruct) {
  int monthDay = tmStruct.tm_mday;
  int currentMonth = tmStruct.tm_mon+1;
  int currentYear = tmStruct.tm_year+1900;
  return String(currentYear) + "-" + padZero(currentMonth) + "-" + padZero(monthDay);
}


void updateHolidays(int year) {
  String endpoint = "https://api-harilibur.vercel.app/api?year=" + String(year);
  
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;

  if (https.begin(*client, endpoint)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      JsonDocument jsonDoc;
        String jsonResponse = https.getString();
        deserializeJson(jsonDoc, jsonResponse);
        for (JsonObject item : jsonDoc.as<JsonArray>()) {
          bool is_national_holiday = item["is_national_holiday"];
          if (!is_national_holiday) continue;
          
          struct tm date;
          const char* holiday_date = item["holiday_date"]; // "2025-12-25", "2025-11-29", "2025-11-20", ...
          strptime(holiday_date, "%Y-%m-%d", &date);
          const char* holiday_name = item["holiday_name"]; // "Hari Raya Natal", "Hari Raya Kuningan", "Umanis ...
          // API response is ordered descending with december first
          holidays.push_front(
            {
              holiday_name,
              date
            }
          );
        }
    }
  }
}

bool updateClosestHoliday(struct tm referenceDate) {
  bool found = false;
  for (struct Holiday holiday : holidays) {
    if (referenceDate.tm_mon > holiday.date.tm_mon) continue;
    if (referenceDate.tm_mday > holiday.date.tm_mday) continue;
    closestHoliday.date.tm_year = holiday.date.tm_year;
    closestHoliday.date.tm_mon = holiday.date.tm_mon;
    closestHoliday.date.tm_mday = holiday.date.tm_mday;
    closestHoliday.name = holiday.name;
    found = true;
    break;
  }
  return found;
}
