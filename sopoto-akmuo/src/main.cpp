#include <Arduino.h>
#include <WebServer.h>
#include <AsyncWebSocket.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "SPIFFS.h"
#include "StringTokenizer.h"
#include "WiFiManager.h"
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
bool is_wifi_connected = false;

WiFiManager wifi_manager;
String ssid;
String wifissidprefix = FPSTR("LANDSORT_");

AsyncWebServer server(81);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
HTTPClient http;

extern uint16_t yearly_temps[];

class Color {
 public:
  Color() : r(0), g(0), b(0) {}
  Color(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
  uint8_t r, g, b;
  String asString() {
    return String(r) + String(" ") + String(g) + String(" ") + String(b);
  }
};

Color warm_color(255, 0, 0);
Color cold_color(0, 0, 255);
Color equal_color(255, 255, 255);
Color fallback_color(64, 64, 64);
Color todays_color(0, 0, 0);

void GetHistoricData() {
  // http://satbaltyk.iopan.gda.pl/files/exports/sopot_sst_mean_daily_average.txt
  http.begin(
      "http://satbaltyk.iopan.gda.pl/files/exports/"
      "sopot_sst_mean_daily_average.txt");
  String payload = http.getString();
  int httpResponseCode = http.GET();
  Serial.print(payload);
}

#define TEMPERATURE_ERROR INT32_MIN

int32_t GetTodayMeanTemperature() {
  http.begin("http://satbaltyk.iopan.gda.pl/files/exports/sopot_sst_mean.txt");
  if (HTTP_CODE_OK != http.GET()) {
    return TEMPERATURE_ERROR;
  }

  String payload = http.getString();
  // Serial.print(payload);

  StringTokenizer lines(payload, "\n");
  lines.nextToken();
  if (!lines.hasNext()) {
    return TEMPERATURE_ERROR;
  }

  String data_str = lines.nextToken();
  StringTokenizer chunks(data_str, ",");
  if (!chunks.hasNext()) {
    return TEMPERATURE_ERROR;
  }
  String temp_str = chunks.nextToken();
  float temp = atof(temp_str.c_str());

  return (int32_t)(temp * 1000);
}

void FadeIn() {}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

int32_t GetYearDay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return -1;
  }

  return timeinfo.tm_yday;
}

void InitSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
  File f = SPIFFS.open("/colors", "r");
  if (f.size() == 6) {
    Serial.println("Color file opened");
    char bytes[6];
    f.readBytes(bytes, 6);
    warm_color = Color(bytes[0], bytes[1], bytes[2]);
    cold_color = Color(bytes[3], bytes[4], bytes[5]);
    Serial.println(String("Warm color: ") + warm_color.asString());
    Serial.println(String("Cold color: ") + cold_color.asString());
  } else {
    Serial.println("Color file is empty, populating");
    if (f.size() > 6) {
      f.close();
      SPIFFS.remove("/colors");
      f = SPIFFS.open("/colors", "w+", true);
    }

    f.write(255);
    f.write(0);
    f.write(0);

    f.write(0);
    f.write(0);
    f.write(255);
  }
  f.close();
}

void SaveColors() {
  File f = SPIFFS.open("/colors", "w");
  f.write(warm_color.r);
  f.write(warm_color.g);
  f.write(warm_color.b);
  f.write(cold_color.r);
  f.write(cold_color.g);
  f.write(cold_color.b);
  f.close();
}

#include <Arduino_JSON.h>
AsyncWebSocket ws("/ws");
JSONVar readings;

String getSensorReadings() {
  readings["rssi"] = String(WiFi.RSSI());
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len &&
      info->opcode == WS_BINARY) {
    Serial.println(String(data[0]) + String(" ") + String(data[1]) +
                   String(" ") + String(data[2]) + String(" ") +
                   String(data[3]));

    if (0 == data[0]) {
      warm_color.r = data[1];
      warm_color.g = data[2];
      warm_color.b = data[3];
      todays_color = warm_color;
      SaveColors();
    }
    if (1 == data[0]) {
      cold_color.r = data[1];
      cold_color.g = data[2];
      cold_color.b = data[3];
      todays_color = cold_color;
      SaveColors();
    }
    // ws.textAll(getSensorReadings());
  }
}

