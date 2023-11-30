#include <Arduino.h>

#include "WiFiManager.h"

WiFiManager wifi_manager;
String ssid;
String wifissidprefix = FPSTR("LANDSORT_");

void setup(void) {
  Serial.begin(9600);
  ssid = wifissidprefix + String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
  WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP);
  Serial.println(ssid);
  wifi_manager.resetSettings();
  wifi_manager.setAPStaticIPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1),
                                   IPAddress(255, 255, 255, 0));
  // wifi_manager.setConfigPortalTimeout(120);
  bool is_wifi_connected = wifi_manager.autoConnect(ssid.c_str());
}

void loop(void) {
}
