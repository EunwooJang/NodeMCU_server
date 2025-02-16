#include "WifiConnect.h"

void WifiConnect::connect() {
  // WiFi.begin("SSID1", "PASSWORD1");
  WiFi.begin("SSID2", "PASSWORD2");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("IP Address: ");
  DEBUG_PRINTLN(WiFi.localIP());
}