void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
             AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(),
                    client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void InitWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void InitWiFi() {
  ssid = wifissidprefix + String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
  WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP);
  Serial.println(ssid);
  // wifi_manager.resetSettings();
  wifi_manager.setAPStaticIPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1),
                                   IPAddress(255, 255, 255, 0));
  wifi_manager.setConfigPortalBlocking(false);
  wifi_manager.setSaveConfigCallback([]() { is_wifi_connected = true; });
  wifi_manager.setHostname("landsort");
  std::vector<const char*> menu = {"wifi",  "info",   "sep",
                                   "erase", "update", "sep"};
  wifi_manager.setMenu(menu);
  is_wifi_connected = wifi_manager.autoConnect(ssid.c_str());
}

void InitHttpServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });
  server.serveStatic("/", SPIFFS, "/");
  server.begin();
}

void InitTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

bool IsYearDaySane(int32_t year_day) {
  return year_day >= 0 && year_day <= 366;
}

#include <ArduinoDMX.h>
#include <ArduinoRS485.h>  // the ArduinoDMX library depends on ArduinoRS485

const int universeSize = 16;

void SendColor(Color c) {
  // Serial.println(String("Color:") + c.r + String(" ") + c.g + String(" ") +
  // c.b);
  DMX.beginTransmission();
  DMX.write(1, c.r);
  DMX.write(2, c.g);
  DMX.write(3, c.b);
  DMX.endTransmission();
}

Color DecideColor(int32_t year_day, int32_t today_temp) {
  if (!IsYearDaySane(year_day)) {
    Serial.println("Could not fetch date info");
    return fallback_color;
  }
  Serial.println(String("Day of year:") + year_day);

  if (today_temp == TEMPERATURE_ERROR) {
    Serial.println("Could not fetch todays temperature");
    return fallback_color;
  }
  Serial.println(String("Todays mean temperature: ") + today_temp);

  Serial.println(String("Historic temperature mean for today:") +
                 yearly_temps[year_day]);

  if (yearly_temps[year_day] > today_temp) {
    Serial.println(String("Colder today ") + cold_color.asString());
    return cold_color;
  }
  if (yearly_temps[year_day] < today_temp) {
    Serial.println(String("Warmer today ") + warm_color.asString());
    return warm_color;
  }
  Serial.println("Today is the same as every day");
  return equal_color;
}

unsigned long wifi_connect_start_time = 0;

void setup(void) {
  Serial.begin(9600);
  DMX.begin(universeSize);
  InitSPIFFS();
  InitWiFi();
  wifi_connect_start_time = millis();
}

unsigned long last_websocket_time = 0;
constexpr unsigned long websocket_send_interval = 500;
bool color_setup_done = false;

void loop(void) {
  if ((millis() - last_websocket_time) > websocket_send_interval) {
    ws.textAll(getSensorReadings());
    ws.cleanupClients();
    last_websocket_time = millis();
  }
  
  SendColor(todays_color);

  wifi_manager.process();

  if (is_wifi_connected && !color_setup_done) {
    color_setup_done = true;
    StartWifiManagerPortal();
    InitWebSocket();
    InitHttpServer();
    InitTime();
  
    uint32_t counter = 0;
    int32_t year_day = -1;
    do {
      year_day = GetYearDay();
      if (!IsYearDaySane(year_day)) {
        delay(100);
      }
    } while(!IsYearDaySane(year_day) && counter++ < 3);

    int32_t today_temp = TEMPERATURE_ERROR;
    do {
      today_temp = GetTodayMeanTemperature();
      if(TEMPERATURE_ERROR == today_temp) {
        delay(100);
      }
    } while(TEMPERATURE_ERROR == today_temp);
    
    todays_color = DecideColor(year_day, today_temp);
  }

  if (!is_wifi_connected && (millis() - wifi_connect_start_time) > 5000) {
    todays_color = fallback_color;
  }
}

void StartWifiManagerPortal() {
  const char* hostname = "landsort";
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
  }
  Serial.println("Initialising WiFi manager web portal");
  std::vector<const char*> menu = {"wifi",   "info", "sep",   "erase",
                                   "update", "sep",  "custom"};
  wifi_manager.setMenu(menu);
  wifi_manager.setCustomMenuHTML(
      "<a href=\"#\" "
      "onclick=\"window.open(`http://${window.location.hostname}:81/`)\" "
      "><button>Deep</button></a><br>");
  wifi_manager.startWebPortal();
}
